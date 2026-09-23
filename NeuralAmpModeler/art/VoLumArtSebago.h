#pragma once

// Sebago Texas Flood (amp 12, fractal case 11): "Clifford Nebula".
// Hero art: Sebago Texas Flood - Clifford Nebula (attractor over bloom+dust, teal->gold density, glowing core)
// PLAY motion: none yet; MakeSebagoAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawSebagoHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, cy, R * 0.5f, kTeal, 0.16f);
  unsigned ds = 5u; Dust(g, ds, cx, cy, R * 0.5f, 70);
  const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
  double X = 0.0, Y = 0.0; const float S = R * 0.22f; const int N = 9000;
  for (int i = 0; i < N; i++)
  {
    double nx = sin(a * Y) + c * cos(a * X), ny = sin(b * X) + d * cos(b * Y);
    X = nx; Y = ny; if (i <= 120) continue;
    const float px = cx + (float)X * S, py = cy - (float)Y * S;
    if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
    const float f = (float)i / (float)N;
    g.FillCircle(WithA(Mix(kTeal, kGold, f), 0.35f), px, py, 0.9f);
  }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeSebagoAnimator()
{
  return nullptr;
}
} // namespace volumart::art
