#pragma once

// Pure scrollbar/gutter geometry for the sidebar amp list (VoLumAmpListControl).
//
// Extracted from VoLumAmpList.h so the math can be unit-tested without an
// IGraphics/IControl instance (mirrors the VoLumTriptychLayout.h split). The
// control delegates to these free functions and only adds the horizontal IRECT
// assembly + the live mScrollOffset/mDragGrabDY state.

#include <algorithm>

namespace volum
{
namespace amplist
{

inline constexpr float kScrollbarW = 6.f;   // visible bar width when scrollable
inline constexpr float kScrollGutter = 7.f; // empty gap between labels and bar
inline constexpr float kMinThumbH = 18.f;   // thumb never shrinks below this

// A list is scrollable once its content overflows its visible height. The 0.5px
// slack avoids a flickering bar when content == height within rounding error.
inline bool Scrollable(float contentH, float rectH)
{
  return contentH > rectH + 0.5f;
}

// Right edge of the row content (labels/icons stop here). Leaves a gutter before
// the scrollbar so labels never crowd the bar; full width when not scrollable.
inline float RowRightX(float rectR, float contentH, float rectH)
{
  return Scrollable(contentH, rectH) ? (rectR - kScrollbarW - kScrollGutter) : rectR;
}

// Vertical scrollbar metrics. Track runs inset 2px from the rect top/bottom.
struct ScrollMetrics
{
  float trackTop = 0.f;
  float trackH = 0.f;
  float thumbH = 0.f;
  float thumbY = 0.f;
  float maxScroll = 0.f;
};

inline ScrollMetrics ComputeScroll(float rectT, float rectB, float rectH, float contentH, float scrollOffset)
{
  ScrollMetrics m;
  m.trackTop = rectT + 2.f;
  m.trackH = (rectB - 2.f) - m.trackTop;
  m.thumbH = std::max(kMinThumbH, m.trackH * (rectH / std::max(contentH, 1.f)));
  m.maxScroll = std::max(0.f, contentH - rectH);
  const float t = (m.maxScroll > 0.f) ? (scrollOffset / m.maxScroll) : 0.f;
  m.thumbY = m.trackTop + (m.trackH - m.thumbH) * t;
  return m;
}

// Map a cursor y (already adjusted for the grab offset within the thumb) to a
// clamped scroll offset.
inline float ThumbYToScroll(float adjustedY, float trackTop, float trackH, float thumbH, float maxScroll)
{
  const float usable = trackH - thumbH;
  float t = (usable > 0.f) ? ((adjustedY - trackTop) / usable) : 0.f;
  t = std::clamp(t, 0.f, 1.f);
  return t * maxScroll;
}

} // namespace amplist
} // namespace volum
