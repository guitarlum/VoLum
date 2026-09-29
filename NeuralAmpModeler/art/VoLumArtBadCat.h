#pragma once

// Bad Cat Mini Cat (amp 1, fractal case 1): "Eyes in the Dark".
// Hero art: Bad Cat Mini Cat - Eyes in the Dark (two glowing cat eyes over a starfield)
// PLAY motion (class B, lids class C): the cat wakes. Its slit pupils swell rounder with the
// level and breathe, glance about in darts and holds, and now and then it slow-blinks (the
// eyes squash shut toward the lower lid). A pick flashes the eyeshine, startles the pupils
// wide and sparks a catchlight; the stars twinkle. Silence: the static art, exactly.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
// The bloom and the starfield behind the eyes.
inline void DrawBadCatSky(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
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
  // clang-format on
}

// The almond outline of the eye centred at (exc, cy): upper lid, then lower lid, 2 x 17 points.
inline void BadCatAlmond(float exc, float cy, float ew, float eh, std::vector<float>& xs, std::vector<float>& ys)
{
  // clang-format off
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
  // clang-format on
}

inline void DrawBadCatEye(IGraphics& g, const IRECT& rect, float exc, bool pupil)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float ew = R * 0.17f, eh = ew * 0.5f;
  std::vector<float> xs, ys;
  BadCatAlmond(exc, cy, ew, eh, xs, ys);
  g.FillConvexPolygon(WithA(kTeal, 0.30f), xs.data(), ys.data(), (int)xs.size());
  for (size_t i = 1; i < xs.size(); i++)
    g.DrawLine(WithA(kTeal, 0.85f), xs[i - 1], ys[i - 1], xs[i], ys[i], nullptr, 2.f);
  g.DrawLine(WithA(kTeal, 0.85f), xs.back(), ys.back(), xs.front(), ys.front(), nullptr, 2.f);
  if (pupil)
  {
    const float pw = ew * 0.13f, ph = eh * 0.92f;
    g.FillEllipse(WithA(kGold, 0.35f), IRECT(exc - pw * 2.4f, cy - ph * 1.15f, exc + pw * 2.4f, cy + ph * 1.15f));
    g.FillEllipse(WithA(kGold, 0.95f), IRECT(exc - pw, cy - ph, exc + pw, cy + ph));
  }
  // clang-format on
}

// Both eyes; without pupils this is the cached eye layer PLAY draws its live pupils into.
inline void DrawBadCatEyes(IGraphics& g, const IRECT& rect, bool pupils)
{
  const float cx = rect.MW(), R = std::min(rect.W(), rect.H());
  const float sep = R * 0.24f;
  DrawBadCatEye(g, rect, cx - sep, pupils);
  DrawBadCatEye(g, rect, cx + sep, pupils);
}

inline void DrawBadCatHero(IGraphics& g, const IRECT& rect)
{
  DrawBadCatSky(g, rect);
  DrawBadCatEyes(g, rect, true);
}

