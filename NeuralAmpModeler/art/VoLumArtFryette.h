#pragma once

// Fryette Deliverance 120 (amp 4, fractal case 3): "Spiral Galaxy".
// Hero art: Spiral galaxy (Fryette Deliverance)
// PLAY motion: none yet; MakeFryetteAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawFryetteHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  // Two logarithmic-spiral particle arms over a soft radial bloom, with a
  // glowing gold core. Deep and atmospheric.
  const float maxR = std::min(w, h) * 0.46f;
  for (int b = 6; b >= 1; b--)
    g.FillCircle(IColor(6, 120, 210, 220), cx, cy, maxR * (float)b / 6.f);
  unsigned rng = 5u;
  auto frand = [&]() -> float { rng = rng * 1664525u + 1013904223u; return (float)rng / (float)0xFFFFFFFFu; };
  for (int arm = 0; arm < 2; arm++)
  {
    const float off = arm * 3.14159f;
    for (int i = 0; i < 560; i++)
    {
      const float t = (float)i / 560.f;
      const float theta = off + t * 4.2f * 3.14159f;
      const float rr = maxR * powf(t, 0.7f);
      const float px = cx + rr * cosf(theta) + (frand() - 0.5f) * rr * 0.12f;
      const float py = cy + rr * sinf(theta) + (frand() - 0.5f) * rr * 0.12f;
      if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
      const int al = (int)(255.f * (0.2f + 0.6f * (1.f - t)));
      g.FillCircle((t < 0.5f) ? IColor(al, 120, 210, 220) : IColor(al, 100, 180, 200), px, py, (t < 0.2f) ? 1.4f : 1.f);
    }
  }
  g.FillCircle(IColor(50, 200, 165, 87), cx, cy, 10.f);
  g.FillCircle(IColor(90, 200, 165, 87), cx, cy, 6.f);
  g.FillCircle(IColor(235, 252, 222, 145), cx, cy, 4.f);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeFryetteAnimator()
{
  return nullptr;
}
} // namespace volumart::art
