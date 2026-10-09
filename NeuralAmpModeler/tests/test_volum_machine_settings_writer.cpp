#include "third_party/doctest.h"
#include "../VoLumMachineSettingsFile.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#if defined(_WIN32)
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <csignal>
  #include <spawn.h>
  #include <sys/wait.h>
  #include <unistd.h>
  #if defined(__APPLE__)
    #include <mach-o/dyld.h>
  #endif
extern char** environ;
#endif

namespace
{

constexpr const char* kHelperTestCase = "volum-machine-settings-lock-helper";
constexpr const char* kHelperPathEnv = "VOLUM_LOCK_HELPER_SETTINGS";

std::filesystem::path TestRoot(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-machine-settings-writer-tests" / name;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root, ec);
  REQUIRE_FALSE(ec);
  return root;
}

nlohmann::json ReadJsonFile(const std::filesystem::path& path)
{
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  nlohmann::json j;
  in >> j;
  return j;
}

nlohmann::json StandaloneDoc(bool lite, bool animate, bool calibrate, double level, int midiCh)
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(
    amps, volum::kAmpCount, 0, nullptr, false, false, false, nullptr, nullptr, lite, calibrate, level, animate);
  j["midiCh"] = midiCh;
  return j;
}

// A plugin instance's machine-file state, as NeuralAmpModeler holds it.
struct Instance
{
  volum::MachineSettingsWriter writer;
  volum::CalibrationEdits edits;
  bool calibrate = false;
  double level = 12.0;

  // _VolumSaveCalibrationDefaults: the edited keys at their live values.
  volum::MachineSettingsWriter::Flush SaveCalibration(const std::filesystem::path& path)
  {
    writer.Queue(edits.TakeKeys(calibrate, level));
    std::error_code ec;
    return writer.FlushPending(path, volum::kMachineSettingsIdleLockMs, 0.0, ec);
  }
};

void SetEnv(const char* name, const std::string& value)
{
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  if (value.empty())
    unsetenv(name);
  else
    setenv(name, value.c_str(), 1);
#endif
}

