#pragma once

// Pure dirty gate for VoLumTunerControl::SetResult. The tuner panel draws
// "Play a note..." for every invalid result, whatever its other fields hold,
// and otherwise note, octave, frequency and cents, so an unchanged result
// needs no repaint.

#include "VoLumTunerDSP.h"

namespace volum
{

inline bool TunerResultDrawsSame(const TunerResult& a, const TunerResult& b)
{
  if (!a.valid || !b.valid)
    return a.valid == b.valid;
  return a.noteIndex == b.noteIndex && a.octave == b.octave && a.frequency == b.frequency && a.cents == b.cents;
}

} // namespace volum
