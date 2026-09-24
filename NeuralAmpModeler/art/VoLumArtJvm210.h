#pragma once

// Marshall JVM 210H (amp 9, fractal case 8): "Levy Nebula".
// Hero art: Marshall JVM 210H - Levy Nebula (Levy C over bloom+dust, gradient stroke into gold, glow)
// PLAY motion (class A): the recursion made visible. Eight comets run in lockstep through the
// curve's eight level-3 self-copies, teal into gold, and the seams between the copies flash each
// time all eight cross them together; the level stretches the trails. Each pick re-folds the
// curve from its base chord, one Levy fold per step, until the light melts back into the curve.
// A PLAY Dual lane is narrower than the curve: there the whole art is fitted into it.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawJvm210Art(IGraphics& g, const IRECT& rect)
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

// DrawJvm210Art's depth-11 Levy C curve, unscaled: 2049 points.
inline std::vector<std::pair<float, float>> Jvm210Levy()
{
  struct Seg
  {
    float x1, y1, x2, y2;
  };
  std::vector<Seg> segs;
  segs.push_back({-1.f, 0.f, 1.f, 0.f});
  for (int depth = 0; depth < 11; depth++)
  {
    std::vector<Seg> next;
    next.reserve(2 * segs.size());
    for (auto& s : segs)
    {
      const float mx = (s.x1 + s.x2) / 2.f + (s.y2 - s.y1) / 2.f, my = (s.y1 + s.y2) / 2.f - (s.x2 - s.x1) / 2.f;
      next.push_back({s.x1, s.y1, mx, my});
      next.push_back({mx, my, s.x2, s.y2});
    }
    segs.swap(next);
  }
  std::vector<std::pair<float, float>> raw;
  raw.reserve(segs.size() + 1);
  raw.push_back({segs[0].x1, segs[0].y1});
  for (auto& s : segs)
    raw.push_back({s.x2, s.y2});
  return raw;
}

// How far the PLAY light reaches past the curve: a comet head's halo at full level and pick.
inline constexpr float kJvm210Reach = 10.f;

// The curve as DrawJvm210Art scales it (its longer side 1.25 R about the centre) plus the
// light's reach; bloom and dust are soft and may run past the lane.
inline ArtBox Jvm210Extent(const IRECT& rect)
{
  float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
  for (const auto& p : Jvm210Levy())
  {
    mnx = std::min(mnx, p.first);
    mny = std::min(mny, p.second);
    mxx = std::max(mxx, p.first);
    mxy = std::max(mxy, p.second);
  }
  const float ext = std::max(mxx - mnx, mxy - mny);
  const float S = std::min(rect.W(), rect.H()) * 1.25f / (ext > 0.f ? ext : 1.f);
  const float hw = 0.5f * (mxx - mnx) * S, hh = 0.5f * (mxy - mny) * S;
  return {rect.MW() - hw - kJvm210Reach, rect.MH() - hh - kJvm210Reach, rect.MW() + hw + kJvm210Reach,
          rect.MH() + hh + kJvm210Reach};
}

inline ArtLaneFit Jvm210Fit(const IRECT& rect)
{
  if (!IsNarrowLane(rect.W(), rect.H()))
    return {};
  return LaneFit(ArtBoxOf(rect), Jvm210Extent(rect), kLaneFitMargin, 0.5f);
}

inline void DrawJvm210Hero(IGraphics& g, const IRECT& rect)
{
  LaneFitted(g, Jvm210Fit(rect), [&] { DrawJvm210Art(g, rect); });
}

inline IColor Jvm210White()
{
  return IColor(255, 240, 250, 252);
}

class Jvm210Animator final : public AdditiveArtAnimator<&DrawJvm210Hero>
{
protected:
  static constexpr int kSegs = 2048; // the hero's depth-11 curve
  static constexpr int kCopies = 8; // its level-3 self-copies, end to end along the curve
  static constexpr int kCopySegs = kSegs / kCopies;
  static constexpr int kBins = 4;
  static constexpr int kTiers = 4;
  static constexpr int kMaxFoldLevel = 8; // folding level 8 into 9: a 512-segment polygon
  static constexpr float kFoldDepth = 9.f;
  static constexpr float kFoldEnd = 1.3f;
  static constexpr float kSegsPerSecond = 160.f;

