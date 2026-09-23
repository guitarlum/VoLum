#pragma once

// Lichtlaerm Prometheus (amp 6, fractal case 5): "Koch Deep".
// Hero art: Lichtlaerm Prometheus - Koch Deep (snowflake over bloom+dust, gradient stroke into gold, glowing core)
// PLAY motion: none yet; MakeLichtlaermAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawLichtlaermHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w * 0.46f, h * 0.5f);
  Bloom(g, cx, cy, R * 1.2f, kTeal, 0.15f);
  unsigned ds = 7u; Dust(g, ds, cx, cy, R * 1.1f, 60);
  struct Seg { float x1, y1, x2, y2; };
  std::vector<Seg> segs;
  for (int i = 0; i < 3; i++)
  {
    const float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
    segs.push_back({cx + R * cosf(a1), cy + R * sinf(a1), cx + R * cosf(a2), cy + R * sinf(a2)});
  }
  for (int depth = 0; depth < 5; depth++)
  {
    std::vector<Seg> next;
    for (auto& s : segs)
    {
      const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
      const float ax = s.x1 + dx / 3.f, ay = s.y1 + dy / 3.f, bx = s.x1 + dx * 2.f / 3.f, by = s.y1 + dy * 2.f / 3.f;
      const float px2 = (s.x1 + s.x2) / 2.f - dy * 0.2887f, py2 = (s.y1 + s.y2) / 2.f + dx * 0.2887f;
      next.push_back({s.x1, s.y1, ax, ay}); next.push_back({ax, ay, px2, py2});
      next.push_back({px2, py2, bx, by}); next.push_back({bx, by, s.x2, s.y2});
    }
    segs = next;
  }
  std::vector<std::pair<float, float>> P; P.push_back({segs[0].x1, segs[0].y1});
  for (auto& s : segs) P.push_back({s.x2, s.y2});
  GradStroke(g, P, kTeal, kGoldHi, 1.9f, true);
  GlowDot(g, kGold, kGoldHi, cx, cy, 3.5f, 10.f);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeLichtlaermAnimator()
{
  return nullptr;
}
} // namespace volumart::art
