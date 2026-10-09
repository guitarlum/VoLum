#pragma once

// How large the standalone window may grow before it no longer fits the screen.
//
// The corner grip scales the whole UI (iPlug2 EUIResizerMode::Scale) up to a fixed
// draw scale, which is far taller than most monitors. Dragged that far the window
// reached past the work area, taking the grip with it: the only way back was the
// keyboard. The limit has to follow the monitor the window is on, so it is computed
// from the work area rather than baked into config.h.
//
// Free of iPlug2 and Win32 so the arithmetic is testable on its own (see
// tests/test_volum_window_fit.cpp); VoLumWindowFitWin.h feeds it real numbers.

#include <algorithm>
#include <cmath>

namespace volum
{

// Largest draw scale at which the window - the UI at baseW x baseH design points,
// times the draw scale and the display scale, plus the title bar and borders - still
// fits a work area of workW x workH pixels. Never below minScale (a tiny or odd work
// area must not make the window impossible to open) and never above hardMax.
//
// Rounded down to a thousandth: the window size is the product truncated to whole
// pixels, so rounding up could overshoot the work area by a pixel.
inline float MaxDrawScaleForWorkArea(float workW, float workH, float chromeW, float chromeH, float baseW, float baseH,
                                     float displayScale, float minScale, float hardMax)
{
  if (!(baseW > 0.f) || !(baseH > 0.f) || !(displayScale > 0.f) || !(workW > 0.f) || !(workH > 0.f))
    return hardMax;

  const float byWidth = (workW - chromeW) / (baseW * displayScale);
  const float byHeight = (workH - chromeH) / (baseH * displayScale);
  const float fit = std::floor(std::min(byWidth, byHeight) * 1000.f) / 1000.f;
  return std::clamp(fit, minScale, std::max(minScale, hardMax));
}

} // namespace volum
