#pragma once

// Where a cab-cycle step lands on the speaker row.
//
// The row has four slots - No Cab plus three cabinets - and a slot can be
// unavailable: No Cab needs a DIRECT capture on the current channel, and a custom
// amp's cab slot needs a capture at all. The mouse simply cannot click those, so
// the keyboard must not land on them either.
//
// Kept free of IControl so the stepping rules are testable without a graphics host
// (see tests/test_volum_cab_step.cpp). VoLumSpeakerRowControl::StepKeyboard is the
// only caller.

namespace volum
{

inline constexpr int kNumCabRowSlots = 4;

// Slot a step in `direction` lands on, or -1 when the gesture must do nothing.
//
// `irCabActive` is what makes a step onto the currently selected slot meaningful:
// with a custom IR as the active cab, that slot is not really selected, and picking
// it retires the IR - the same thing clicking the highlighted button does.
inline int NextSelectableCab(int selected, int direction, bool irCabActive, const bool (&selectable)[kNumCabRowSlots])
{
  if (direction == 0)
    return -1;

  const int step = direction > 0 ? 1 : -1;
  for (int i = 1; i <= kNumCabRowSlots; ++i)
  {
    const int candidate = ((selected + step * i) % kNumCabRowSlots + kNumCabRowSlots) % kNumCabRowSlots;
    if (!selectable[candidate])
      continue;
    if (candidate == selected && !irCabActive)
      return -1;
    return candidate;
  }
  return -1;
}

} // namespace volum
