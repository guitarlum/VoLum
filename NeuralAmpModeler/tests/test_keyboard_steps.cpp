#include "third_party/doctest.h"
#include "../config.h"

// EParams enum only — avoid pulling IGraphics via NeuralAmpModeler.h
enum EParams {
  kInputLevel = 0, kNoiseGateThreshold, kToneBass, kToneMid, kToneTreble, kOutputLevel,
  kNoiseGateActive, kEQActive, kIRToggle,
  kDelayActive, kDelayTime, kDelayFeedback, kDelayMix, kDelayMode,
  kDelayTone, kDelayAge, kDelayPingPong,
  kReverbActive, kReverbMix, kReverbDecay, kReverbTone, kReverbPreDelay, kReverbShimmer, kReverbMode,
  kReverbSubMode,
  kBoostActive, kBoostDrive, kBoostTone, kBoostLevel,
  kPreCompActive, kPreCompAmount, kPreCompRatio, kPreCompAttack, kPreCompRelease, kPreCompMix, kPreCompLevel,
  kPreNam1Active, kPreNam1Capture, kPreNam1Gain, kPreNam1Bass, kPreNam1Mid, kPreNam1MidFreq, kPreNam1Treble, kPreNam1Level,
  kPreNam2Active, kPreNam2Capture, kPreNam2Gain, kPreNam2Bass, kPreNam2Mid, kPreNam2MidFreq, kPreNam2Treble, kPreNam2Level,
  kCalibrateInput, kInputCalibrationLevel, kOutputMode, kVoLumAmpeteRig,
  kDualAmpActive, kDualAmpRoute, kMainAmpPan, kSupportAmpIdx, kSupportSpeakerIdx, kSupportChannelIdx,
  kSupportInputLevel, kSupportNoiseGateThreshold, kSupportToneBass, kSupportToneMid, kSupportToneTreble,
  kSupportOutputLevel, kSupportNoiseGateActive, kSupportEQActive, kSupportAmpPan, kNumParams
};

#include "../VoLumKeyboardModel.h"

TEST_CASE("Keyboard step: delay time = 5ms normal, 1ms fine")
{
  CHECK(volum::keyboard::StepForParam(kDelayTime, false) == 5.0);
  CHECK(volum::keyboard::StepForParam(kDelayTime, true) == 1.0);
}

TEST_CASE("Keyboard step: delay mix = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kDelayMix, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kDelayMix, true) == 0.01);
}

TEST_CASE("Keyboard step: delay feedback = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kDelayFeedback, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kDelayFeedback, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb mix = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kReverbMix, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kReverbMix, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb decay = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kReverbDecay, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kReverbDecay, true) == 0.01);
}

TEST_CASE("Keyboard step: reverb pre-delay = 5ms normal, 1ms fine")
{
  CHECK(volum::keyboard::StepForParam(kReverbPreDelay, false) == 5.0);
  CHECK(volum::keyboard::StepForParam(kReverbPreDelay, true) == 1.0);
}

TEST_CASE("Keyboard step: reverb shimmer = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kReverbShimmer, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kReverbShimmer, true) == 0.01);
}

TEST_CASE("Keyboard step: tone knobs = 0.5 normal, 0.1 fine")
{
  int toneParams[] = {kToneBass, kToneMid, kToneTreble, kReverbTone, kSupportToneBass, kSupportToneMid, kSupportToneTreble};
  for (int i = 0; i < 7; i++)
  {
    CHECK(volum::keyboard::StepForParam(toneParams[i], false) == 0.5);
    CHECK(volum::keyboard::StepForParam(toneParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: level knobs = 0.5 normal, 0.1 fine")
{
  int levelParams[] = {kInputLevel, kOutputLevel, kSupportInputLevel, kSupportOutputLevel,
                       kPreNam1Gain, kPreNam1Level, kPreNam2Gain, kPreNam2Level, kPreCompLevel};
  for (int i = 0; i < 9; i++)
  {
    CHECK(volum::keyboard::StepForParam(levelParams[i], false) == 0.5);
    CHECK(volum::keyboard::StepForParam(levelParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: noise gate thresholds = 1.0 normal, 0.1 fine")
{
  int thresholdParams[] = {kNoiseGateThreshold, kSupportNoiseGateThreshold};
  for (int i = 0; i < 2; i++)
  {
    CHECK(volum::keyboard::StepForParam(thresholdParams[i], false) == 1.0);
    CHECK(volum::keyboard::StepForParam(thresholdParams[i], true) == 0.1);
  }
}

TEST_CASE("Keyboard step: delay tone/age = 0.05 normal, 0.01 fine")
{
  CHECK(volum::keyboard::StepForParam(kDelayTone, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kDelayTone, true) == 0.01);
  CHECK(volum::keyboard::StepForParam(kDelayAge, false) == 0.05);
  CHECK(volum::keyboard::StepForParam(kDelayAge, true) == 0.01);
}

