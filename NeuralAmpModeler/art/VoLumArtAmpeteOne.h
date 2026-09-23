#pragma once

// Ampete One (amp 0, fractal case 0): "Dragon Nebula".
// Hero art: Ampete One - Dragon Nebula (dragon curve, left-anchored across ~2/3 width)
// PLAY motion: none yet; MakeAmpeteOneAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawAmpeteOneHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  std::vector<int> turns;
  for (int i = 0; i < 12; i++) // 12 iters -> landscape orientation (matches the classic 1.1.0 layout)
  {
    std::vector<int> next;
    for (auto t : turns) next.push_back(t);
    next.push_back(1);
    for (int j = (int)turns.size() - 1; j >= 0; j--) next.push_back(1 - turns[j]);
    turns = next;
  }
  std::vector<std::pair<float, float>> raw;
  raw.push_back({0.f, 0.f});
  {
    float px = 0.f, py = 0.f; int dir = 0;
    const float dxs[] = {1, 0, -1, 0}, dys[] = {0, -1, 0, 1};
    for (int i = 0; i < (int)turns.size(); i++) { px += dxs[dir]; py += dys[dir]; raw.push_back({px, py}); dir = (dir + (turns[i] ? 1 : 3)) % 4; }
  }
  float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
  for (auto& p : raw) { mnx = std::min(mnx, p.first); mny = std::min(mny, p.second); mxx = std::max(mxx, p.first); mxy = std::max(mxy, p.second); }
  const float extX = mxx - mnx, extY = mxy - mny;
  // Left-anchored: span ~60% of the width (capped by height), off-centre like the classic layout.
  const float S = std::min((w * 0.60f) / (extX > 0.f ? extX : 1.f), (h * 0.90f) / (extY > 0.f ? extY : 1.f));
  const float artW = extX * S, artH = extY * S;
  const float ox = rect.L + w * 0.05f, oy = cy - artH * 0.5f;
  const float acx = ox + artW * 0.5f, acy = oy + artH * 0.5f; // art centre for atmosphere
  Bloom(g, acx, acy, R * 0.6f, kTeal, 0.17f);
  unsigned ds = 7u; Dust(g, ds, acx, acy, R * 0.5f, 80);
  std::vector<std::pair<float, float>> P; P.reserve(raw.size());
  for (auto& p : raw) P.push_back({ox + (p.first - mnx) * S, oy + (p.second - mny) * S});
  GradStroke(g, P, kMid, kGoldHi, 2.f, true);
  GlowDot(g, kGold, kGoldHi, P.back().first, P.back().second, 4.5f, 10.f);
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeAmpeteOneAnimator()
{
  return nullptr;
}
} // namespace volumart::art
