#include "third_party/doctest.h"
#include "../VoLumMachineSettingsFile.h"
#include "../VoLumSettingsFileIO.h"
#include "../VoLumUpdateState.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

std::filesystem::path TestRoot(const char* name)
{
  auto root = std::filesystem::temp_directory_path() / "volum-settings-atomic-write-tests" / name;
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

bool HasAtomicTempFile(const std::filesystem::path& dir)
{
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
  {
    if (entry.path().filename().string().find(".tmp.") != std::string::npos)
      return true;
  }
  return false;
}

std::string ReadSourceText(const char* fileName)
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / fileName;
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end()); // CRLF checkouts
  return text;
}

std::string FunctionBody(const std::string& source, const char* signature)
{
  const auto start = source.find(signature);
  REQUIRE(start != std::string::npos);
  const auto end = source.find("\n}\n", start);
  REQUIRE(end != std::string::npos);
  return source.substr(start, end - start);
}

// The standalone's document as _VolumSaveSettingsToFile builds it: its own keys
// plus the shared machine keys from its in-memory copy.
nlohmann::json StandaloneDoc(bool lite, bool animate, bool calibrate, double level, int midiCh)
{
  volum::VoLumAmpSettings amps[volum::kAmpCount]{};
  nlohmann::json j = volum::VolumUserSettingsToJson(
    amps, volum::kAmpCount, 0, nullptr, false, false, false, nullptr, nullptr, lite, calibrate, level, animate);
  j["midiCh"] = midiCh;
  j["volumUiMode"] = "play";
  return j;
}

} // namespace

TEST_CASE("F-12: a standalone whole-file save keeps a plugin's newer Lite, Animate art and calibration")
{
  const auto root = TestRoot("two-writer-keys");
  const auto path = root / "volum-settings.json";
  std::error_code ec;

  // Both processes start from the same file: Full, animation on, no calibration.
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));
  nlohmann::json standaloneSynced = volum::MachineSharedKeyValues(false, true, false, 12.0);
  nlohmann::json pluginSynced = volum::MachineSharedKeyValues(false, true, false, 12.0);

  // A VST3 instance switches to Lite, turns art animation off, sets calibration.
  REQUIRE(volum::MergeMachineSettingsKeys(path, {{"liteMode", true}}, pluginSynced, ec));
  REQUIRE(volum::MergeMachineSettingsKeys(path, {{"animatePlayArt", false}}, pluginSynced, ec));
  REQUIRE(volum::MergeMachineSettingsKeys(
    path, {{"CalibrateInput", true}, {"InputCalibrationLevel", -7.5}}, pluginSynced, ec));

  // The standalone, still holding the old values in memory, saves after a MIDI change.
  REQUIRE(volum::WriteWholeMachineSettings(path, StandaloneDoc(false, true, false, 12.0, 5), standaloneSynced, ec));
  auto disk = ReadJsonFile(path);
  CHECK(disk["liteMode"] == true);
  CHECK(disk["animatePlayArt"] == false);
  CHECK(disk["CalibrateInput"] == true);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(-7.5));
  CHECK(disk["midiCh"] == 5);
  CHECK(disk["volumUiMode"] == "play");

  // A second stale save does not flip them back either.
  REQUIRE(volum::WriteWholeMachineSettings(path, StandaloneDoc(false, true, false, 12.0, 6), standaloneSynced, ec));
  disk = ReadJsonFile(path);
  CHECK(disk["liteMode"] == true);
  CHECK(disk["animatePlayArt"] == false);
  CHECK(disk["midiCh"] == 6);

  // A key the standalone did change is its to write; the others stay the plugin's.
  REQUIRE(volum::WriteWholeMachineSettings(path, StandaloneDoc(false, true, false, -3.0, 6), standaloneSynced, ec));
  disk = ReadJsonFile(path);
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(-3.0));
  CHECK(disk["liteMode"] == true);
  CHECK(disk["CalibrateInput"] == true);

  // And the plugin's next single-key merge keeps every standalone key.
  REQUIRE(volum::MergeMachineSettingsKeys(path, {{"liteMode", false}}, pluginSynced, ec));
  disk = ReadJsonFile(path);
  CHECK(disk["liteMode"] == false);
  CHECK(disk["midiCh"] == 6);
  CHECK(disk["volumUiMode"] == "play");
  CHECK(disk["InputCalibrationLevel"] == doctest::Approx(-3.0));
  CHECK_FALSE(HasAtomicTempFile(root));
}

