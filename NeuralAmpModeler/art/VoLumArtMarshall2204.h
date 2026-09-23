#pragma once

// Marshall 2204 (amp 7, fractal case 6): "Windswept Glow".
// Hero art: Marshall 2204 - Windswept Glow (tree leaned into a gust over a dawn glow, glowing embers)
// PLAY motion: none yet; MakeMarshall2204Animator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawMarshall2204Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, rect.B, R * 0.8f, kGold, 0.12f);
  Bloom(g, cx, cy, R * 0.6f, kTeal, 0.06f);
  const float lean = 12.f;
  struct Br { float x, y, a, l; int d; };
  auto grow = [&](int maxD, auto cb) {
    std::vector<Br> stk; stk.push_back({cx, rect.T + h * 0.94f, -90.f, R * 0.34f, 0});
    while (!stk.empty())
    {
      Br b = stk.back(); stk.pop_back();
      if (b.d > maxD || b.l < 2.5f) continue;
      const float rad = b.a * 3.14159f / 180.f, ex = b.x + b.l * cosf(rad), ey = b.y + b.l * sinf(rad);
      cb(b, ex, ey);
      const float sp = 22.f + b.d * 4.f;
      stk.push_back({ex, ey, b.a - sp + lean, b.l * 0.67f, b.d + 1});
      stk.push_back({ex, ey, b.a + sp + lean, b.l * 0.67f, b.d + 1});
    }
  };
  grow(9, [&](const Br& b, float ex, float ey) {
    const IColor c = WithA(Mix(kDim, kTeal, std::max(0.f, 1.f - b.d / 6.f)), 0.9f);
    if (b.d < 3) GlowLine(g, kTeal, c, b.x, b.y, ex, ey, 2.6f, 4.f);
    else g.DrawLine(c, b.x, b.y, ex, ey, nullptr, 1.f);
  });
  grow(9, [&](const Br& b, float ex, float ey) {
    if (b.d >= 7) GlowDot(g, kGold, (b.d % 2) ? kGoldHi : kTeal, ex, ey, 1.7f, 4.f);
  });
  {
    unsigned s = 9u;
    for (int i = 0; i < 26; i++)
    {
      const float px = rect.L + w * (0.55f + 0.4f * Frand(s)), py = rect.T + h * (0.1f + 0.5f * Frand(s));
      GlowDot(g, kGold, Frand(s) > 0.5f ? kGoldHi : kTeal, px, py, Frand(s) * 1.6f + 0.5f, 4.f);
    }
  }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeMarshall2204Animator()
{
  return nullptr;
}
} // namespace volumart::art
