#pragma once

// THC Sunset (amp 14, fractal case 13): "Dark Sun".
// Hero art: THC Sunset - "Dark Sun" (Dark Souls eclipse: dark core, ring-of-fire corona, light shaft)
// PLAY motion (class B, crossfade to live): the ring of fire turns while long corona
// streamers wheel the other way behind it; flares circle the ring and the spikes flicker
// and reach with the level, a pick throws the whole corona out and lifts a prominence
// loop off the rim. The sun breathes, the shaft shimmers and the outrun grid rolls in.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
// Synthwave sky and the sun's bloom (everything under the corona).
inline void DrawThcSunsetSky(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const IColor sOrange(255, 233, 138, 90), sMag(255, 201, 74, 140), sTeal(255, 120, 210, 220);
  const float horizon = rect.T + h * 0.60f;
  const float R0 = std::min(w, h) * 0.17f, cyS = horizon - R0 * 2.35f;
  // synthwave sky
  g.PathRect(IRECT(rect.L, rect.T, rect.R, horizon));
  g.PathFill(IPattern::CreateLinearGradient(rect.L, rect.T, rect.L, horizon,
                                            {{WithA(sTeal, 0.04f), 0.f}, {WithA(sMag, 0.09f), 0.72f}, {WithA(sOrange, 0.15f), 1.f}}));
  Bloom(g, cx, cyS, R0 * 3.2f, sOrange, 0.11f);
  // clang-format on
}

// The resting ring of fire: 96 spikes, Frand(31) consumed as length, colour, alpha.
inline void DrawThcSunsetCorona(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const IColor sGold(255, 252, 222, 145), sOrange(255, 233, 138, 90);
  const float horizon = rect.T + h * 0.60f;
  const float R0 = std::min(w, h) * 0.17f, cyS = horizon - R0 * 2.35f;
  const float TAU = 6.28318f;
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
  // clang-format on
}

// Dark core, rims, light shaft, praise burst and the glowing horizon.
inline void DrawThcSunsetCore(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  const float tkThin = kHeroTkThin;
  // clang-format off
  using namespace volumart;
  const IColor sGold(255, 252, 222, 145), sOrange(255, 233, 138, 90);
  const float horizon = rect.T + h * 0.60f;
  const float R0 = std::min(w, h) * 0.17f, cyS = horizon - R0 * 2.35f;
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
  // clang-format on
}

// The grid floor's horizontal lines.
inline void DrawThcSunsetHorizontals(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float horizon = rect.T + h * 0.60f;
  for (int i = 1; i <= 8; i++)
  {
    const float t = (float)i / 8.f, yy = horizon + (rect.B - horizon) * t * t;
    g.DrawLine(IColor((int)(255.f * (0.26f * (1.f - t) + 0.08f)), 120, 210, 220), rect.L, yy, rect.R, yy, nullptr, 1.f);
  }
  // clang-format on
}

// The grid floor's perspective lines.
inline void DrawThcSunsetVerticals(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float horizon = rect.T + h * 0.60f;
  for (int i = -12; i <= 12; i++)
  {
    const float fx = cx + ((float)i / 12.f) * w * 0.55f;
    g.DrawLine(IColor(38, 120, 210, 220), cx + ((float)i / 12.f) * w * 0.05f, horizon, fx, rect.B, nullptr, 1.f);
  }
  // clang-format on
}

inline void DrawThcSunsetHero(IGraphics& g, const IRECT& rect)
{
  DrawThcSunsetSky(g, rect);
  DrawThcSunsetCorona(g, rect);
  DrawThcSunsetCore(g, rect);
  DrawThcSunsetHorizontals(g, rect);
  DrawThcSunsetVerticals(g, rect);
}

struct ThcSunsetSun
{
  float cx, horizon, R0, cyS;
};

inline ThcSunsetSun ThcSunsetSunAt(const IRECT& rect)
{
  const float w = rect.W(), h = rect.H();
  const float horizon = rect.T + h * 0.60f;
  const float R0 = std::min(w, h) * 0.17f;
  return {rect.MW(), horizon, R0, horizon - R0 * 2.35f};
}

static const IColor kThcGold(255, 252, 222, 145), kThcOrange(255, 233, 138, 90), kThcMag(255, 201, 74, 140),
  kThcHot(255, 255, 240, 196);

