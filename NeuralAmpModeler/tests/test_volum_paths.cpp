#include "third_party/doctest.h"
#include "VoLumTestTempDir.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumPaths.h"
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
  #include <stdlib.h>
#elif defined(__linux__)
  #include <stdlib.h>
  #include <unistd.h>
#endif

TEST_CASE("DiscoverChannels sorts by label and matches speaker prefix")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_discover_test";
  fs::remove_all(tmp);
  fs::create_directories(tmp / "Ampete One");
  std::ofstream((tmp / "Ampete One" / "V30-ZZ-2.nam").string()).close();
  std::ofstream((tmp / "Ampete One" / "V30-AA-1.nam").string()).close();
  std::ofstream((tmp / "Ampete One" / "ignored.txt").string()).close();

  const auto ch = volum::DiscoverChannels(tmp, "Ampete One", "V30");
  REQUIRE(ch.size() == 2);
  REQUIRE(ch[0].label < ch[1].label);
  REQUIRE(ch[0].filename == "V30-AA-1.nam");
  REQUIRE(ch[1].filename == "V30-ZZ-2.nam");
}

TEST_CASE("FindRigsRootDirectory returns a rigs tree in typical dev/repo layout")
{
  namespace fs = std::filesystem;
  const fs::path root = volum::FindRigsRootDirectory();
  REQUIRE(!root.empty());
  REQUIRE(fs::is_directory(root));
  // Bundled catalog expects this folder when developing from the VoLum repo.
  REQUIRE(fs::is_directory(root / "Ampete One"));
}

TEST_CASE("Rigs root skips a tree without pedal captures for a complete one")
{
  // A leftover or older install (amps, no PrePedals captures) used to win just by
  // existing; the pedal menu then had nothing real to load.
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_pick_rigs_root";
  fs::remove_all(tmp);
  const fs::path stale = tmp / "stale" / "VoLumRigs";
  const fs::path portable = tmp / "portable" / "VoLumRigs";
  fs::create_directories(stale / "Ampete One");
  fs::create_directories(stale / "PrePedals");
  fs::create_directories(portable / "Ampete One");
  fs::create_directories(portable / "PrePedals");
  std::ofstream((portable / "PrePedals" / "FX-Minotaur-Klon-1.nam").string()).close();

  std::error_code ec;
  CHECK_FALSE(volum::IsCompleteRigsRoot(stale));
  CHECK(volum::IsCompleteRigsRoot(portable));
  CHECK(volum::PickRigsRoot({tmp / "nowhere", stale, portable}) == fs::weakly_canonical(portable, ec));

  // Nothing complete: the first existing tree still serves its amps.
  CHECK(volum::PickRigsRoot({tmp / "nowhere", stale}) == fs::weakly_canonical(stale, ec));
  CHECK(volum::PickRigsRoot({tmp / "nowhere"}).empty());

  // A factory bank alone also marks VoLum's own tree.
  std::ofstream((stale / "factory-presets.json").string()).close();
  CHECK(volum::IsCompleteRigsRoot(stale));
  fs::remove_all(tmp);
}

TEST_CASE("Windows rigs candidates put the tree beside the binary before the registry")
{
  namespace fs = std::filesystem;
  const fs::path moduleDir = fs::path("D:") / "Portable" / "VoLum";
  const fs::path registryRoot = fs::path("C:") / "Program Files" / "VoLum" / "VoLumRigs";
  const auto candidates = volum::WindowsRigsCandidates(moduleDir, registryRoot);
  REQUIRE(candidates.size() >= 3);
  CHECK(candidates.front() == moduleDir / "VoLumRigs");
  CHECK(candidates.back() == registryRoot);
  CHECK(volum::WindowsRigsCandidates({}, registryRoot) == std::vector<fs::path>{registryRoot});
}

TEST_CASE("Factory presets come from the first candidate tree that holds the file")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_first_rigs_file";
  fs::remove_all(tmp);
  const fs::path beside = tmp / "beside" / "VoLumRigs";
  const fs::path installed = tmp / "installed" / "VoLumRigs";
  fs::create_directories(beside);
  fs::create_directories(installed);
  std::ofstream((installed / "factory-presets.json").string()).close();

  // A 1.2.x install's tree has no file, so a newer build beside it falls through.
  std::error_code ec;
  CHECK(volum::FirstRigsFile({beside, installed}, "factory-presets.json")
        == fs::weakly_canonical(installed / "factory-presets.json", ec));
  std::ofstream((beside / "factory-presets.json").string()).close();
  CHECK(volum::FirstRigsFile({beside, installed}, "factory-presets.json")
        == fs::weakly_canonical(beside / "factory-presets.json", ec));
  CHECK(volum::FirstRigsFile({tmp / "nowhere"}, "factory-presets.json").empty());
  fs::remove_all(tmp);
}

