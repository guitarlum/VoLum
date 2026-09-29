#include "third_party/doctest.h"
#include "../config.h"

// EParams comes from the same dependency-free header the plugin uses, so this
// order pin measures against the real enum (no hand-maintained mirror to drift).
// VoLumParams.h has no IGraphics/iPlug dependency, so it is safe in a test TU.
#include "../VoLumParams.h"

TEST_CASE("EParam indices are contiguous from kInputLevel through kNumParams")
{
  CHECK(kInputLevel == 0);
  CHECK(kNoiseGateThreshold == 1);
  CHECK(kToneBass == 2);
  CHECK(kToneMid == 3);
  CHECK(kToneTreble == 4);
  CHECK(kOutputLevel == 5);

  CHECK(kDelayActive == kIRToggle + 1);
  CHECK(kDelayTime == kDelayActive + 1);
  CHECK(kDelayFeedback == kDelayTime + 1);
  CHECK(kDelayMix == kDelayFeedback + 1);
  CHECK(kDelayMode == kDelayMix + 1);

  CHECK(kDelayTone == kDelayMode + 1);
  CHECK(kDelayAge == kDelayTone + 1);
  CHECK(kDelayPingPong == kDelayAge + 1);

  CHECK(kReverbActive == kDelayPingPong + 1);
  CHECK(kReverbMix == kReverbActive + 1);
  CHECK(kReverbDecay == kReverbMix + 1);
  CHECK(kReverbTone == kReverbDecay + 1);
  CHECK(kReverbPreDelay == kReverbTone + 1);
  CHECK(kReverbShimmer == kReverbPreDelay + 1);
  CHECK(kReverbMode == kReverbShimmer + 1);

  CHECK(kReverbSubMode == kReverbMode + 1);

  CHECK(kBoostActive == kReverbSubMode + 1);
  CHECK(kBoostDrive == kBoostActive + 1);
  CHECK(kBoostTone == kBoostDrive + 1);
  CHECK(kBoostLevel == kBoostTone + 1);

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
  CHECK(kSupportIRToggle == kSupportAmpPan + 1);

  CHECK(kPrePitchActive == kSupportIRToggle + 1);
  CHECK(kPrePitchMode == kPrePitchActive + 1);
  CHECK(kPrePitchSemitones == kPrePitchMode + 1);
  CHECK(kPrePitchMix == kPrePitchSemitones + 1);
  CHECK(kPrePitchOctDown == kPrePitchMix + 1);
  CHECK(kPrePitchOctUp == kPrePitchOctDown + 1);
  CHECK(kPrePitchDry == kPrePitchOctUp + 1);
  CHECK(kPrePitchVoicing == kPrePitchDry + 1);
  CHECK(kPrePitchLevel == kPrePitchVoicing + 1);

  CHECK(kTremoloActive == kPrePitchLevel + 1);
  CHECK(kTremoloMode == kTremoloActive + 1);
  CHECK(kTremoloRate == kTremoloMode + 1);
  CHECK(kTremoloDepth == kTremoloRate + 1);
  CHECK(kTremoloShape == kTremoloDepth + 1);
  CHECK(kTremoloMix == kTremoloShape + 1);
  CHECK(kTremoloCrossover == kTremoloMix + 1);
  CHECK(kTremoloSync == kTremoloCrossover + 1);
  CHECK(kTremoloDivision == kTremoloSync + 1);

  CHECK(kPrePitchTransChar == kTremoloDivision + 1);

  CHECK(kDelaySync == kPrePitchTransChar + 1);
  CHECK(kDelayDivision == kDelaySync + 1);

  CHECK(kChorusActive == kDelayDivision + 1);
  CHECK(kChorusMode == kChorusActive + 1);
  CHECK(kChorusRate == kChorusMode + 1);
  CHECK(kChorusDepth == kChorusRate + 1);
  CHECK(kChorusTone == kChorusDepth + 1);
  CHECK(kChorusWidth == kChorusTone + 1);
  CHECK(kChorusMix == kChorusWidth + 1);
  CHECK(kNumParams == kChorusMix + 1);
  CHECK(kChorusActive >= kVoLumChunkParamPrefixCount);
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
  // Transpose-engine rework appended kPrePitchTransChar (Drop/Instant): 90 -> 91.
  // Delay tempo sync appended kDelaySync + kDelayDivision: 91 -> 93.
  // POST Chorus pedal appended 7 params (Active, Mode, Rate, Depth, Tone, Width,
  // Mix): 93 -> 100. The chunk prefix stays frozen at 93 - anything past it is
  // id-tail JSON only, which is what makes this append safe for 1.2.2 readers.
  CHECK(kNumParams == 100);
  CHECK(kVoLumChunkParamPrefixCount == 93);
  CHECK(kNumParams >= kVoLumChunkParamPrefixCount);
}