// This test binary again, running only the helper case below.
class HelperProcess
{
public:
  bool Start()
  {
#if defined(_WIN32)
    std::wstring exe(32768, L'\0');
    exe.resize(GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size())));
    std::wstring cmd =
      L"\"" + exe + L"\" --minimal=true --test-case="
      + std::wstring(kHelperTestCase, kHelperTestCase + std::char_traits<char>::length(kHelperTestCase));
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    // Suspended, so the job owns the child before it runs: if this process dies
    // without reaching ~HelperProcess, Windows still ends the child.
    mJob = CreateJobObjectW(nullptr, nullptr);
    if (mJob != nullptr)
    {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
      limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(mJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    if (!CreateProcessW(
          exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &si, &mInfo))
      return false;
    if (mJob != nullptr)
      AssignProcessToJobObject(mJob, mInfo.hProcess);
    ResumeThread(mInfo.hThread);
    CloseHandle(mInfo.hThread);
    mStarted = true;
    return true;
#else
    std::string exe(4096, '\0');
  #if defined(__APPLE__)
    uint32_t size = static_cast<uint32_t>(exe.size());
    if (_NSGetExecutablePath(exe.data(), &size) != 0)
      return false;
    exe.resize(std::char_traits<char>::length(exe.c_str()));
  #else
    const ssize_t n = readlink("/proc/self/exe", exe.data(), exe.size() - 1);
    if (n <= 0)
      return false;
    exe.resize(static_cast<size_t>(n));
  #endif
    std::string filter = std::string("--test-case=") + kHelperTestCase;
    std::string minimal = "--minimal=true";
    char* argv[] = {exe.data(), minimal.data(), filter.data(), nullptr};
    if (posix_spawn(&mPid, exe.c_str(), nullptr, nullptr, argv, environ) != 0)
      return false;
    mStarted = true;
    return true;
#endif
  }

  int Wait()
  {
    if (!mStarted)
      return -1;
    mStarted = false;
#if defined(_WIN32)
    WaitForSingleObject(mInfo.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(mInfo.hProcess, &code);
    CloseHandle(mInfo.hProcess);
    if (mJob != nullptr)
      CloseHandle(mJob);
    mJob = nullptr;
    return static_cast<int>(code);
#else
    int status = 0;
    if (waitpid(mPid, &status, 0) != mPid)
      return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
  }

  // The child idles up to 30 s waiting for a "release" that an early parent
  // failure never sends. Ends it now instead of letting teardown wait that out.
  void Terminate()
  {
    if (!mStarted)
      return;
#if defined(_WIN32)
    TerminateProcess(mInfo.hProcess, 1);
#else
    kill(mPid, SIGKILL);
#endif
  }

  // Normal paths call Wait() after the child has exited; reaching here still
  // started means the test failed early, so the child is killed, not awaited.
  ~HelperProcess()
  {
    Terminate();
    Wait();
  }

private:
  bool mStarted = false;
#if defined(_WIN32)
  PROCESS_INFORMATION mInfo{};
  HANDLE mJob = nullptr;
#else
  pid_t mPid = 0;
#endif
};

bool WaitForFile(const std::filesystem::path& path, std::chrono::milliseconds limit)
{
  const auto deadline = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < deadline)
  {
    if (std::filesystem::exists(path))
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

} // namespace

// Runs only as the child of the cross-process case below; a no-op otherwise.
// Plays a standalone save: reads the file under the lock, says so, waits for
// the parent's "release", then writes its own key from that read.
TEST_CASE("volum-machine-settings-lock-helper")
{
  const char* target = std::getenv(kHelperPathEnv);
  if (target == nullptr || *target == '\0')
    return;
  const std::filesystem::path path(target);
  std::error_code ec;
  const bool wrote = volum::UpdateMachineSettingsFile(
    path,
    [&](nlohmann::json& doc, volum::MachineSettingsRead state) {
      if (state != volum::MachineSettingsRead::Ok)
        return false;
      std::ofstream(path.string() + ".held") << "held";
      WaitForFile(path.string() + ".release", std::chrono::seconds(30));
      doc["midiCh"] = 5;
      return true;
    },
    ec, /*lockTimeoutMs=*/30000);
  CHECK(wrote);
}

TEST_CASE("F-12: a calibration level edit does not write this instance's stale toggle")
{
  const auto root = TestRoot("calibration-single-key");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));
  Instance a;
  Instance b;

  // B turns calibration on; A, still showing it off, then moves only the level.
  b.calibrate = true;
  b.edits.Mark(/*toggleEdited=*/true);
  REQUIRE(b.SaveCalibration(path) == volum::MachineSettingsWriter::Flush::Written);
  a.level = -5.0;
  a.edits.Mark(/*toggleEdited=*/false);
  REQUIRE(a.SaveCalibration(path) == volum::MachineSettingsWriter::Flush::Written);
  auto disk = ReadJsonFile(path);
  CHECK(disk["CalibrateInput"] == true);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(-5.0));

  // And the reverse: a toggle-only edit keeps the other instance's newer level.
  b.calibrate = false;
  b.edits.Mark(/*toggleEdited=*/true);
  REQUIRE(b.SaveCalibration(path) == volum::MachineSettingsWriter::Flush::Written);
  disk = ReadJsonFile(path);
  CHECK(disk["CalibrateInput"] == false);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(-5.0));
  CHECK(disk["midiCh"] == 0);

  // Both edited: both written. Nothing edited: nothing written.
  a.calibrate = true;
  a.level = 3.0;
  a.edits.Mark(true);
  a.edits.Mark(false);
  REQUIRE(a.SaveCalibration(path) == volum::MachineSettingsWriter::Flush::Written);
  disk = ReadJsonFile(path);
  CHECK(disk["CalibrateInput"] == true);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(3.0));
  CHECK_FALSE(a.edits.Any());
  CHECK(a.SaveCalibration(path) == volum::MachineSettingsWriter::Flush::Nothing);
}

