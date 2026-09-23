#pragma once

// Ampete One (amp 0, fractal case 0): "Dragon Nebula".
// Hero art: Ampete One - Dragon Nebula (dragon curve, left-anchored across ~2/3 width)
// PLAY motion (class A): breath of fire. Flames run along the coiled body from the teal
// tail into the gold head, which flares and exhales a plume of embers. The level sets the
// pace (through the clock), flame length and brightness, and adds a second flame when
// loud; each pick lights the tail and fires a fast breath through the whole body.

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

class AmpeteOneAnimator final : public AdditiveArtAnimator<&DrawAmpeteOneHero>
{
private:
  struct Comet
  {
    float head, len, amp, speed, shed; // head / len in body points; speed in points per second
    uint32_t salt;
  };

  struct Spark
  {
    float x, y, r, a;
    int hue;
  };

  static constexpr float kClockSpeed = 700.f;
  static constexpr float kPickSpeed = 4600.f; // a pick's breath crosses the body in ~0.9 s
  // A clock flame waits this many points past the head before it restarts at the tail;
  // longer than its length plus its embers' and plume's flight, so nothing pops at the wrap.
  static constexpr float kOffstage = 720.f;
  static constexpr float kPickLen = 260.f;
  static constexpr float kEmberLife = 0.5f;
  static constexpr float kPlumeLife = 0.42f;
  static constexpr float kPlumeStep = 10.f;
  static constexpr int kSiteStep = 16;
  static constexpr int kTiers = 4;
  static constexpr int kAlphaBins = 3;
  static constexpr int kMaxSparks = 400;

protected:
  // Same dragon as DrawAmpeteOneHero, same transform.
  void PrepareExtras(const IRECT& r) override
  {
    const float cy = r.MH(), w = r.W(), h = r.H();
    std::vector<int> turns;
    for (int i = 0; i < 12; i++)
    {
      std::vector<int> next;
      for (auto t : turns)
        next.push_back(t);
      next.push_back(1);
      for (int j = (int)turns.size() - 1; j >= 0; j--)
        next.push_back(1 - turns[j]);
      turns = next;
    }
    std::vector<std::pair<float, float>> raw;
    raw.push_back({0.f, 0.f});
    {
      float px = 0.f, py = 0.f;
      int dir = 0;
      const float dxs[] = {1, 0, -1, 0}, dys[] = {0, -1, 0, 1};
      for (int i = 0; i < (int)turns.size(); i++)
      {
        px += dxs[dir];
        py += dys[dir];
        raw.push_back({px, py});
        dir = (dir + (turns[i] ? 1 : 3)) % 4;
      }
    }
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : raw)
    {
      mnx = std::min(mnx, p.first);
      mny = std::min(mny, p.second);
      mxx = std::max(mxx, p.first);
      mxy = std::max(mxy, p.second);
    }
    const float extX = mxx - mnx, extY = mxy - mny;
    const float S = std::min((w * 0.60f) / (extX > 0.f ? extX : 1.f), (h * 0.90f) / (extY > 0.f ? extY : 1.f));
    const float artW = extX * S, artH = extY * S;
    const float ox = r.L + w * 0.05f, oy = cy - artH * 0.5f;
    const float acx = ox + artW * 0.5f, acy = oy + artH * 0.5f;
    mPts.clear();
    mPts.reserve(2 * raw.size());
    for (auto& p : raw)
    {
      mPts.push_back(ox + (p.first - mnx) * S);
      mPts.push_back(oy + (p.second - mny) * S);
    }
    mN = static_cast<int>(mPts.size() / 2);
    if (mN < 2)
      return;
    // One lattice step is ~4 px in the mono panel and ~2 px in a dual lane.
    mUnit = std::clamp(S / 4.f, 0.45f, 1.25f);
    mPlume = std::max(0.7f, mUnit);
    mGlowW = 2.5f + 5.f * mUnit;
    mCoreW = 1.1f + 1.4f * mUnit;
    mTailX = mPts[0];
    mTailY = mPts[1];
    mHeadX = mPts[2 * (mN - 1)];
    mHeadY = mPts[2 * (mN - 1) + 1];
    // The mouth opens along the last segment and away from the body.
    float sx = mHeadX - mPts[2 * (mN - 2)], sy = mHeadY - mPts[2 * (mN - 2) + 1];
    const float sl = std::hypot(sx, sy);
    sx = sl > 0.f ? sx / sl : 0.f;
    sy = sl > 0.f ? sy / sl : 0.f;
    float ax = mHeadX - acx, ay = mHeadY - acy;
    const float al = std::hypot(ax, ay);
    ax = al > 1e-3f ? ax / al : 0.f;
    ay = al > 1e-3f ? ay / al : 0.f;
    const float dx = sx + ax, dy = sy + ay;
    mMouth = (dx == 0.f && dy == 0.f) ? 3.14159265f : std::atan2(dy, dx);
    mPath.reserve(2 * 420);
    mSparks.reserve(kMaxSparks);
    mDots.reserve(3 * kMaxSparks);
  }

  float BaseDim(const ArtMotion& m) const override { return 0.2f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    (void)r;
    if (mN < 2)
      return;
    const float wake = m.Wake();
    const float last = static_cast<float>(mN - 1);
    const double cycle = static_cast<double>(mN) + kOffstage;
    Comet comets[2 + kOnsetSlots];
    int n = 0;
    const float clockAmp = std::min(1.f, wake * (0.45f + 0.55f * m.energy) + 0.3f * m.pick);
    for (int c = 0; c < 2; ++c)
    {
      const float amp = clockAmp * (c == 0 ? 1.f : Smoothstep(0.3f, 0.75f, m.energy));
      if (amp <= 0.f)
        continue;
      const double run = m.clock * kClockSpeed + c * 0.5 * cycle;
      const double lap = std::floor(run / cycle);
      const uint32_t salt = (static_cast<uint32_t>(lap) * 2u + static_cast<uint32_t>(c)) * 2246822519u ^ 0x5eedu;
      comets[n++] = {
        static_cast<float>(run - lap * cycle), 140.f + 110.f * m.energy + 60.f * m.pick, amp, kClockSpeed, 0.4f, salt};
    }
    float ignite = 0.f;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kMaxEventAge)
        continue;
      const uint32_t pickId = m.onsets - static_cast<uint32_t>(k);
      const float hv = Hash01(pickId * 747796405u + m.seed);
      const float hs = Hash01(pickId * 2891336453u ^ (m.seed + 77u));
      const float amp = (0.75f + 0.25f * hv) * (0.7f + 0.3f * m.energy) * (1.f - Smoothstep(1.15f, 1.45f, age));
      if (amp <= 0.f)
        continue;
      const float speed = kPickSpeed * (0.92f + 0.16f * hs);
      comets[n++] = {age * speed, kPickLen, amp, speed, 0.18f, pickId * 2654435761u ^ 0xf1a3u};
      ignite = std::max(ignite, amp * (1.f - Smoothstep(0.f, 0.3f, age)));
    }

    mSparks.clear();
    float flare = 0.f;
    for (int i = 0; i < n; ++i)
    {
      const Comet& c = comets[i];
      DrawComet(g, c);
      ShedEmbers(c);
      Exhale(c);
      const float since = std::max(0.f, (c.head - last - c.len) / c.speed);
      flare += c.amp * Smoothstep(last - 200.f, last, c.head) * (1.f - Smoothstep(0.f, 0.3f, since));
    }
    DrawSparks(g);

    static const IColor kWhite(255, 240, 250, 252);
    if (ignite > 0.f)
    {
      BloomAdd(g, mTailX, mTailY, 12.f + 10.f * ignite, kTeal, 0.6f, ignite);
      GlowDotAdd(g, kTeal, kWhite, mTailX, mTailY, 1.5f + 1.5f * ignite, 4.f + 5.f * ignite, ignite);
    }
    const float ph = static_cast<float>(std::fmod(m.clock * 2.0, 6.283185307179586));
    const float breath = wake * (0.1f + 0.2f * m.energy) * (0.5f + 0.5f * std::sin(ph));
    flare = std::min(1.f, flare);
    const float heat = std::min(1.f, flare + breath);
    if (heat > 0.f)
    {
      BloomAdd(g, mHeadX, mHeadY, 16.f + 26.f * flare, kGold, 0.55f, heat);
      GlowDotAdd(g, kGold, kGoldHi, mHeadX, mHeadY, 2.5f + 2.5f * flare, 6.f + 8.f * flare, 0.85f * heat);
    }
  }

