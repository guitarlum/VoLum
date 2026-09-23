#pragma once

// Bad Cat Mini Cat (amp 1, fractal case 1): "Eyes in the Dark".
// Hero art: Bad Cat Mini Cat - Eyes in the Dark (two glowing cat eyes over a starfield)
// PLAY motion: none yet; MakeBadCatAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawBadCatHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float sep = R * 0.24f, ew = R * 0.17f, eh = ew * 0.5f;
  Bloom(g, cx, cy, R * 0.75f, kTeal, 0.10f);
  {
    unsigned s = 4u;
    for (int i = 0; i < 44; i++)
    {
      const float px = rect.L + Frand(s) * w, py = rect.T + Frand(s) * h;
      const int al = (int)((0.06f + 0.14f * Frand(s)) * 255.f);
      g.FillCircle(IColor(al, 120, 210, 220), px, py, Frand(s) * 1.f + 0.3f);
    }
  }
  auto drawEye = [&](float exc) {
    std::vector<float> xs, ys;
    const int NS = 16;
    auto q = [&](float ax, float ay, float bx, float by, float dx3, float dy3) {
      for (int i = 0; i <= NS; i++)
      {
        const float t = (float)i / NS, mt = 1.f - t;
        xs.push_back(exc + mt * mt * ax + 2.f * mt * t * bx + t * t * dx3);
        ys.push_back(cy + mt * mt * ay + 2.f * mt * t * by + t * t * dy3);
      }
    };
    q(-ew, 0.f, 0.f, -eh, ew, 0.f);
    q(ew, 0.f, 0.f, eh, -ew, 0.f);
    g.FillConvexPolygon(WithA(kTeal, 0.30f), xs.data(), ys.data(), (int)xs.size());
    for (size_t i = 1; i < xs.size(); i++)
      g.DrawLine(WithA(kTeal, 0.85f), xs[i - 1], ys[i - 1], xs[i], ys[i], nullptr, 2.f);
    g.DrawLine(WithA(kTeal, 0.85f), xs.back(), ys.back(), xs.front(), ys.front(), nullptr, 2.f);
    const float pw = ew * 0.13f, ph = eh * 0.92f;
    g.FillEllipse(WithA(kGold, 0.35f), IRECT(exc - pw * 2.4f, cy - ph * 1.15f, exc + pw * 2.4f, cy + ph * 1.15f));
    g.FillEllipse(WithA(kGold, 0.95f), IRECT(exc - pw, cy - ph, exc + pw, cy + ph));
  };
  drawEye(cx - sep);
  drawEye(cx + sep);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeBadCatAnimator()
{
  return nullptr;
}
} // namespace volumart::art
