#pragma once

// Brunetti XL 2 (amp 2, fractal case 2): "Verdant Fern".
// Hero art: Brunetti XL 2 - Verdant Fern (layered fern, teal->gold tips, ground bloom)
// PLAY motion: none yet; MakeBrunettiAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawBrunettiHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  Bloom(g, cx, rect.T + h * 0.86f, std::min(w, h) * 0.8f, kTeal, 0.13f);
  auto fern = [&](int n, float fh, float fsc, float bx, float by, IColor base, IColor tip, float rr, float a, bool gold) {
    float px = 0.f, py = 0.f; unsigned s = 42u;
    for (int i = 0; i < n; i++)
    {
      s = s * 1103515245u + 12345u; float rv = (float)(s % 1000) / 1000.f; float nx, ny;
      if (rv < 0.01f) { nx = 0.f; ny = 0.16f * py; }
      else if (rv < 0.86f) { nx = 0.85f * px + 0.04f * py; ny = -0.04f * px + 0.85f * py + 1.6f; }
      else if (rv < 0.93f) { nx = 0.2f * px - 0.26f * py; ny = 0.23f * px + 0.22f * py + 1.6f; }
      else { nx = -0.15f * px + 0.28f * py; ny = 0.26f * px + 0.24f * py + 0.44f; }
      px = nx; py = ny; if (i == 0) continue;
      const float sx = bx + px * fsc, sy = by - py * fh;
      if (sx < rect.L - 4.f || sx > rect.R + 4.f || sy < rect.T - 4.f || sy > rect.B + 4.f) continue;
      IColor c = Mix(base, tip, std::min(1.f, py / 9.f));
      if (gold && py > 7.6f) c = Mix(c, kGoldHi, (py - 7.6f) / 2.4f);
      g.FillCircle(WithA(c, a), sx, sy, rr);
    }
  };
  // Horizontal scale tied to height at the 1.1.0 fern ratio (~2.53:1 horiz:vert per unit,
  // i.e. 38px:15px) so the fronds keep their classic proportions and do not stretch with
  // the hero panel width or cramp when height-tied 1:1.
  fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, rect.L + w * 0.55f, rect.T + h * 0.9f, IColor(255, 45, 90, 110), kMid, 0.9f, 0.32f, false);
  fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false);
  fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeBrunettiAnimator()
{
  return nullptr;
}
} // namespace volumart::art
