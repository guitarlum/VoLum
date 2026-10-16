#include "third_party/doctest.h"
#include "VoLumTestTempDir.h"
#include "../VoLumAmpeteCatalog.h"
#include "../VoLumPaths.h"
#include <cstdlib>
#include <fstream>

#ifdef _WIN32
  #include <stdlib.h>
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
