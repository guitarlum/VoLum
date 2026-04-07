#include "third_party/doctest.h"
#include <filesystem>

#include "activations.h"
#include "get_dsp.h"

namespace
{
std::filesystem::path RepoRoot()
{
  namespace fs = std::filesystem;
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path AmpeteDir()
{
  return RepoRoot() / "rigs" / "Ampete One";
}
} // namespace

TEST_CASE("Ampete NAM files exist")
{
  namespace fs = std::filesystem;
  const auto dir = AmpeteDir();
  INFO(dir.string());
  REQUIRE(fs::is_directory(dir));
  REQUIRE(fs::exists(dir / "AMP-Ampt-1.nam"));
}

TEST_CASE("Load Ampete NAM via nam::get_dsp(path)")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = std::filesystem::u8path((AmpeteDir() / "AMP-Ampt-1.nam").string());
  REQUIRE(std::filesystem::exists(path));
  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  model->Reset(48000.0, 512);
}

TEST_CASE("Load second Ampete NAM via path")
{
  nam::activations::Activation::enable_fast_tanh();
  const auto path = std::filesystem::u8path((AmpeteDir() / "G12-Ampt-2.nam").string());
  REQUIRE(std::filesystem::exists(path));
  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  model->Reset(44100.0, 256);
}