  // Same curve and transform as DrawJvm210Art; in a Dual lane the extras draw under the
  // same lane fit as the hero.
  void PrepareExtras(const IRECT& r) override
  {
    mFit = Jvm210Fit(r);
    const float cx = r.MW(), cy = r.MH(), w = r.W(), h = r.H();
    const float R = std::min(w, h);
    const std::vector<std::pair<float, float>> raw = Jvm210Levy();
    float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
    for (auto& p : raw)
    {
      mnx = std::min(mnx, p.first);
      mny = std::min(mny, p.second);
      mxx = std::max(mxx, p.first);
      mxy = std::max(mxy, p.second);
    }
    const float bcx = (mnx + mxx) / 2.f, bcy = (mny + mxy) / 2.f, ext = std::max(mxx - mnx, mxy - mny);
    const float S = R * 1.25f / (ext > 0.f ? ext : 1.f);
    if (static_cast<int>(raw.size()) != kSegs + 1)
    {
      mP.clear();
      return;
    }
    mP.resize(2 * raw.size());
    for (size_t i = 0; i < raw.size(); ++i)
    {
      mP[2 * i] = cx + (raw[i].first - bcx) * S;
      mP[2 * i + 1] = cy + (raw[i].second - bcy) * S;
    }
    mPath.reserve(2 * (2 * (1 << kMaxFoldLevel) + 1));
    mDots.reserve(3 * (1 << kMaxFoldLevel));
  }

  // The curve steps back a little so the light running through it reads.
  float BaseDim(const ArtMotion& m) const override { return 0.25f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    (void)r;
    if (mP.empty())
      return;
    LaneFitted(g, mFit, [&] {
      DrawComets(g, m);
      DrawFolds(g, m);
    });
  }

private:
  static IColor BinColour(int bin) { return Mix(Mix(kTeal, kGold, (bin + 0.5f) / kBins), Jvm210White(), 0.12f); }