private:
  void At(float s, float& x, float& y) const
  {
    const int i0 = std::min(static_cast<int>(s), mN - 2);
    const float f = s - static_cast<float>(i0);
    x = mPts[2 * i0] + (mPts[2 * i0 + 2] - mPts[2 * i0]) * f;
    y = mPts[2 * i0 + 1] + (mPts[2 * i0 + 3] - mPts[2 * i0 + 1]) * f;
  }

  // Polyline along the body from point position s0 to s1 (0 <= s0 < s1 <= last).
  void Span(float s0, float s1)
  {
    mPath.clear();
    float x = 0.f, y = 0.f;
    At(s0, x, y);
    mPath.push_back(x);
    mPath.push_back(y);
    for (int i = static_cast<int>(s0) + 1; static_cast<float>(i) < s1; ++i)
    {
      mPath.push_back(mPts[2 * i]);
      mPath.push_back(mPts[2 * i + 1]);
    }
    At(s1, x, y);
    mPath.push_back(x);
    mPath.push_back(y);
  }

  void StrokeSpan(IGraphics& g, const IColor& c, float width, const IStrokeOptions& o, const IBlend& b) const
  {
    g.PathClear();
    g.PathMoveTo(mPath[0], mPath[1]);
    for (size_t i = 2; i + 1 < mPath.size(); i += 2)
      g.PathLineTo(mPath[i], mPath[i + 1]);
    g.PathStroke(c, width, o, &b);
  }

  // A flame: one soft additive stroke, then tiers brightening toward its front, which
  // warm from teal to gold with the body's own gradient.
  void DrawComet(IGraphics& g, const Comet& c)
  {
    const float last = static_cast<float>(mN - 1);
    const float s0 = std::max(0.f, c.head - c.len), s1 = std::min(last, c.head);
    if (s1 - s0 < 0.5f)
      return;
    const IColor glow = Mix(kTeal, kGold, 0.5f * (s0 + s1) / last);
    const IBlend soft(EBlend::Add, 0.45f * c.amp);
    Span(s0, s1);
    StrokeSpan(g, glow, mGlowW, RoundStroke(), soft);
    // Butt caps: tiers meet end to end, so their joints do not add up into beads.
    IStrokeOptions butt = RoundStroke();
    butt.mCapOption = ELineCap::Butt;
    for (int k = 0; k < kTiers; ++k)
    {
      const float f0 = static_cast<float>(k) / kTiers, f1 = static_cast<float>(k + 1) / kTiers;
      const float a0 = std::max(s0, c.head - c.len * (1.f - f0)), a1 = std::min(s1, c.head - c.len * (1.f - f1));
      if (a1 - a0 < 0.25f)
        continue;
      const IBlend add(EBlend::Add, std::min(0.9f, c.amp * (0.25f + 0.75f * std::pow(f1, 1.4f))));
      Span(a0, a1);
      StrokeSpan(g, Mix(kTeal, kGoldHi, 0.5f * (a0 + a1) / last), mCoreW * (0.75f + 0.25f * f1), butt, add);
    }
    if (c.head <= last)
    {
      static const IColor kWhite(255, 240, 250, 252);
      float hx = 0.f, hy = 0.f;
      At(c.head, hx, hy);
      BloomAdd(g, hx, hy, mUnit * (9.f + 7.f * c.amp), glow, 0.5f, c.amp);
      GlowDotAdd(g, glow, Mix(glow, kWhite, 0.5f), hx, hy, 0.4f + 1.1f * mUnit, 1.f + 3.f * mUnit, c.amp);
    }
  }

  // Sparks rise off fixed sites along the body once the flame front has passed them.
  void ShedEmbers(const Comet& c)
  {
    const float last = static_cast<float>(mN - 1);
    const float back = c.speed * kEmberLife;
    const int k0 = std::max(0, static_cast<int>(std::ceil((c.head - back) / kSiteStep)));
    const int k1 = static_cast<int>(std::min(c.head, last) / kSiteStep);
    for (int k = k0; k <= k1; ++k)
    {
      const uint32_t id = static_cast<uint32_t>(k) * 2654435761u ^ c.salt;
      if (Hash01(id) >= c.shed)
        continue;
      const int i = k * kSiteStep;
      const float age = (c.head - static_cast<float>(i)) / c.speed, x = age / kEmberLife;
      if (x < 0.f || x >= 1.f)
        continue;
      const float h1 = Hash01(id + 0x68e31da4u), h2 = Hash01(id + 0xb5297a4du);
      const float ex = mPts[2 * i] + mUnit * 6.f * x * Noise1(age * 2.6f + 9.f * h1, id);
      const float ey = mPts[2 * i + 1] - mUnit * (8.f + 14.f * h1) * x * (1.6f - 0.6f * x);
      const float fade = 1.f - x;
      AddSpark(ex, ey, mUnit * (0.7f + 0.8f * h2) * (1.f - 0.4f * x), c.amp * fade * std::sqrt(fade),
               static_cast<float>(i) < 0.45f * last ? 0 : 1);
    }
  }

  // The part of a flame that runs past the head leaves the mouth as a fan of embers
  // that slows, rises and fades.
  void Exhale(const Comet& c)
  {
    const float out = c.head - static_cast<float>(mN - 1);
    if (out <= 0.f)
      return;
    for (int q = 0;; ++q)
    {
      const float e = static_cast<float>(q) * kPlumeStep;
      if (e > out)
        break;
      const float keep = 1.f - Smoothstep(0.7f * c.len, c.len, e);
      if (keep <= 0.f)
        break;
      const float age = (out - e) / c.speed, x = age / kPlumeLife;
      if (x >= 1.f)
        continue;
      const uint32_t id = (static_cast<uint32_t>(q) * 2654435761u ^ c.salt) + 0x7f4a7c15u;
      const float h1 = Hash01(id), h2 = Hash01(id + 0x68e31da4u), h3 = Hash01(id + 0xb5297a4du);
      const float ang = mMouth + (h1 - 0.5f) * 0.9f + 0.3f * x * Noise1(age * 3.f, id);
      const float reach = mPlume * (20.f + 28.f * h2) * (0.6f + 0.4f * c.amp) * x * (2.f - x);
      const float px = mHeadX + std::cos(ang) * reach;
      const float py = mHeadY + std::sin(ang) * reach - mPlume * (5.f + 9.f * h3) * x * x;
      const float fade = 1.f - x;
      AddSpark(px, py, mPlume * (0.9f + 1.f * h3) * (1.f + 0.5f * x), c.amp * keep * fade * std::sqrt(fade), 1);
    }
  }

  void AddSpark(float x, float y, float r, float a, int hue)
  {
    if (a <= 0.02f || static_cast<int>(mSparks.size()) >= kMaxSparks)
      return;
    mSparks.push_back({x, y, r, std::min(a, 1.f), hue});
  }

  // Sparks in cool (tail half) and warm inks, three brightness bins, halo then core.
  void DrawSparks(IGraphics& g)
  {
    if (mSparks.empty())
      return;
    const IColor halo[2] = {Mix(kTeal, kGold, 0.3f), kGold};
    const IColor core[2] = {Mix(kTeal, kGoldHi, 0.4f), Mix(kGold, kGoldHi, 0.6f)};
    for (int hue = 0; hue < 2; ++hue)
      for (int k = 0; k < kAlphaBins; ++k)
      {
        const float lo = static_cast<float>(k) / kAlphaBins, hi = static_cast<float>(k + 1) / kAlphaBins;
        for (int pass = 0; pass < 2; ++pass)
        {
          mDots.clear();
          for (const Spark& s : mSparks)
            if (s.hue == hue && s.a > lo && s.a <= hi)
            {
              mDots.push_back(s.x);
              mDots.push_back(s.y);
              mDots.push_back(pass == 0 ? s.r * 2.4f : s.r);
            }
          const IBlend add(EBlend::Add, pass == 0 ? 0.3f + 0.3f * hi : std::min(1.f, 0.45f + 0.55f * hi));
          BatchDots(g, pass == 0 ? halo[hue] : core[hue], mDots.data(), static_cast<int>(mDots.size() / 3), &add);
        }
      }
  }

  std::vector<float> mPts;
  std::vector<float> mPath;
  std::vector<Spark> mSparks;
  std::vector<float> mDots;
  int mN = 0;
  float mUnit = 1.f, mPlume = 1.f, mGlowW = 7.5f, mCoreW = 2.5f;
  float mTailX = 0.f, mTailY = 0.f, mHeadX = 0.f, mHeadY = 0.f, mMouth = 3.14159265f;
};

inline std::unique_ptr<ArtAnimator> MakeAmpeteOneAnimator()
{
  return std::make_unique<AmpeteOneAnimator>();
}
} // namespace volumart::art
