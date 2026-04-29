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

  if (prevStr.empty())
    _putenv_s("LOCALAPPDATA", "");
  else
    _putenv_s("LOCALAPPDATA", prevStr.c_str());

  std::error_code ec;
  REQUIRE(fs::weakly_canonical(p, ec) == fs::weakly_canonical(tmp / "VoLum" / "volum-settings.json", ec));
}
#endif