class BadCatAnimator final : public ArtAnimator
{
public:
  // Silence blits the static art itself. Sounding, the sky and the pupil-less eyes are
  // cached; the eyes do not overlap, so drawing both pupils after both eyes is the same image.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mRest.Ok(g))
      mRest.Build(g, owner, r, [&] { DrawBadCatHero(g, r); });
    if (!mSky.Ok(g))
      mSky.Build(g, owner, r, [&] { DrawBadCatSky(g, r); });
    if (!mEyes.Ok(g))
      mEyes.Build(g, owner, r, [&] { DrawBadCatEyes(g, r, false); });
    PrepareGeometry(r);
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest())
    {
      mRest.DrawLit(g, r, m.bloom);
      return;
    }
    const float wake = m.Wake(), E = m.energy;
    float flash = 0.f, swell = 0.f;
    PickResponse(m, flash, swell);
    const float kick = std::max(m.pick, flash);
    const float closure = wake * BlinkClosure(m.clock, m.seed);
    const float sy = 1.f - 0.9f * closure;
    const bool blinking = sy < 1.f;
    const IMatrix lid = AboutPoint(mCx, mCy + mEh * 0.25f, 0.f, 1.f, sy);

    mSky.DrawLit(g, r, m.bloom);
    DrawTwinkles(g, m.clock, wake * (0.3f + 0.55f * E));
    if (blinking)
    {
      mEyes.DrawXform(g, r, lid);
      mEyes.DrawXform(g, r, lid, m.bloom, true);
      g.PathTransformSave();
      g.PathTransformMatrix(lid);
    }
    else
      mEyes.DrawLit(g, r, m.bloom);

    DrawRims(g, std::min(0.9f, wake * (0.28f + 0.37f * E) + 0.55f * kick + 0.35f * closure));

    float gx = 0.f, gy = 0.f;
    Gaze(m.clock, m.seed, gx, gy);
    const float reach = wake * (0.4f + 0.6f * E);
    const float dx = gx * mEw * 0.14f * reach, dy = gy * mEh * 0.12f * reach;
    const float pw0 = mEw * 0.13f, ph0 = mEh * 0.92f;
    const float breathe = 1.f + 0.5f * Noise1(static_cast<float>(m.clock) * 0.35f, m.seed + 40u);
    const float pw = std::clamp(pw0 * (1.f + 1.3f * E * breathe + 0.35f * m.pick + 0.7f * swell), pw0, mEw * 0.32f);
    const float ph = ph0 * (1.f - 0.08f * E);
    const float haloW = pw0 * 2.4f + (pw - pw0) * 1.4f;
    const float haloA = std::min(0.7f, 0.35f * (1.f + 0.8f * E));
    const float fade = 1.f - 0.7f * Smoothstep(0.4f, 0.95f, closure);
    const float shine = std::min(0.85f, wake * (0.3f + 0.3f * E) + 0.75f * flash);

    for (int e = 0; e < 2; ++e)
      DrawShine(g, e, mExc[e] + dx, mCy + dy, shine);
    for (int e = 0; e < 2; ++e)
    {
      const float px = mExc[e] + dx, py = mCy + dy;
      g.FillEllipse(WithA(kGold, haloA * fade), IRECT(px - haloW, py - ph * 1.15f, px + haloW, py + ph * 1.15f));
      g.FillEllipse(WithA(kGold, 0.95f * fade), IRECT(px - pw, py - ph, px + pw, py + ph));
    }
    // Eyeshine: the pupils flare hot on a pick, and a wet catchlight sits at their upper left.
    static const IColor kWhite(255, 240, 250, 252);
    const float flare = std::min(1.f, 0.2f * wake * E + 0.8f * flash) * fade;
    const float catchW = std::min(1.f, wake * (0.2f + 0.3f * E) + 0.9f * flash);
    const float spark = mEw * (0.06f + 0.22f * flash);
    float seg[16];
    for (int e = 0; e < 2; ++e)
    {
      const float px = mExc[e] + dx, py = mCy + dy;
      if (flare > 0.01f)
      {
        const IBlend add(EBlend::Add, flare);
        g.FillEllipse(
          WithA(kGoldHi, 0.9f), IRECT(px - pw * 0.8f, py - ph * 0.9f, px + pw * 0.8f, py + ph * 0.9f), &add);
      }
      const float lx = px - pw - mEw * 0.05f, ly = py - ph * 0.32f;
      GlowDotAdd(g, kGoldHi, kWhite, lx, ly, 0.9f + 1.1f * flash, 2.5f + 4.5f * flash, catchW);
      float* s = seg + 8 * e;
      s[0] = lx - spark;
      s[1] = ly;
      s[2] = lx + spark;
      s[3] = ly;
      s[4] = lx;
      s[5] = ly - spark * 0.7f;
      s[6] = lx;
      s[7] = ly + spark * 0.7f;
    }
    if (flash > 0.02f)
    {
      const IBlend add(EBlend::Add, std::min(1.f, flash));
      BatchLines(g, kWhite, 1.2f, seg, 4, &add);
    }
    if (blinking)
      g.PathTransformRestore();
  }

  void DropLayers() override
  {
    mRest.Reset();
    mSky.Reset();
    mEyes.Reset();
  }