  // One comet per level-3 copy, all at the same place in their copies. The copies are
  // congruent (turned in 90-degree steps), so the eight trace the same figure at once.
  void DrawComets(IGraphics& g, const ArtMotion& m)
  {
    const float wake = m.Wake();
    const float amp = std::min(1.f, wake * (0.5f + 0.5f * m.energy) + 0.35f * m.pick);
    if (amp <= 0.f)
      return;
    const float head = static_cast<float>(std::fmod(m.clock * kSegsPerSecond, static_cast<double>(kCopySegs)));
    const float trail = 32.f + 56.f * m.energy + 40.f * m.pick;
    // Butt caps: tiers meet end to end, so their joints do not add up into beads.
    IStrokeOptions tierStroke = RoundStroke();
    tierStroke.mCapOption = ELineCap::Butt;
    const IBlend glow(EBlend::Add, 0.42f * amp);
    const IBlend halo(EBlend::Add, 0.5f * amp);
    for (int bin = 0; bin < kBins; ++bin)
    {
      const IColor col = BinColour(bin);
      const int c0 = bin * kCopies / kBins, c1 = (bin + 1) * kCopies / kBins;
      g.PathClear();
      for (int c = c0; c < c1; ++c)
        PathRun(g, c * kCopySegs + head - trail, c * kCopySegs + head);
      g.PathStroke(col, 7.f + 4.f * m.pick, RoundStroke(), &glow);
      for (int k = 0; k < kTiers; ++k)
      {
        const float f0 = static_cast<float>(k) / kTiers, f1 = static_cast<float>(k + 1) / kTiers;
        const IBlend add(EBlend::Add, std::min(0.9f, amp * std::pow(f1, 1.3f) * (0.85f + 0.3f * m.pick)));
        g.PathClear();
        for (int c = c0; c < c1; ++c)
        {
          const float tail = c * kCopySegs + head - trail;
          PathRun(g, tail + trail * f0, tail + trail * f1);
        }
        g.PathStroke(col, 1.8f + 1.4f * f1, tierStroke, &add);
      }
      mDots.clear();
      for (int c = c0; c < c1; ++c)
      {
        float x = 0.f, y = 0.f;
        At(c * kCopySegs + head, x, y);
        mDots.push_back(x);
        mDots.push_back(y);
        mDots.push_back(3.5f + 2.5f * m.energy + 4.f * m.pick);
      }
      BatchDots(g, col, mDots.data(), c1 - c0, &halo);
    }
    mDots.clear();
    for (int c = 0; c < kCopies; ++c)
    {
      float x = 0.f, y = 0.f;
      At(c * kCopySegs + head, x, y);
      mDots.push_back(x);
      mDots.push_back(y);
      mDots.push_back(1.3f + 1.5f * m.pick);
      if (m.pick > 0.01f)
        BloomAdd(g, x, y, 12.f + 14.f * m.pick, BinColour(c * kBins / kCopies), 0.6f, m.pick);
    }
    const IBlend core(EBlend::Add, amp);
    BatchDots(g, Jvm210White(), mDots.data(), kCopies, &core);

    // The nine seams between the copies (ends included) flash as the eight heads cross them.
    const float into = 1.f - (kCopySegs - head) / 12.f;
    const float beat = std::max(std::exp(-head / 20.f), into > 0.f ? into * into : 0.f);
    const float jw = amp * beat * (0.55f + 0.45f * m.energy);
    if (jw <= 0.01f)
      return;
    const IBlend jHalo(EBlend::Add, std::min(1.f, 0.6f * jw));
    for (int bin = 0; bin < kBins; ++bin)
    {
      mDots.clear();
      for (int j = 0; j <= kCopies; ++j)
        if (std::min(kBins - 1, j * kBins / kCopies) == bin)
        {
          mDots.push_back(mP[2 * j * kCopySegs]);
          mDots.push_back(mP[2 * j * kCopySegs + 1]);
          mDots.push_back(5.f + 4.f * m.energy);
        }
      BatchDots(g, BinColour(bin), mDots.data(), static_cast<int>(mDots.size() / 3), &jHalo);
    }
    mDots.clear();
    for (int j = 0; j <= kCopies; ++j)
    {
      mDots.push_back(mP[2 * j * kCopySegs]);
      mDots.push_back(mP[2 * j * kCopySegs + 1]);
      mDots.push_back(1.8f);
    }
    const IBlend jCore(EBlend::Add, std::min(1.f, jw));
    BatchDots(g, Jvm210White(), mDots.data(), kCopies + 1, &jCore);
  }

