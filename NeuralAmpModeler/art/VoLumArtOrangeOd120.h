#pragma once

// Orange OD120 (amp 10, fractal case 9): "Ember Bulb".
// Hero art: Orange OD120 - Ember Bulb (Mandelbrot bulb, teal-dominant with only faintly warm tips, no gold embers)
// PLAY motion (class A, stays teal): the bulb smoulders. Embers crawl along the coast of
// the dark bulbs, hottest under a slow heat glow that roams the field; the level sets how
// much of the coast burns and how far the heat roams. A pick flashes the field and sends a
// ripple of light out from the bulbs along the Mandelbrot's own escape-time contours.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawOrangeOd120Hero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
  Bloom(g, cx, cy, R * 0.55f, kTeal, 0.12f);
  unsigned ds = 6u; Dust(g, ds, cx, cy, std::min(pw, ph) * 0.5f, 90); // stars kept inside the art footprint
  const float step = 2.f;
  for (float px = 0; px < pw; px += step)
    for (float py = 0; py < ph; py += step)
    {
      double cr = -0.745 + (px / pw - 0.5) * 0.01, ci = 0.186 + (py / ph - 0.5) * 0.01;
      double zr = 0, zi = 0; int it = 0;
      while (zr * zr + zi * zi < 4.0 && it < 40) { double t = zr * zr - zi * zi + cr; zi = 2 * zr * zi + ci; zr = t; it++; }
      if (it < 40 && it > 2) { const float f = (float)it / 40.f; g.FillCircle(WithA(Mix(kTeal, kGold, f * 0.28f), 0.25f + 0.6f * f), pl + px, pt + py, step * 0.8f); }
    }
  // clang-format on
}

class OrangeOd120Animator final : public AdditiveArtAnimator<&DrawOrangeOd120Hero>
{
  struct Ember
  {
    float x, y;
    uint32_t id;
  };
  struct Contour
  {
    float x, y, grain;
    int level;
    uint32_t id;
  };
  struct Lit
  {
    float x, y, a;
  };

protected:
  static constexpr int kMaxIter = 40; // the hero's escape limit
  static constexpr int kMinLevel = 18; // the outermost contour the field has
  static constexpr int kMaxCoast = 640;
  static constexpr int kMaxContour = 1800;
  static constexpr int kMaxEmbersLit = 240;
  static constexpr int kMaxRippleLit = 300;
  static constexpr int kTiers = 3;
  static constexpr float kRippleLife = 1.3f;
  static constexpr float kFrontStart = 39.5f, kFrontTravel = 22.f; // escape-time levels
  static constexpr float kAhead = 1.2f, kBehind = 3.5f;

