#pragma once

// Orange ORS100 (amp 11, fractal case 10): "Julia Nebula".
// Hero art: Orange ORS100 - Julia Nebula (compact Julia, teal with only faint warm tips, no centre dot)
// PLAY motion (class A, palette cycling): light flows inward along the Julia set's
// escape-time contours toward the filaments, warming as it arrives, and the filament
// tips glint as each crest reaches them. A pick sends a fast surge of rings inward and
// flashes the filament edge warm. Silence: the static nebula.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawOrangeOrs100Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
  Bloom(g, cx, cy, R * 0.5f, kTeal, 0.12f);
  unsigned ds = 2u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 70); // stars kept inside the art footprint
  const float step = 2.f;
  for (float px = 0; px < pw; px += step)
    for (float py = 0; py < ph; py += step)
    {
      double zr = (px / pw - 0.5) * 3.0, zi = (py / ph - 0.5) * 2.4; int it = 0;
      while (zr * zr + zi * zi < 4.0 && it < 32) { double t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++; }
      if (it < 32 && it > 2) { const float f = (float)it / 32.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.3f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
    }
  // clang-format on
}

class OrangeOrs100Animator final : public AdditiveArtAnimator<&DrawOrangeOrs100Hero>
{
  using Base = AdditiveArtAnimator<&DrawOrangeOrs100Hero>;

public:
  // The full art and the escape-time grid come first, then one band layer per call,
  // so entering PLAY never builds all six layers in one frame.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    const bool fullReady = mFull.Ok(g);
    Base::Prepare(g, owner, r);
    if (!fullReady)
      return false;
    for (int k = 0; k < kBandLayers; ++k)
      if (!mBands[k].Ok(g))
      {
        mBands[k].Build(g, owner, r, [&] { PaintBand(g, k); });
        return k == kBandLayers - 1;
      }
    return true;
  }

  void DropLayers() override
  {
    Base::DropLayers();
    for (ArtLayer& band : mBands)
      band.Reset();
  }

protected:
  static constexpr int kLevels = 32; // the hero's iteration cap
  static constexpr int kFirstLevel = 3;
  static constexpr int kCycleBands = 4;
  static constexpr int kEdgeBand = kCycleBands;
  static constexpr int kBandLayers = kCycleBands + 1;
  static constexpr int kEdgeLevel = 12;
  static constexpr int kMaxGlints = 140;
  static constexpr int kTiers = 3;
  static constexpr double kCycleRadPerSecond = 6.283185307179586 / 3.2;
  static constexpr float kSurgeRadPerSecond = 6.2831853f * 2.f;
  static constexpr float kQuarter = 1.5707963f;

  // The hero's own step-2 grid and escape loop, sampled once; each drawn dot lands in
  // the list of its iteration count, at the hero's exact centre.
  void PrepareExtras(const IRECT& r) override
  {
    if (mSampled)
      return;
    mSampled = true;
    const float cx = r.MW(), cy = r.MH(), w = r.W(), h = r.H();
    // clang-format off
    const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
    const float step = 2.f;
    for (float px = 0; px < pw; px += step)
      for (float py = 0; py < ph; py += step)
      {
        double zr = (px / pw - 0.5) * 3.0, zi = (py / ph - 0.5) * 2.4; int it = 0;
        while (zr * zr + zi * zi < 4.0 && it < 32) { double t = zr * zr - zi * zi - 0.7; zi = 2 * zr * zi + 0.27015; zr = t; it++; }
        if (it < 32 && it > 2) { mLevelXY[it].push_back(pl + px); mLevelXY[it].push_back(pt + py); }
      }
    // clang-format on
    FindGlints();
  }

  // The contours step back a little so the flowing crests read against darker troughs.
  float BaseDim(const ArtMotion& m) const override { return 0.22f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    const float wake = m.Wake();
    const float cycle = static_cast<float>(std::fmod(m.clock * kCycleRadPerSecond, 6.283185307179586));
    const float flow = wake * (0.36f + 0.3f * m.energy);
    float surge[kOnsetSlots], surgePhase[kOnsetSlots];
    for (int j = 0; j < kOnsetSlots; ++j)
    {
      const float age = m.onsetAge[j];
      const float var = Hash01((m.onsets - static_cast<uint32_t>(j)) * 747796405u + m.seed);
      surge[j] = PickFade(age) * (0.45f + 0.2f * var);
      surgePhase[j] = PickFade(age) > 0.f ? age * kSurgeRadPerSecond * (0.85f + 0.3f * var) : 0.f;
    }
    // Iteration level L sits in band L % 4, so a crest stepping through the bands in
    // order walks every contour inward. Additive light adds w^2: summing the squares of
    // (0.5 + 0.5 cos) over four quarter-phased bands is constant, so the wave travels
    // without the whole nebula pulsing.
    for (int k = 0; k < kCycleBands; ++k)
    {
      const float s = flow * Crest(cycle - kQuarter * k);
      float e2 = s * s;
      for (int j = 0; j < kOnsetSlots; ++j)
      {
        const float v = surge[j] * Crest(surgePhase[j] - kQuarter * k);
        e2 += v * v;
      }
      mBands[k].DrawAdd(g, r, std::min(1.f, std::sqrt(e2)));
    }
    const float fresh = PickFade(m.onsetAge[0]);
    const float glow = wake * (0.22f + 0.28f * m.energy), strike = 0.8f * m.pick, after = 0.5f * fresh;
    mBands[kEdgeBand].DrawAdd(g, r, std::min(1.f, std::sqrt(glow * glow + strike * strike + after * after)));
    DrawGlints(g, m, cycle, flow, fresh);
  }

