#include "third_party/doctest.h"
#include "../config.h"

// EParams enum only — avoid pulling IGraphics via NeuralAmpModeler.h
enum EParams {
  kInputLevel = 0, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel,
  kNoiseGateActive, kEQActive, kIRToggle,
  kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayMode,
  kReverbActive, kReverbMix, kReverbDecay, kReverbTone, kReverbMode,
  kBoostActive, kBoostDrive, kBoostTone, kBoostLevel,
  kCalibrateInput, kInputCalibrationLevel, kOutputMode, kVoLumAmpeteRig, kNumParams
};

// Mirror of NAMKnobControl::GetKeyboardStep — must stay in sync.

static double ExpectedStep(int paramIdx, bool fine)
{
  switch (paramIdx)
  {
    case kToneBass:
    case kToneMid:
    case kToneTreble:
    case kReverbTone:
    case kBoostTone:
    case kBoostDrive:
      return fine ? 0.1 : 0.5;
    case kDelayTime:
      return fine ? 1.0 : 5.0;
    case kDelayFeedback:
    case kDelayMix:
    case kReverbMix:
    case kReverbDecay:
      return fine ? 0.01 : 0.05;
    default:
      return fine ? 0.1 : 1.0;
  }
}

TEST_CASE("Keyboard step: delay time = 5ms normal, 1ms fine")
{
  CHECK(ExpectedStep(kDelayTime, false) == 5.0);
  CHECK(ExpectedStep(kDelayTime, true) == 1.0);
}

TEST_CASE("Keyboard step: delay mix = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kDelayMix, false) == 0.05);
  CHECK(ExpectedStep(kDelayMix, true) == 0.01);
}

TEST_CASE("Keyboard step: delay feedback = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kDelayFeedback, false) == 0.05);
  CHECK(ExpectedStep(kDelayFeedback, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb mix = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kReverbMix, false) == 0.05);
  CHECK(ExpectedStep(kReverbMix, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb decay = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kReverbDecay, false) == 0.05);
  CHECK(ExpectedStep(kReverbDecay, true) == 0.01);
}

TEST_CASE("Keyboard step: tone knobs = 0.5 normal, 0.1 fine")
{
  int toneParams[] = {kToneBass, kToneMid, kToneTreble, kReverbTone};
  for (int i = 0; i < 4; i++)
  {
    CHECK(ExpectedStep(toneParams[i], false) == 0.5);
    CHECK(ExpectedStep(toneParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: input/output = 1.0 normal, 0.1 fine")
{
  int gainParams[] = {kInputLevel, kOutputLevel, kNoiseGateThreshold};
  for (int i = 0; i < 3; i++)
  {
    CHECK(ExpectedStep(gainParams[i], false) == 1.0);
    CHECK(ExpectedStep(gainParams[i], true) == 0.1);
  }
}
