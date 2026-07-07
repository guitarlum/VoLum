#include "third_party/doctest.h"

#include <cmath>

#include "../VoLumContentStore.h"

using namespace volum::content;

// -----------------------------------------------------------------------------
// Per-IR shaping (VoLum 1.2.1): trim + low/high cut stored in the IR library.
// These pin the pure model helpers (clamp, deterministic auto-normalize, the
// stepper ladders shared with the popover) plus the JSON round-trip / migration.
// -----------------------------------------------------------------------------

TEST_CASE("IR clamp helpers keep values inside the musical ranges")
{
  CHECK(ClampIrTrimDb(100.0) == doctest::Approx(kIrTrimDbMax));
  CHECK(ClampIrTrimDb(-100.0) == doctest::Approx(kIrTrimDbMin));
  CHECK(ClampIrTrimDb(3.5) == doctest::Approx(3.5));

  // 0 (or negative) means OFF for both cuts and must pass through unclamped.
  CHECK(ClampIrLowCutHz(0.0) == doctest::Approx(0.0));
  CHECK(ClampIrLowCutHz(-5.0) == doctest::Approx(0.0));
  CHECK(ClampIrLowCutHz(5.0) == doctest::Approx(20.0)); // below floor -> floor
  CHECK(ClampIrLowCutHz(5000.0) == doctest::Approx(kIrLowCutHzMax));
  CHECK(ClampIrLowCutHz(120.0) == doctest::Approx(120.0));

  CHECK(ClampIrHighCutHz(0.0) == doctest::Approx(0.0));
  CHECK(ClampIrHighCutHz(200.0) == doctest::Approx(kIrHighCutHzMin)); // below floor
  CHECK(ClampIrHighCutHz(50000.0) == doctest::Approx(kIrHighCutHzMax));
  CHECK(ClampIrHighCutHz(6000.0) == doctest::Approx(6000.0));
}

TEST_CASE("AutoNormalizeIrTrimDb undoes the baked -18 dB deterministically")
{
  // Effective convolution gain ~ L2 energy; trim = 18 - 20*log10(L2), clamped.
  CHECK(AutoNormalizeIrTrimDb(1.0) == doctest::Approx(18.0)); // unity IR -> undo bake
  CHECK(AutoNormalizeIrTrimDb(0.0) == doctest::Approx(0.0)); // silent/invalid -> no trim

  // A quiet IR (L2 = 0.1 -> +38 dB) saturates the +24 dB ceiling.
  CHECK(AutoNormalizeIrTrimDb(0.1) == doctest::Approx(kIrTrimDbMax));
  // A hot IR (L2 = 100 -> -22 dB) stays inside the floor.
  CHECK(AutoNormalizeIrTrimDb(100.0) == doctest::Approx(-22.0));
  // A very hot IR saturates the -24 dB floor.
  CHECK(AutoNormalizeIrTrimDb(1000.0) == doctest::Approx(kIrTrimDbMin));

  // Round-trip a known norm through the closed form (chosen to stay off the rails).
  const double l2 = 2.0;
  CHECK(AutoNormalizeIrTrimDb(l2) == doctest::Approx(18.0 - 20.0 * std::log10(l2)));
}

TEST_CASE("Trim stepper snaps to 0.5 dB and saturates at the rails")
{
  CHECK(StepIrTrimDb(0.0, +1) == doctest::Approx(0.5));
  CHECK(StepIrTrimDb(0.0, -1) == doctest::Approx(-0.5));
  CHECK(StepIrTrimDb(3.3, +1) == doctest::Approx(4.0)); // 3.3+0.5=3.8 snaps to the 0.5 grid
  CHECK(StepIrTrimDb(kIrTrimDbMax, +1) == doctest::Approx(kIrTrimDbMax)); // rail
  CHECK(StepIrTrimDb(kIrTrimDbMin, -1) == doctest::Approx(kIrTrimDbMin)); // rail
}