private:
  static constexpr int kStars = 44;
  static constexpr int kStarTiers = 4;
  static constexpr double kGazeHold = 2.6;
  static constexpr double kBlinkWindow = 8.5;

  struct Star
  {
    float x, y, r, rate, phase;
  };

  // Same geometry as the hero: eye centres, almond outlines, and the stars replayed from Frand(4).
  void PrepareGeometry(const IRECT& r)
  {
    const float w = r.W(), h = r.H(), R = std::min(w, h);
    mCx = r.MW();
    mCy = r.MH();
    mEw = R * 0.17f;
    mEh = mEw * 0.5f;
    const float sep = R * 0.24f;
    mExc[0] = mCx - sep;
    mExc[1] = mCx + sep;
    for (int e = 0; e < 2; ++e)
    {
      std::vector<float> xs, ys;
      BadCatAlmond(mExc[e], mCy, mEw, mEh, xs, ys);
      mAlmond[e].clear();
      for (size_t i = 0; i < xs.size(); ++i)
      {
        mAlmond[e].push_back(xs[i]);
        mAlmond[e].push_back(ys[i]);
      }
    }
    mStars.clear();
    unsigned s = 4u;
    for (int i = 0; i < kStars; i++)
    {
      const float px = r.L + Frand(s) * w, py = r.T + Frand(s) * h;
      Frand(s);
      const float rad = Frand(s) * 1.f + 0.3f;
      const uint32_t k = static_cast<uint32_t>(i) * 2654435761u;
      mStars.push_back({px, py, rad, 0.8f + 1.4f * Hash01(k ^ 2u), 6.2831853f * Hash01(k ^ 1u)});
    }
    mStarA.assign(mStars.size(), 0.f);
    mHalo.reserve(3 * mStars.size());
    mCore.reserve(3 * mStars.size());
  }

  // Pick light (instant, `flash`) and the pupils' startle (a beat later, `swell`), over the
  // last picks so a new pick never cuts the previous one short; both are gone by kMaxEventAge.
  static void PickResponse(const ArtMotion& m, float& flash, float& swell)
  {
    flash = 0.f;
    swell = 0.f;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kMaxEventAge)
        continue;
      const float tail = 1.f - age / kMaxEventAge;
      const float var = 0.7f + 0.3f * Hash01((m.onsets - static_cast<uint32_t>(k)) * 747796405u + m.seed);
      flash = std::max(flash, var * std::exp(-6.f * age) * tail);
      swell = std::max(swell, var * Smoothstep(0.f, 0.07f, age) * std::exp(-2.6f * age) * tail);
    }
  }

  // One slow blink per window (sometimes two), at a hashed moment: 0 open .. 1 shut.
  static float Lid(float u)
  {
    if (u <= 0.f || u >= 0.7f)
      return 0.f;
    return u < 0.18f ? Smoothstep(0.f, 0.18f, u) : 1.f - Smoothstep(0.26f, 0.7f, u);
  }

  static float BlinkClosure(double clock, uint32_t seed)
  {
    const double win = std::floor(clock / kBlinkWindow);
    const uint32_t n = static_cast<uint32_t>(static_cast<int64_t>(win));
    const float start = 1.2f + Hash01(n * 2654435761u ^ (seed * 131u + 7u)) * static_cast<float>(kBlinkWindow - 3.0);
    const float u = static_cast<float>(clock - win * kBlinkWindow) - start;
    float c = Lid(u);
    if (Hash01(n * 2246822519u ^ (seed * 131u + 11u)) < 0.3f)
      c = std::max(c, Lid(u - 0.8f));
    return c;
  }

  // Where the cat looks, -1..1: holds a hashed target, then darts to the next.
  static void GazeTarget(uint32_t i, uint32_t seed, float& x, float& y)
  {
    const uint32_t k = i * 2654435761u;
    if (Hash01(k ^ (seed * 977u + 1u)) < 0.3f)
    {
      x = 0.f;
      y = 0.f;
      return;
    }
    x = 2.f * Hash01(k ^ (seed * 977u + 3u)) - 1.f;
    y = 1.2f * Hash01(k ^ (seed * 977u + 5u)) - 0.6f;
  }

  static void Gaze(double clock, uint32_t seed, float& x, float& y)
  {
    const double s = clock / kGazeHold;
    const double n = std::floor(s);
    const uint32_t i = static_cast<uint32_t>(static_cast<int64_t>(n));
    float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
    GazeTarget(i - 1u, seed, x0, y0);
    GazeTarget(i, seed, x1, y1);
    const float dart = Smoothstep(0.f, 0.08f, static_cast<float>(s - n));
    const float t = static_cast<float>(clock) * 0.7f;
    x = x0 + (x1 - x0) * dart + 0.12f * Noise1(t, seed + 60u);
    y = y0 + (y1 - y0) * dart + 0.12f * Noise1(t, seed + 61u);
  }

  void PathAlmond(IGraphics& g, int e) const
  {
    const std::vector<float>& a = mAlmond[e];
    if (a.size() < 4)
      return;
    g.PathMoveTo(a[0], a[1]);
    for (size_t i = 2; i + 1 < a.size(); i += 2)
      g.PathLineTo(a[i], a[i + 1]);
    g.PathClose();
  }

  // Additive glow along both almond outlines: a wide teal halo under a bright rim.
  void DrawRims(IGraphics& g, float glow) const
  {
    if (glow <= 0.f)
      return;
    static const IColor kRim(255, 170, 235, 240);
    const IBlend wide(EBlend::Add, 0.7f * glow), rim(EBlend::Add, glow);
    g.PathClear();
    PathAlmond(g, 0);
    PathAlmond(g, 1);
    g.PathStroke(kTeal, 7.f, RoundStroke(), &wide);
    g.PathClear();
    PathAlmond(g, 0);
    PathAlmond(g, 1);
    g.PathStroke(kRim, 2.2f, RoundStroke(), &rim);
  }

  // The iris lit from within, brightest round the pupil.
  void DrawShine(IGraphics& g, int e, float px, float py, float shine) const
  {
    if (shine <= 0.f)
      return;
    const IBlend add(EBlend::Add, shine);
    g.PathClear();
    PathAlmond(g, e);
    g.PathFill(IPattern::CreateRadialGradient(
                 px, py, mEw * 0.8f, {{WithA(Mix(kGold, kTeal, 0.35f), 0.85f), 0.f}, {WithA(kTeal, 0.28f), 1.f}}),
               IFillOptions(), &add);
  }

  void DrawTwinkles(IGraphics& g, double clock, float gain)
  {
    if (gain <= 0.f || mStars.empty())
      return;
    static const IColor kStarCore(255, 220, 248, 252);
    for (size_t i = 0; i < mStars.size(); ++i)
    {
      const Star& s = mStars[i];
      const float wave =
        static_cast<float>(std::sin(static_cast<double>(s.rate) * clock + static_cast<double>(s.phase)));
      const float w2 = wave > 0.f ? wave * wave : 0.f;
      mStarA[i] = gain * w2 * w2;
    }
    for (int k = 0; k < kStarTiers; ++k)
    {
      const float lo = std::max(0.05f, static_cast<float>(k) / kStarTiers);
      const float hi = static_cast<float>(k + 1) / kStarTiers;
      mHalo.clear();
      mCore.clear();
      for (size_t i = 0; i < mStars.size(); ++i)
      {
        if (mStarA[i] <= lo || mStarA[i] > hi)
          continue;
        const Star& s = mStars[i];
        mHalo.push_back(s.x);
        mHalo.push_back(s.y);
        mHalo.push_back(s.r * 2.6f + 1.2f);
        mCore.push_back(s.x);
        mCore.push_back(s.y);
        mCore.push_back(s.r * 0.9f + 0.2f);
      }
      const int n = static_cast<int>(mHalo.size() / 3);
      if (n == 0)
        continue;
      const IBlend haloAdd(EBlend::Add, 0.25f + 0.35f * hi), coreAdd(EBlend::Add, 0.4f + 0.55f * hi);
      BatchDots(g, kTeal, mHalo.data(), n, &haloAdd);
      BatchDots(g, kStarCore, mCore.data(), n, &coreAdd);
    }
  }

  ArtLayer mRest;
  ArtLayer mSky;
  ArtLayer mEyes;
  float mCx = 0.f, mCy = 0.f, mEw = 0.f, mEh = 0.f;
  float mExc[2] = {0.f, 0.f};
  std::vector<float> mAlmond[2];
  std::vector<Star> mStars;
  std::vector<float> mStarA;
  std::vector<float> mHalo;
  std::vector<float> mCore;
};

inline std::unique_ptr<ArtAnimator> MakeBadCatAnimator()
{
  return std::make_unique<BadCatAnimator>();
}
} // namespace volumart::art