  // The hero's grid and formula on every other base sample. Coast = escaping samples
  // touching the set (the rims of the dark bulbs); contour = samples with a neighbour
  // that escapes sooner, i.e. the outer edge of their escape-time band.
  void PrepareExtras(const IRECT& r) override
  {
    const float cx = r.MW(), cy = r.MH(), w = r.W(), h = r.H();
    const float pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
    const float step = 2.f;
    mCx = cx;
    mCy = cy;
    mPw = pw;
    mPh = ph;
    mR = std::min(w, h);
    std::vector<float> xs, ys;
    for (float px = 0; px < pw; px += 2.f * step)
      xs.push_back(px);
    for (float py = 0; py < ph; py += 2.f * step)
      ys.push_back(py);
    const int nx = static_cast<int>(xs.size()), ny = static_cast<int>(ys.size());
    std::vector<uint8_t> esc(static_cast<size_t>(nx) * static_cast<size_t>(ny));
    for (int i = 0; i < nx; ++i)
      for (int j = 0; j < ny; ++j)
      {
        const float px = xs[static_cast<size_t>(i)], py = ys[static_cast<size_t>(j)];
        double cr = -0.745 + (px / pw - 0.5) * 0.01, ci = 0.186 + (py / ph - 0.5) * 0.01;
        double zr = 0, zi = 0;
        int it = 0;
        while (zr * zr + zi * zi < 4.0 && it < kMaxIter)
        {
          double t = zr * zr - zi * zi + cr;
          zi = 2 * zr * zi + ci;
          zr = t;
          it++;
        }
        esc[static_cast<size_t>(i) * static_cast<size_t>(ny) + static_cast<size_t>(j)] = static_cast<uint8_t>(it);
      }
    auto escAt = [&](int i, int j) {
      return static_cast<int>(esc[static_cast<size_t>(i) * static_cast<size_t>(ny) + static_cast<size_t>(j)]);
    };
    mCoast.clear();
    mContour.clear();
    const int di[4] = {-1, 1, 0, 0}, dj[4] = {0, 0, -1, 1};
    for (int i = 0; i < nx; ++i)
      for (int j = 0; j < ny; ++j)
      {
        const int v = escAt(i, j);
        if (v >= kMaxIter || v <= 2)
          continue;
        bool coast = false, contour = false;
        for (int k = 0; k < 4; ++k)
        {
          const int a = i + di[k], b = j + dj[k];
          if (a < 0 || a >= nx || b < 0 || b >= ny)
            continue;
          const int n = escAt(a, b);
          coast = coast || n >= kMaxIter;
          contour = contour || n < v;
        }
        const uint32_t id = static_cast<uint32_t>(i * ny + j) + 1u;
        const float x = pl + xs[static_cast<size_t>(i)], y = pt + ys[static_cast<size_t>(j)];
        if (coast)
          mCoast.push_back({x, y, id});
        if (contour && v >= kMinLevel)
          mContour.push_back({x, y, Hash01(id * 2654435761u ^ 23u), v, id});
      }
    // Hash order, so a cap (here, or on the lit count per frame) thins evenly.
    auto order = [](uint32_t id) { return Hash01(id * 2654435761u ^ 9u); };
    std::sort(mCoast.begin(), mCoast.end(), [&](const Ember& a, const Ember& b) { return order(a.id) < order(b.id); });
    if (static_cast<int>(mCoast.size()) > kMaxCoast)
      mCoast.resize(static_cast<size_t>(kMaxCoast));
    std::sort(
      mContour.begin(), mContour.end(), [&](const Contour& a, const Contour& b) { return order(a.id) < order(b.id); });
    if (static_cast<int>(mContour.size()) > kMaxContour)
      mContour.resize(static_cast<size_t>(kMaxContour));
    std::stable_sort(
      mContour.begin(), mContour.end(), [](const Contour& a, const Contour& b) { return a.level < b.level; });
    size_t n = 0;
    for (int L = 0; L <= kMaxIter; ++L)
    {
      while (n < mContour.size() && mContour[n].level < L)
        ++n;
      mLevelStart[L] = static_cast<int>(n);
    }
    mEmberLit.reserve(static_cast<size_t>(kMaxEmbersLit));
    mRippleLit.reserve(static_cast<size_t>(kMaxRippleLit));
    mDots.reserve(3 * static_cast<size_t>(std::max(kMaxEmbersLit, kMaxRippleLit)));
  }

  // The field sinks a little so the embers read as heat inside it.
  float BaseDim(const ArtMotion& m) const override { return 0.14f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    const float wake = m.Wake();
    const float t = static_cast<float>(m.clock);
    const float reach = 0.7f + 0.3f * m.energy;
    const float hx = mCx + mPw * 0.3f * reach * static_cast<float>(std::sin(0.29 * m.clock + 1.1));
    const float hy = mCy + mPh * 0.28f * reach * static_cast<float>(std::sin(0.41 * m.clock + 0.4));
    BloomAdd(
      g, hx, hy, mR * (0.34f + 0.06f * m.energy), Mix(kTeal, kGoldHi, 0.12f), 0.6f, wake * (0.35f + 0.4f * m.energy));
    mFull.DrawAdd(g, r, 0.3f * m.pick);
    SmoulderCoast(m, wake, t, hx, hy);
    RippleContours(m);
    DrawDots(g, mEmberLit, Mix(kTeal, kGoldHi, 0.22f), IColor(255, 226, 248, 230), 3.4f, 1.25f);
    DrawDots(g, mRippleLit, kTeal, IColor(255, 205, 246, 250), 2.8f, 0.95f);
  }

private:
  // Coast embers: heat = the roaming glow plus two drifting noise fields, so hot spots crawl
  // along the rims; the level lowers the threshold. A pick sparks a fresh random few.
  void SmoulderCoast(const ArtMotion& m, float wake, float t, float hx, float hy)
  {
    mEmberLit.clear();
    if (wake <= 0.f && m.pick <= 0.f)
      return;
    const float gain = wake * (0.5f + 0.5f * m.energy);
    const float thr = 0.62f - 0.36f * m.energy;
    const float sigma = mR * 0.24f, inv = 1.f / (2.f * sigma * sigma);
    const uint32_t flare = m.onsets * 747796405u + m.seed;
    for (const Ember& e : mCoast)
    {
      if (static_cast<int>(mEmberLit.size()) >= kMaxEmbersLit)
        return;
      const float dx = e.x - hx, dy = e.y - hy;
      const float roam = std::exp(-(dx * dx + dy * dy) * inv);
      const float crawl = std::clamp(0.5f
                                       + 0.9f
                                           * (Noise1(0.019f * e.x + 0.011f * e.y - 0.31f * t, 9u)
                                              + Noise1(0.008f * e.x - 0.017f * e.y + 0.23f * t, 21u)),
                                     0.f, 1.f);
      const float heat = 0.55f * roam + 0.5f * crawl;
      const float glow = gain * Smoothstep(thr, 1.f, heat) * (0.75f + 0.5f * Noise1(t * 3.3f, e.id));
      const float spark = Hash01(e.id * 2654435761u ^ flare);
      const float a = glow + 0.8f * m.pick * spark * spark * spark;
      if (a > 0.03f)
        mEmberLit.push_back({e.x, e.y, std::min(a, 1.f)});
    }
  }

