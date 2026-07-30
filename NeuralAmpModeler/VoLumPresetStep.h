#pragma once

// Preset-bar arrow arithmetic, kept free of IControl/iPlug2 so it can be unit
// tested without a graphics host (see tests/test_volum_preset_bar.cpp).
// Used by VoLumPresetBarControl::Step in VoLumPresetBar.h.

namespace volum
{
// Index the < / > arrows should recall, given the currently selected index
// (negative when nothing is selected), a direction, and the bank size.
// Returns -1 when there is nothing to recall.
//
// The "nothing selected" case is the whole reason this is a named function: the
// bar starts at -1 after SetList, after a factory reset, and after the active
// preset is deleted. Treating -1 as 0 and then stepping made `>` recall the
// second preset and skip the first entirely, while `<` correctly wrapped to the
// last - the two arrows disagreed about where the bank begins.
inline int StepPresetIndex(int currentIdx, int dir, int count)
{
  if (count <= 0)
    return -1;
  if (currentIdx < 0)
    return dir >= 0 ? 0 : count - 1;
  return ((currentIdx + dir) % count + count) % count;
}
} // namespace volum
