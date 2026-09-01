#include "third_party/doctest.h"
#include "../VoLumUpdateCheck.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

TEST_CASE("Update versions compare semantic triples")
{
  volum::update::Version parsed;
  CHECK(volum::update::ParseVersion("1.3.0", parsed));
  CHECK(parsed.major == 1);
  CHECK(parsed.minor == 3);
  CHECK(parsed.patch == 0);
  CHECK(volum::update::ParseVersion("v2.0.1", parsed));
  CHECK_FALSE(volum::update::ParseVersion("1.3", parsed));
  CHECK_FALSE(volum::update::ParseVersion("1.3.0-beta", parsed));
  CHECK(volum::update::CompareVersionStrings("1.3.0", "1.2.9") > 0);
  CHECK(volum::update::CompareVersionStrings("1.3.0", "1.3.0") == 0);
  CHECK(volum::update::CompareVersionStrings("1.2.9", "1.3.0") < 0);
}

TEST_CASE("Update manifest accepts its schema and ignores future keys")
{
  const std::string json = R"({
    "schema": 1,
    "stable": {
      "version": "1.3.0",
      "published": "2026-08-31",
      "url": "https://github.com/guitarlum/VoLum/releases/tag/v1.3.0",
      "notes": "Update reminder.",
      "downloads": {"win": "unused", "mac": "unused"},
      "future": {"safe": true}
    },
    "message": null,
    "futureRoot": 42
  })";

  volum::update::Manifest manifest;
  CHECK_NOTHROW(CHECK(volum::update::ParseManifest(json, manifest)));
  CHECK(manifest.version == "1.3.0");
  CHECK(manifest.notes == "Update reminder.");
  CHECK(manifest.url == "https://github.com/guitarlum/VoLum/releases/tag/v1.3.0");
}

TEST_CASE("Update manifest rejects malformed and wrong-type input without throwing")
{
  const std::string cases[] = {
    "",
    R"({"schema":1,"stable":)",
    R"({"schema":"1","stable":{"version":"1.3.0","notes":"n","url":"https://example.com"}})",
    R"({"schema":1,"stable":{"version":130,"notes":"n","url":"https://example.com"}})",
    R"({"schema":1,"stable":{"version":"1.3.0","notes":[],"url":"https://example.com"}})",
    R"({"schema":1,"stable":{"version":"1.3.0","notes":"n","url":false}})",
    R"({"schema":1,"stable":{"version":"1.3.0","notes":"n","url":"http://example.com"}})",
  };

  for (const auto& json : cases)
  {
    volum::update::Manifest manifest;
    bool accepted = true;
    CHECK_NOTHROW(accepted = volum::update::ParseManifest(json, manifest));
    CHECK_FALSE(accepted);
  }
}

TEST_CASE("Update throttle includes the exact 24 hour boundary")
{
  constexpr std::int64_t last = 1'000'000;
  CHECK_FALSE(volum::update::ShouldCheck(last + volum::update::kCheckIntervalSeconds - 1, last));
  CHECK(volum::update::ShouldCheck(last + volum::update::kCheckIntervalSeconds, last));
  CHECK(volum::update::ShouldCheck(last + volum::update::kCheckIntervalSeconds + 1, last));
  CHECK_FALSE(volum::update::ShouldCheck(last - 1, last));
  CHECK(volum::update::ShouldCheck(last, 0));
}

TEST_CASE("Update badge survives Settings open and clears from update row")
{
  volum::update::BadgeState state{"1.3.0", ""};
  CHECK(volum::update::ShouldShowBadge(state, "1.2.2"));
  volum::update::OnSettingsOpened(state);
  CHECK(volum::update::ShouldShowBadge(state, "1.2.2"));
  volum::update::MarkSeen(state);
  CHECK_FALSE(volum::update::ShouldShowBadge(state, "1.2.2"));
  CHECK(volum::update::IsUpdateAvailable(state, "1.2.2"));
}

TEST_CASE("Update Check now marks the known release seen before rechecking")
{
  volum::update::BadgeState state{"1.3.0", "1.2.9"};
  CHECK(volum::update::ShouldShowBadge(state, "1.2.2"));
  volum::update::MarkSeen(state);
  CHECK(state.lastSeenVersion == "1.3.0");
  CHECK_FALSE(volum::update::ShouldShowBadge(state, "1.2.2"));
  CHECK(volum::update::IsUpdateAvailable(state, "1.2.2"));

  state.latestKnownVersion = "1.3.1";
  CHECK(volum::update::ShouldShowBadge(state, "1.2.2"));
}

TEST_CASE("VOLUM_FAKE_UPDATE injects 2.0.0 without touching the network")
{
  CHECK(volum::update::FakeUpdateManifest().version == "2.0.0");
  CHECK(volum::update::FakeUpdateManifest().notes.find("signal chain") != std::string::npos);
  CHECK(volum::update::FakeUpdateManifest().url.rfind("https://", 0) == 0);
  CHECK(volum::update::CompareVersionStrings("2.0.0", "1.2.2") > 0);
}

TEST_CASE("Settings gear opener does not consume the update badge")
{
  const auto sourcePath = std::filesystem::path(__FILE__).parent_path().parent_path() / "VoLumLayoutBuild.inc.cpp";
  std::ifstream in(sourcePath, std::ios::binary);
  REQUIRE(in.good());
  std::ostringstream text;
  text << in.rdbuf();
  const std::string source = text.str();
  const auto gear = source.find("// Gear button");
  const auto settings = source.find("new NAMSettingsPageControl", gear);
  REQUIRE(gear != std::string::npos);
  REQUIRE(settings != std::string::npos);
  const std::string opener = source.substr(gear, settings - gear);
  CHECK(opener.find("MarkSeen") == std::string::npos);
  CHECK(opener.find("_VolumUseAvailableUpdate") == std::string::npos);
  CHECK(opener.find("_VolumCheckForUpdatesNow") == std::string::npos);
}
