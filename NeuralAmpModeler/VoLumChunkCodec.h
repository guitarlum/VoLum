#pragma once

#include <algorithm>

#include "VoLumAmpeteCatalog.h"
#include "VoLumChunkVersion.h"
#include "VoLumPrePedalCaptures.h"

namespace volum
{

struct VoLumChunkSelection
{
  int ampIdx = 0;
  int speakerIdx = 3;
  int channelIdx = 0;
};

inline VoLumChunkSelection ClampChunkSelection(VoLumChunkSelection selection)
{
  selection.ampIdx = std::clamp(selection.ampIdx, 0, kAmpCount - 1);
  selection.speakerIdx = std::clamp(selection.speakerIdx, 0, 3);
  selection.channelIdx = std::max(0, selection.channelIdx);
  return selection;
}

inline void ClampPreCaptureSlots(VoLumAmpSettings& settings, int captureCount = kPreCaptureMaxParamIndex)
{
  settings.preNam1Capture = ClampPreCaptureIndex(settings.preNam1Capture, captureCount);
  settings.preNam2Capture = ClampPreCaptureIndex(settings.preNam2Capture, captureCount);
}

inline void ResetPreCaptureSlots(VoLumAmpSettings& settings)
{
  settings.preNam1Capture = kPreCaptureEmptyIndex;
  settings.preNam2Capture = kPreCaptureEmptyIndex;
}

inline bool ShouldResetPreCaptureSlotsForChunkVersion(const ChunkVersion& version)
{
  return !(version >= ChunkVersion(0, 8, 4));
}

template<typename Chunk>
void PutCurrentPerAmpSettings(Chunk& chunk, const VoLumAmpSettings& s)
{
  chunk.Put(&s.speakerIdx);
  chunk.Put(&s.channelIdx);
  chunk.Put(&s.inputLevel);
  chunk.Put(&s.gateThreshold);
  chunk.Put(&s.toneBass);
  chunk.Put(&s.toneMid);
  chunk.Put(&s.toneTreble);
  chunk.Put(&s.outputLevel);

  int ng = s.noiseGateActive ? 1 : 0;
  int eq = s.eqActive ? 1 : 0;
  int pc = s.preCompActive ? 1 : 0;
  int p1 = s.preNam1Active ? 1 : 0;
  int p2 = s.preNam2Active ? 1 : 0;

  chunk.Put(&ng);
  chunk.Put(&eq);
  chunk.Put(&pc);
  chunk.Put(&s.preCompAmount);
  chunk.Put(&s.preCompRatio);
  chunk.Put(&s.preCompAttack);
  chunk.Put(&s.preCompRelease);
  chunk.Put(&s.preCompMix);
  chunk.Put(&s.preCompLevel);
  chunk.Put(&p1);
  chunk.Put(&s.preNam1Capture);
  chunk.Put(&s.preNam1Gain);
  chunk.Put(&s.preNam1Bass);
  chunk.Put(&s.preNam1Mid);
  chunk.Put(&s.preNam1MidFreq);
  chunk.Put(&s.preNam1Treble);
  chunk.Put(&s.preNam1Level);
  chunk.Put(&p2);
  chunk.Put(&s.preNam2Capture);
  chunk.Put(&s.preNam2Gain);
  chunk.Put(&s.preNam2Bass);
  chunk.Put(&s.preNam2Mid);
  chunk.Put(&s.preNam2MidFreq);
  chunk.Put(&s.preNam2Treble);
  chunk.Put(&s.preNam2Level);

  int dual = s.dualAmpActive ? 1 : 0;
  int supportNg = s.supportNoiseGateActive ? 1 : 0;
  int supportEq = s.supportEqActive ? 1 : 0;
  int supportPolarityInvert = s.supportPolarityInvert ? 1 : 0;
  chunk.Put(&dual);
  chunk.Put(&s.dualAmpRoute);
  chunk.Put(&s.mainAmpPan);
  chunk.Put(&s.supportAmpIdx);
  chunk.Put(&s.supportSpeakerIdx);
  chunk.Put(&s.supportChannelIdx);
  chunk.Put(&s.supportInputLevel);
  chunk.Put(&s.supportGateThreshold);
  chunk.Put(&s.supportToneBass);
  chunk.Put(&s.supportToneMid);
  chunk.Put(&s.supportToneTreble);
  chunk.Put(&s.supportOutputLevel);
  chunk.Put(&supportNg);
  chunk.Put(&supportEq);
  chunk.Put(&s.supportAmpPan);
  chunk.Put(&supportPolarityInvert);

  // POST per-amp tail. Always written by current builds; gated on read by
  // ChunkHasPostPerAmpSettings so legacy chunks restore factory POST defaults
  // instead of inheriting the previously selected amp.
  PutPostPerAmpSettings(chunk, s);
}

template<typename Chunk>
void PutCurrentVoLumChunkState(Chunk& chunk, VoLumChunkSelection selection, const VoLumAmpSettings* ampSettings,
                               int ampCount)
{
  selection = ClampChunkSelection(selection);
  chunk.Put(&selection.ampIdx);
  chunk.Put(&selection.speakerIdx);
  chunk.Put(&selection.channelIdx);
  for (int i = 0; i < ampCount; ++i)
    PutCurrentPerAmpSettings(chunk, ampSettings[i]);
}

template<typename Chunk>
void PutPrePostLockFlags(Chunk& chunk, bool preLocked, bool postLocked)
{
  int pre = preLocked ? 1 : 0;
  int post = postLocked ? 1 : 0;
  chunk.Put(&pre);
  chunk.Put(&post);
}

template<typename Chunk>
int GetLegacyPerAmpSettings(const Chunk& chunk, int pos, VoLumAmpSettings& s)
{
  pos = chunk.Get(&s.speakerIdx, pos);
  pos = chunk.Get(&s.channelIdx, pos);
  pos = chunk.Get(&s.inputLevel, pos);
  pos = chunk.Get(&s.gateThreshold, pos);
  pos = chunk.Get(&s.toneBass, pos);
  pos = chunk.Get(&s.toneMid, pos);
  pos = chunk.Get(&s.toneTreble, pos);
  pos = chunk.Get(&s.outputLevel, pos);

  int ng = 1;
  int eq = 1;
  pos = chunk.Get(&ng, pos);
  pos = chunk.Get(&eq, pos);
  s.noiseGateActive = (ng != 0);
  s.eqActive = (eq != 0);
  return pos;
}

template<typename Chunk>
int GetExtendedPerAmpSettings(const Chunk& chunk, int pos, VoLumAmpSettings& s, bool has081PreCompControls)
{
  int pc = 0;
  int p1 = 0;
  int p2 = 0;
  pos = chunk.Get(&pc, pos);
  pos = chunk.Get(&s.preCompAmount, pos);
  if (has081PreCompControls)
  {
    pos = chunk.Get(&s.preCompRatio, pos);
    pos = chunk.Get(&s.preCompAttack, pos);
    pos = chunk.Get(&s.preCompRelease, pos);
    pos = chunk.Get(&s.preCompMix, pos);
  }
  pos = chunk.Get(&s.preCompLevel, pos);
  pos = chunk.Get(&p1, pos);
  pos = chunk.Get(&s.preNam1Capture, pos);
  pos = chunk.Get(&s.preNam1Gain, pos);
  pos = chunk.Get(&s.preNam1Bass, pos);
  pos = chunk.Get(&s.preNam1Mid, pos);
  pos = chunk.Get(&s.preNam1MidFreq, pos);
  pos = chunk.Get(&s.preNam1Treble, pos);
  pos = chunk.Get(&s.preNam1Level, pos);
  pos = chunk.Get(&p2, pos);
  pos = chunk.Get(&s.preNam2Capture, pos);
  pos = chunk.Get(&s.preNam2Gain, pos);
  pos = chunk.Get(&s.preNam2Bass, pos);
  pos = chunk.Get(&s.preNam2Mid, pos);
  pos = chunk.Get(&s.preNam2MidFreq, pos);
  pos = chunk.Get(&s.preNam2Treble, pos);
  pos = chunk.Get(&s.preNam2Level, pos);

  s.preCompActive = (pc != 0);
  s.preNam1Active = (p1 != 0);
  s.preNam2Active = (p2 != 0);
  ClampPreCaptureSlots(s);
  return pos;
}

template<typename Chunk>
int GetDualAmpPerAmpSettings(const Chunk& chunk, int pos, VoLumAmpSettings& s, bool hasSupportPolarityInvert = true)
{
  int dual = 0;
  int supportNg = 1;
  int supportEq = 1;
  int supportPolarityInvert = 0;
  pos = chunk.Get(&dual, pos);
  pos = chunk.Get(&s.dualAmpRoute, pos);
  pos = chunk.Get(&s.mainAmpPan, pos);
  pos = chunk.Get(&s.supportAmpIdx, pos);
  pos = chunk.Get(&s.supportSpeakerIdx, pos);
  pos = chunk.Get(&s.supportChannelIdx, pos);
  pos = chunk.Get(&s.supportInputLevel, pos);
  pos = chunk.Get(&s.supportGateThreshold, pos);
  pos = chunk.Get(&s.supportToneBass, pos);
  pos = chunk.Get(&s.supportToneMid, pos);
  pos = chunk.Get(&s.supportToneTreble, pos);
  pos = chunk.Get(&s.supportOutputLevel, pos);
  pos = chunk.Get(&supportNg, pos);
  pos = chunk.Get(&supportEq, pos);
  pos = chunk.Get(&s.supportAmpPan, pos);
  if (hasSupportPolarityInvert)
    pos = chunk.Get(&supportPolarityInvert, pos);

  s.dualAmpActive = (dual != 0);
  s.dualAmpRoute = std::clamp(s.dualAmpRoute, 0, 2);
  s.mainAmpPan = std::clamp(s.mainAmpPan, -1.0, 1.0);
  s.supportAmpIdx = std::clamp(s.supportAmpIdx, -1, kAmpCount - 1);
  s.supportSpeakerIdx = std::clamp(s.supportSpeakerIdx, 0, 3);
  s.supportChannelIdx = std::max(0, s.supportChannelIdx);
  s.supportInputLevel = std::clamp(s.supportInputLevel, -20.0, 20.0);
  s.supportGateThreshold = std::clamp(s.supportGateThreshold, -100.0, 0.0);
  s.supportToneBass = std::clamp(s.supportToneBass, 0.0, 10.0);
  s.supportToneMid = std::clamp(s.supportToneMid, 0.0, 10.0);
  s.supportToneTreble = std::clamp(s.supportToneTreble, 0.0, 10.0);
  s.supportOutputLevel = std::clamp(s.supportOutputLevel, -40.0, 10.0);
  s.supportNoiseGateActive = (supportNg != 0);
  s.supportEqActive = (supportEq != 0);
  s.supportAmpPan = std::clamp(s.supportAmpPan, -1.0, 1.0);
  if (hasSupportPolarityInvert)
    s.supportPolarityInvert = (supportPolarityInvert != 0);
  return pos;
}

template<typename Chunk>
void PutPostPerAmpSettings(Chunk& chunk, const VoLumAmpSettings& s)
{
  int valid = s.postValid ? 1 : 0;
  int delAct = s.postDelayActive ? 1 : 0;
  int delPP = s.postDelayPingPong ? 1 : 0;
  int revAct = s.postReverbActive ? 1 : 0;
  chunk.Put(&valid);
  chunk.Put(&delAct);
  chunk.Put(&s.postDelayMode);
  chunk.Put(&delPP);
  chunk.Put(&revAct);
  chunk.Put(&s.postReverbMode);
  chunk.Put(&s.postReverbSubMode);
  chunk.Put(&s.postDelayTime);
  chunk.Put(&s.postDelayFeedback);
  chunk.Put(&s.postDelayMix);
  chunk.Put(&s.postDelayTone);
  chunk.Put(&s.postDelayAge);
  chunk.Put(&s.postReverbMix);
  chunk.Put(&s.postReverbDecay);
  chunk.Put(&s.postReverbTone);
  chunk.Put(&s.postReverbPreDelay);
  chunk.Put(&s.postReverbShimmer);
  for (int i = 0; i < kVoLumDelayModeCount; ++i)
  {
    int pingPong = s.postDelayModes[i].pingPong ? 1 : 0;
    chunk.Put(&s.postDelayModes[i].time);
    chunk.Put(&s.postDelayModes[i].feedback);
    chunk.Put(&s.postDelayModes[i].mix);
    chunk.Put(&s.postDelayModes[i].tone);
    chunk.Put(&s.postDelayModes[i].age);
    chunk.Put(&pingPong);
  }
  for (int i = 0; i < kVoLumReverbModeCount; ++i)
  {
    chunk.Put(&s.postReverbModes[i].mix);
    chunk.Put(&s.postReverbModes[i].decay);
    chunk.Put(&s.postReverbModes[i].tone);
    chunk.Put(&s.postReverbModes[i].preDelay);
    chunk.Put(&s.postReverbModes[i].shimmer);
    chunk.Put(&s.postReverbModes[i].subMode);
  }
  for (int i = 0; i < 3; ++i)
  {
    chunk.Put(&s.postOktaverbSubModes[i].mix);
    chunk.Put(&s.postOktaverbSubModes[i].decay);
    chunk.Put(&s.postOktaverbSubModes[i].tone);
    chunk.Put(&s.postOktaverbSubModes[i].preDelay);
    chunk.Put(&s.postOktaverbSubModes[i].shimmer);
  }
}

template<typename Chunk>
int GetPostPerAmpSettings(const Chunk& chunk, int pos, VoLumAmpSettings& s, bool hasPostSnapshots = true)
{
  int valid = 0;
  int delAct = 0;
  int delPP = 0;
  int revAct = 0;
  pos = chunk.Get(&valid, pos);
  pos = chunk.Get(&delAct, pos);
  pos = chunk.Get(&s.postDelayMode, pos);
  pos = chunk.Get(&delPP, pos);
  pos = chunk.Get(&revAct, pos);
  pos = chunk.Get(&s.postReverbMode, pos);
  pos = chunk.Get(&s.postReverbSubMode, pos);
  pos = chunk.Get(&s.postDelayTime, pos);
  pos = chunk.Get(&s.postDelayFeedback, pos);
  pos = chunk.Get(&s.postDelayMix, pos);
  pos = chunk.Get(&s.postDelayTone, pos);
  pos = chunk.Get(&s.postDelayAge, pos);
  pos = chunk.Get(&s.postReverbMix, pos);
  pos = chunk.Get(&s.postReverbDecay, pos);
  pos = chunk.Get(&s.postReverbTone, pos);
  pos = chunk.Get(&s.postReverbPreDelay, pos);
  pos = chunk.Get(&s.postReverbShimmer, pos);

  s.postValid = (valid != 0);
  s.postDelayActive = (delAct != 0);
  s.postDelayPingPong = (delPP != 0);
  s.postReverbActive = (revAct != 0);
  s.postDelayMode = std::clamp(s.postDelayMode, 0, 2);
  s.postReverbMode = std::clamp(s.postReverbMode, 0, 2);
  s.postReverbSubMode = std::clamp(s.postReverbSubMode, 0, 2);
  s.postDelayTime = std::clamp(s.postDelayTime, 10.0, 2000.0);
  s.postDelayFeedback = std::clamp(s.postDelayFeedback, 0.0, 0.99);
  s.postDelayMix = std::clamp(s.postDelayMix, 0.0, 1.0);
  s.postDelayTone = std::clamp(s.postDelayTone, 0.0, 1.0);
  s.postDelayAge = std::clamp(s.postDelayAge, 0.0, 1.0);
  s.postReverbMix = std::clamp(s.postReverbMix, 0.0, 1.0);
  s.postReverbDecay = std::clamp(s.postReverbDecay, 0.1, 10.0);
  s.postReverbTone = std::clamp(s.postReverbTone, 0.0, 10.0);
  s.postReverbPreDelay = std::clamp(s.postReverbPreDelay, 0.0, 200.0);
  s.postReverbShimmer = std::clamp(s.postReverbShimmer, 0.0, 1.0);
  if (hasPostSnapshots)
  {
    for (int i = 0; i < kVoLumDelayModeCount; ++i)
    {
      int pingPong = 0;
      pos = chunk.Get(&s.postDelayModes[i].time, pos);
      pos = chunk.Get(&s.postDelayModes[i].feedback, pos);
      pos = chunk.Get(&s.postDelayModes[i].mix, pos);
      pos = chunk.Get(&s.postDelayModes[i].tone, pos);
      pos = chunk.Get(&s.postDelayModes[i].age, pos);
      pos = chunk.Get(&pingPong, pos);
      s.postDelayModes[i].time = std::clamp(s.postDelayModes[i].time, 10.0, 2000.0);
      s.postDelayModes[i].feedback = std::clamp(s.postDelayModes[i].feedback, 0.0, 0.99);
      s.postDelayModes[i].mix = std::clamp(s.postDelayModes[i].mix, 0.0, 1.0);
      s.postDelayModes[i].tone = std::clamp(s.postDelayModes[i].tone, 0.0, 1.0);
      s.postDelayModes[i].age = std::clamp(s.postDelayModes[i].age, 0.0, 1.0);
      s.postDelayModes[i].pingPong = (pingPong != 0);
    }
    for (int i = 0; i < kVoLumReverbModeCount; ++i)
    {
      pos = chunk.Get(&s.postReverbModes[i].mix, pos);
      pos = chunk.Get(&s.postReverbModes[i].decay, pos);
      pos = chunk.Get(&s.postReverbModes[i].tone, pos);
      pos = chunk.Get(&s.postReverbModes[i].preDelay, pos);
      pos = chunk.Get(&s.postReverbModes[i].shimmer, pos);
      pos = chunk.Get(&s.postReverbModes[i].subMode, pos);
      s.postReverbModes[i].mix = std::clamp(s.postReverbModes[i].mix, 0.0, 1.0);
      s.postReverbModes[i].decay = std::clamp(s.postReverbModes[i].decay, 0.1, 10.0);
      s.postReverbModes[i].tone = std::clamp(s.postReverbModes[i].tone, 0.0, 10.0);
      s.postReverbModes[i].preDelay = std::clamp(s.postReverbModes[i].preDelay, 0.0, 200.0);
      s.postReverbModes[i].shimmer = std::clamp(s.postReverbModes[i].shimmer, 0.0, 1.0);
      s.postReverbModes[i].subMode = std::clamp(s.postReverbModes[i].subMode, 0, 2);
    }
    for (int i = 0; i < 3; ++i)
    {
      pos = chunk.Get(&s.postOktaverbSubModes[i].mix, pos);
      pos = chunk.Get(&s.postOktaverbSubModes[i].decay, pos);
      pos = chunk.Get(&s.postOktaverbSubModes[i].tone, pos);
      pos = chunk.Get(&s.postOktaverbSubModes[i].preDelay, pos);
      pos = chunk.Get(&s.postOktaverbSubModes[i].shimmer, pos);
      s.postOktaverbSubModes[i].mix = std::clamp(s.postOktaverbSubModes[i].mix, 0.0, 1.0);
      s.postOktaverbSubModes[i].decay = std::clamp(s.postOktaverbSubModes[i].decay, 0.1, 10.0);
      s.postOktaverbSubModes[i].tone = std::clamp(s.postOktaverbSubModes[i].tone, 0.0, 10.0);
      s.postOktaverbSubModes[i].preDelay = std::clamp(s.postOktaverbSubModes[i].preDelay, 0.0, 200.0);
      s.postOktaverbSubModes[i].shimmer = std::clamp(s.postOktaverbSubModes[i].shimmer, 0.0, 1.0);
    }
  }
  return pos;
}

template<typename Chunk>
int GetVoLumChunkSelection(const Chunk& chunk, int pos, VoLumChunkSelection& selection)
{
  pos = chunk.Get(&selection.ampIdx, pos);
  pos = chunk.Get(&selection.speakerIdx, pos);
  pos = chunk.Get(&selection.channelIdx, pos);
  selection = ClampChunkSelection(selection);
  return pos;
}

template<typename Chunk>
int GetPrePostLockFlags(const Chunk& chunk, int pos, bool& preLocked, bool& postLocked)
{
  int pre = 0;
  int post = 0;
  pos = chunk.Get(&pre, pos);
  pos = chunk.Get(&post, pos);
  preLocked = (pre != 0);
  postLocked = (post != 0);
  return pos;
}

} // namespace volum