TEST_CASE("F-12: a key this process wrote on its own is no longer its stale copy")
{
  // The standalone's own Lite toggle merges the key at once. Its later whole-file
  // saves must treat that value as synced, or a plugin's newer flip back would be
  // overwritten by the standalone's in-memory Lite.
  const auto root = TestRoot("direct-write-synced");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));
  nlohmann::json standaloneSynced = volum::MachineSharedKeyValues(false, true, false, 12.0);
  nlohmann::json pluginSynced = standaloneSynced;

  REQUIRE(volum::MergeMachineSettingsKeys(path, {{"liteMode", true}}, standaloneSynced, ec));
  CHECK(standaloneSynced["liteMode"] == true);
  REQUIRE(volum::MergeMachineSettingsKeys(path, {{"liteMode", false}}, pluginSynced, ec));

  REQUIRE(volum::WriteWholeMachineSettings(path, StandaloneDoc(true, true, false, 12.0, 2), standaloneSynced, ec));
  CHECK(ReadJsonFile(path)["liteMode"] == false);
}

TEST_CASE("F-12: KeepOtherWritersMachineKeys writes only what this process changed")
{
  const nlohmann::json synced = volum::MachineSharedKeyValues(false, true, false, 12.0);
  const nlohmann::json disk = {{"liteMode", true}, {"animatePlayArt", false}, {"midiCh", 9}};

  nlohmann::json mine = StandaloneDoc(false, true, true, 12.0, 1);
  volum::KeepOtherWritersMachineKeys(mine, disk, synced);
  CHECK(mine["liteMode"] == true); // unchanged here: disk wins
  CHECK(mine["animatePlayArt"] == false); // unchanged here: disk wins
  CHECK(mine["CalibrateInput"] == true); // changed here: mine wins
  CHECK(mine["InputCalibrationLevel"] == doctest::Approx(12.0)); // not on disk: mine stays
  CHECK(mine["midiCh"] == 1); // not a shared key: the standalone owns it

  // Nothing known about the last sync: the whole document is this process's.
  nlohmann::json blind = StandaloneDoc(false, true, false, 12.0, 1);
  volum::KeepOtherWritersMachineKeys(blind, disk, nlohmann::json());
  CHECK(blind["liteMode"] == false);
}

