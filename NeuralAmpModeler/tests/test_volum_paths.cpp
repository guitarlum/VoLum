#include "third_party/doctest.h"
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
  const fs::path tmp = fs::temp_directory_path() / "volum_discover_test";
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
  const fs::path tmp = fs::temp_directory_path() / "volum_utf8_leaf";
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

  const fs::path tmp = fs::temp_directory_path() / "volum_localappdata_test";
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