TEST_CASE("F-12: a busy settings lock keeps the change queued and retries it later")
{
  const auto root = TestRoot("busy-lock-retry");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));
  const auto before = ReadJsonFile(path);

  volum::MachineSettingsWriter plugin;
  plugin.NoteLoaded(volum::MachineSharedKeyValues(false, true, false, 12.0));
  volum::MachineSettingsWriter standalone;
  standalone.NoteLoaded(volum::MachineSharedKeyValues(false, true, false, 12.0));

  volum::content::RegistryFileLock holder;
  REQUIRE(holder.Acquire(volum::MachineSettingsLockPath(path)));

  plugin.Queue({{"liteMode", true}});
  CHECK(plugin.FlushPending(path, 50, 100.0, ec) == volum::MachineSettingsWriter::Flush::Failed);
  CHECK(ec == std::errc::timed_out);
  CHECK(plugin.Pending() == nlohmann::json{{"liteMode", true}});
  CHECK(plugin.TakeFailureToLog());
  CHECK_FALSE(plugin.TakeFailureToLog()); // one line per run of failures
  CHECK_FALSE(plugin.RetryDue(100.0 + volum::MachineSettingsWriter::kRetryAfterMs - 1));
  CHECK(plugin.RetryDue(100.0 + volum::MachineSettingsWriter::kRetryAfterMs));

  CHECK_FALSE(standalone.WriteWhole(path, StandaloneDoc(false, true, false, 12.0, 7), 50, 0.0, ec));
  CHECK(ec == std::errc::timed_out);
  CHECK(ReadJsonFile(path) == before);

  holder.Release();
  CHECK(plugin.FlushPending(path, 50, 2000.0, ec) == volum::MachineSettingsWriter::Flush::Written);
  CHECK_FALSE(plugin.HasPending());
  CHECK(plugin.Synced()["liteMode"] == true);
  REQUIRE(standalone.WriteWhole(path, StandaloneDoc(false, true, false, 12.0, 7), 50, 0.0, ec));
  const auto disk = ReadJsonFile(path);
  CHECK(disk["liteMode"] == true);
  CHECK(disk["midiCh"] == 7);

  // A reload makes the file's values live; a key still queued from before is dropped.
  plugin.Queue({{"animatePlayArt", false}});
  plugin.NoteLoaded(volum::MachineSharedKeyValues(true, true, false, 12.0));
  CHECK_FALSE(plugin.HasPending());
}

TEST_CASE("F-12: a retried key does not resurrect over a newer value another writer wrote meanwhile")
{
  const auto root = TestRoot("retry-stale-key");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));
  const auto shared = volum::MachineSharedKeyValues(false, true, false, 12.0);

  volum::MachineSettingsWriter a; // times out, retries later
  a.NoteLoaded(shared);
  volum::MachineSettingsWriter b; // writes while A waits
  b.NoteLoaded(shared);

  volum::content::RegistryFileLock holder;
  REQUIRE(holder.Acquire(volum::MachineSettingsLockPath(path)));
  a.Queue({{"InputCalibrationLevel", -5.0}, {"liteMode", true}});
  REQUIRE(a.FlushPending(path, 50, 0.0, ec) == volum::MachineSettingsWriter::Flush::Failed);
  holder.Release();

  // B writes a newer level, not liteMode.
  b.Queue({{"InputCalibrationLevel", 3.0}});
  REQUIRE(b.FlushPending(path, 50, 0.0, ec) == volum::MachineSettingsWriter::Flush::Written);

  // A's retry keeps its untouched liteMode, but the level on disk is newer than A's queued one.
  REQUIRE(a.FlushPending(path, 50, 2000.0, ec) == volum::MachineSettingsWriter::Flush::Written);
  auto disk = ReadJsonFile(path);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(3.0));
  CHECK(disk["liteMode"] == true);
  CHECK_FALSE(a.HasPending());
  CHECK(a.Synced()["InputCalibrationLevel"] == doctest::Approx(3.0)); // adopted, so later saves know it
  CHECK(a.Synced()["liteMode"] == true);

  // Every queued key beaten: nothing is written and the disk values win.
  REQUIRE(holder.Acquire(volum::MachineSettingsLockPath(path)));
  a.Queue({{"InputCalibrationLevel", 9.0}});
  REQUIRE(a.FlushPending(path, 50, 3000.0, ec) == volum::MachineSettingsWriter::Flush::Failed);
  holder.Release();
  b.Queue({{"InputCalibrationLevel", 4.0}});
  REQUIRE(b.FlushPending(path, 50, 3000.0, ec) == volum::MachineSettingsWriter::Flush::Written);
  CHECK(a.FlushPending(path, 50, 4100.0, ec) == volum::MachineSettingsWriter::Flush::Superseded);
  CHECK_FALSE(a.HasPending());
  disk = ReadJsonFile(path);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(4.0));
  CHECK(a.Synced()["InputCalibrationLevel"] == doctest::Approx(4.0));

  // A fresh edit after the failure is the user's latest word and still wins.
  REQUIRE(holder.Acquire(volum::MachineSettingsLockPath(path)));
  a.Queue({{"InputCalibrationLevel", 1.0}});
  REQUIRE(a.FlushPending(path, 50, 5000.0, ec) == volum::MachineSettingsWriter::Flush::Failed);
  holder.Release();
  b.Queue({{"InputCalibrationLevel", 6.0}});
  REQUIRE(b.FlushPending(path, 50, 5000.0, ec) == volum::MachineSettingsWriter::Flush::Written);
  a.Queue({{"InputCalibrationLevel", 2.0}});
  REQUIRE(a.FlushPending(path, 50, 6100.0, ec) == volum::MachineSettingsWriter::Flush::Written);
  CHECK(ReadJsonFile(path)["InputCalibrationLevel"] == doctest::Approx(2.0));
}