TEST_CASE("F-12: settings writers wait for another process's read-merge-write")
{
  // Atomic replace keeps the file whole, not the writes. Another process that has
  // read the file and is about to replace it holds the lock; a writer here that
  // went ahead would have its key replaced by that process's older read.
  const auto root = TestRoot("two-writer-lock");
  const auto path = root / "volum-settings.json";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, StandaloneDoc(false, true, false, 12.0, 0), ec));

  SUBCASE("a plugin merge waits for the standalone's save")
  {
    volum::content::RegistryFileLock standalone;
    REQUIRE(standalone.Acquire(volum::MachineSettingsLockPath(path)));
    nlohmann::json standaloneDoc = ReadJsonFile(path);

    std::atomic<bool> done{false};
    bool ok = false;
    std::error_code pluginEc;
    std::thread plugin([&]() {
      nlohmann::json synced;
      ok = volum::MergeMachineSettingsKeys(path, {{"liteMode", true}}, synced, pluginEc);
      done.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK_FALSE(done.load());

    standaloneDoc["midiCh"] = 5;
    REQUIRE(volum::WriteJsonAtomically(path, standaloneDoc, ec));
    standalone.Release();
    plugin.join();

    CHECK(ok);
    const auto disk = ReadJsonFile(path);
    CHECK(disk["midiCh"] == 5);
    CHECK(disk["liteMode"] == true);
  }

  SUBCASE("a standalone save waits for a plugin's merge")
  {
    volum::content::RegistryFileLock plugin;
    REQUIRE(plugin.Acquire(volum::MachineSettingsLockPath(path)));
    nlohmann::json pluginDoc = ReadJsonFile(path);

    std::atomic<bool> done{false};
    bool ok = false;
    std::error_code standaloneEc;
    std::thread standalone([&]() {
      nlohmann::json synced = volum::MachineSharedKeyValues(false, true, false, 12.0);
      ok = volum::WriteWholeMachineSettings(path, StandaloneDoc(false, true, false, 12.0, 5), synced, standaloneEc);
      done.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK_FALSE(done.load());

    pluginDoc["liteMode"] = true;
    REQUIRE(volum::WriteJsonAtomically(path, pluginDoc, ec));
    plugin.Release();
    standalone.join();

    CHECK(ok);
    const auto disk = ReadJsonFile(path);
    CHECK(disk["midiCh"] == 5);
    CHECK(disk["liteMode"] == true);
  }

  SUBCASE("a wedged holder makes the write fail instead of hanging")
  {
    volum::content::RegistryFileLock holder;
    REQUIRE(holder.Acquire(volum::MachineSettingsLockPath(path)));
    const auto before = ReadJsonFile(path);
    std::error_code lockEc;
    CHECK_FALSE(volum::UpdateMachineSettingsFile(
      path,
      [](nlohmann::json& doc, volum::MachineSettingsRead) {
        doc["liteMode"] = true;
        return true;
      },
      lockEc, /*lockTimeoutMs=*/50));
    CHECK(lockEc == std::errc::timed_out);
    CHECK(ReadJsonFile(path) == before);
  }
}

TEST_CASE("F-12: every volum-settings.json writer goes through the locked merge")
{
  const std::string scene = ReadSourceText("VoLumSettingsScene.inc.cpp");
  const std::string save = FunctionBody(scene, "bool NeuralAmpModeler::_VolumSaveSettingsToFile(int lockTimeoutMs)");
  CHECK(save.find("mVolumMachineSettings.WriteWhole(settingsPath, std::move(j), lockTimeoutMs") != std::string::npos);
  CHECK(save.find("WriteJsonAtomically(settingsPath") == std::string::npos);

  const std::string machineBool = FunctionBody(scene, "void NeuralAmpModeler::_VolumSaveMachineBool(");
  CHECK(machineBool.find("mVolumMachineSettings.Queue({{key, value}});") != std::string::npos);
  CHECK(machineBool.find("_VolumFlushMachineKeys(volum::kMachineSettingsIdleLockMs);") != std::string::npos);
  CHECK(machineBool.find("WriteJsonAtomically") == std::string::npos);

  // Only the calibration key the user edited, never both (CalibrationEdits).
  const std::string calibration =
    FunctionBody(scene, "void NeuralAmpModeler::_VolumSaveCalibrationDefaults(int lockTimeoutMs)");
  CHECK(calibration.find("mVolumCalibrationEdits.TakeKeys(") != std::string::npos);
  CHECK(calibration.find("\"CalibrateInput\"") == std::string::npos);
  CHECK(calibration.find("WriteJsonAtomically") == std::string::npos);
  const std::string flush = FunctionBody(scene, "bool NeuralAmpModeler::_VolumFlushMachineKeys(int lockTimeoutMs)");
  CHECK(flush.find("mVolumMachineSettings.FlushPending(settingsPath, lockTimeoutMs") != std::string::npos);

  const std::string plugin = ReadSourceText("NeuralAmpModeler.cpp");
  const std::string paramUi = FunctionBody(plugin, "void NeuralAmpModeler::OnParamChangeUI(");
  CHECK(paramUi.find("mVolumCalibrationEdits.Mark(/*toggleEdited=*/paramIdx == kCalibrateInput);")
        != std::string::npos);
  // A failed write stays pending: OnIdle re-dirties the full save and retries queued keys.
  const std::string idle = FunctionBody(plugin, "void NeuralAmpModeler::OnIdle()");
  CHECK(
    idle.find("if (!_VolumSaveSettingsToFile(volum::kMachineSettingsIdleLockMs))\n        mVolumSettingsDirty = true;")
    != std::string::npos);
  CHECK(idle.find("mVolumMachineSettings.RetryDue(VolumWriteNowMs())") != std::string::npos);
  const std::string dtor = FunctionBody(plugin, "NeuralAmpModeler::~NeuralAmpModeler()");
  CHECK(dtor.find("_VolumFlushMachineKeys(volum::kMachineSettingsFinalLockMs);") != std::string::npos);

  // The sync point is the load: whatever it put live is what this process "has".
  const std::string load = FunctionBody(scene, "void NeuralAmpModeler::_VolumLoadSettingsFromFile()");
  const auto early = load.find("_VolumNoteMachineKeysSynced();\n    return;");
  CHECK(early != std::string::npos);
  CHECK(load.rfind("_VolumNoteMachineKeysSynced();") > load.find("catch (...)"));
  const std::string note = FunctionBody(scene, "void NeuralAmpModeler::_VolumNoteMachineKeysSynced()");
  CHECK(note.find("mVolumMachineSettings.NoteLoaded(") != std::string::npos);
  CHECK(note.find("mVolumLiteMode.load(), mVolumAnimatePlayArt.load(),") != std::string::npos);

  // Pack import replaces the file under the same lock.
  const std::string pack = ReadSourceText("VoLumPack.h");
  const auto restore = pack.find("WriteWholeFile(settingsTmp, sanitizedSettings)");
  REQUIRE(restore != std::string::npos);
  const auto lock = pack.rfind("WithMachineSettingsLock(", restore);
  REQUIRE(lock != std::string::npos);
  CHECK(restore - lock < 200);
}

TEST_CASE("ReplaceFileAtomically refuses POSIX rename over a write-bit-clear file")
{
  const auto path = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumSettingsFileIO.h";
  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  const std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(src.find("st.permissions() & std::filesystem::perms::owner_write") != std::string::npos);
}

TEST_CASE("WriteJsonAtomically writes complete JSON and removes temp file")
{
  const auto root = TestRoot("golden");
  const auto path = root / "volum-settings.json";
  const nlohmann::json payload = {
    {"version", 6},
    {"lastAmpIdx", 3},
    {"amps", {{"Ampete One", {{"speaker", 2}, {"channel", 1}}}}},
  };

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, payload, ec));
  CHECK_FALSE(ec);
  CHECK(std::filesystem::exists(path));
  CHECK_FALSE(HasAtomicTempFile(root));
  CHECK(ReadJsonFile(path) == payload);
}

TEST_CASE("WriteJsonAtomically refuses a read-only target and leaves it intact")
{
  // macOS CI: POSIX rename replaces a chmod u-w file. The replace helper must
  // honor the write bit so a backup lock cannot wipe the library.
  const auto root = TestRoot("read-only-target");
  const auto path = root / "volum-settings.json";
  const nlohmann::json original = {{"writer", "original"}, {"value", 1}};
  const nlohmann::json replacement = {{"writer", "replacement"}, {"value", 2}};

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, original, ec));
  std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::replace);

  CHECK_FALSE(volum::WriteJsonAtomically(path, replacement, ec));
  CHECK(ec);
  std::filesystem::permissions(path, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
  CHECK(ReadJsonFile(path) == original);
  CHECK_FALSE(HasAtomicTempFile(root));
}

