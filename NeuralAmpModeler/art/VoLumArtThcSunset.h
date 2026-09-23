#pragma once

// THC Sunset (amp 14, fractal case 13): "Dark Sun".
// Hero art: THC Sunset - "Dark Sun" (Dark Souls eclipse: dark core, ring-of-fire corona, light shaft)
// PLAY motion: none yet; MakeThcSunsetAnimator() returns nullptr, so PLAY draws the
// static art exactly as BUILD does. See .scratch/1.3.0-feedback/art-worker-brief.md.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawThcSunsetHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  const float tkThin = kHeroTkThin;
  // clang-format off
  using namespace volumart;
  const IColor sGold(255, 252, 222, 145), sOrange(255, 233, 138, 90), sMag(255, 201, 74, 140), sTeal(255, 120, 210, 220);
  const float horizon = rect.T + h * 0.60f;
  const float R0 = std::min(w, h) * 0.17f, cyS = horizon - R0 * 2.35f;
  const float TAU = 6.28318f;
  // synthwave sky
  g.PathRect(IRECT(rect.L, rect.T, rect.R, horizon));
  g.PathFill(IPattern::CreateLinearGradient(rect.L, rect.T, rect.L, horizon,
                                            {{WithA(sTeal, 0.04f), 0.f}, {WithA(sMag, 0.09f), 0.72f}, {WithA(sOrange, 0.15f), 1.f}}));
  Bloom(g, cx, cyS, R0 * 3.2f, sOrange, 0.11f);
  // ring-of-fire corona: irregular jagged flame spikes around the rim
  unsigned seed = 31u;
  for (int i = 0; i < 96; i++)
  {
    const float a = (float)i / 96.f * TAU;
    const float len = R0 * (1.02f + Frand(seed) * 0.16f + 0.04f * sinf(a * 7.f));
    const float ix = cx + cosf(a) * R0 * 0.99f, iy = cyS + sinf(a) * R0 * 0.99f;
    const float ox = cx + cosf(a) * len, oy = cyS + sinf(a) * len;
    const IColor& col = (Frand(seed) > 0.5f) ? sGold : sOrange;
    g.DrawLine(WithA(col, 0.18f), ix, iy, ox, oy, nullptr, 3.0f); // glow
    g.DrawLine(WithA(col, 0.5f + 0.45f * Frand(seed)), ix, iy, ox, oy, nullptr, 1.4f);
  }
  // dark eclipse core: near-black disc with a faint warm inner glow
  g.PathCircle(cx, cyS, R0);
  g.PathFill(IPattern::CreateRadialGradient(cx, cyS, R0,
                                            {{WithA(sOrange, 0.16f), 0.f}, {IColor(235, 12, 12, 17), 0.45f}, {IColor(250, 7, 8, 12), 1.f}}));
  // bright rim + faint inner ring (annular "darksign" look)
  g.DrawCircle(WithA(sGold, 0.22f), cx, cyS, R0, nullptr, 6.f);
  g.DrawCircle(WithA(sGold, 0.95f), cx, cyS, R0, nullptr, 2.f);
  g.DrawCircle(WithA(sGold, 0.40f), cx, cyS, R0 * 0.86f, nullptr, 1.f);
  // light shaft down to the horizon (gradient beam + dust)
  const float topY = cyS + R0 * 0.92f, wt = R0 * 0.20f, wb = R0 * 0.05f;
  g.PathClear();
  g.PathMoveTo(cx - wt, topY);
  g.PathLineTo(cx + wt, topY);
  g.PathLineTo(cx + wb, horizon);
  g.PathLineTo(cx - wb, horizon);
  g.PathClose();
  g.PathFill(IPattern::CreateLinearGradient(cx, topY, cx, horizon, {{WithA(sGold, 0.34f), 0.f}, {WithA(sGold, 0.f), 1.f}}));
  g.DrawLine(WithA(sGold, 0.5f), cx - wt * 0.7f, topY, cx - wb, horizon, nullptr, 1.f);
  g.DrawLine(WithA(sGold, 0.5f), cx + wt * 0.7f, topY, cx + wb, horizon, nullptr, 1.f);
  Dust(g, 7u, cx, (topY + horizon) * 0.5f, wt * 1.5f, 10);
  // praise-burst at the base of the shaft on the horizon
  for (int k = -4; k <= 4; k++)
  {
    const float a = -1.5708f + k * 0.16f, l = R0 * (0.5f - std::abs(k) * 0.05f);
    g.DrawLine(WithA(sGold, 0.5f), cx, horizon, cx + cosf(a) * l, horizon + sinf(a) * l, nullptr, 1.f);
  }
  // glowing horizon + perspective grid floor
  g.DrawLine(WithA(sOrange, 0.5f), rect.L, horizon, rect.R, horizon, nullptr, 4.f);
  g.DrawLine(WithA(sGold, 0.9f), rect.L, horizon, rect.R, horizon, nullptr, tkThin);
  for (int i = 1; i <= 8; i++)
  {
    const float t = (float)i / 8.f, yy = horizon + (rect.B - horizon) * t * t;
    g.DrawLine(IColor((int)(255.f * (0.26f * (1.f - t) + 0.08f)), 120, 210, 220), rect.L, yy, rect.R, yy, nullptr, 1.f);
  }
  for (int i = -12; i <= 12; i++)
  {
    const float fx = cx + ((float)i / 12.f) * w * 0.55f;
    g.DrawLine(IColor(38, 120, 210, 220), cx + ((float)i / 12.f) * w * 0.05f, horizon, fx, rect.B, nullptr, 1.f);
  }
  // clang-format on
}

inline std::unique_ptr<ArtAnimator> MakeThcSunsetAnimator()
{
  return nullptr;
}
} // namespace volumart::art