TEST_CASE("Low-cut ladder climbs from OFF and clamps at the top")
{
  const double first = StepIrLowCutHz(0.0, +1); // OFF -> first engaged rung
  CHECK(first == doctest::Approx(20.0));
  CHECK(StepIrLowCutHz(20.0, -1) == doctest::Approx(0.0)); // back to OFF
  CHECK(StepIrLowCutHz(0.0, -1) == doctest::Approx(0.0)); // already OFF, stays

  // Walk to the top rung and confirm it saturates at kIrLowCutHzMax.
  double hz = 0.0;
  for (int i = 0; i < 40; ++i)
    hz = StepIrLowCutHz(hz, +1);
  CHECK(hz == doctest::Approx(kIrLowCutHzMax));
}

TEST_CASE("High-cut ladder treats OFF as fully open at the top")
{
  // Stepping DOWN from OFF engages the brightest (highest) cut.
  CHECK(StepIrHighCutHz(0.0, -1) == doctest::Approx(kIrHighCutHzMax));
  CHECK(StepIrHighCutHz(kIrHighCutHzMax, +1) == doctest::Approx(0.0)); // back to OFF
  CHECK(StepIrHighCutHz(0.0, +1) == doctest::Approx(0.0)); // already OFF, stays

  // Walk DOWN to the darkest rung and confirm it saturates at kIrHighCutHzMin.
  double hz = 0.0;
  for (int i = 0; i < 40; ++i)
    hz = StepIrHighCutHz(hz, -1);
  CHECK(hz == doctest::Approx(kIrHighCutHzMin));
}

TEST_CASE("IR shaping round-trips through the registry JSON")
{
  Registry r;
  IRItem ir;
  ir.id = "ir_shape";
  ir.name = "Shaped IR";
  ir.file = "ir/ir_shape__x.wav";
  ir.trimDb = -6.5;
  ir.lowCutHz = 80.0;
  ir.highCutHz = 6000.0;
  ir.trimCalibrated = true;
  r.irs.push_back(ir);

  const Registry back = RegistryFromJson(RegistryToJson(r));
  REQUIRE(back.irs.size() == 1);
  CHECK(back.irs[0].trimDb == doctest::Approx(-6.5));
  CHECK(back.irs[0].lowCutHz == doctest::Approx(80.0));
  CHECK(back.irs[0].highCutHz == doctest::Approx(6000.0));
  // Presence of trimDb on load marks the entry calibrated (no re-normalize).
  CHECK(back.irs[0].trimCalibrated);
}

TEST_CASE("Out-of-range shaping on load is clamped, not rejected")
{
  nlohmann::json j;
  j["schemaVersion"] = kContentSchemaVersion;
  j["irLibrary"] = nlohmann::json::array();
  j["irLibrary"].push_back({{"id", "ir_hot"},
                            {"name", "Hot"},
                            {"path", "ir/ir_hot.wav"},
                            {"trimDb", 999.0},
                            {"lowCutHz", 99999.0},
                            {"highCutHz", 5.0}});

  const Registry r = RegistryFromJson(j);
  REQUIRE(r.irs.size() == 1);
  CHECK(r.irs[0].trimDb == doctest::Approx(kIrTrimDbMax));
  CHECK(r.irs[0].lowCutHz == doctest::Approx(kIrLowCutHzMax));
  CHECK(r.irs[0].highCutHz == doctest::Approx(kIrHighCutHzMin));
  CHECK(r.irs[0].trimCalibrated);
}

TEST_CASE("A pre-1.2.1 IR (no trimDb key) loads uncalibrated with cuts off")
{
  // v2 registry: IR entry predates the shaping fields. It must load cleanly and
  // stay uncalibrated so the plugin auto-normalizes it once from the .wav.
  nlohmann::json j;
  j["schemaVersion"] = 2;
  j["irLibrary"] = nlohmann::json::array();
  j["irLibrary"].push_back({{"id", "ir_legacy"}, {"name", "Legacy"}, {"path", "ir/ir_legacy.wav"}});

  const Registry r = RegistryFromJson(j);
  REQUIRE(r.irs.size() == 1);
  CHECK(r.irs[0].trimDb == doctest::Approx(0.0));
  CHECK(r.irs[0].lowCutHz == doctest::Approx(0.0));
  CHECK(r.irs[0].highCutHz == doctest::Approx(0.0));
  CHECK_FALSE(r.irs[0].trimCalibrated);
}
