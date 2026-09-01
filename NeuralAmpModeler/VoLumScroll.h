#pragma once

// Shared vertical-list scrollbar: geometry (from the sidebar split) plus the
// drag/wheel interaction every growable list must use. Draw() may not be the
// only place the thumb rect exists — mouse-down has to hit the same numbers.

#include "VoLumAmpListScroll.h"

#include <algorithm>
#include <cmath>

namespace volum
{
namespace scroll
{

using amplist::ComputeScroll;
using amplist::kMinThumbH;
using amplist::kScrollbarW;
using amplist::kScrollGutter;
using amplist::Scrollable;
using amplist::ScrollMetrics;
using amplist::ThumbYToScroll;

inline float WheelDelta(float notches, float rowH)
{
  return -notches * rowH * 1.5f;
}

// Precision touchpads stream many fractional events (|d| < 1). Map those 1:1
// to pixels. A wheel notch / coarse detent sends |d| >= 1 and jumps 1.5 rows.
inline float ListWheelDelta(float notches, float rowH)
{
  if (std::abs(notches) < 1.f)
    return -notches * rowH;
  return WheelDelta(notches, rowH);
}

inline float ClampScroll(float scroll, float maxScroll)
{
  return std::clamp(scroll, 0.f, std::max(0.f, maxScroll));
}

struct Interaction
{
  bool dragging = false;
  float grabDY = 0.f;

  bool HitTrack(float x, float y, float trackL, float trackR, float trackT, float trackB) const
  {
    return x >= trackL && x <= trackR && y >= trackT && y <= trackB;
  }

  bool HitThumb(float x, float y, float trackL, float trackR, float thumbY, float thumbH) const
  {
    return x >= trackL && x <= trackR && y >= thumbY && y <= thumbY + thumbH;
  }

  // Returns true if this down started a thumb drag.
  bool OnDown(float x, float y, float trackL, float trackR, const ScrollMetrics& m)
  {
    dragging = false;
    grabDY = 0.f;
    if (m.maxScroll <= 0.5f)
      return false;
    if (!HitTrack(x, y, trackL, trackR, m.trackTop, m.trackTop + m.trackH))
      return false;
    grabDY = HitThumb(x, y, trackL, trackR, m.thumbY, m.thumbH) ? (y - m.thumbY) : (m.thumbH * 0.5f);
    dragging = true;
    return true;
  }

  float OnDrag(float y, const ScrollMetrics& m) const
  {
    if (!dragging)
      return -1.f;
    return ThumbYToScroll(y - grabDY, m.trackTop, m.trackH, m.thumbH, m.maxScroll);
  }

  void OnUp()
  {
    dragging = false;
    grabDY = 0.f;
  }
};

} // namespace scroll
} // namespace volum
