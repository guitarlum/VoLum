#pragma once

#include "VoLumAmpeteCatalog.h"

#include <cmath>
#include <string>

namespace volum
{

inline bool NearlyEqual(double a, double b)
{
  return std::fabs(a - b) < 1e-9;
}

inline bool PrePitchModesEqual(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  for (int mode = 0; mode < kVoLumPitchModeCount; ++mode)
    if (!NearlyEqual(a.prePitchModes[mode].mix, b.prePitchModes[mode].mix)
        || !NearlyEqual(a.prePitchModes[mode].dry, b.prePitchModes[mode].dry)
        || !NearlyEqual(a.prePitchModes[mode].level, b.prePitchModes[mode].level)
        || a.prePitchModes[mode].voicing != b.prePitchModes[mode].voicing)
      return false;
  return true;
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
         && a.prePitchTransChar == b.prePitchTransChar && PrePitchModesEqual(a, b);
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

inline bool TremoloModeSnapshotEquals(const TremoloModeSnapshot& a, const TremoloModeSnapshot& b)
{
  return NearlyEqual(a.rate, b.rate) && NearlyEqual(a.depth, b.depth) && NearlyEqual(a.shape, b.shape)
         && NearlyEqual(a.mix, b.mix) && NearlyEqual(a.crossover, b.crossover);
}

inline bool ChorusModeSnapshotEquals(const ChorusModeSnapshot& a, const ChorusModeSnapshot& b)
{
  return NearlyEqual(a.rate, b.rate) && NearlyEqual(a.depth, b.depth) && NearlyEqual(a.tone, b.tone)
         && NearlyEqual(a.width, b.width) && NearlyEqual(a.mix, b.mix);
}

inline bool PostBlockEquals(const VoLumAmpSettings& a, const VoLumAmpSettings& b)
{
  if (a.postChorusActive != b.postChorusActive || a.postChorusMode != b.postChorusMode)
    return false;

  if (!NearlyEqual(a.postChorusRate, b.postChorusRate) || !NearlyEqual(a.postChorusDepth, b.postChorusDepth)
      || !NearlyEqual(a.postChorusTone, b.postChorusTone) || !NearlyEqual(a.postChorusWidth, b.postChorusWidth)
      || !NearlyEqual(a.postChorusMix, b.postChorusMix))
    return false;

  for (int mode = 0; mode < kVoLumChorusModeCount; ++mode)
    if (!ChorusModeSnapshotEquals(a.postChorusModes[mode], b.postChorusModes[mode]))
      return false;

  if (a.postDelayActive != b.postDelayActive || a.postReverbActive != b.postReverbActive
      || a.postDelayMode != b.postDelayMode || a.postDelayPingPong != b.postDelayPingPong
      || a.postDelaySync != b.postDelaySync || a.postDelayDivision != b.postDelayDivision
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
  for (int mode = 0; mode < kVoLumTremoloModeCount; ++mode)
    if (!TremoloModeSnapshotEquals(a.postTremoloModes[mode], b.postTremoloModes[mode]))
      return false;

  return true;
}

// Field-for-field copies of the blocks PreBlockEquals / PostBlockEquals observe.
// postValid is copied with POST because a stored preset is a written scene, even
// though lock-dirty itself ignores the sentinel.
inline void CopyPreBlock(const VoLumAmpSettings& src, VoLumAmpSettings& dst)
{
  dst.preCompActive = src.preCompActive;
  dst.preCompAmount = src.preCompAmount;
  dst.preCompRatio = src.preCompRatio;
  dst.preCompAttack = src.preCompAttack;
  dst.preCompRelease = src.preCompRelease;
  dst.preCompMix = src.preCompMix;
  dst.preCompLevel = src.preCompLevel;
  dst.preNam1Active = src.preNam1Active;
  dst.preNam1Capture = src.preNam1Capture;
  dst.preNam1Gain = src.preNam1Gain;
  dst.preNam1Bass = src.preNam1Bass;
  dst.preNam1Mid = src.preNam1Mid;
  dst.preNam1MidFreq = src.preNam1MidFreq;
  dst.preNam1Treble = src.preNam1Treble;
  dst.preNam1Level = src.preNam1Level;
  dst.preNam2Active = src.preNam2Active;
  dst.preNam2Capture = src.preNam2Capture;
  dst.preNam2Gain = src.preNam2Gain;
  dst.preNam2Bass = src.preNam2Bass;
  dst.preNam2Mid = src.preNam2Mid;
  dst.preNam2MidFreq = src.preNam2MidFreq;
  dst.preNam2Treble = src.preNam2Treble;
  dst.preNam2Level = src.preNam2Level;
  dst.prePitchActive = src.prePitchActive;
  dst.prePitchMode = src.prePitchMode;
  dst.prePitchSemitones = src.prePitchSemitones;
  dst.prePitchMix = src.prePitchMix;
  dst.prePitchOctDown = src.prePitchOctDown;
  dst.prePitchOctUp = src.prePitchOctUp;
  dst.prePitchDry = src.prePitchDry;
  dst.prePitchVoicing = src.prePitchVoicing;
  dst.prePitchLevel = src.prePitchLevel;
  dst.prePitchTransChar = src.prePitchTransChar;
  for (int mode = 0; mode < kVoLumPitchModeCount; ++mode)
    dst.prePitchModes[mode] = src.prePitchModes[mode];
}

inline void CopyPostBlock(const VoLumAmpSettings& src, VoLumAmpSettings& dst)
{
  dst.postValid = src.postValid;
  dst.postDelayActive = src.postDelayActive;
  dst.postDelayTime = src.postDelayTime;
  dst.postDelayFeedback = src.postDelayFeedback;
  dst.postDelayMix = src.postDelayMix;
  dst.postDelayMode = src.postDelayMode;
  dst.postDelayTone = src.postDelayTone;
  dst.postDelayAge = src.postDelayAge;
  dst.postDelayPingPong = src.postDelayPingPong;
  dst.postDelaySync = src.postDelaySync;
  dst.postDelayDivision = src.postDelayDivision;
  dst.postReverbActive = src.postReverbActive;
  dst.postReverbMix = src.postReverbMix;
  dst.postReverbDecay = src.postReverbDecay;
  dst.postReverbTone = src.postReverbTone;
  dst.postReverbPreDelay = src.postReverbPreDelay;
  dst.postReverbShimmer = src.postReverbShimmer;
  dst.postReverbMode = src.postReverbMode;
  dst.postReverbSubMode = src.postReverbSubMode;
  dst.postTremoloActive = src.postTremoloActive;
  dst.postTremoloMode = src.postTremoloMode;
  dst.postTremoloRate = src.postTremoloRate;
  dst.postTremoloDepth = src.postTremoloDepth;
  dst.postTremoloShape = src.postTremoloShape;
  dst.postTremoloMix = src.postTremoloMix;
  dst.postTremoloCrossover = src.postTremoloCrossover;
  dst.postTremoloSync = src.postTremoloSync;
  dst.postTremoloDivision = src.postTremoloDivision;
  dst.postChorusActive = src.postChorusActive;
  dst.postChorusMode = src.postChorusMode;
  dst.postChorusRate = src.postChorusRate;
  dst.postChorusDepth = src.postChorusDepth;
  dst.postChorusTone = src.postChorusTone;
  dst.postChorusWidth = src.postChorusWidth;
  dst.postChorusMix = src.postChorusMix;
  for (int mode = 0; mode < kVoLumDelayModeCount; ++mode)
    dst.postDelayModes[mode] = src.postDelayModes[mode];
  for (int mode = 0; mode < kVoLumReverbModeCount; ++mode)
    dst.postReverbModes[mode] = src.postReverbModes[mode];
  for (int subMode = 0; subMode < 3; ++subMode)
    dst.postOktaverbSubModes[subMode] = src.postOktaverbSubModes[subMode];
  for (int mode = 0; mode < kVoLumTremoloModeCount; ++mode)
    dst.postTremoloModes[mode] = src.postTremoloModes[mode];
  for (int mode = 0; mode < kVoLumChorusModeCount; ++mode)
    dst.postChorusModes[mode] = src.postChorusModes[mode];
}

// The sounding rig for a preset snapshot: locked PRE/POST live in the overlay,
// not on the amp slot. Capture, dirty, and the save-time baseline must all use
// this view so a locked block is stored and `(unsaved)` agrees.
inline VoLumAmpSettings SoundingPresetScene(VoLumAmpSettings slot, bool preLocked,
                                            const VoLumAmpSettings& liveLockedPre, bool postLocked,
                                            const VoLumAmpSettings& liveLockedPost)
{
  if (preLocked)
    CopyPreBlock(liveLockedPre, slot);
  if (postLocked)
    CopyPostBlock(liveLockedPost, slot);
  return slot;
}

// Confirm-time re-resolve: the prompt named `id` at `capturedIdx`. The
// process-global library may have shifted. Empty id keeps the old positional
// behaviour (rows that have no identity).
inline int ResolveConfirmRowIndex(const std::string& id, int capturedIdx, int indexNow)
{
  if (id.empty())
    return capturedIdx;
  return indexNow;
}

} // namespace volum