TEST_CASE("F-12: another process holding the settings lock delays a plugin's merge, which then keeps both writes")
{
  const auto root = TestRoot("cross-process-lock");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));

  HelperProcess child;
  SetEnv(kHelperPathEnv, path.string());
  const bool started = child.Start();
  SetEnv(kHelperPathEnv, "");
  REQUIRE(started);
  REQUIRE(WaitForFile(path.string() + ".held", std::chrono::seconds(60)));

  volum::MachineSettingsWriter plugin;
  plugin.Queue({{"liteMode", true}});
  CHECK(plugin.FlushPending(path, 50, 0.0, ec) == volum::MachineSettingsWriter::Flush::Failed);
  CHECK(ec == std::errc::timed_out);
  CHECK(plugin.HasPending());

  std::atomic<bool> done{false};
  volum::MachineSettingsWriter::Flush result = volum::MachineSettingsWriter::Flush::Nothing;
  std::error_code retryEc;
  std::thread retry([&]() {
    result = plugin.FlushPending(path, 30000, 2000.0, retryEc);
    done.store(true);
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  CHECK_FALSE(done.load());
  std::ofstream(path.string() + ".release") << "release";
  retry.join();

  CHECK(result == volum::MachineSettingsWriter::Flush::Written);
  CHECK(child.Wait() == 0);
  const auto disk = ReadJsonFile(path);
  CHECK(disk["midiCh"] == 5);
  CHECK(disk["liteMode"] == true);
}

TEST_CASE("F-63: the standalone's Output mode survives a plugin's key merge and reloads only in the standalone")
{
  const auto root = TestRoot("output-mode-writer");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  volum::MachineSettingsWriter standalone;
  nlohmann::json doc = StandaloneDoc(false, true, false, 12.0, 0);
  doc[volum::kOutputModeMachineKey] = volum::kOutputModeRaw;
  REQUIRE(standalone.WriteWhole(path, doc, 50, 0.0, ec));

  volum::MachineSettingsWriter plugin;
  plugin.Queue({{"liteMode", true}});
  REQUIRE(plugin.FlushPending(path, 50, 0.0, ec) == volum::MachineSettingsWriter::Flush::Written);

  const auto disk = ReadJsonFile(path);
  CHECK(volum::OutputModeFromMachineSettings(true, disk, volum::kOutputModeDefault) == volum::kOutputModeRaw);
  CHECK(volum::OutputModeFromMachineSettings(false, disk, volum::kOutputModeCalibrated)
        == volum::kOutputModeCalibrated);
}