TEST_CASE("FindFactoryPresetsFile reads the rigs root's bank, else the repo's in a dev build")
{
  // Bites only where an install without the file owns the rigs root (registry); CI has none.
  namespace fs = std::filesystem;
  const fs::path repoFile =
    fs::path(__FILE__).parent_path().parent_path().parent_path() / "rigs" / "factory-presets.json";
  const fs::path rootFile = volum::FindRigsRootDirectory() / "factory-presets.json";
  std::error_code ec;
  const fs::path expected = fs::is_regular_file(rootFile, ec) ? rootFile : repoFile;
  CHECK(volum::FindFactoryPresetsFile() == fs::weakly_canonical(expected, ec));
}

TEST_CASE("Diezel Herbert Mk1 rig files expose 4 channels for every speaker mode")
{
  namespace fs = std::filesystem;
  const fs::path root = fs::path(__FILE__).parent_path().parent_path().parent_path() / "rigs";
  REQUIRE(fs::is_directory(root / "Diezel Herbert Mk1"));

  for (const char* prefix : volum::kSpeakerPrefixes)
  {
    const auto channels = volum::DiscoverChannels(root, "Diezel Herbert Mk1", prefix);
    INFO(prefix);
    REQUIRE(channels.size() == 4);
    CHECK(channels[0].filename == std::string(prefix) + "-Herb-1.nam");
    CHECK(channels[1].filename == std::string(prefix) + "-Herb-2.nam");
    CHECK(channels[2].filename == std::string(prefix) + "-Herb-3.nam");
    CHECK(channels[3].filename == std::string(prefix) + "-Herb-4.nam");
    CHECK(channels[0].label == "1");
    CHECK(channels[1].label == "2");
    CHECK(channels[2].label == "3");
    CHECK(channels[3].label == "4");
  }
}

TEST_CASE("Every factory amp has a uniform channel set across all speaker cabs")
{
  // INVARIANT that underpins the 1.2.0 channel-first redesign: because every
  // bundled amp exposes the same gain stages for AMP/G12/G65/V30, the persisted
  // factory channelIdx (a per-speaker index) is identical to an amp-wide index --
  // so switching the UI to channel-first navigation needs NO serialization
  // migration and old 1.0/1.1 projects load unchanged. If a future amp ships a
  // non-uniform channel set, this test fails and forces a speaker-snap + a
  // migration decision before release.
  namespace fs = std::filesystem;
  const fs::path root = fs::path(__FILE__).parent_path().parent_path().parent_path() / "rigs";
  REQUIRE(fs::is_directory(root));

  for (const auto& amp : volum::kAmps)
  {
    REQUIRE(fs::is_directory(root / amp.folderName));
    std::vector<std::string> reference;
    bool haveReference = false;
    for (const char* prefix : volum::kSpeakerPrefixes)
    {
      const auto channels = volum::DiscoverChannels(root, amp.folderName, prefix);
      std::vector<std::string> labels;
      for (const auto& c : channels)
        labels.push_back(c.label);
      INFO(amp.folderName << " / " << prefix);
      REQUIRE_FALSE(labels.empty()); // every cab has at least one gain stage
      if (!haveReference)
      {
        reference = labels;
        haveReference = true;
      }
      else
      {
        CHECK(labels == reference); // same gain stages for every cab
      }
    }
  }
}

TEST_CASE("Channel and pedal discovery read UTF-8 leaves, not native string()")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_utf8_leaf";
  fs::remove_all(tmp);
  fs::create_directories(tmp / "Ampete One");
  const fs::path nam = tmp / "Ampete One" / "V30-AA-1.nam";
  std::ofstream(nam.string()).close();
  CHECK(volum::PathLeafUtf8(nam) == "V30-AA-1.nam");
  CHECK(volum::PathStemUtf8(nam) == "V30-AA-1");
  const auto ch = volum::DiscoverChannels(tmp, "Ampete One", "V30");
  REQUIRE(ch.size() == 1);
  CHECK(ch[0].filename == "V30-AA-1.nam");
  fs::remove_all(tmp);
}

