#pragma once

// Fryette Deliverance 120 (amp 4, fractal case 3): "Spiral Galaxy".
// Hero art: Spiral galaxy (Fryette Deliverance)
// PLAY motion (class B, crossfade to live): the galaxy turns. Both arms rotate as one
// density wave (a turn per ~40 motion-seconds, arms trailing) and wind tighter as the
// level rises; disc stars orbit faster inside, brightening as they cross an arm, and
// star-forming knots twinkle. A pick lights a nova in an arm and flares the gold core.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
// Radial bin (0..2) of arm particle i: t < 1/3, t < 2/3, the rest.
inline int FryetteArmBin(int i)
{
  return i * 3 / 560;
}

inline void DrawFryetteBloom(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  const float maxR = std::min(w, h) * 0.46f;
  for (int b = 6; b >= 1; b--)
    g.FillCircle(IColor(6, 120, 210, 220), cx, cy, maxR * (float)b / 6.f);
  // clang-format on
}

// bin < 0 draws every particle; 0..2 only that bin's. The rng stream is always
// replayed in full so every particle keeps its hero position.
inline void DrawFryetteArms(IGraphics& g, const IRECT& rect, int bin = -1)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  const float maxR = std::min(w, h) * 0.46f;
  unsigned rng = 5u;
  auto frand = [&]() -> float { rng = rng * 1664525u + 1013904223u; return (float)rng / (float)0xFFFFFFFFu; };
  for (int arm = 0; arm < 2; arm++)
  {
    const float off = arm * 3.14159f;
    for (int i = 0; i < 560; i++)
    {
      const float t = (float)i / 560.f;
      const float theta = off + t * 4.2f * 3.14159f;
      const float rr = maxR * powf(t, 0.7f);
      const float px = cx + rr * cosf(theta) + (frand() - 0.5f) * rr * 0.12f;
      const float py = cy + rr * sinf(theta) + (frand() - 0.5f) * rr * 0.12f;
      if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
      if (bin >= 0 && FryetteArmBin(i) != bin) continue;
      const int al = (int)(255.f * (0.2f + 0.6f * (1.f - t)));
      g.FillCircle((t < 0.5f) ? IColor(al, 120, 210, 220) : IColor(al, 100, 180, 200), px, py, (t < 0.2f) ? 1.4f : 1.f);
    }
  }
  // clang-format on
}

inline void DrawFryetteCore(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  g.FillCircle(IColor(50, 200, 165, 87), cx, cy, 10.f);
  g.FillCircle(IColor(90, 200, 165, 87), cx, cy, 6.f);
  g.FillCircle(IColor(235, 252, 222, 145), cx, cy, 4.f);
  // clang-format on
}

inline void DrawFryetteHero(IGraphics& g, const IRECT& rect)
{
  // Two logarithmic-spiral particle arms over a soft radial bloom, with a
  // glowing gold core. Deep and atmospheric.
  DrawFryetteBloom(g, rect);
  DrawFryetteArms(g, rect);
  DrawFryetteCore(g, rect);
}

// A pick's nova: a quick flash, then a ~1 s fade, gone before kMaxEventAge.
inline float FryetteNovaEnvelope(float age)
{
  if (age < 0.f || age >= kMaxEventAge)
    return 0.f;
  const float rise = std::min(1.f, age / 0.05f);
  return rise * std::exp(-3.2f * std::max(0.f, age - 0.05f)) * (1.f - Smoothstep(0.9f, 1.45f, age));
}

