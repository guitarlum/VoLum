#include "third_party/doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "get_dsp.h"

#include "../VoLumOutputMode.h"

// Input calibration, end to end.
//
// The 1.2.1 report was "input calibration seems to do nothing". It turns out the
// feature is correct but inert on every model available here: calibration needs a
// model that declares the level it was captured at (`input_level_dbu`), and none
// of the 249 bundled rigs carry that key. A capture whose trainer left it null
// counts as "unknown" too, which is easy to miss because the key is present.
//
// These tests pin both halves: the arithmetic against real calibrated models that
// ship with NAMCore, and the "no metadata, no offset" behavior against a real
// bundled rig. Nothing here fabricates an input level for a shipped rig - we do
// not know their capture levels, and inventing one would mis-stage real gain.

using volum::ComputeInputGainDb;

namespace
{
std::filesystem::path RepoRoot()
{
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::filesystem::path ExampleModel(const char* leaf)
{
  return RepoRoot() / "NeuralAmpModelerCore" / "example_models" / leaf;
}
} // namespace

// ---- the arithmetic ---------------------------------------------------------

TEST_CASE("Input calibration offsets the knob by interface level minus capture level")
{
  // Interface calibrated at 12 dBu, model captured at 18.3 dBu: the model expects
  // a hotter signal than the interface delivers at 0 dBFS, so the offset is
  // negative and the model sees the same drive the real amp did.
  CHECK(ComputeInputGainDb(0.0, true, true, 18.3, 12.0) == doctest::Approx(-6.3));
  // The knob rides on top of the offset rather than being replaced by it.
  CHECK(ComputeInputGainDb(3.0, true, true, 18.3, 12.0) == doctest::Approx(-3.3));
  // A hotter interface than the capture pushes the other way.
  CHECK(ComputeInputGainDb(0.0, true, true, 12.0, 18.3) == doctest::Approx(6.3));
  // Matched levels are a no-op.
  CHECK(ComputeInputGainDb(-2.5, true, true, 15.0, 15.0) == doctest::Approx(-2.5));
}

TEST_CASE("Input calibration is a pass-through when the switch is off")
{
  CHECK(ComputeInputGainDb(4.0, false, true, 18.3, 12.0) == doctest::Approx(4.0));
}

TEST_CASE("Input calibration is a pass-through when the model has no capture level")
{
  // This is the reported "does nothing" case, and it is correct behavior: with no
  // declared capture level there is nothing to align the interface level against.
  CHECK(ComputeInputGainDb(4.0, true, false, 0.0, 12.0) == doctest::Approx(4.0));
  // Even a wild placeholder level must not leak in while the model says "unknown".
  CHECK(ComputeInputGainDb(0.0, true, false, 99.0, 12.0) == doctest::Approx(0.0));
}

// ---- against real models ----------------------------------------------------

TEST_CASE("A real calibrated model reports its capture level and drives the offset")
{
  const auto path = ExampleModel("wavenet.nam");
  INFO(path.string());
  REQUIRE(std::filesystem::exists(path));

  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  REQUIRE(model->HasInputLevel());
  CHECK(model->GetInputLevel() == doctest::Approx(18.3));

  // Same path the plugin takes in _SetInputGain.
  const double gainDb = ComputeInputGainDb(0.0, true, model->HasInputLevel(), model->GetInputLevel(), 12.0);
  CHECK(gainDb == doctest::Approx(-6.3));
}

TEST_CASE("A model whose input_level_dbu is null counts as uncalibrated")
{
  // The key is present but null, which is what a trainer writes when it did not
  // measure the level. Reading the key's presence instead of its value would apply
  // a bogus 0 dBu capture level and mis-stage the gain by ~12 dB.
  const auto src = ExampleModel("lstm.nam");
  REQUIRE(std::filesystem::exists(src));
  std::string body;
  {
    std::ifstream in(src, std::ios::binary);
    body.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  }
  const std::string key = "\"input_level_dbu\": 18.3";
  const auto at = body.find(key);
  REQUIRE(at != std::string::npos);
  body.replace(at, key.size(), "\"input_level_dbu\": null");

  const auto tmp = std::filesystem::temp_directory_path() / "volum-input-cal-null.nam";
  {
    std::ofstream out(tmp, std::ios::binary);
    out << body;
  }

  auto model = nam::get_dsp(tmp);
  REQUIRE(model != nullptr);
  CHECK_FALSE(model->HasInputLevel());
  CHECK(ComputeInputGainDb(0.0, true, model->HasInputLevel(), 0.0, 12.0) == doctest::Approx(0.0));

  std::error_code ec;
  std::filesystem::remove(tmp, ec);
}

TEST_CASE("Bundled factory rigs declare loudness but no capture level")
{
  // Documents why calibration is inert on the shipped rigs, and guards the claim:
  // if a future rig import ever does carry input_level_dbu, this test tells us the
  // calibration story for factory amps has changed and the UI copy needs revisiting.
  const auto path = RepoRoot() / "rigs" / "Ampete One" / "AMP-Ampt-1.nam";
  INFO(path.string());
  REQUIRE(std::filesystem::exists(path));

  auto model = nam::get_dsp(path);
  REQUIRE(model != nullptr);
  CHECK(model->HasLoudness()); // output normalization works
  CHECK_FALSE(model->HasInputLevel()); // input calibration has nothing to align to

  CHECK(ComputeInputGainDb(2.0, true, model->HasInputLevel(), 0.0, 12.0) == doctest::Approx(2.0));
}