class ThcSunsetAnimator final : public ArtAnimator
{
public:
  // Rest is the whole static art in one layer, so silence is bit-exact. Sounding, the
  // art is recomposed from its passes round the live corona and grid lines:
  // sky, streamers, corona, breath, prominences, eclipse (core..horizon + verticals), grid.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mRest.Ok(g))
      mRest.Build(g, owner, r, [&] { DrawThcSunsetHero(g, r); });
    if (!mSky.Ok(g))
      mSky.Build(g, owner, r, [&] { DrawThcSunsetSky(g, r); });
    if (!mCorona.Ok(g))
      mCorona.Build(g, owner, r, [&] { DrawThcSunsetCorona(g, r); });
    if (!mEclipse.Ok(g))
      mEclipse.Build(g, owner, r, [&] {
        DrawThcSunsetCore(g, r);
        DrawThcSunsetVerticals(g, r);
      });
    ReplaySpikes(r);
    PlaceRays();
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest())
    {
      mRest.DrawLit(g, r, m.bloom);
      return;
    }
    const float wake = m.Wake();
    const ThcSunsetSun sun = ThcSunsetSunAt(r);
    const float spin = Phase(m.clock, kCoronaTurn, kTau);
    mSky.DrawLit(g, r, m.bloom);
    DrawRays(g, sun, m, wake);
    mCorona.Draw(g, r, 1.f - wake);
    mCorona.DrawAdd(g, r, m.bloom * (1.f - wake));
    DrawLiveCorona(g, sun, m, wake, spin);
    DrawBreath(g, sun, m, wake);
    DrawProminences(g, sun, m, spin);
    mEclipse.DrawLit(g, r, m.bloom);
    DrawGrid(g, r, sun, m, wake);
    DrawShaft(g, sun, m, wake);
    DrawPraise(g, sun, m, wake);
  }

  void DropLayers() override
  {
    mRest.Reset();
    mSky.Reset();
    mCorona.Reset();
    mEclipse.Reset();
  }