inline void DrawFryetteNova(IGraphics& g, float x, float y, const IColor& tint, float env, float w)
{
  if (w <= 0.01f)
    return;
  static const IColor kNovaWhite(255, 236, 244, 255);
  BloomAdd(g, x, y, 8.f + 22.f * env, tint, 0.5f, w);
  const float len = 5.f + 19.f * env, s = 0.45f * len;
  const float rays[8] = {x - len, y, x + len, y, x, y - len, x, y + len};
  const float spikes[8] = {x - s, y, x + s, y, x, y - s, x, y + s};
  const IBlend faint(EBlend::Add, std::min(1.f, 0.55f * w));
  const IBlend bright(EBlend::Add, std::min(1.f, 0.9f * w));
  BatchLines(g, tint, 0.8f, rays, 2, &faint);
  BatchLines(g, kNovaWhite, 1.3f, spikes, 2, &bright);
  GlowDotAdd(g, tint, kNovaWhite, x, y, 1.1f + 1.4f * env, 2.5f + 5.f * env, w);
}

class FryetteAnimator final : public ArtAnimator
{
public:
  // mRest is the hero drawn as one layer, so silence is bit-exact. The bins are the
  // arms cut into three radial rings that turn about the core while sounding.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mUnder.Ok(g))
      mUnder.Build(g, owner, r, [&] { DrawFryetteBloom(g, r); });
    for (int k = 0; k < kBins; ++k)
      if (!mBin[k].Ok(g))
        mBin[k].Build(g, owner, r, [&] { DrawFryetteArms(g, r, k); });
    if (!mRest.Ok(g))
      mRest.Build(g, owner, r, [&] { DrawFryetteHero(g, r); });
    PlaceStars(r);
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
    const float cx = r.MW(), cy = r.MH(), R = std::min(r.W(), r.H());
    // Negative degrees turn against the winding, so the arms trail. The inner ring
    // leads with the level: the galaxy winds tighter as you dig in.
    const float pattern = -static_cast<float>(std::fmod(m.clock * kPatternDegPerSecond, 360.0));
    float binDeg[kBins], binRad[kBins];
    for (int k = 0; k < kBins; ++k)
    {
      binDeg[k] = pattern + kWindDeg[k] * m.energy;
      binRad[k] = binDeg[k] * (3.14159265f / 180.f);
    }
    mUnder.DrawLit(g, r, m.bloom);
    if (wake < 1.f)
      for (const ArtLayer& bin : mBin)
        bin.Draw(g, r, 1.f - wake);
    if (wake > 0.f)
      for (int k = 0; k < kBins; ++k)
      {
        const IMatrix spin = AboutPoint(cx, cy, binDeg[k]);
        mBin[k].DrawXform(g, r, spin, wake);
        const float lift = std::min(1.f, m.bloom + kBinLift[k] * m.energy + 0.2f * m.pick) * wake;
        if (lift > 0.02f)
          mBin[k].DrawXform(g, r, spin, lift, true);
      }
    if (wake > 0.f)
    {
      LightStars(m, cx, cy, binRad, wake);
      DrawStars(g);
    }
    DrawNovae(g, m, cx, cy, binRad, wake);
    DrawFryetteCore(g, r);
    const float breathe = 0.5f + 0.5f * std::sin(static_cast<float>(std::fmod(m.clock * 1.3, kTwoPi)));
    const float glow = std::min(1.f, wake * (0.3f + 0.4f * m.energy + 0.15f * m.energy * breathe) + 0.5f * m.pick);
    BloomAdd(g, cx, cy, R * (0.08f + 0.05f * m.energy + 0.06f * m.pick), kGold, 0.4f, glow);
    GlowDotAdd(g, kGold, kGoldHi, cx, cy, 4.f + 3.f * m.pick, 8.f + 6.f * m.energy, 0.8f * glow);
  }

  void DropLayers() override
  {
    mUnder.Reset();
    for (ArtLayer& bin : mBin)
      bin.Reset();
    mRest.Reset();
  }