private:
  struct Glint
  {
    float x, y, level;
    uint32_t id;
  };

  struct Lit
  {
    float x, y, a;
  };

  static float Crest(float phase) { return 0.5f + 0.5f * std::cos(phase); }

  // A pick's afterglow: eases in over 60 ms, gone by kMaxEventAge.
  static float PickFade(float age)
  {
    if (!(age < kMaxEventAge))
      return 0.f;
    const float left = 1.f - age / kMaxEventAge;
    return Smoothstep(0.f, 0.06f, age) * left * left;
  }

  void FillLevel(IGraphics& g, int level, const IColor& c) const
  {
    const std::vector<float>& xy = mLevelXY[level];
    if (xy.size() < 2)
      return;
    g.PathClear();
    for (size_t i = 0; i + 1 < xy.size(); i += 2)
      g.PathCircle(xy[i], xy[i + 1], 2.f * 0.8f);
    g.PathFill(c, IFillOptions(), nullptr);
  }

  // Cycle bands: deep teal light that warms toward the filaments. Edge band: every
  // dot near the set boundary in a warmer ink, for the pick flash.
  void PaintBand(IGraphics& g, int k) const
  {
    const IColor flowInk(255, 90, 196, 212);
    for (int level = kFirstLevel; level < kLevels; ++level)
    {
      if (k == kEdgeBand)
      {
        if (level >= kEdgeLevel)
        {
          const float deep = static_cast<float>(level - kEdgeLevel) / static_cast<float>(kLevels - 1 - kEdgeLevel);
          FillLevel(g, level, WithA(Mix(kTeal, kGold, 0.3f + 0.4f * deep), 0.85f));
        }
      }
      else if (level % kCycleBands == k)
        FillLevel(g, level, WithA(Mix(flowInk, kGold, 0.45f * level / static_cast<float>(kLevels)), 0.8f));
    }
  }

  // The deepest dots of the filament edge (closest to the Julia set), with a hash so
  // they spread along the whole boundary instead of the few deepest pockets.
  void FindGlints()
  {
    struct Candidate
    {
      float score;
      Glint glint;
    };
    std::vector<Candidate> cands;
    for (int level = kEdgeLevel; level < kLevels; ++level)
    {
      const std::vector<float>& xy = mLevelXY[level];
      for (size_t i = 0; i + 1 < xy.size(); i += 2)
      {
        const uint32_t id = static_cast<uint32_t>(level) * 65536u + static_cast<uint32_t>(i / 2);
        const float score = static_cast<float>(level) + 6.f * Hash01(id * 2654435761u ^ 10u);
        cands.push_back({score, {xy[i], xy[i + 1], static_cast<float>(level), id}});
      }
    }
    if (static_cast<int>(cands.size()) > kMaxGlints)
    {
      std::nth_element(cands.begin(), cands.begin() + kMaxGlints, cands.end(),
                       [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
      cands.erase(cands.begin() + kMaxGlints, cands.end());
    }
    mGlints.clear();
    for (const Candidate& c : cands)
      mGlints.push_back(c.glint);
    mLit.reserve(mGlints.size());
    mDots.reserve(3 * mGlints.size());
  }

  // A glint lights as the crest reaches its contour; a pick flares a different half of
  // them each time.
  void DrawGlints(IGraphics& g, const ArtMotion& m, float cycle, float flow, float fresh)
  {
    const uint32_t pickSalt = m.onsets * 747796405u + m.seed;
    const float flare = 0.9f * m.pick + 0.7f * fresh;
    const float t = static_cast<float>(m.clock);
    mLit.clear();
    for (const Glint& gl : mGlints)
    {
      const float s = Crest(cycle - kQuarter * gl.level);
      float a = 1.3f * flow * s * s * (0.6f + 0.8f * Noise1(t * 2.5f, gl.id));
      if (flare > 0.f && Hash01(gl.id * 2654435761u ^ pickSalt) < 0.55f)
        a += flare;
      if (a > 0.03f)
        mLit.push_back({gl.x, gl.y, std::min(a, 1.f)});
    }
    if (mLit.empty())
      return;
    const IColor halo = Mix(kTeal, kGoldHi, 0.45f);
    static const IColor kHot(255, 255, 244, 210);
    for (int k = 0; k < kTiers; ++k)
    {
      const float lo = static_cast<float>(k) / kTiers, hi = static_cast<float>(k + 1) / kTiers;
      for (int pass = 0; pass < 2; ++pass)
      {
        mDots.clear();
        for (const Lit& l : mLit)
          if (l.a > lo && l.a <= hi)
          {
            mDots.push_back(l.x);
            mDots.push_back(l.y);
            mDots.push_back(pass == 0 ? 3.f : 1.1f);
          }
        const int n = static_cast<int>(mDots.size() / 3);
        const IBlend add(EBlend::Add, pass == 0 ? 0.3f + 0.35f * hi : std::min(1.f, 0.45f + 0.5f * hi));
        BatchDots(g, pass == 0 ? halo : kHot, mDots.data(), n, &add);
      }
    }
  }

  ArtLayer mBands[kBandLayers];
  std::vector<float> mLevelXY[kLevels];
  bool mSampled = false;
  std::vector<Glint> mGlints;
  std::vector<Lit> mLit;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeOrangeOrs100Animator()
{
  return std::make_unique<OrangeOrs100Animator>();
}
} // namespace volumart::art
