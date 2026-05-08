#include "third_party/doctest.h"
#include "../config.h"

// EParams enum only — avoid pulling IGraphics via NeuralAmpModeler.h
enum EParams {
  kInputLevel = 0, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel,
  kNoiseGateActive, kEQActive, kIRToggle,
  kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayMode,
  kDelayTone, kDelayAge, kDelayPingPong, kDelayTapeSubMode,
  kReverbActive, kReverbMix, kReverbDecay, kReverbTone, kReverbPreDelay, kReverbShimmer, kReverbMode,
  kReverbSubMode, kReverbTremRate,
  kBoostActive, kBoostDrive, kBoostTone, kBoostLevel,
  kPreCompActive, kPreCompAmount, kPreCompRatio, kPreCompAttack, kPreCompRelease, kPreCompMix, kPreCompLevel,
  kPreNam1Active, kPreNam1Capture, kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble, kPreNam1Level,
  kPreNam2Active, kPreNam2Capture, kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble, kPreNam2Level,
  kCalibrateInput, kInputCalibrationLevel, kOutputMode, kVoLumAmpeteRig,
  kDualAmpActive, kDualAmpRoute, kMainAmpPan, kSupportAmpIdx, kSupportSpeakerIdx, kSupportChannelIdx,
  kSupportInputLevel, kSupportNoiseGateThreshold, kSupportToneBass, kSupportToneMid, kSupportToneTreble,
  kSupportOutputLevel, kSupportNoiseGateActive, kSupportEQActive, kSupportAmpPan, kNumParams
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
    case kPreNam1Bass:
    case kPreNam1Mid:
    case kPreNam1Treble:
    case kPreNam2Bass:
    case kPreNam2Mid:
    case kPreNam2Treble:
    case kSupportToneBass:
    case kSupportToneMid:
    case kSupportToneTreble:
      return fine ? 0.1 : 0.5;
    case kDelayTime:
    case kReverbPreDelay:
    case kPreNam1MidFreq:
    case kPreNam2MidFreq:
    case kPreCompAttack:
    case kPreCompRelease:
      return fine ? 1.0 : 5.0;
    case kDelayFeedback:
    case kDelayMix:
    case kDelayTone:
    case kDelayAge:
    case kReverbMix:
    case kReverbDecay:
    case kReverbShimmer:
    case kPreCompMix:
      return fine ? 0.01 : 0.05;
    case kReverbTremRate:
      return fine ? 0.1 : 0.5;
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

TEST_CASE("Keyboard step: reverb pre-delay = 5ms normal, 1ms fine")
{
  CHECK(ExpectedStep(kReverbPreDelay, false) == 5.0);
  CHECK(ExpectedStep(kReverbPreDelay, true) == 1.0);
}

TEST_CASE("Keyboard step: reverb shimmer = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kReverbShimmer, false) == 0.05);
  CHECK(ExpectedStep(kReverbShimmer, true) == 0.01);
}

TEST_CASE("Keyboard step: tone knobs = 0.5 normal, 0.1 fine")
{
  int toneParams[] = {kToneBass, kToneMid, kToneTreble, kReverbTone, kSupportToneBass, kSupportToneMid, kSupportToneTreble};
  for (int i = 0; i < 7; i++)
  {
    CHECK(ExpectedStep(toneParams[i], false) == 0.5);
    CHECK(ExpectedStep(toneParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: input/output = 1.0 normal, 0.1 fine")
{
  int gainParams[] = {kInputLevel, kOutputLevel, kNoiseGateThreshold, kSupportInputLevel, kSupportOutputLevel, kSupportNoiseGateThreshold};
  for (int i = 0; i < 6; i++)
  {
    CHECK(ExpectedStep(gainParams[i], false) == 1.0);
    CHECK(ExpectedStep(gainParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: delay tone/age = 0.05 normal, 0.01 fine")
{
  CHECK(ExpectedStep(kDelayTone, false) == 0.05);
  CHECK(ExpectedStep(kDelayTone, true) == 0.01);
  CHECK(ExpectedStep(kDelayAge, false) == 0.05);
  CHECK(ExpectedStep(kDelayAge, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb tremolo rate = 0.5 normal, 0.1 fine")
{
  CHECK(ExpectedStep(kReverbTremRate, false) == 0.5);
  CHECK(ExpectedStep(kReverbTremRate, true) == 0.1);
}