private:
  static constexpr int kBins = 3;
  static constexpr int kTiers = 3;
  static constexpr int kDiscStars = 64;
  static constexpr int kKnots = 18;
  static constexpr double kTwoPi = 6.283185307179586;
  static constexpr double kPatternDegPerSecond = 9.0;
  static constexpr double kPatternRadPerSecond = kPatternDegPerSecond * 3.141592653589793 / 180.0;
  // Kept small: the rings meet at a seam, and a larger twist shows a kink there.
  static constexpr float kWindDeg[kBins] = {-5.f, -1.5f, 1.5f};
  static constexpr float kBinLift[kBins] = {0.5f, 0.36f, 0.24f};

  struct DiscStar
  {
    float rr, theta0, armTheta, size;
    double omega;
    int bin;
    uint32_t id;
  };

  struct Knot
  {
    float theta, rr, freq, phase, size;
    int bin;
  };

  struct Lit
  {
    float x, y, r, a;
  };

  void PlaceStars(const IRECT& r)
  {
    const float maxR = std::min(r.W(), r.H()) * 0.46f;
    const float PIf = 3.14159f;
    mMaxR = maxR;
    mDisc.clear();
    for (int i = 0; i < kDiscStars; ++i)
    {
      const uint32_t id = static_cast<uint32_t>(i);
      const float rho = 0.12f + 0.84f * std::sqrt(Hash01(id * 2654435761u ^ 0x51u));
      const float tArm = std::pow(rho, 1.f / 0.7f); // the arm's t at this radius
      DiscStar s;
      s.rr = maxR * rho;
      s.theta0 = 2.f * PIf * Hash01(id * 2654435761u ^ 0x52u);
      s.armTheta = tArm * 4.2f * PIf;
      s.size = 0.6f + 0.5f * Hash01(id * 2654435761u ^ 0x53u);
      // Flat rotation curve: inside ~0.6 maxR the stars overtake the arms, outside they lag.
      s.omega = kPatternRadPerSecond * 0.85 / (static_cast<double>(rho) + 0.25);
      s.bin = std::min(2, static_cast<int>(tArm * 3.f));
      s.id = id + 101u;
      mDisc.push_back(s);
    }
    mKnots.clear();
    for (int j = 0; j < kKnots; ++j)
    {
      const uint32_t id = static_cast<uint32_t>(j);
      const float u = 0.16f + 0.8f * Hash01(id * 2654435761u ^ 0x61u);
      Knot kn;
      kn.theta = static_cast<float>(j & 1) * PIf + u * 4.2f * PIf + 0.05f * (Hash01(id * 2654435761u ^ 0x62u) - 0.5f);
      kn.rr = maxR * std::pow(u, 0.7f) * (1.f + 0.08f * (Hash01(id * 2654435761u ^ 0x63u) - 0.5f));
      kn.freq = 1.3f + 1.1f * Hash01(id * 2654435761u ^ 0x64u);
      kn.phase = 2.f * PIf * Hash01(id * 2654435761u ^ 0x65u);
      kn.size = 1.f + 0.5f * (1.f - u);
      kn.bin = std::min(2, static_cast<int>(u * 3.f));
      mKnots.push_back(kn);
    }
    mLit.reserve(kDiscStars + kKnots);
    mDots.reserve(3 * (kDiscStars + kKnots));
  }

  void LightStars(const ArtMotion& m, float cx, float cy, const float* binRad, float wake)
  {
    mLit.clear();
    const float tf = static_cast<float>(m.clock);
    const float disc = wake * (0.35f + 0.65f * m.energy);
    for (const DiscStar& s : mDisc)
    {
      const float ang = s.theta0 - static_cast<float>(std::fmod(m.clock * s.omega, kTwoPi));
      // cos(2d): the two arms are pi apart, so either one lights the star.
      const float d = ang - (s.armTheta + binRad[s.bin]);
      const float crest = std::pow(0.5f + 0.5f * std::cos(2.f * d), 20.f);
      const float a = disc * (0.22f + 0.78f * crest) * (0.8f + 0.4f * Noise1(tf * 0.9f, s.id));
      if (a > 0.02f)
        mLit.push_back({cx + s.rr * std::cos(ang), cy + s.rr * std::sin(ang), s.size + 0.5f * crest, std::min(a, 1.f)});
    }
    const float knotAmp = wake * (0.3f + 0.7f * m.energy);
    for (const Knot& kn : mKnots)
    {
      const float sw =
        std::sin(static_cast<float>(std::fmod(m.clock * static_cast<double>(kn.freq), kTwoPi)) + kn.phase);
      const float a = sw > 0.f ? knotAmp * sw * sw * sw * sw : 0.f;
      if (a <= 0.02f)
        continue;
      const float ang = kn.theta + binRad[kn.bin];
      mLit.push_back({cx + kn.rr * std::cos(ang), cy + kn.rr * std::sin(ang), kn.size, std::min(a, 1.f)});
    }
  }

  // Lit stars in three brightness tiers: a halo fill (upper tiers) and a core fill per tier.
  void DrawStars(IGraphics& g)
  {
    static const IColor kStarWhite(255, 236, 244, 255);
    const IColor tierCol[kTiers] = {Mix(kMid, kBlue, 0.5f), kBlue, Mix(kBlue, kStarWhite, 0.55f)};
    for (int k = 0; k < kTiers; ++k)
    {
      const float lo = static_cast<float>(k) / kTiers, hi = static_cast<float>(k + 1) / kTiers;
      for (int pass = k == 0 ? 1 : 0; pass < 2; ++pass)
      {
        mDots.clear();
        for (const Lit& l : mLit)
          if (l.a > lo && l.a <= hi)
          {
            mDots.push_back(l.x);
            mDots.push_back(l.y);
            mDots.push_back(pass == 0 ? 2.8f * l.r : l.r);
          }
        const int n = static_cast<int>(mDots.size() / 3);
        const IBlend add(EBlend::Add, pass == 0 ? 0.45f * hi : 0.9f * hi);
        BatchDots(g, tierCol[k], mDots.data(), n, &add);
      }
    }
  }

  // One nova per recent pick, on a hashed point of an arm. It rides its ring: live
  // pose at wake, rest pose at 1 - wake, like the arms themselves.
  void DrawNovae(IGraphics& g, const ArtMotion& m, float cx, float cy, const float* binRad, float wake)
  {
    const float PIf = 3.14159f;
    const float maxR = mMaxR;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float env = FryetteNovaEnvelope(m.onsetAge[k]);
      if (env <= 0.004f || m.onsets < static_cast<uint32_t>(k + 1))
        continue;
      const uint32_t pickId = m.onsets - static_cast<uint32_t>(k);
      const float h1 = Hash01(pickId * 747796405u + m.seed);
      const float h2 = Hash01(pickId * 2654435761u ^ 0x71u);
      const float h3 = Hash01(pickId * 2654435761u ^ 0x72u);
      const float h4 = Hash01(pickId * 2654435761u ^ 0x73u);
      const float u = 0.28f + 0.62f * h1;
      const float theta = (h2 < 0.5f ? 0.f : PIf) + u * 4.2f * PIf;
      const float rr = maxR * std::pow(u, 0.7f) * (1.f + 0.08f * (h3 - 0.5f));
      const IColor& tint = h4 > 0.8f ? kGoldHi : kBlue;
      const float ang = theta + binRad[std::min(2, static_cast<int>(u * 3.f))];
      DrawFryetteNova(g, cx + rr * std::cos(ang), cy + rr * std::sin(ang), tint, env, env * wake);
      DrawFryetteNova(g, cx + rr * std::cos(theta), cy + rr * std::sin(theta), tint, env, env * (1.f - wake));
    }
  }

  ArtLayer mUnder;
  ArtLayer mBin[kBins];
  ArtLayer mRest;
  float mMaxR = 0.f;
  std::vector<DiscStar> mDisc;
  std::vector<Knot> mKnots;
  std::vector<Lit> mLit;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeFryetteAnimator()
{
  return std::make_unique<FryetteAnimator>();
}
} // namespace volumart::art