  // Each pick's front runs from the set outward through the escape-time levels: fast off
  // the rims, slowing as the bands widen into the field; a soft wake trails it inward.
  void RippleContours(const ArtMotion& m)
  {
    mRippleLit.clear();
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kRippleLife)
        continue;
      const float u = std::max(0.f, age) / kRippleLife;
      const float front = kFrontStart - kFrontTravel * std::pow(u, 0.4f);
      const uint32_t pickId = m.onsets - static_cast<uint32_t>(k);
      const float s = (0.7f + 0.3f * Hash01(pickId * 747796405u + m.seed)) * std::pow(1.f - u, 1.3f);
      const int lo = std::max(kMinLevel, static_cast<int>(std::floor(front - kAhead)));
      const int hi = std::min(kMaxIter - 1, static_cast<int>(std::ceil(front + kBehind)));
      for (int L = lo; L <= hi; ++L)
      {
        const float d = static_cast<float>(L) - front;
        const float wgt = d < 0.f ? 1.f + d / kAhead : 1.f - d / kBehind;
        if (wgt <= 0.f)
          continue;
        const float a = s * wgt * (d < 0.f ? 1.f : wgt);
        if (a < 0.03f)
          continue;
        for (int n = mLevelStart[L]; n < mLevelStart[L + 1]; ++n)
        {
          if (static_cast<int>(mRippleLit.size()) >= kMaxRippleLit)
            return;
          const Contour& c = mContour[static_cast<size_t>(n)];
          mRippleLit.push_back({c.x, c.y, std::min(1.f, a * (0.85f + 0.3f * c.grain))});
        }
      }
    }
  }

  // One soft haze under the brightest, then halo and core fills in three brightness tiers.
  void DrawDots(IGraphics& g, const std::vector<Lit>& lit, const IColor& halo, const IColor& core, float haloR,
                float coreR)
  {
    if (lit.empty())
      return;
    mDots.clear();
    for (const Lit& l : lit)
      if (l.a > 0.5f)
      {
        mDots.push_back(l.x);
        mDots.push_back(l.y);
        mDots.push_back(haloR * 2.f);
      }
    const IBlend haze(EBlend::Add, 0.4f);
    BatchDots(g, halo, mDots.data(), static_cast<int>(mDots.size() / 3), &haze);
    for (int k = 0; k < kTiers; ++k)
    {
      const float lo = static_cast<float>(k) / kTiers, hi = static_cast<float>(k + 1) / kTiers;
      for (int pass = 0; pass < 2; ++pass)
      {
        mDots.clear();
        for (const Lit& l : lit)
          if (l.a > lo && l.a <= hi)
          {
            mDots.push_back(l.x);
            mDots.push_back(l.y);
            mDots.push_back(pass == 0 ? haloR : coreR);
          }
        const IBlend add(EBlend::Add, pass == 0 ? 0.3f + 0.35f * hi : std::min(1.f, 0.45f + 0.55f * hi));
        BatchDots(g, pass == 0 ? halo : core, mDots.data(), static_cast<int>(mDots.size() / 3), &add);
      }
    }
  }

  float mCx = 0.f, mCy = 0.f, mPw = 0.f, mPh = 0.f, mR = 0.f;
  std::vector<Ember> mCoast;
  std::vector<Contour> mContour;
  int mLevelStart[kMaxIter + 1] = {};
  std::vector<Lit> mEmberLit;
  std::vector<Lit> mRippleLit;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeOrangeOd120Animator()
{
  return std::make_unique<OrangeOd120Animator>();
}
} // namespace volumart::art
