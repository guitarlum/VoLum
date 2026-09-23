#pragma once

// Orange ORS100 (amp 11, fractal case 10): "Julia Nebula".
// Hero art: Orange ORS100 - Julia Nebula (compact Julia, teal with only faint warm tips, no centre dot)
// PLAY motion: none yet; MakeOrangeOrs100Animator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawOrangeOrs100Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
  Bloom(g, cx, cy, R * 0.5f, kTeal, 0.12f);
  unsigned ds = 2u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 70); // stars kept inside the art footprint
  const float step = 2.f;
  for (float px = 0; px < pw; px += step)
    for (float py = 0; py < ph; py += step)
    {
      double zr = (px / pw - 0.5) * 3.0, zi = (py / ph - 0.5) * 2.4; int it = 0;
      while (zr * zr + zi * zi < 4.0 && it < 32) { double t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++; }
      if (it < 32 && it > 2) { const float f = (float)it / 32.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.3f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
    }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeOrangeOrs100Animator()
{
  return nullptr;
}
} // namespace volumart::art
