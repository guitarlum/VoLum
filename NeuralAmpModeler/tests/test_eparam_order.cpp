#include "third_party/doctest.h"
#include "../config.h"

// EParams enum — local copy to avoid IGraphics dependency.
// Must match NeuralAmpModeler.h exactly. If it drifts, tests fail → that's the point.
enum EParams {
  kInputLevel = 0, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel,
  kNoiseGateActive, kEQActive, kIRToggle,
  kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayMode,
  kReverbActive, kReverbMix, kReverbDecay, kReverbTone, kReverbMode,
  kBoostActive, kBoostDrive, kBoostTone, kBoostLevel,
  kCalibrateInput, kInputCalibrationLevel, kOutputMode, kVoLumAmpeteRig, kNumParams
};

TEST_CASE("EParam: core amp params at expected indices")
{
  CHECK(kInputLevel == 0);
  CHECK(kNoiseGateThreshold == 1);
  CHECK(kToneBass == 2);
  CHECK(kToneMid == 3);
  CHECK(kToneTreble == 4);
  CHECK(kOutputLevel == 5);
}

TEST_CASE("EParam: delay params are contiguous after IRToggle")
{
  CHECK(kDelayActive == kIRToggle + 1);
  CHECK(kDelayTime == kDelayActive + 1);
  CHECK(kDelayFeedback == kDelayTime + 1);
  CHECK(kDelayMix == kDelayFeedback + 1);
  CHECK(kDelayMode == kDelayMix + 1);
}

TEST_CASE("EParam: reverb params follow delay")
{
  CHECK(kReverbActive == kDelayMode + 1);
  CHECK(kReverbMix == kReverbActive + 1);
  CHECK(kReverbDecay == kReverbMix + 1);
  CHECK(kReverbTone == kReverbDecay + 1);
  CHECK(kReverbMode == kReverbTone + 1);
}

TEST_CASE("EParam: boost params follow reverb")
{
  CHECK(kBoostActive == kReverbMode + 1);
  CHECK(kBoostDrive == kBoostActive + 1);
  CHECK(kBoostTone == kBoostDrive + 1);
  CHECK(kBoostLevel == kBoostTone + 1);
}

TEST_CASE("EParam: calibration params at end before kNumParams")
{
  CHECK(kCalibrateInput == kBoostLevel + 1);
  CHECK(kInputCalibrationLevel == kCalibrateInput + 1);
  CHECK(kOutputMode == kInputCalibrationLevel + 1);
  CHECK(kVoLumAmpeteRig == kOutputMode + 1);
  CHECK(kNumParams == kVoLumAmpeteRig + 1);
}

TEST_CASE("EParam: total count is stable")
{
  CHECK(kNumParams == 27);
}