#ifdef _WIN32
TEST_CASE("VolumUserSettingsFilePath uses LOCALAPPDATA")
{
  // Hosted CI runners may not reflect _putenv_s("LOCALAPPDATA") in getenv() the same way as dev machines;
  // mutating LOCALAPPDATA is still validated locally and in manual runs.
  if (std::getenv("CI") || std::getenv("GITHUB_ACTIONS"))
    return;

  namespace fs = std::filesystem;
  const char* prev = std::getenv("LOCALAPPDATA");
  const std::string prevStr = prev ? std::string(prev) : std::string();

  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_localappdata_test";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const std::string la = tmp.string();
  _putenv_s("LOCALAPPDATA", la.c_str());

  const fs::path p = volum::VolumUserSettingsFilePath();
  const fs::path dual = volum::VolumDualAmpSettingsFilePath();

  if (prevStr.empty())
    _putenv_s("LOCALAPPDATA", "");
  else
    _putenv_s("LOCALAPPDATA", prevStr.c_str());

  std::error_code ec;
  REQUIRE(fs::weakly_canonical(p, ec) == fs::weakly_canonical(tmp / "VoLum" / "volum-settings.json", ec));
  REQUIRE(fs::weakly_canonical(dual, ec) == fs::weakly_canonical(tmp / "VoLum" / "volum-dual-amp-settings.json", ec));
}
#endif

#ifdef __APPLE__
TEST_CASE("VolumUserSettingsFilePath is under Application Support on macOS")
{
  const char* home = std::getenv("HOME");
  REQUIRE(home);
  const auto p = volum::VolumUserSettingsFilePath();
  CHECK(p == std::filesystem::path(home) / "Library" / "Application Support" / "VoLum" / "volum-settings.json");
  CHECK(p.generic_string().find('\\') == std::string::npos);
}
#endif

#ifdef __linux__

namespace
{

class EnvGuard
{
public:
  explicit EnvGuard(const char* name)
  : mName(name)
  {
    if (const char* value = std::getenv(name))
    {
      mHad = true;
      mValue = value;
    }
  }

  ~EnvGuard()
  {
    if (mHad)
      ::setenv(mName, mValue.c_str(), 1);
    else
      ::unsetenv(mName);
  }

  void Set(const std::string& value) { ::setenv(mName, value.c_str(), 1); }
  void Unset() { ::unsetenv(mName); }

private:
  const char* mName;
  bool mHad = false;
  std::string mValue;
};

} // namespace

TEST_CASE("Linux user paths sit under XDG_DATA_HOME/VoLum")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_xdg_data_home";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  EnvGuard xdg("XDG_DATA_HOME");
  xdg.Set(tmp.string());

  const fs::path root = tmp / "VoLum";
  const fs::path settings = volum::VolumUserSettingsFilePath();
  const fs::path update = volum::VolumUpdateStateFilePath();
  const fs::path content = volum::VolumContentDir();
  const fs::path dual = volum::VolumDualAmpSettingsFilePath();
  const fs::path log = volum::VolumDiagLogFilePath();
  CHECK_FALSE(settings.empty());
  CHECK_FALSE(update.empty());
  CHECK_FALSE(content.empty());
  CHECK_FALSE(dual.empty());
  CHECK_FALSE(log.empty());
  CHECK(settings == root / "volum-settings.json");
  CHECK(update == root / "volum-update-state.json");
  CHECK(content == root / "content");
  CHECK(dual == root / "volum-dual-amp-settings.json");
  CHECK(log == root / "volum.log");
  fs::remove_all(tmp);
}

TEST_CASE("Linux user paths fall back to HOME/.local/share when XDG_DATA_HOME is unset or relative")
{
  namespace fs = std::filesystem;
  const fs::path home = volum_test::ProcessTempRoot() / "volum_xdg_home_fallback";
  fs::remove_all(home);
  fs::create_directories(home);
  EnvGuard xdg("XDG_DATA_HOME");
  EnvGuard homeEnv("HOME");
  homeEnv.Set(home.string());

  xdg.Unset();
  const fs::path root = home / ".local" / "share" / "VoLum";
  CHECK(volum::VolumUserSettingsFilePath() == root / "volum-settings.json");
  CHECK(volum::VolumContentDir() == root / "content");
  CHECK(volum::VolumDiagLogFilePath() == root / "volum.log");

  xdg.Set("");
  CHECK(volum::VolumUpdateStateFilePath() == root / "volum-update-state.json");

  xdg.Set("relative/not-absolute");
  CHECK(volum::VolumDualAmpSettingsFilePath() == root / "volum-dual-amp-settings.json");

  xdg.Unset();
  homeEnv.Unset();
  CHECK(volum::VolumUserSettingsFilePath().empty());
  fs::remove_all(home);
}

