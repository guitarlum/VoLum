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
int GetVoLumChunkSelection(const Chunk& chunk, int pos, VoLumChunkSelection& selection)
{
  pos = chunk.Get(&selection.ampIdx, pos);
  pos = chunk.Get(&selection.speakerIdx, pos);
  pos = chunk.Get(&selection.channelIdx, pos);
  selection = ClampChunkSelection(selection);
  return pos;
}

} // namespace volum
