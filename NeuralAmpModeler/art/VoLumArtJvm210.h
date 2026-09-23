#pragma once

// Marshall JVM 210H (amp 9, fractal case 8): "Levy Nebula".
// Hero art: Marshall JVM 210H - Levy Nebula (Levy C over bloom+dust, gradient stroke into gold, glow)
// PLAY motion: none yet; MakeJvm210Animator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawJvm210Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, cy, R * 0.55f, kTeal, 0.16f);
  unsigned ds = 8u; Dust(g, ds, cx, cy, R * 0.5f, 70);
  struct Seg { float x1, y1, x2, y2; };
  std::vector<Seg> segs; segs.push_back({-1.f, 0.f, 1.f, 0.f});
  for (int depth = 0; depth < 11; depth++)
  {
    std::vector<Seg> next;
    for (auto& s : segs)
    {
      const float mx = (s.x1 + s.x2) / 2.f + (s.y2 - s.y1) / 2.f, my = (s.y1 + s.y2) / 2.f - (s.x2 - s.x1) / 2.f;
      next.push_back({s.x1, s.y1, mx, my}); next.push_back({mx, my, s.x2, s.y2});
    }
    segs = next;
  }
  std::vector<std::pair<float, float>> raw; raw.push_back({segs[0].x1, segs[0].y1});
  for (auto& s : segs) raw.push_back({s.x2, s.y2});
  float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
  for (auto& p : raw) { mnx = std::min(mnx, p.first); mny = std::min(mny, p.second); mxx = std::max(mxx, p.first); mxy = std::max(mxy, p.second); }
  const float bcx = (mnx + mxx) / 2.f, bcy = (mny + mxy) / 2.f, ext = std::max(mxx - mnx, mxy - mny);
  const float S = R * 1.25f / (ext > 0.f ? ext : 1.f);
  std::vector<std::pair<float, float>> P; for (auto& p : raw) P.push_back({cx + (p.first - bcx) * S, cy + (p.second - bcy) * S});
  GradStroke(g, P, kTeal, kGoldHi, 1.6f, true);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeJvm210Animator()
{
  return nullptr;
}
} // namespace volumart::art
