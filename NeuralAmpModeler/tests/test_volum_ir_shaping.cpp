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

// -----------------------------------------------------------------------------
// Popover affordances (1.2.1 polish): rail detection for graying out steppers, and
// free typed entry. Both live next to the ladders so the popover cannot disagree
// with the model about where a rail is or what a typed number means.
// -----------------------------------------------------------------------------

TEST_CASE("Stepper availability follows the ladders' rails")
{
  CHECK(IrTrimStepAvail(0.0).canDown);
  CHECK(IrTrimStepAvail(0.0).canUp);
  CHECK_FALSE(IrTrimStepAvail(kIrTrimDbMax).canUp);
  CHECK(IrTrimStepAvail(kIrTrimDbMax).canDown);
  CHECK_FALSE(IrTrimStepAvail(kIrTrimDbMin).canDown);

  // Low cut: OFF is the bottom rung, kIrLowCutHzMax the top.
  CHECK_FALSE(IrLowCutStepAvail(0.0).canDown);
  CHECK(IrLowCutStepAvail(0.0).canUp);
  CHECK_FALSE(IrLowCutStepAvail(kIrLowCutHzMax).canUp);
  CHECK(IrLowCutStepAvail(kIrLowCutHzMax).canDown);

  // High cut is inverted: OFF is the fully-open TOP rung, so "up" is the dead end.
  CHECK_FALSE(IrHighCutStepAvail(0.0).canUp);
  CHECK(IrHighCutStepAvail(0.0).canDown);
  CHECK_FALSE(IrHighCutStepAvail(kIrHighCutHzMin).canDown);
  CHECK(IrHighCutStepAvail(kIrHighCutHzMin).canUp);
}

TEST_CASE("Typed IR values parse the spellings a user actually types")
{
  CHECK(ParseIrTypedValue("2500").kind == IrTypedKind::Number);
  CHECK(ParseIrTypedValue("2500").value == doctest::Approx(2500.0));
  CHECK(ParseIrTypedValue("2.5k").value == doctest::Approx(2500.0));
  CHECK(ParseIrTypedValue("2.5 kHz").value == doctest::Approx(2500.0));
  CHECK(ParseIrTypedValue("800 Hz").value == doctest::Approx(800.0));
  CHECK(ParseIrTypedValue("-3 dB").value == doctest::Approx(-3.0));
  CHECK(ParseIrTypedValue("+3").value == doctest::Approx(3.0));

  CHECK(ParseIrTypedValue("off").kind == IrTypedKind::Off);
  CHECK(ParseIrTypedValue("OFF").kind == IrTypedKind::Off);
  CHECK(ParseIrTypedValue("").kind == IrTypedKind::Off);
  CHECK(ParseIrTypedValue("0").kind == IrTypedKind::Off);
  CHECK(ParseIrTypedValue("0 Hz").kind == IrTypedKind::Off);

  CHECK(ParseIrTypedValue("banana").kind == IrTypedKind::Invalid);
  CHECK(ParseIrTypedValue("2.5 furlongs").kind == IrTypedKind::Invalid);
}

TEST_CASE("Typed IR values are continuous within range, not snapped to a ladder rung")
{
  // 137 Hz is between the 120 and 150 rungs and must survive as typed.
  CHECK(ApplyTypedIrLowCutHz("137", 80.0) == doctest::Approx(137.0));
  CHECK(ApplyTypedIrHighCutHz("7350", 6000.0) == doctest::Approx(7350.0));
  CHECK(ApplyTypedIrTrimDb("-3.7", 0.0) == doctest::Approx(-3.7));

  // Out of range clamps rather than rejecting - the intent is unambiguous.
  CHECK(ApplyTypedIrTrimDb("99", 0.0) == doctest::Approx(kIrTrimDbMax));
  CHECK(ApplyTypedIrLowCutHz("5", 80.0) == doctest::Approx(20.0));
  CHECK(ApplyTypedIrLowCutHz("5000", 80.0) == doctest::Approx(kIrLowCutHzMax));
  CHECK(ApplyTypedIrHighCutHz("200", 6000.0) == doctest::Approx(kIrHighCutHzMin));

  // "off" disables the cuts; for a makeup gain it means unity, not silence.
  CHECK(ApplyTypedIrLowCutHz("off", 80.0) == doctest::Approx(0.0));
  CHECK(ApplyTypedIrHighCutHz("off", 6000.0) == doctest::Approx(0.0));
  CHECK(ApplyTypedIrTrimDb("off", -6.0) == doctest::Approx(0.0));

  // Garbage keeps the current value instead of resetting the control.
  CHECK(ApplyTypedIrTrimDb("banana", -6.0) == doctest::Approx(-6.0));
  CHECK(ApplyTypedIrLowCutHz("??", 80.0) == doctest::Approx(80.0));
  CHECK(ApplyTypedIrHighCutHz("nope", 6000.0) == doctest::Approx(6000.0));

  // A typed value the stepper can then keep walking from: it moves to the ADJACENT
  // rung in each direction rather than rounding to the nearest one first, which
  // would skip the 150 Hz rung 137 is sitting just below.
  CHECK(StepIrLowCutHz(ApplyTypedIrLowCutHz("137", 0.0), +1) == doctest::Approx(150.0));
  CHECK(StepIrLowCutHz(ApplyTypedIrLowCutHz("137", 0.0), -1) == doctest::Approx(120.0));
  CHECK(StepIrHighCutHz(ApplyTypedIrHighCutHz("7350", 0.0), +1) == doctest::Approx(8000.0));
  CHECK(StepIrHighCutHz(ApplyTypedIrHighCutHz("7350", 0.0), -1) == doctest::Approx(6000.0));
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
