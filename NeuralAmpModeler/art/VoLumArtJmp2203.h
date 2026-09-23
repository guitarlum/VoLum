#pragma once

// Marshall JMP 2203 (amp 8, fractal case 7): "Depth Triforce".
// Hero art: Marshall JMP 2203 - Depth Triforce (stacked offset triforce outlines receding teal->gold)
// PLAY motion: none yet; MakeJmp2203Animator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawJmp2203Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float cy2 = cy + h * 0.02f, baseS = std::min(w * 0.34f, h * 0.42f), R = std::min(w, h);
  Bloom(g, cx, cy2, R * 0.5f, kGold, 0.10f);
  auto M = [](float x1, float y1, float x2, float y2) { return std::make_pair((x1 + x2) / 2.f, (y1 + y2) / 2.f); };
  auto triOutline = [&](float ccx, float ccy, float S, const IColor& col, float thick, bool glow) {
    const float Ax = ccx, Ay = ccy - S, Bx = ccx - S * 0.866f, By = ccy + S * 0.5f, Cx = ccx + S * 0.866f, Cy = ccy + S * 0.5f;
    const auto mAB = M(Ax, Ay, Bx, By), mAC = M(Ax, Ay, Cx, Cy), mBC = M(Bx, By, Cx, Cy);
    struct Tri { float x1, y1, x2, y2, x3, y3; };
    const Tri T[3] = {{Ax, Ay, mAB.first, mAB.second, mAC.first, mAC.second},
                      {mAB.first, mAB.second, Bx, By, mBC.first, mBC.second},
                      {mAC.first, mAC.second, mBC.first, mBC.second, Cx, Cy}};
    for (auto& t : T)
    {
      if (glow)
      {
        GlowLine(g, kGold, col, t.x1, t.y1, t.x2, t.y2, thick, 5.f);
        GlowLine(g, kGold, col, t.x2, t.y2, t.x3, t.y3, thick, 5.f);
        GlowLine(g, kGold, col, t.x3, t.y3, t.x1, t.y1, thick, 5.f);
      }
      else
      {
        g.DrawLine(col, t.x1, t.y1, t.x2, t.y2, nullptr, thick);
        g.DrawLine(col, t.x2, t.y2, t.x3, t.y3, nullptr, thick);
        g.DrawLine(col, t.x3, t.y3, t.x1, t.y1, nullptr, thick);
      }
    }
  };
  const int K = 6;
  for (int i = K - 1; i >= 0; --i) // back (teal, small, high) -> front (gold, full, glowing)
  {
    const float dt = (float)i / (float)(K - 1);
    const float S = baseS * (1.f - dt * 0.14f), off = dt * R * 0.07f;
    const IColor col = WithA(Mix(kGold, kTeal, dt), 0.85f - dt * 0.5f);
    triOutline(cx, cy2 - off, S, col, i == 0 ? 2.2f : 1.2f, i == 0);
  }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeJmp2203Animator()
{
  return nullptr;
}
} // namespace volumart::art