private:
  static constexpr int kSpikes = 96;
  static constexpr int kRays = 10;
  static constexpr int kLoopPts = 16;
  static constexpr double kTau = 6.283185307179586;
  static constexpr double kCoronaTurn = kTau / 40.0; // rad per motion second, clockwise
  static constexpr double kRayTurn = kTau / 64.0; // counter-clockwise
  static constexpr double kFlareRate = 1.7;
  static constexpr double kFlickRate = 5.0;
  static constexpr double kBreathRate = 1.3;
  static constexpr double kShimmerRate = 2.3;
  static constexpr double kGridRate = 0.45; // grid lines rolled in per motion second
  static constexpr double kNoisePeriod = 65536.0;
  // Streamer tips trail their counter-clockwise turn.
  static constexpr float kCurl = 0.14f;

  struct Spike
  {
    float a, len, alpha;
    bool gold;
  };

  struct Live
  {
    float x0, y0, x1, y1, heat, boost;
  };

  struct Ray
  {
    float a, reach, spread;
  };

  static float Phase(double clock, double rate, double period)
  {
    return static_cast<float>(std::fmod(clock * rate, period));
  }

  // The hero corona's own Frand(31) sequence, replayed in its order.
  void ReplaySpikes(const IRECT& r)
  {
    const float R0 = ThcSunsetSunAt(r).R0;
    const float TAU = 6.28318f;
    unsigned seed = 31u;
    for (int i = 0; i < kSpikes; i++)
    {
      Spike& sp = mSpikes[i];
      sp.a = (float)i / 96.f * TAU;
      sp.len = R0 * (1.02f + Frand(seed) * 0.16f + 0.04f * sinf(sp.a * 7.f));
      sp.gold = Frand(seed) > 0.5f;
      sp.alpha = 0.5f + 0.45f * Frand(seed);
    }
  }

  void PlaceRays()
  {
    for (int j = 0; j < kRays; ++j)
    {
      const uint32_t id = static_cast<uint32_t>(j) * 2654435761u;
      mRays[j].a = (static_cast<float>(j) + 0.6f * (Hash01(id ^ 0x51u) - 0.5f)) * static_cast<float>(kTau) / kRays;
      mRays[j].reach = 1.5f + 0.95f * Hash01(id ^ 0x52u);
      mRays[j].spread = 0.06f + 0.05f * Hash01(id ^ 0x53u);
    }
  }

  // One tapered, curved streamer from under the core out to len, fading radially from the rim.
  static void Streamer(IGraphics& g, const ThcSunsetSun& sun, float th, float spread, float len, const IColor& c,
                       float a, float w)
  {
    const float r0 = sun.R0 * 0.96f, rm = 0.5f * (sun.R0 + len), mid = th + 0.45f * kCurl, tip = th + kCurl;
    g.PathClear();
    g.PathMoveTo(sun.cx + std::cos(th - spread) * r0, sun.cyS + std::sin(th - spread) * r0);
    g.PathLineTo(sun.cx + std::cos(mid - 0.4f * spread) * rm, sun.cyS + std::sin(mid - 0.4f * spread) * rm);
    g.PathLineTo(sun.cx + std::cos(tip) * len, sun.cyS + std::sin(tip) * len);
    g.PathLineTo(sun.cx + std::cos(mid + 0.4f * spread) * rm, sun.cyS + std::sin(mid + 0.4f * spread) * rm);
    g.PathLineTo(sun.cx + std::cos(th + spread) * r0, sun.cyS + std::sin(th + spread) * r0);
    g.PathClose();
    // NanoVG uses only the first and last stop; the first stop's offset is the inner radius.
    const IPattern fade =
      IPattern::CreateRadialGradient(sun.cx, sun.cyS, len, {{WithA(c, a), sun.R0 / len}, {WithA(c, 0.f), 1.f}});
    const IBlend add(EBlend::Add, w);
    g.PathFill(fade, IFillOptions(), &add);
  }

  void DrawRays(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float wake)
  {
    const float amp = std::min(1.f, wake * (0.4f + 0.6f * m.energy) + 0.35f * m.pick);
    if (amp <= 0.f)
      return;
    const float spin = -Phase(m.clock, kRayTurn, kTau);
    const float drift = Phase(m.clock, 0.45, kNoisePeriod), shimmer = Phase(m.clock, 0.7, kNoisePeriod);
    const float reachGain = 0.8f + 0.2f * m.energy + 0.3f * m.pick;
    // Streamers fade out before the horizon instead of spilling onto the floor.
    const float floorDist = 0.96f * (sun.horizon - sun.cyS);
    const IColor soft = Mix(kThcOrange, kThcMag, 0.3f);
    for (int j = 0; j < kRays; ++j)
    {
      const Ray& ray = mRays[j];
      const float th = ray.a + spin;
      float len = sun.R0 * ray.reach * reachGain * (1.f + 0.35f * Noise1(drift, 200u + static_cast<uint32_t>(j)));
      const float down = std::sin(th + kCurl) + 0.5f * ray.spread;
      if (down > 0.05f)
        len = std::min(len, floorDist / down);
      if (len < 1.3f * sun.R0)
        continue;
      const float w = std::min(1.f, amp * (0.8f + 0.4f * Noise1(shimmer, 300u + static_cast<uint32_t>(j))));
      Streamer(g, sun, th, ray.spread * (1.f + 0.3f * m.pick), len, soft, 0.5f, w);
      Streamer(g, sun, th, ray.spread * 0.3f, len * 0.92f, kGold, 0.55f, w);
    }
  }

  // The ring turns rigidly (each spike keeps its resting length and colour); three flare
  // bulges run round it the other way, every spike flickers, and a pick throws them all out.
  void DrawLiveCorona(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float wake, float spin)
  {
    if (wake <= 0.f)
      return;
    const float E = m.energy, P = m.pick;
    const float flarePh = Phase(m.clock, kFlareRate, kTau);
    const float flickX = Phase(m.clock, kFlickRate, kNoisePeriod);
    const uint32_t pickSeed = m.onsets * 747796405u + m.seed;
    const float root = sun.R0 * 0.99f;
    int nGold = 0, nOrange = 0;
    for (int i = 0; i < kSpikes; ++i)
    {
      const Spike& sp = mSpikes[i];
      const float th = sp.a + spin;
      const float cs = std::cos(th), sn = std::sin(th);
      const float wave = 0.5f + 0.5f * std::sin(3.f * th + flarePh);
      const float flick = Noise1(flickX, static_cast<uint32_t>(i));
      const float burst = 0.7f + 0.3f * Hash01(static_cast<uint32_t>(i) * 2654435761u ^ pickSeed);
      const float gain = E * (1.3f * wave + 0.5f * flick) + 1.6f * P * burst;
      const float len = sp.len + (sp.len - root + 0.05f * sun.R0) * gain;
      Live& l = mLive[i];
      l.x0 = sun.cx + cs * root;
      l.y0 = sun.cyS + sn * root;
      l.x1 = sun.cx + cs * len;
      l.y1 = sun.cyS + sn * len;
      l.heat = std::min(1.f, 0.5f * E * wave * wave + 0.45f * P);
      l.boost = 1.f + 0.35f * E + 0.3f * E * wave + 0.3f * P;
      float* seg = sp.gold ? &mGlow[0][4 * nGold++] : &mGlow[1][4 * nOrange++];
      seg[0] = l.x0;
      seg[1] = l.y0;
      seg[2] = l.x1;
      seg[3] = l.y1;
    }
    const float glowA = std::min(1.f, 0.18f * (1.f + 0.5f * E + 0.6f * P)) * wake;
    const float glowW = 3.f + 2.f * E + 1.5f * P;
    BatchLines(g, WithA(kThcGold, glowA), glowW, mGlow[0], nGold);
    BatchLines(g, WithA(kThcOrange, glowA), glowW, mGlow[1], nOrange);
    for (int i = 0; i < kSpikes; ++i)
    {
      const Spike& sp = mSpikes[i];
      const Live& l = mLive[i];
      const IColor col = Mix(sp.gold ? kThcGold : kThcOrange, kThcHot, l.heat);
      g.DrawLine(
        WithA(col, std::min(1.f, sp.alpha * l.boost) * wake), l.x0, l.y0, l.x1, l.y1, nullptr, 1.4f + 0.5f * l.heat);
    }
  }

  // Warm light round the rim. A ring, not a disc: the core's gradient is nearly clear at
  // its centre, so light under it would show through the dark sun.
  void DrawBreath(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float wake)
  {
    const float pulse = 0.85f + 0.15f * std::sin(Phase(m.clock, kBreathRate, kTau));
    const float w = std::min(1.f, wake * (0.55f * m.energy * pulse + 0.45f * m.pick));
    if (w <= 0.f)
      return;
    const float R = sun.R0 * 2.5f, inner = sun.R0 * 0.97f;
    const IPattern warm = IPattern::CreateRadialGradient(
      sun.cx, sun.cyS, R, {{WithA(kThcOrange, 0.7f), inner / R}, {WithA(kThcOrange, 0.f), 1.f}});
    const IBlend add(EBlend::Add, w);
    g.PathClear();
    g.PathCircle(sun.cx, sun.cyS, 0.5f * (R + inner));
    g.PathStroke(warm, R - inner, IStrokeOptions(), &add);
  }

  // Each pick lifts a pink prominence loop off the rim (riding the ring's turn); it rises,
  // hangs and is gone by kMaxEventAge. Its feet sit under the rim drawn next.
  void DrawProminences(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float spin)
  {
    const IColor core = Mix(kThcMag, kThcGold, 0.55f);
    const float PIf = 3.14159265f;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kMaxEventAge || m.onsets <= static_cast<uint32_t>(k))
        continue;
      const uint32_t id = (m.onsets - static_cast<uint32_t>(k)) * 747796405u + m.seed;
      const float life = 1.f - age / kMaxEventAge, fade = life * life;
      const float foot = static_cast<float>(kTau) * Hash01(id) + spin;
      const float span = 0.09f + 0.09f * Hash01(id ^ 0x68e31da4u);
      const float lean = 0.6f * (Hash01(id ^ 0xb5297a4du) - 0.5f);
      const float rise = sun.R0 * (0.22f + 0.2f * Hash01(id ^ 0x1b56c4e9u))
                         * (0.35f + 0.65f * Smoothstep(0.f, 0.45f, age)) * (1.f + 0.2f * age);
      for (int p = 0; p < kLoopPts; ++p)
      {
        const float u = static_cast<float>(p) / static_cast<float>(kLoopPts - 1);
        const float lift = std::pow(std::max(0.f, std::sin(u * PIf)), 0.7f);
        const float ang = foot + span * (2.f * u - 1.f) + lean * span * lift;
        const float rad = sun.R0 * 0.98f + rise * lift;
        mLoop[2 * p] = sun.cx + std::cos(ang) * rad;
        mLoop[2 * p + 1] = sun.cyS + std::sin(ang) * rad;
      }
      const IBlend glow(EBlend::Add, 0.55f * fade), hot(EBlend::Add, 0.9f * fade);
      BatchPolyline(g, kThcMag, 6.f, mLoop, kLoopPts, &glow);
      BatchPolyline(g, core, 1.8f, mLoop, kLoopPts, &hot);
    }
  }

  // Resting horizontals at 1 - wake; live ones roll toward the viewer at wake, born dark
  // at the horizon. Same RGB as the verticals, so drawing them after is order-safe.
  void DrawGrid(IGraphics& g, const IRECT& r, const ThcSunsetSun& sun, const ArtMotion& m, float wake)
  {
    const float depth = r.B - sun.horizon;
    auto line = [&](float t, float a) {
      if (a <= 0.f)
        return;
      const float yy = sun.horizon + depth * t * t;
      g.DrawLine(IColor((int)(255.f * a), 120, 210, 220), r.L, yy, r.R, yy, nullptr, 1.f);
    };
    if (wake < 1.f)
      for (int i = 1; i <= 8; i++)
      {
        const float t = (float)i / 8.f;
        line(t, (0.26f * (1.f - t) + 0.08f) * (1.f - wake));
      }
    if (wake <= 0.f)
      return;
    const float scroll = Phase(m.clock, kGridRate, 1.0);
    for (int i = 0; i <= 8; i++)
    {
      const float t = (static_cast<float>(i) + scroll) / 8.f;
      if (t > 1.f)
        break;
      line(t, (0.26f * (1.f - t) + 0.08f) * std::min(1.f, 8.f * t) * wake);
    }
  }

  void DrawShaft(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float wake)
  {
    const float shimmer = 0.8f + 0.2f * std::sin(Phase(m.clock, kShimmerRate, kTau));
    const float w = std::min(1.f, wake * (0.7f * m.energy + 0.5f * m.pick) * shimmer);
    if (w <= 0.f)
      return;
    const float cx = sun.cx, topY = sun.cyS + sun.R0 * 0.92f, wt = sun.R0 * 0.20f, wb = sun.R0 * 0.05f;
    g.PathClear();
    g.PathMoveTo(cx - wt, topY);
    g.PathLineTo(cx + wt, topY);
    g.PathLineTo(cx + wb, sun.horizon);
    g.PathLineTo(cx - wb, sun.horizon);
    g.PathClose();
    const IPattern beam =
      IPattern::CreateLinearGradient(cx, topY, cx, sun.horizon, {{WithA(kGold, 0.55f), 0.f}, {WithA(kGold, 0.f), 1.f}});
    const IBlend add(EBlend::Add, w);
    g.PathFill(beam, IFillOptions(), &add);
  }

  // The praise burst on the horizon reaches up on a pick.
  void DrawPraise(IGraphics& g, const ThcSunsetSun& sun, const ArtMotion& m, float wake)
  {
    const float w = std::min(1.f, wake * (0.3f * m.energy + 0.9f * m.pick));
    if (w <= 0.f)
      return;
    const float reach = 1.f + 0.2f * m.energy + 1.4f * m.pick;
    float seg[4 * 9];
    int n = 0;
    for (int k = -4; k <= 4; k++, n++)
    {
      const float a = -1.5708f + k * 0.16f, l = sun.R0 * (0.5f - std::abs(k) * 0.05f) * reach;
      seg[4 * n] = sun.cx;
      seg[4 * n + 1] = sun.horizon;
      seg[4 * n + 2] = sun.cx + std::cos(a) * l;
      seg[4 * n + 3] = sun.horizon + std::sin(a) * l;
    }
    const IBlend add(EBlend::Add, w);
    BatchLines(g, WithA(kGold, 0.8f), 1.2f, seg, n, &add);
  }

  ArtLayer mRest;
  ArtLayer mSky;
  ArtLayer mCorona;
  ArtLayer mEclipse;
  Spike mSpikes[kSpikes] = {};
  Live mLive[kSpikes] = {};
  Ray mRays[kRays] = {};
  float mGlow[2][4 * kSpikes] = {};
  float mLoop[2 * kLoopPts] = {};
};

inline std::unique_ptr<ArtAnimator> MakeThcSunsetAnimator()
{
  return std::make_unique<ThcSunsetAnimator>();
}
} // namespace volumart::art
