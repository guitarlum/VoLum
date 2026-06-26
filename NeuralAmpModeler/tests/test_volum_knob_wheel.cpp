#include "third_party/doctest.h"
#include "../config.h"

// EParams enum only - avoid pulling IGraphics via NeuralAmpModeler.h
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
  kSupportOutputLevel, kSupportNoiseGateActive, kSupportEQActive, kSupportAmpPan,
  kSupportIRToggle,
  kPrePitchActive, kPrePitchMode, kPrePitchSemitones, kPrePitchMix, kPrePitchOctDown, kPrePitchOctUp,
  kPrePitchDry, kPrePitchVoicing, kPrePitchLevel, kPrePitchQuality, kPrePitchDetune, kPrePitchTimbre,
  kTremoloActive, kTremoloMode, kTremoloRate, kTremoloDepth, kTremoloShape, kTremoloMix,
  kTremoloCrossover, kTremoloSync, kTremoloDivision,
  kNumParams
};

#include "../VoLumKeyboardModel.h"

TEST_CASE("Knob wheel accumulator emits one step per whole delta")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(1.0) == 1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.0));
}

TEST_CASE("Knob wheel accumulator combines smooth-scroll fractions")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 0);
  CHECK(wheel.OnDelta(0.25) == 1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.0));
}

TEST_CASE("Knob wheel accumulator drops stale remainder on direction change")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(0.4) == 0);
  CHECK(wheel.OnDelta(-0.5) == 0);
  CHECK(wheel.ResidualForTests() == doctest::Approx(-0.5));
  CHECK(wheel.OnDelta(-0.6) == -1);
  CHECK(wheel.ResidualForTests() == doctest::Approx(-0.1));
}

TEST_CASE("Knob wheel accumulator emits multiple steps for large deltas")
{
  volum::keyboard::WheelAccumulator wheel;

  CHECK(wheel.OnDelta(3.7) == 3);
  CHECK(wheel.ResidualForTests() == doctest::Approx(0.7));
}
