#pragma once

#include "VoLumAmpeteCatalog.h"

#include <cmath>

namespace volum
{

inline bool NearlyEqual(double a, double b)
{
  return std::fabs(a - b) < 1e-9;
}

inline bool PreBlockEquals(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  return a.preCompActive == b.preCompActive && NearlyEqual(a.preCompAmount, b.preCompAmount)
         && NearlyEqual(a.preCompRatio, b.preCompRatio) && NearlyEqual(a.preCompAttack, b.preCompAttack)
         && NearlyEqual(a.preCompRelease, b.preCompRelease) && NearlyEqual(a.preCompMix, b.preCompMix)
         && NearlyEqual(a.preCompLevel, b.preCompLevel) && a.preNam1Active == b.preNam1Active
         && a.preNam1Capture == b.preNam1Capture && NearlyEqual(a.preNam1Gain, b.preNam1Gain)
         && NearlyEqual(a.preNam1Bass, b.preNam1Bass) && NearlyEqual(a.preNam1Mid, b.preNam1Mid)
         && NearlyEqual(a.preNam1MidFreq, b.preNam1MidFreq) && NearlyEqual(a.preNam1Treble, b.preNam1Treble)
         && NearlyEqual(a.preNam1Level, b.preNam1Level) && a.preNam2Active == b.preNam2Active
         && a.preNam2Capture == b.preNam2Capture && NearlyEqual(a.preNam2Gain, b.preNam2Gain)
         && NearlyEqual(a.preNam2Bass, b.preNam2Bass) && NearlyEqual(a.preNam2Mid, b.preNam2Mid)
         && NearlyEqual(a.preNam2MidFreq, b.preNam2MidFreq) && NearlyEqual(a.preNam2Treble, b.preNam2Treble)
         && NearlyEqual(a.preNam2Level, b.preNam2Level) && a.prePitchActive == b.prePitchActive
         && a.prePitchMode == b.prePitchMode && NearlyEqual(a.prePitchSemitones, b.prePitchSemitones)
         && NearlyEqual(a.prePitchMix, b.prePitchMix) && NearlyEqual(a.prePitchOctDown, b.prePitchOctDown)
         && NearlyEqual(a.prePitchOctUp, b.prePitchOctUp) && NearlyEqual(a.prePitchDry, b.prePitchDry)
         && a.prePitchVoicing == b.prePitchVoicing && NearlyEqual(a.prePitchLevel, b.prePitchLevel)
         && a.prePitchTransChar == b.prePitchTransChar;
}

inline bool DelayModeSnapshotEquals(const DelayModeSnapshot& a, const DelayModeSnapshot& b)
{
  return NearlyEqual(a.time, b.time) && NearlyEqual(a.feedback, b.feedback) && NearlyEqual(a.mix, b.mix)
         && NearlyEqual(a.tone, b.tone) && NearlyEqual(a.age, b.age) && a.pingPong == b.pingPong;
}

inline bool ReverbModeSnapshotEquals(const ReverbModeSnapshot& a, const ReverbModeSnapshot& b)
{
  return NearlyEqual(a.mix, b.mix) && NearlyEqual(a.decay, b.decay) && NearlyEqual(a.tone, b.tone)
         && NearlyEqual(a.preDelay, b.preDelay) && NearlyEqual(a.shimmer, b.shimmer) && a.subMode == b.subMode;
}

inline bool OktaverbSubModeSnapshotEquals(const OktaverbSubModeSnapshot& a, const OktaverbSubModeSnapshot& b)
{
  return NearlyEqual(a.mix, b.mix) && NearlyEqual(a.decay, b.decay) && NearlyEqual(a.tone, b.tone)
         && NearlyEqual(a.preDelay, b.preDelay) && NearlyEqual(a.shimmer, b.shimmer);
}

inline bool PostBlockEquals(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  if (a.postDelayActive != b.postDelayActive || a.postReverbActive != b.postReverbActive
      || a.postDelayMode != b.postDelayMode || a.postDelayPingPong != b.postDelayPingPong
      || a.postReverbMode != b.postReverbMode || a.postReverbSubMode != b.postReverbSubMode
      || a.postTremoloActive != b.postTremoloActive || a.postTremoloMode != b.postTremoloMode
      || a.postTremoloSync != b.postTremoloSync || a.postTremoloDivision != b.postTremoloDivision)
    return false;

  if (!NearlyEqual(a.postDelayTime, b.postDelayTime) || !NearlyEqual(a.postDelayFeedback, b.postDelayFeedback)
      || !NearlyEqual(a.postDelayMix, b.postDelayMix) || !NearlyEqual(a.postDelayTone, b.postDelayTone)
      || !NearlyEqual(a.postDelayAge, b.postDelayAge) || !NearlyEqual(a.postReverbMix, b.postReverbMix)
      || !NearlyEqual(a.postReverbDecay, b.postReverbDecay) || !NearlyEqual(a.postReverbTone, b.postReverbTone)
      || !NearlyEqual(a.postReverbPreDelay, b.postReverbPreDelay)
      || !NearlyEqual(a.postReverbShimmer, b.postReverbShimmer) || !NearlyEqual(a.postTremoloRate, b.postTremoloRate)
      || !NearlyEqual(a.postTremoloDepth, b.postTremoloDepth) || !NearlyEqual(a.postTremoloShape, b.postTremoloShape)
      || !NearlyEqual(a.postTremoloMix, b.postTremoloMix)
      || !NearlyEqual(a.postTremoloCrossover, b.postTremoloCrossover))
    return false;

  for (int mode = 0; mode < kVoLumDelayModeCount; ++mode)
    if (!DelayModeSnapshotEquals(a.postDelayModes[mode], b.postDelayModes[mode]))
      return false;
  for (int mode = 0; mode < kVoLumReverbModeCount; ++mode)
    if (!ReverbModeSnapshotEquals(a.postReverbModes[mode], b.postReverbModes[mode]))
      return false;
  for (int subMode = 0; subMode < 3; ++subMode)
    if (!OktaverbSubModeSnapshotEquals(a.postOktaverbSubModes[subMode], b.postOktaverbSubModes[subMode]))
      return false;

  return true;
}

} // namespace volum