TEST_CASE("WriteJsonAtomically leaves existing file untouched when target path is invalid")
{
  const auto root = TestRoot("invalid-target");
  const auto path = root / "volum-settings.json";
  const nlohmann::json original = {{"writer", "original"}, {"value", 1}};
  const nlohmann::json replacement = {{"writer", "replacement"}, {"value", 2}};

  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, original, ec));
  CHECK(ReadJsonFile(path) == original);

  const auto invalidTarget = root / "blocking-directory" / "nested.json";
  std::filesystem::create_directories(invalidTarget, ec);
  REQUIRE_FALSE(ec);
  CHECK_FALSE(volum::WriteJsonAtomically(invalidTarget, replacement, ec));
  CHECK(ec);
  CHECK(ReadJsonFile(path) == original);
}

TEST_CASE("WriteJsonAtomically concurrent writers leave a complete parseable file")
{
  const auto root = TestRoot("race");
  const auto path = root / "volum-settings.json";
  const nlohmann::json first = {{"writer", "first"}, {"value", 1}, {"payload", std::string(2048, 'a')}};
  const nlohmann::json second = {{"writer", "second"}, {"value", 2}, {"payload", std::string(2048, 'b')}};

  for (int iteration = 0; iteration < 25; ++iteration)
  {
    std::atomic<bool> start{false};
    std::error_code ec1;
    std::error_code ec2;
    bool ok1 = false;
    bool ok2 = false;

    std::thread t1([&]() {
      while (!start.load()) {}
      ok1 = volum::WriteJsonAtomically(path, first, ec1);
    });
    std::thread t2([&]() {
      while (!start.load()) {}
      ok2 = volum::WriteJsonAtomically(path, second, ec2);
    });

    start.store(true);
    t1.join();
    t2.join();

    CHECK(ok1);
    CHECK_FALSE(ec1);
    CHECK(ok2);
    CHECK_FALSE(ec2);

    const nlohmann::json loaded = ReadJsonFile(path);
    CHECK((loaded == first || loaded == second));
    CHECK_FALSE(HasAtomicTempFile(root));
  }
}

