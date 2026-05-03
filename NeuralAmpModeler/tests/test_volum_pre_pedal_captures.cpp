#include "third_party/doctest.h"
#include "../VoLumPrePedalCaptures.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
} // namespace

TEST_CASE("PrePedals discovery sorts unknown NAM captures after curated captures")
{
  namespace fs = std::filesystem;
  const fs::path tmp = fs::temp_directory_path() / "volum_pre_pedals_test";
  fs::remove_all(tmp);
  fs::create_directories(tmp / "PrePedals");
  std::ofstream((tmp / "PrePedals" / "20-Zed.nam").string()).close();
  std::ofstream((tmp / "PrePedals" / "10-Alpha.nam").string()).close();
  std::ofstream((tmp / "PrePedals" / "FX-OriginEffects-Halcyon-1.nam").string()).close();
  std::ofstream((tmp / "PrePedals" / "ignore.txt").string()).close();

  const auto captures = volum::DiscoverPrePedalCaptures(tmp);

  REQUIRE(captures.size() == 3);
  CHECK(captures[0].filename == "FX-OriginEffects-Halcyon-1.nam");
  CHECK(captures[0].label == "Halcyon TS");
  CHECK(captures[0].group == volum::PrePedalCaptureGroup::TsBoost);
  CHECK(captures[1].filename == "10-Alpha.nam");
  CHECK(captures[1].label == "10-Alpha");
  CHECK(captures[1].group == volum::PrePedalCaptureGroup::None);
  CHECK(captures[2].filename == "20-Zed.nam");
  CHECK(captures[2].label == "20-Zed");

  fs::remove_all(tmp);
}

TEST_CASE("PrePedals empty index and capture bounds stay stable")
{
  CHECK(volum::kPreCaptureEmptyIndex == 0);
  CHECK(volum::ClampPreCaptureIndex(-1, 12) == 0);
  CHECK(volum::ClampPreCaptureIndex(5, 12) == 5);
  CHECK(volum::ClampPreCaptureIndex(15, 12) == 12);
  CHECK(volum::ClampPreCaptureIndex(200, 200) == volum::kPreCaptureMaxParamIndex);
  CHECK_FALSE(volum::ShouldLoadPrePedalCapture(false, 1));
  CHECK_FALSE(volum::ShouldLoadPrePedalCapture(true, volum::kPreCaptureEmptyIndex));
  CHECK(volum::ShouldLoadPrePedalCapture(true, 1));
}

TEST_CASE("Repository PrePedals directory is discoverable")
{
  namespace fs = std::filesystem;
  const auto prePedals = RepoRoot() / "rigs" / "PrePedals";
  REQUIRE(fs::is_directory(prePedals));

  const auto captures = volum::DiscoverPrePedalCaptures(RepoRoot() / "rigs");
  for (const auto& capture : captures)
  {
    CAPTURE(capture.filename);
    CHECK(capture.filename.size() > 4);
    CHECK(capture.label.empty() == false);
    CHECK(fs::exists(prePedals / capture.filename));
  }
}

TEST_CASE("Repository PrePedals use curated labels and type order")
{
  const auto captures = volum::DiscoverPrePedalCaptures(RepoRoot() / "rigs");

  const std::vector<std::string> filenames = {
    "FX-Minotaur-Klon-1.nam",
    "FX-PettyJohn-Myth-1.nam",
    "FX-OriginEffects-Halcyon-1.nam",
    "FX-OriginEffects-Halcyon-2.nam",
    "FX-PettyJohn-Mash-1.nam",
    "FX-OriginEffects-Revival-1.nam",
    "FX-Beetronics-Fatbee-1.nam",
    "FX-PettyJohn-Nuke-1.nam",
    "FX-JHS-Bender-1.nam",
  };
  const std::vector<std::string> labels = {
    "Klon",
    "PettyJohn Myth",
    "Halcyon TS",
    "Halcyon TS +Gain",
    "PettyJohn Mash",
    "Revival Drive",
    "Fatbee",
    "PettyJohn Nuke",
    "JHS Bender",
  };
  const std::vector<std::string> shortLabels = {
    "Klon",
    "Myth",
    "TS",
    "TS+",
    "Mash",
    "Revi",
    "FatB",
    "Nuke",
    "Bndr",
  };
  const std::vector<volum::PrePedalCaptureGroup> groups = {
    volum::PrePedalCaptureGroup::Klon,
    volum::PrePedalCaptureGroup::Klon,
    volum::PrePedalCaptureGroup::TsBoost,
    volum::PrePedalCaptureGroup::TsBoost,
    volum::PrePedalCaptureGroup::TsBoost,
    volum::PrePedalCaptureGroup::Distortion,
    volum::PrePedalCaptureGroup::Fuzz,
    volum::PrePedalCaptureGroup::Fuzz,
    volum::PrePedalCaptureGroup::Fuzz,
  };

  REQUIRE(captures.size() == filenames.size());
  for (size_t i = 0; i < captures.size(); ++i)
  {
    CAPTURE(i);
    CHECK(captures[i].filename == filenames[i]);
    CHECK(captures[i].label == labels[i]);
    CHECK(captures[i].shortLabel == shortLabels[i]);
    CHECK(captures[i].group == groups[i]);
  }

  CHECK(std::string(volum::PrePedalCaptureGroupLabel(volum::PrePedalCaptureGroup::Klon)) == "Klon");
  CHECK(std::string(volum::PrePedalCaptureGroupLabel(volum::PrePedalCaptureGroup::TsBoost)) == "TS / Boost");
  CHECK(std::string(volum::PrePedalCaptureGroupLabel(volum::PrePedalCaptureGroup::Distortion)) == "Distortion");
  CHECK(std::string(volum::PrePedalCaptureGroupLabel(volum::PrePedalCaptureGroup::Fuzz)) == "Fuzz");
}
