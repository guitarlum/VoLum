#include "third_party/doctest.h"
#include "../VoLumPrePedalCaptures.h"

#include <filesystem>
#include <fstream>

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}
} // namespace

TEST_CASE("PrePedals discovery sorts NAM captures and derives labels")
{
  namespace fs = std::filesystem;
  const fs::path tmp = fs::temp_directory_path() / "volum_pre_pedals_test";
  fs::remove_all(tmp);
  fs::create_directories(tmp / "PrePedals");
  std::ofstream((tmp / "PrePedals" / "20-Zed.nam").string()).close();
  std::ofstream((tmp / "PrePedals" / "10-Alpha.nam").string()).close();
  std::ofstream((tmp / "PrePedals" / "ignore.txt").string()).close();

  const auto captures = volum::DiscoverPrePedalCaptures(tmp);

  REQUIRE(captures.size() == 2);
  CHECK(captures[0].filename == "10-Alpha.nam");
  CHECK(captures[0].label == "10");
  CHECK(captures[1].filename == "20-Zed.nam");
  CHECK(captures[1].label == "20");

  fs::remove_all(tmp);
}

TEST_CASE("PrePedals empty index and capture bounds stay stable")
{
  CHECK(volum::kPreCaptureEmptyIndex == 0);
  CHECK(volum::ClampPreCaptureIndex(-1, 12) == 0);
  CHECK(volum::ClampPreCaptureIndex(5, 12) == 5);
  CHECK(volum::ClampPreCaptureIndex(15, 12) == 12);
  CHECK(volum::ClampPreCaptureIndex(200, 200) == volum::kPreCaptureMaxParamIndex);
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