TEST_CASE("A document containing invalid UTF-8 fails the write instead of throwing")
{
  // Every user-entered name - amp, IR, pedal, preset, cabinet - is serialized
  // through here, and nlohmann::json::dump() throws type_error on invalid UTF-8
  // anywhere in the document. The individual name cuts are character-aware, but
  // this is the one place all of them pass through, so it has to be the backstop:
  // an exception here would escape into UI or host code during a save.
  const auto root = TestRoot("invalid-utf8");
  const auto path = root / "settings.json";

  nlohmann::json good;
  good["name"] = "fine";
  std::error_code ec;
  REQUIRE(volum::WriteJsonAtomically(path, good, ec));
  REQUIRE_FALSE(ec);

  nlohmann::json bad;
  bad["name"] = std::string("half a glyph: ") + "\xF0\x9F\x98"; // truncated U+1F600

  bool ok = true;
  CHECK_NOTHROW(ok = volum::WriteJsonAtomically(path, bad, ec));
  CHECK_FALSE(ok);
  CHECK(ec == std::errc::invalid_argument);

  // The previous good file survives untouched, and no temp file is left behind.
  const nlohmann::json loaded = ReadJsonFile(path);
  CHECK(loaded == good);
  CHECK_FALSE(HasAtomicTempFile(root));
}

TEST_CASE("Update sidecar round-trips independently of user settings")
{
  const auto root = TestRoot("update-sidecar");
  const auto path = root / "volum-update-state.json";
  volum::update::UpdateState expected;
  expected.lastCheckUtc = 1'787'000'000;
  expected.lastSeenVersion = "1.3.0";
  expected.latestKnownVersion = "1.3.1";
  expected.latestKnownUrl = "https://github.com/guitarlum/VoLum/releases/tag/v1.3.1";
  expected.latestKnownNotes = "Maintenance release.";
  expected.autoCheck = false;

  REQUIRE(volum::update::SaveUpdateState(path, expected));
  const auto loaded = volum::update::LoadUpdateState(path);
  CHECK(loaded.lastCheckUtc == expected.lastCheckUtc);
  CHECK(loaded.lastSeenVersion == expected.lastSeenVersion);
  CHECK(loaded.latestKnownVersion == expected.latestKnownVersion);
  CHECK(loaded.latestKnownUrl == expected.latestKnownUrl);
  CHECK(loaded.latestKnownNotes == expected.latestKnownNotes);
  CHECK(loaded.autoCheck == expected.autoCheck);
  CHECK_FALSE(HasAtomicTempFile(root));
}
