#pragma once

// Orange OD120 (amp 10, fractal case 9): "Ember Bulb".
// Hero art: Orange OD120 - Ember Bulb (Mandelbrot bulb, teal-dominant with only faintly warm tips, no gold embers)
// PLAY motion: none yet; MakeOrangeOd120Animator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawOrangeOd120Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
  Bloom(g, cx, cy, R * 0.55f, kTeal, 0.12f);
  unsigned ds = 6u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 90); // stars kept inside the art footprint
  const float step = 2.f;
  for (float px = 0; px < pw; px += step)
    for (float py = 0; py < ph; py += step)
    {
      double cr = -0.745 + (px / pw - 0.5) * 0.01, ci = 0.186 + (py / ph - 0.5) * 0.01;
      double zr = 0, zi = 0; int it = 0;
      while (zr * zr + zi * zi < 4.0 && it < 40) { double t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++; }
      if (it < 40 && it > 2) { const float f = (float)it / 40.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.28f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
    }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeOrangeOd120Animator()
{
  return nullptr;
}
} // namespace volumart::art
