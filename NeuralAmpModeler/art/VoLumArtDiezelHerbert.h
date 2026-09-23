#pragma once

// Diezel Herbert Mk1 (amp 3, fractal case 14): "Lichtenberg Glow".
// Hero art: Diezel Herbert Mk1 - Lichtenberg Glow (dual-seed discharge over a teal bloom, glowing terminals)
// PLAY motion: none yet; MakeDiezelHerbertAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

#include <unordered_set>

namespace volumart::art
{
inline void DrawDiezelHerbertHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  const IColor bright = kHeroBright, mid = kHeroMid, dim = kHeroDim;
  // clang-format off
  // Two opposing seeds (top and bottom), DLA-style branching toward random
  // attractor points distributed across the panel. Result: dual branching
  // electric arcs evoking high-voltage transformer discharge.
  volumart::Bloom(g, cx, cy, std::min(w, h) * 0.55f, volumart::kTeal, 0.16f);
  struct Pt { float x, y; };
  std::vector<Pt> pts;
  pts.reserve(1500);
  pts.push_back({cx, rect.B - 16.f});
  pts.push_back({cx, rect.T + 16.f});

  // Spatial hash to skip near-duplicate placements (keeps the drawing crisp)
  const float cellSz = 6.f;
  std::unordered_set<long long> grid;
  auto cellKey = [&](float x, float y) -> long long {
    long long ix = (long long)std::floor(x / cellSz);
    long long iy = (long long)std::floor(y / cellSz);
    return (ix << 32) ^ (long long)(unsigned long long)iy;
  };
  grid.insert(cellKey(pts[0].x, pts[0].y));
  grid.insert(cellKey(pts[1].x, pts[1].y));

  // Randomly distributed attractors - bias toward column near center
  constexpr int kAttractors = 360;
  std::vector<Pt> targets;
  targets.reserve(kAttractors);
  unsigned rng = 0xBEEFu;
  auto frand = [&]() -> float {
    rng = rng * 1664525u + 1013904223u;
    return (float)rng / (float)0xFFFFFFFFu;
  };
  for (int i = 0; i < kAttractors; ++i)
  {
    targets.push_back({cx + (frand() - 0.5f) * w * 0.72f,
                       rect.T + 18.f + frand() * (h - 36.f)});
  }

  constexpr int kIters = 1400;
  for (int i = 0; i < kIters; ++i)
  {
    const Pt& target = targets[(int)(frand() * kAttractors) % kAttractors];

    // Sample-based nearest search (fast enough; pts grows to ~1.4k)
    int trials = std::min(48, (int)pts.size());
    int nearestIdx = 0;
    float bestD = 1e30f;
    for (int k = 0; k < trials; ++k)
    {
      int idx2 = (int)(frand() * pts.size()) % (int)pts.size();
      float dx = pts[idx2].x - target.x;
      float dy = pts[idx2].y - target.y;
      float d = dx * dx + dy * dy;
      if (d < bestD) { bestD = d; nearestIdx = idx2; }
    }

    const Pt& parent = pts[nearestIdx];
    float dx = target.x - parent.x;
    float dy = target.y - parent.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.001f) continue;
    float ang = std::atan2(dy, dx) + (frand() - 0.5f) * 0.6f;
    float len = 9.f + frand() * 5.f;
    Pt next{parent.x + std::cos(ang) * len, parent.y + std::sin(ang) * len};
    if (next.x < rect.L + 14.f || next.x > rect.R - 14.f
        || next.y < rect.T + 12.f || next.y > rect.B - 12.f) continue;

    long long key = cellKey(next.x, next.y);
    if (!grid.insert(key).second) continue;

    IColor col;
    float thickness;
    if (i < 90)        { col = bright; thickness = 2.4f; }
    else if (i < 380)  { col = mid;    thickness = 1.5f; }
    else               { col = dim;    thickness = 1.0f; }
    g.DrawLine(col, parent.x, parent.y, next.x, next.y, nullptr, thickness);
    pts.push_back(next);
  }

  // Glowing terminals (a whisper of gold only at hero size).
  volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.B - 16.f, 2.6f, 5.f);
  volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.T + 16.f, 2.6f, 5.f);
  g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.B - 16.f, 1.2f);
  g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.T + 16.f, 1.2f);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeDiezelHerbertAnimator()
{
  return nullptr;
}
} // namespace volumart::art
