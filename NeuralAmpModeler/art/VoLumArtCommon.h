#pragma once

// Shared drawing vocabulary of the factory hero arts (art/VoLumArt<Amp>.h), the
// custom-amp art and the triptych motifs. Every art file includes this and
// VoLumArtAnimator.h only; never VoLumFractalArt.h or VoLumColorHelpers.h,
// which include each other.

#include "IControls.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace iplug;
using namespace igraphics;

// ---------------------------------------------------------------------------
// Shared "atmosphere" helpers for the 1.2.0 art glow-up. These mirror the JS
// helpers in tools/art_1_2_0_preview.html (bloom / dust / glow / gradient
// stroke) so hero art gains depth. Canvas radial/linear gradients + shadowBlur
// are emulated with concentric translucent fills and layered-alpha strokes.
// Gold accents are hero/card-size only; callers must not use gold in minis.
// ---------------------------------------------------------------------------
namespace volumart
{
static const IColor kTeal(255, 120, 210, 220), kMid(255, 100, 180, 200), kDim(255, 80, 150, 170),
  kBlue(255, 150, 205, 245), kGold(255, 200, 165, 87), kGoldHi(255, 252, 222, 145);

inline float Frand(unsigned& s)
{
  s = s * 1664525u + 1013904223u;
  return (float)s / (float)0xFFFFFFFFu;
}

inline IColor WithA(const IColor& c, float a)
{
  int ai = (int)(a * 255.f);
  return IColor(ai < 0 ? 0 : (ai > 255 ? 255 : ai), c.R, c.G, c.B);
}

inline IColor Mix(const IColor& a, const IColor& b, float t)
{
  if (t < 0.f)
    t = 0.f;
  else if (t > 1.f)
    t = 1.f;
  return IColor((int)(a.A + (b.A - a.A) * t), (int)(a.R + (b.R - a.R) * t), (int)(a.G + (b.G - a.G) * t),
                (int)(a.B + (b.B - a.B) * t));
}

// Soft radial bloom. Matches the preview harness's single smooth radial gradient
// (peak at centre -> peak*0.3 at 0.6R -> transparent at R) instead of stacking
// opaque circles, so the light source stays dim and sits in the background with
// no hard centre hotspot.
inline void Bloom(IGraphics& g, float cx, float cy, float R, const IColor& c, float peak)
{
  if (R <= 0.f || peak <= 0.f)
    return;
  const int a0 = std::min(255, (int)(peak * 255.f));
  const int a1 = std::min(255, (int)(peak * 0.3f * 255.f));
  g.PathCircle(cx, cy, R);
  g.PathFill(IPattern::CreateRadialGradient(
    cx, cy, R, {{IColor(a0, c.R, c.G, c.B), 0.f}, {IColor(a1, c.R, c.G, c.B), 0.6f}, {IColor(0, c.R, c.G, c.B), 1.f}}));
}

// Drifting dust dots inside a radius. Hero-only (callers gate on big).
inline void Dust(IGraphics& g, unsigned seed, float cx, float cy, float R, int n)
{
  unsigned s = seed;
  for (int i = 0; i < n; i++)
  {
    const float a = Frand(s) * 6.28318f, rr = R * std::sqrt(Frand(s));
    const float px = cx + rr * cosf(a), py = cy + rr * sinf(a);
    const int al = (int)((0.08f + 0.28f * Frand(s)) * 255.f);
    const IColor& c = (Frand(s) > 0.75f) ? kTeal : kMid;
    g.FillCircle(IColor(al, c.R, c.G, c.B), px, py, Frand(s) * 1.3f + 0.3f);
  }
}

// Glowing line: wide faint pass under a bright core.
inline void GlowLine(IGraphics& g, const IColor& glowCol, const IColor& core, float x1, float y1, float x2, float y2,
                     float w, float glowW)
{
  g.DrawLine(WithA(glowCol, (glowCol.A / 255.f) * 0.32f), x1, y1, x2, y2, nullptr, w + glowW);
  g.DrawLine(core, x1, y1, x2, y2, nullptr, w);
}

// Glowing dot: two faint halo passes under a bright core.
inline void GlowDot(IGraphics& g, const IColor& glowCol, const IColor& core, float cx, float cy, float r, float glowR)
{
  g.FillCircle(WithA(glowCol, 0.26f), cx, cy, r + glowR);
  g.FillCircle(WithA(glowCol, 0.45f), cx, cy, r + glowR * 0.5f);
  g.FillCircle(core, cx, cy, r);
}

// Gradient polyline stroke c0->c1 (alpha ~0.85), optional faint teal glow underlay.
inline void GradStroke(IGraphics& g, const std::vector<std::pair<float, float>>& P, const IColor& c0, const IColor& c1,
                       float lw, bool glowOn)
{
  if (glowOn)
    for (size_t i = 1; i < P.size(); i++)
      g.DrawLine(WithA(kTeal, 0.22f), P[i - 1].first, P[i - 1].second, P[i].first, P[i].second, nullptr, lw + 4.f);
  for (size_t i = 1; i < P.size(); i++)
  {
    const float t = (float)i / (float)P.size();
    g.DrawLine(WithA(Mix(c0, c1, t), 0.85f), P[i - 1].first, P[i - 1].second, P[i].first, P[i].second, nullptr, lw);
  }
}

// The hero inks every factory art shares (formerly locals of DrawHeroFractalArt).
static const IColor kHeroBright(150, 120, 210, 220), kHeroMid(80, 100, 180, 200), kHeroDim(45, 80, 150, 170);
inline constexpr float kHeroTk = 2.f;
inline constexpr float kHeroTkThin = 1.5f;
} // namespace volumart
