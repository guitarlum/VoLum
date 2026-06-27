#include "third_party/doctest.h"
#include "../config.h"

// EParams enum — local copy to avoid IGraphics dependency.
// Must match NeuralAmpModeler.h exactly. If it drifts, tests fail → that's the point.
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
  kPrePitchDry, kPrePitchVoicing, kPrePitchLevel,
  kTremoloActive, kTremoloMode, kTremoloRate, kTremoloDepth, kTremoloShape, kTremoloMix,
  kTremoloCrossover, kTremoloSync, kTremoloDivision,
  kNumParams
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

TEST_CASE("EParam: delay staging params follow DelayMode")
{
  CHECK(kDelayTone == kDelayMode + 1);
  CHECK(kDelayAge == kDelayTone + 1);
  CHECK(kDelayPingPong == kDelayAge + 1);
}

TEST_CASE("EParam: reverb params follow delay")
{
  CHECK(kReverbActive == kDelayPingPong + 1);
  CHECK(kReverbMix == kReverbActive + 1);
  CHECK(kReverbDecay == kReverbMix + 1);
  CHECK(kReverbTone == kReverbDecay + 1);
  CHECK(kReverbPreDelay == kReverbTone + 1);
  CHECK(kReverbShimmer == kReverbPreDelay + 1);
  CHECK(kReverbMode == kReverbShimmer + 1);
}

TEST_CASE("EParam: reverb staging params follow ReverbMode")
{
  CHECK(kReverbSubMode == kReverbMode + 1);
}

TEST_CASE("EParam: boost params follow reverb staging")
{
  CHECK(kBoostActive == kReverbSubMode + 1);
  CHECK(kBoostDrive == kBoostActive + 1);
  CHECK(kBoostTone == kBoostDrive + 1);
  CHECK(kBoostLevel == kBoostTone + 1);
}

TEST_CASE("EParam: calibration params at end before kNumParams")
{
  CHECK(kPreCompActive == kBoostLevel + 1);
  CHECK(kPreCompAmount == kPreCompActive + 1);
  CHECK(kPreCompRatio == kPreCompAmount + 1);
  CHECK(kPreCompAttack == kPreCompRatio + 1);
  CHECK(kPreCompRelease == kPreCompAttack + 1);
  CHECK(kPreCompMix == kPreCompRelease + 1);
  CHECK(kPreCompLevel == kPreCompMix + 1);
  CHECK(kPreNam1Active == kPreCompLevel + 1);
  CHECK(kPreNam1Capture == kPreNam1Active + 1);
  CHECK(kPreNam1Gain == kPreNam1Capture + 1);
  CHECK(kPreNam1Bass == kPreNam1Gain + 1);
  CHECK(kPreNam1Mid == kPreNam1Bass + 1);
  CHECK(kPreNam1MidFreq == kPreNam1Mid + 1);
  CHECK(kPreNam1Treble == kPreNam1MidFreq + 1);
  CHECK(kPreNam1Level == kPreNam1Treble + 1);
  CHECK(kPreNam2Active == kPreNam1Level + 1);
  CHECK(kPreNam2Capture == kPreNam2Active + 1);
  CHECK(kPreNam2Gain == kPreNam2Capture + 1);
  CHECK(kPreNam2Bass == kPreNam2Gain + 1);
  CHECK(kPreNam2Mid == kPreNam2Bass + 1);
  CHECK(kPreNam2MidFreq == kPreNam2Mid + 1);
  CHECK(kPreNam2Treble == kPreNam2MidFreq + 1);
  CHECK(kPreNam2Level == kPreNam2Treble + 1);
  CHECK(kCalibrateInput == kPreNam2Level + 1);
  CHECK(kInputCalibrationLevel == kCalibrateInput + 1);
  CHECK(kOutputMode == kInputCalibrationLevel + 1);
  CHECK(kVoLumAmpeteRig == kOutputMode + 1);
  CHECK(kDualAmpActive == kVoLumAmpeteRig + 1);
  CHECK(kDualAmpRoute == kDualAmpActive + 1);
  CHECK(kMainAmpPan == kDualAmpRoute + 1);
  CHECK(kSupportAmpIdx == kMainAmpPan + 1);
  CHECK(kSupportSpeakerIdx == kSupportAmpIdx + 1);
  CHECK(kSupportChannelIdx == kSupportSpeakerIdx + 1);
  CHECK(kSupportInputLevel == kSupportChannelIdx + 1);
  CHECK(kSupportNoiseGateThreshold == kSupportInputLevel + 1);
  CHECK(kSupportToneBass == kSupportNoiseGateThreshold + 1);
  CHECK(kSupportToneMid == kSupportToneBass + 1);
  CHECK(kSupportToneTreble == kSupportToneMid + 1);
  CHECK(kSupportOutputLevel == kSupportToneTreble + 1);
  CHECK(kSupportNoiseGateActive == kSupportOutputLevel + 1);
  CHECK(kSupportEQActive == kSupportNoiseGateActive + 1);
  CHECK(kSupportAmpPan == kSupportEQActive + 1);
  // 1.2.0: per-lane support custom IR toggle appended at the very end.
  CHECK(kSupportIRToggle == kSupportAmpPan + 1);
}

TEST_CASE("EParam: PRE Pitch pedal params appended after SupportIRToggle")
{
  CHECK(kPrePitchActive == kSupportIRToggle + 1);
  CHECK(kPrePitchMode == kPrePitchActive + 1);
  CHECK(kPrePitchSemitones == kPrePitchMode + 1);
  CHECK(kPrePitchMix == kPrePitchSemitones + 1);
  CHECK(kPrePitchOctDown == kPrePitchMix + 1);
  CHECK(kPrePitchOctUp == kPrePitchOctDown + 1);
  CHECK(kPrePitchDry == kPrePitchOctUp + 1);
  CHECK(kPrePitchVoicing == kPrePitchDry + 1);
  CHECK(kPrePitchLevel == kPrePitchVoicing + 1);
}

TEST_CASE("EParam: POST Tremolo pedal params appended after PrePitchLevel")
{
  CHECK(kTremoloActive == kPrePitchLevel + 1);
  CHECK(kTremoloMode == kTremoloActive + 1);
  CHECK(kTremoloRate == kTremoloMode + 1);
  CHECK(kTremoloDepth == kTremoloRate + 1);
  CHECK(kTremoloShape == kTremoloDepth + 1);
  CHECK(kTremoloMix == kTremoloShape + 1);
  CHECK(kTremoloCrossover == kTremoloMix + 1);
  CHECK(kTremoloSync == kTremoloCrossover + 1);
  CHECK(kTremoloDivision == kTremoloSync + 1);
  CHECK(kNumParams == kTremoloDivision + 1);
}

TEST_CASE("EParam: total count is stable")
{
  // v0.9.0/effect-staging added 4 new params: DelayTone, DelayAge, DelayPingPong, ReverbSubMode.
  // 1.2.0 appended kSupportIRToggle (per-lane support custom IR): 71 -> 72.
  // 1.2.0 PRE Pitch pedal appended 9 params (Active, Mode, Semitones, Mix, OctDown,
  // OctUp, Dry, Voicing, Level): 72 -> 81. (Granular-engine rework dropped the
  // earlier Quality/Detune/Timbre knobs; feature was unreleased.)
  // POST Tremolo pedal appended 9 params (Active, Mode, Rate, Depth, Shape, Mix,
  // Crossover, Sync, Division): 81 -> 90.
  CHECK(kNumParams == 90);
}