TEST_CASE("Linux module path uses dladdr for a plugin .so and /proc/self/exe for the app")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_linux_module";
  fs::remove_all(tmp);
  fs::create_directories(tmp);
  const fs::path soFile = tmp / "VoLum.so";
  const fs::path exeFile = tmp / "VoLum";
  const fs::path exeLink = tmp / "VoLum-link";
  std::ofstream(soFile.string()).close();
  std::ofstream(exeFile.string()).close();
  std::error_code ec;
  fs::create_symlink(exeFile, exeLink, ec);
  REQUIRE_FALSE(ec);

  CHECK(volum::LinuxChooseModuleFile(soFile, exeFile) == soFile);
  CHECK(volum::LinuxChooseModuleFile(exeLink, exeFile) == exeFile);
  CHECK(volum::LinuxChooseModuleFile({}, exeFile) == exeFile);
  CHECK(volum::LinuxChooseModuleFile(soFile, {}) == soFile);

  const fs::path live = volum::LinuxModuleFile();
  char buf[4096];
  const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  REQUIRE(n > 0);
  buf[n] = '\0';
  CHECK(fs::equivalent(live, fs::path(buf), ec));
  fs::remove_all(tmp);
}

TEST_CASE("Linux rigs search order is beside the binary, bundle Resources, XDG, then /usr/share")
{
  namespace fs = std::filesystem;
  const fs::path tmp = volum_test::ProcessTempRoot() / "volum_linux_rigs_order";
  fs::remove_all(tmp);
  const fs::path module = tmp / "VoLum.vst3" / "Contents" / "x86_64-linux" / "VoLum.so";
  const fs::path data = tmp / "xdg" / "VoLum";
  const fs::path system = tmp / "usr" / "share" / "VoLum" / "VoLumRigs";
  fs::create_directories(module.parent_path());
  fs::create_directories(module.parent_path().parent_path() / "Resources" / "VoLumRigs" / "PrePedals");
  fs::create_directories(data / "VoLumRigs");
  fs::create_directories(system);
  std::ofstream(module.string()).close();

  const auto candidates = volum::LinuxRigsCandidates(module, data, system);
  REQUIRE(candidates.size() == 4);
  CHECK(candidates[0] == module.parent_path() / "VoLumRigs");
  CHECK(candidates[1] == module.parent_path().parent_path() / "Resources" / "VoLumRigs");
  CHECK(candidates[2] == data / "VoLumRigs");
  CHECK(candidates[3] == system);

  std::error_code ec;
  fs::create_directories(candidates[0]);
  std::ofstream((candidates[0] / "readme.txt").string()).close();
  std::ofstream((candidates[1] / "PrePedals" / "FX-Minotaur-Klon-1.nam").string()).close();
  CHECK(volum::PickRigsRoot(candidates) == fs::weakly_canonical(candidates[1], ec));

  std::ofstream((candidates[0] / "factory-presets.json").string()).close();
  CHECK(volum::PickRigsRoot(candidates) == fs::weakly_canonical(candidates[0], ec));

  EnvGuard xdg("XDG_DATA_HOME");
  const fs::path xdgRoot = tmp / "live-xdg";
  fs::create_directories(xdgRoot);
  xdg.Set(xdgRoot.string());
  const auto live = volum::RigsRootCandidates();
  REQUIRE(live.size() >= 6);
  const fs::path moduleFile = volum::LinuxModuleFile();
  CHECK(live[0] == moduleFile.parent_path() / "VoLumRigs");
  CHECK(live[1] == moduleFile.parent_path().parent_path() / "Resources" / "VoLumRigs");
  CHECK(live[2] == xdgRoot / "VoLum" / "VoLumRigs");
  CHECK(live[3] == fs::path("/usr/share/VoLum/VoLumRigs"));
  CHECK(live[live.size() - 2] == fs::path("VoLumRigs"));
  CHECK(live.back() == fs::path("rigs"));
  fs::remove_all(tmp);
}
#endif