  // A pick re-folds the curve. A newer pick takes over from an older fold within 0.2 s;
  // three picks cannot land inside 0.2 s (onset refractory), so the fold that drops out
  // of the last onset slot is already dark. Picks in quick succession fold dimmer.
  void DrawFolds(IGraphics& g, const ArtMotion& m)
  {
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kFoldEnd)
        continue;
      const float older = k + 1 < kOnsetSlots ? m.onsetAge[k + 1] : kNoOnset;
      float w = Smoothstep(0.f, 0.12f, age) * (1.f - Smoothstep(0.8f, kFoldEnd, age));
      w *= 0.2f + 0.8f * Smoothstep(0.12f, 0.5f, older - age);
      if (k > 0)
        w *= 1.f - Smoothstep(0.f, 0.2f, m.onsetAge[k - 1]);
      if (w <= 0.004f)
        continue;
      const float level = std::min(kFoldDepth - 0.001f, kFoldDepth * std::pow(age / 1.1f, 0.85f));
      const float tint = Hash01((m.onsets - static_cast<uint32_t>(k)) * 747796405u + m.seed);
      DrawFold(g, level, w, Mix(kGold, kTeal, 0.35f * tint));
    }
  }

  // The level-n Levy polygon (the curve's own points at every 2048 / 2^n) with each
  // segment's crease pushed from its midpoint out to the next level's vertex.
  void DrawFold(IGraphics& g, float level, float w, const IColor& col)
  {
    static const IColor kHot(255, 255, 240, 200);
    const int lev = std::min(kMaxFoldLevel, static_cast<int>(level));
    const float s = Smoothstep(0.f, 1.f, level - static_cast<float>(lev));
    const int n = 1 << lev, step = kSegs >> lev;
    const float crease = 1.f - Smoothstep(3.f, 6.f, level);
    mPath.clear();
    mDots.clear();
    for (int j = 0; j < n; ++j)
    {
      const int a = j * step, b = a + step, c = a + step / 2;
      const float mx = 0.5f * (mP[2 * a] + mP[2 * b]), my = 0.5f * (mP[2 * a + 1] + mP[2 * b + 1]);
      const float vx = mx + (mP[2 * c] - mx) * s, vy = my + (mP[2 * c + 1] - my) * s;
      mPath.push_back(mP[2 * a]);
      mPath.push_back(mP[2 * a + 1]);
      mPath.push_back(vx);
      mPath.push_back(vy);
      if (crease > 0.f)
      {
        mDots.push_back(vx);
        mDots.push_back(vy);
        mDots.push_back(3.5f);
      }
    }
    mPath.push_back(mP[2 * kSegs]);
    mPath.push_back(mP[2 * kSegs + 1]);
    const int pts = static_cast<int>(mPath.size() / 2);
    const IBlend glow(EBlend::Add, 0.4f * w);
    BatchPolyline(g, col, 6.f, mPath.data(), pts, &glow);
    const IBlend core(EBlend::Add, std::min(1.f, 0.95f * w));
    BatchPolyline(g, col, 1.7f, mPath.data(), pts, &core);
    if (crease <= 0.f)
      return;
    const IBlend creaseHalo(EBlend::Add, std::min(1.f, 0.6f * w * crease));
    BatchDots(g, col, mDots.data(), n, &creaseHalo);
    for (int j = 0; j < n; ++j)
      mDots[3 * j + 2] = 1.2f;
    const IBlend creaseCore(EBlend::Add, std::min(1.f, w * crease));
    BatchDots(g, kHot, mDots.data(), n, &creaseCore);
  }

  void At(float s, float& x, float& y) const
  {
    const int i0 = std::clamp(static_cast<int>(s), 0, kSegs - 1);
    const float f = s - static_cast<float>(i0);
    x = mP[2 * i0] + (mP[2 * i0 + 2] - mP[2 * i0]) * f;
    y = mP[2 * i0 + 1] + (mP[2 * i0 + 3] - mP[2 * i0 + 1]) * f;
  }

  // Sub-path(s) along the curve from sample s0 to s1 (s0 < s1 < kSegs, s1 - s0 < kSegs). A
  // run that starts before the curve's first point continues from its last: the comet that
  // just left the gold end still fades there while its successor enters the teal end.
  void PathRun(IGraphics& g, float s0, float s1) const
  {
    const float N = static_cast<float>(kSegs);
    if (s1 <= 0.f)
      PathSpan(g, s0 + N, s1 + N);
    else if (s0 < 0.f)
    {
      PathSpan(g, s0 + N, N);
      PathSpan(g, 0.f, s1);
    }
    else
      PathSpan(g, s0, s1);
  }

  void PathSpan(IGraphics& g, float s0, float s1) const
  {
    if (s1 - s0 < 1e-3f)
      return;
    float x = 0.f, y = 0.f;
    At(s0, x, y);
    g.PathMoveTo(x, y);
    for (float s = std::floor(s0) + 1.f; s < s1; s += 1.f)
    {
      At(s, x, y);
      g.PathLineTo(x, y);
    }
    At(s1, x, y);
    g.PathLineTo(x, y);
  }

  ArtLaneFit mFit;
  std::vector<float> mP; // the hero's 2049 curve points, x y interleaved
  std::vector<float> mPath;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeJvm210Animator()
{
  return std::make_unique<Jvm210Animator>();
}
} // namespace volumart::art
