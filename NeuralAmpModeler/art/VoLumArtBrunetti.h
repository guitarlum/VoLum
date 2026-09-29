#pragma once

// Brunetti XL 2 (amp 2, fractal case 2): "Verdant Fern".
// Hero art: Brunetti XL 2 - Verdant Fern (layered fern, teal->gold tips, ground bloom)
// PLAY motion (class B/C, bend): wind blows through the fern from the left. Gust fronts
// cross the frame as a sheen on the leaflets and bow the frond from its planted foot as
// they pass (the back fern later and less); the level is the wind's strength. A pick is
// a gust: the frond whips and springs back, and a puff of spores leaves the gold tips.
// A PLAY Dual lane is too narrow for the fronds: there the whole art is fitted into it.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
// Points outside cull (padded by 4) are skipped; cull is rect unless a Dual lane fit
// brings more of the art space into view.
inline void DrawBrunettiArt(IGraphics& g, const IRECT& rect, const IRECT& cull)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  Bloom(g, cx, rect.T + h * 0.86f, std::min(w, h) * 0.8f, kTeal, 0.13f);
  auto fern = [&](int n, float fh, float fsc, float bx, float by, IColor base, IColor tip, float rr, float a, bool gold) {
    float px = 0.f, py = 0.f; unsigned s = 42u;
    for (int i = 0; i < n; i++)
    {
      s = s * 1103515245u + 12345u; float rv = (float)(s % 1000) / 1000.f; float nx, ny;
      if (rv < 0.01f) { nx = 0.f; ny = 0.16f * py; }
      else if (rv < 0.86f) { nx = 0.85f * px + 0.04f * py; ny = -0.04f * px + 0.85f * py + 1.6f; }
      else if (rv < 0.93f) { nx = 0.2f * px - 0.26f * py; ny = 0.23f * px + 0.22f * py + 1.6f; }
      else { nx = -0.15f * px + 0.28f * py; ny = 0.26f * px + 0.24f * py + 0.44f; }
      px = nx; py = ny; if (i == 0) continue;
      const float sx = bx + px * fsc, sy = by - py * fh;
      if (sx < cull.L - 4.f || sx > cull.R + 4.f || sy < cull.T - 4.f || sy > cull.B + 4.f) continue;
      IColor c = Mix(base, tip, std::min(1.f, py / 9.f));
      if (gold && py > 7.6f) c = Mix(c, kGoldHi, (py - 7.6f) / 2.4f);
      g.FillCircle(WithA(c, a), sx, sy, rr);
    }
  };
  // Horizontal scale tied to height at the 1.1.0 fern ratio (~2.53:1 horiz:vert per unit,
  // i.e. 38px:15px) so the fronds keep their classic proportions and do not stretch with
  // the hero panel width or cramp when height-tied 1:1.
  fern(6000, (h * 0.78f) / 10.5f, h * 0.188f, rect.L + w * 0.55f, rect.T + h * 0.9f, IColor(255, 45, 90, 110), kMid, 0.9f, 0.32f, false);
  fern(11000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false);
  fern(9000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true);
  // clang-format on
}

// One fern pass of DrawBrunettiArt, with its arguments: 0 = back, 1 = front, 2 = front gold-tipped.
struct BrunettiFern
{
  int n;
  float fh, fsc, bx, by;
  IColor base, tip;
  float rr, a;
  bool gold;
};

inline BrunettiFern BrunettiFernAt(const IRECT& rect, int pass)
{
  const float w = rect.W(), h = rect.H();
  // clang-format off
  if (pass == 0)
    return {6000, (h * 0.78f) / 10.5f, h * 0.188f, rect.L + w * 0.55f, rect.T + h * 0.9f, IColor(255, 45, 90, 110), kMid, 0.9f, 0.32f, false};
  if (pass == 1)
    return {11000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.1f, 0.7f, false};
  return {9000, (h * 0.86f) / 10.5f, h * 0.207f, rect.L + w * 0.5f, rect.T + h * 0.94f, IColor(255, 55, 120, 140), kTeal, 1.15f, 0.55f, true};
  // clang-format on
}

// The pass's points in DrawBrunettiArt's order (same LCG, maps and cull); nothing is drawn.
template <class Visit>
inline void BrunettiFernPoints(const IRECT& cull, const BrunettiFern& f, Visit&& visit)
{
  // clang-format off
  float px = 0.f, py = 0.f; unsigned s = 42u;
  for (int i = 0; i < f.n; i++)
  {
    s = s * 1103515245u + 12345u; float rv = (float)(s % 1000) / 1000.f; float nx, ny;
    if (rv < 0.01f) { nx = 0.f; ny = 0.16f * py; }
    else if (rv < 0.86f) { nx = 0.85f * px + 0.04f * py; ny = -0.04f * px + 0.85f * py + 1.6f; }
    else if (rv < 0.93f) { nx = 0.2f * px - 0.26f * py; ny = 0.23f * px + 0.22f * py + 1.6f; }
    else { nx = -0.15f * px + 0.28f * py; ny = 0.26f * px + 0.24f * py + 0.44f; }
    px = nx; py = ny; if (i == 0) continue;
    const float sx = f.bx + px * f.fsc, sy = f.by - py * f.fh;
    if (sx < cull.L - 4.f || sx > cull.R + 4.f || sy < cull.T - 4.f || sy > cull.B + 4.f) continue;
    visit(i, py, sx, sy);
  }
  // clang-format on
}

inline constexpr int kBrunettiBands = 4;
// Band edges in frond height (py / 10). The frond bends as one straight chord per band.
inline constexpr float kBrunettiBandEdge[kBrunettiBands + 1] = {0.f, 0.34f, 0.58f, 0.79f, 1.f};

inline int BrunettiBandOf(float py)
{
  const float s = py / 10.f;
  int k = 0;
  while (k < kBrunettiBands - 1 && s >= kBrunettiBandEdge[k + 1])
    ++k;
  return k;
}

// The pass's points in one bend band (band < 0: all of them), coloured as the hero colours them.
inline void DrawBrunettiFernBand(IGraphics& g, const IRECT& cull, const BrunettiFern& f, int band)
{
  BrunettiFernPoints(cull, f, [&](int, float py, float sx, float sy) {
    if (band >= 0 && BrunettiBandOf(py) != band)
      return;
    // clang-format off
    IColor c = Mix(f.base, f.tip, std::min(1.f, py / 9.f));
    if (f.gold && py > 7.6f) c = Mix(c, kGoldHi, (py - 7.6f) / 2.4f);
    g.FillCircle(WithA(c, f.a), sx, sy, f.rr);
    // clang-format on
  });
}

// The front frond's bend at full level as a gust front passes, x h at its tip (Draw:
// E * (0.012 + 0.045 * lean + sway)); the back frond bends 0.6 x as far.
inline constexpr float kBrunettiWindReach = 0.06f;

// Every leaflet of the three passes, upright and bent downwind, with a glint's radius;
// the ground bloom is soft light and may run past the lane.
inline ArtBox BrunettiExtent(const IRECT& rect)
{
  const IRECT everywhere(-1e6f, -1e6f, 1e6f, 1e6f);
  ArtBox box{1e9f, 1e9f, -1e9f, -1e9f};
  for (int pass = 0; pass < 3; ++pass)
  {
    const BrunettiFern f = BrunettiFernAt(rect, pass);
    const float tip = (pass == 0 ? 0.6f : 1.f) * kBrunettiWindReach * rect.H();
    BrunettiFernPoints(everywhere, f, [&](int, float py, float sx, float sy) {
      const float s = std::clamp(py / 10.f, 0.f, 1.f), s2 = s * s;
      box.Add(sx, sy, 2.4f);
      box.Add(sx + tip * s2 * (6.f - 4.f * s + s2) / 3.f, sy, 2.4f);
    });
  }
  return box;
}

inline ArtLaneFit BrunettiFit(const IRECT& rect)
{
  if (!IsNarrowLane(rect.W(), rect.H()))
    return {};
  return LaneFit(ArtBoxOf(rect), BrunettiExtent(rect), kLaneFitMargin, 1.f);
}

inline void DrawBrunettiHero(IGraphics& g, const IRECT& rect)
{
  const ArtLaneFit fit = BrunettiFit(rect);
  const IRECT cull = IRectOf(fit.Unmap(ArtBoxOf(rect)));
  LaneFitted(g, fit, [&] { DrawBrunettiArt(g, rect, cull); });
}

class BrunettiAnimator final : public ArtAnimator
{
  struct Leaf
  {
    float x, y;
    int band;
    uint32_t id;
    bool gold;
  };

  struct Tip
  {
    float x, y;
    int band;
  };

public:
  // mRest is the hero itself, so silence is bit-exact. The swaying fern is the back pass
  // plus the two front passes cut into height bands, one layer each, built over the next
  // frames; until they exist the art stays at rest. In a Dual lane the layers are built
  // under the lane fit and the live light drawn under it; mSeen is the art space in view.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mRest.Ok(g))
    {
      mFit = BrunettiFit(r);
      mSeen = IRectOf(mFit.Unmap(ArtBoxOf(r)));
      mRest.Build(g, owner, r, [&] { DrawBrunettiHero(g, r); });
      FindLeaves(r);
      return false;
    }
    if (!mBack.Ok(g))
    {
      const BrunettiFern back = BrunettiFernAt(r, 0);
      mBack.Build(g, owner, r, [&] { LaneFitted(g, mFit, [&] { DrawBrunettiFernBand(g, mSeen, back, -1); }); });
      return false;
    }
    for (int k = 0; k < kBrunettiBands; ++k)
      if (!mBand[k].Ok(g))
      {
        const BrunettiFern front = BrunettiFernAt(r, 1), gold = BrunettiFernAt(r, 2);
        mBand[k].Build(g, owner, r, [&] {
          LaneFitted(g, mFit, [&] {
            DrawBrunettiFernBand(g, mSeen, front, k);
            DrawBrunettiFernBand(g, mSeen, gold, k);
          });
        });
        return k == kBrunettiBands - 1;
      }
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest() || !Ready(g))
    {
      mRest.DrawLit(g, r, m.bloom);
      return;
    }
    const float h = r.H(), E = m.energy, wake = m.Wake(), tf = static_cast<float>(m.clock);
    const double t = m.clock;
    const double drift = kWindRate * t + 0.8 * std::sin(0.23 * t);
    // The wind on the frond, less the spring's recoil once the front has passed.
    auto lean = [&](float x) -> float {
      const double ph = drift - (x - r.L) / (kWindReach * h);
      return Wind(ph) - 0.4f * Wind(ph - 1.6);
    };
    float gust = 0.f, gustBack = 0.f;
    for (int k = 0; k < kOnsetSlots; ++k)
      if (m.onsetAge[k] < kMaxEventAge)
      {
        const float v = PickVariation(m, k);
        gust += v * GustSpring(m.onsetAge[k]);
        gustBack += v * GustSpring(m.onsetAge[k] - 0.08f);
      }
    const float sway = static_cast<float>(0.008 * std::sin(1.7 * t + 0.4) + 0.006 * std::sin(2.9 * t + 2.1));
    const float swayBack = static_cast<float>(0.007 * std::sin(1.4 * t + 2.6) + 0.005 * std::sin(2.5 * t + 0.3));
    const float bend = h * (E * (0.012f + 0.045f * lean(mFrontX) + sway) + 0.1f * std::tanh(0.9f * gust));
    const float flutter = h * E * 0.012f * Noise1(tf * 3.1f, 23u);
    const float bendBack =
      0.6f * h * (E * (0.012f + 0.045f * lean(mBackX) + swayBack) + 0.1f * std::tanh(0.9f * gustBack));
    for (int k = 0; k < kBrunettiBands; ++k)
      Chord(kBrunettiBandEdge[k], kBrunettiBandEdge[k + 1], bend, flutter, mFrontBy, mFrontH, mA[k], mB[k]);
    Chord(0.f, 1.f, bendBack, 0.f, mBackBy, mBackH, mBackA, mBackB);

    const float gx = r.MW(), gy = r.T + h * 0.86f, gR = std::min(r.W(), h) * 0.8f;
    LaneFitted(g, mFit, [&] {
      Bloom(g, gx, gy, gR, kTeal, 0.13f);
      BloomAdd(g, gx, gy, gR, kTeal, 0.13f, m.bloom);
    });
    DrawBent(g, r, mBack, mBackA, mBackB, m.bloom);
    for (int k = 0; k < kBrunettiBands; ++k)
      DrawBent(g, r, mBand[k], mA[k], mB[k], m.bloom);
    LaneFitted(g, mFit, [&] {
      DrawSheen(g, r, m, drift, wake);
      DrawSpores(g, r, m, wake);
    });
  }

  void DropLayers() override
  {
    mRest.Reset();
    mBack.Reset();
    for (ArtLayer& l : mBand)
      l.Reset();
  }

private:
  static constexpr double kTau = 6.283185307179586;
  static constexpr double kWindRate = 1.15; // rad per motion-second: a gust front every ~5.5 s
  static constexpr float kWindReach = 0.5f; // x h px per rad: fronts cross at ~0.58 h per second
  static constexpr double kFrontShare = 0.4; // of each wind period a front takes to pass a point
  static constexpr float kLeafShare = 0.045f;
  static constexpr int kMaxTips = 64;
  static constexpr int kAmbientSpores = 40;
  static constexpr int kPuffSpores = 8;
  static constexpr double kSporeLife = 3.2;

  bool Ready(IGraphics& g) const
  {
    if (!mBack.Ok(g))
      return false;
    for (const ArtLayer& l : mBand)
      if (!l.Ok(g))
        return false;
    return true;
  }

  // Leaflets for the sheen (a sparse sample of the front pass) and the gold tips the
  // spores leave from, at their exact base positions.
  void FindLeaves(const IRECT& r)
  {
    const BrunettiFern back = BrunettiFernAt(r, 0), front = BrunettiFernAt(r, 1), gold = BrunettiFernAt(r, 2);
    mFrontBy = front.by;
    mFrontH = 10.f * front.fh;
    mBackBy = back.by;
    mBackH = 10.f * back.fh;
    mFrontX = front.bx + 1.2f * front.fsc;
    mBackX = back.bx + 1.2f * back.fsc;
    mFrondLeft = front.bx - 2.3f * front.fsc;
    mLeaves.clear();
    BrunettiFernPoints(mSeen, front, [&](int i, float py, float sx, float sy) {
      if (Hash01(static_cast<uint32_t>(i) * 2654435761u ^ 0x1eafu) < kLeafShare)
        mLeaves.push_back({sx, sy, BrunettiBandOf(py), static_cast<uint32_t>(i), py > 7.6f});
    });
    std::vector<Tip> tips;
    BrunettiFernPoints(mSeen, gold, [&](int, float py, float sx, float sy) {
      if (py > 8.4f)
        tips.push_back({sx, sy, BrunettiBandOf(py)});
    });
    mTips.clear();
    const size_t n = tips.size(), keep = std::min(n, static_cast<size_t>(kMaxTips));
    for (size_t k = 0; k < keep; ++k)
      mTips.push_back(tips[k * n / keep]);
    for (std::vector<float>& b : mBucket)
      b.reserve(3 * (mLeaves.size() + kAmbientSpores + kOnsetSlots * kPuffSpores));
  }

  // Gust fronts sweeping left to right: 0 between fronts, each front its own strength.
  static float Wind(double phase)
  {
    const double turn = phase / kTau;
    const double n = std::floor(turn);
    const float q = static_cast<float>((turn - n) / kFrontShare);
    if (q >= 1.f)
      return 0.f;
    const float b = std::sin(3.14159265f * q);
    const float strength =
      0.45f + 0.55f * Hash01(static_cast<uint32_t>(static_cast<int64_t>(n)) * 2654435761u ^ 0x5eedu);
    return strength * b * b;
  }

  // A pick's gust: the frond is pushed with the wind, springs back past upright and settles.
  static float GustSpring(float age)
  {
    if (age <= 0.f)
      return 0.f;
    return std::exp(-3.f * age) * std::sin(7.5f * age) * (1.f - Smoothstep(1.1f, 1.5f, age));
  }

  static float PickVariation(const ArtMotion& m, int slot)
  {
    return 0.7f + 0.3f * Hash01((m.onsets - static_cast<uint32_t>(slot)) * 747796405u + m.seed);
  }

  // The frond's bend (a loaded cantilever from the foot, plus a flutter at the tip) as
  // the chord x' = x + A + B y over band s0..s1, so neighbouring bands meet without a tear.
  static void Chord(float s0, float s1, float bend, float flutter, float by, float H, float& A, float& B)
  {
    auto at = [bend, flutter](float s) {
      const float s2 = s * s;
      return bend * s2 * (6.f - 4.f * s + s2) / 3.f + flutter * s2 * s2;
    };
    const float d0 = at(s0), K = (at(s1) - d0) / ((s1 - s0) * H);
    B = -K;
    A = d0 + K * (by - s0 * H);
  }

  // A and B bend the art space; the layer holds the fitted art, so the chord is carried
  // through the lane fit: x' = x + s A - B ty + B y.
  void DrawBent(IGraphics& g, const IRECT& r, const ArtLayer& layer, float A, float B, float bloom) const
  {
    const IMatrix xf(1.0, 0.0, B, 1.0, mFit.s * A - B * mFit.ty, 0.0);
    layer.DrawXform(g, r, xf);
    if (bloom > 0.f)
      layer.DrawXform(g, r, xf, bloom, true);
  }

  // xyr triples, halo radius in place: one additive halo fill, then one core fill.
  static void DrawGlints(IGraphics& g, std::vector<float>& xyr, const IColor& halo, const IColor& core, float coreR,
                         float haloW, float coreW)
  {
    const int n = static_cast<int>(xyr.size() / 3);
    if (n <= 0)
      return;
    const IBlend haloAdd(EBlend::Add, haloW);
    BatchDots(g, halo, xyr.data(), n, &haloAdd);
    for (int i = 0; i < n; ++i)
      xyr[3 * i + 2] = coreR;
    const IBlend coreAdd(EBlend::Add, coreW);
    BatchDots(g, core, xyr.data(), n, &coreAdd);
  }

  // Leaflets catch the light as a front passes them (flickering as they turn), a pick's
  // gust sweeps a quicker sheen across the frond, and the gold tips glow with the level.
  void DrawSheen(IGraphics& g, const IRECT& r, const ArtMotion& m, double drift, float wake)
  {
    const float h = r.H(), E = m.energy, tf = static_cast<float>(m.clock);
    float front[kOnsetSlots], frontW[kOnsetSlots];
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      front[k] = mFrondLeft - 0.15f * h + a * 3.f * h;
      frontW[k] = a < kMaxEventAge ? PickVariation(m, k) * (1.f - Smoothstep(0.45f, 1.3f, a)) : 0.f;
    }
    const float tipGlow = 0.16f * E * wake + 0.35f * m.pick;
    const float invSigma = 1.f / (0.13f * h);
    for (std::vector<float>& b : mBucket)
      b.clear();
    for (const Leaf& l : mLeaves)
    {
      const float flick = 0.5f + Noise1(tf * 2.4f, l.id);
      const float amb = E * Wind(drift - (l.x - r.L) / (kWindReach * h));
      float lit = wake * amb * amb * (0.35f + 0.65f * flick);
      for (int k = 0; k < kOnsetSlots; ++k)
        if (frontW[k] > 0.f)
        {
          const float d = (l.x - front[k]) * invSigma;
          if (d > -2.5f && d < 2.5f)
            lit += frontW[k] * std::exp(-d * d) * (0.55f + 0.45f * flick);
        }
      if (l.gold)
        lit += tipGlow * (0.4f + 0.6f * flick);
      if (lit < 0.06f)
        continue;
      std::vector<float>& b = mBucket[(l.gold ? 2 : 0) + (lit > 0.5f ? 1 : 0)];
      b.push_back(l.x + mA[l.band] + mB[l.band] * l.y);
      b.push_back(l.y);
      b.push_back(2.4f);
    }
    static const IColor kLeafHot(255, 200, 245, 240);
    DrawGlints(g, mBucket[0], kTeal, kLeafHot, 1.f, 0.28f, 0.4f);
    DrawGlints(g, mBucket[1], kTeal, kLeafHot, 1.f, 0.45f, 0.65f);
    DrawGlints(g, mBucket[2], kGold, kGoldHi, 1.f, 0.28f, 0.4f);
    DrawGlints(g, mBucket[3], kGold, kGoldHi, 1.f, 0.45f, 0.65f);
  }

  // Spores drift up off the gold tips on the wind (more with the level); each pick shakes
  // a puff of them loose that is gone by kMaxEventAge. A spore rides its tip as it leaves.
  void DrawSpores(IGraphics& g, const IRECT& r, const ArtMotion& m, float wake)
  {
    if (mTips.empty())
      return;
    const float h = r.H(), E = m.energy, tf = static_cast<float>(m.clock);
    const int nTips = static_cast<int>(mTips.size());
    auto tipAt = [&](float hv) -> const Tip& { return mTips[std::min(nTips - 1, static_cast<int>(hv * nTips))]; };
    mBucket[0].clear();
    mBucket[1].clear();
    auto emit = [&](float x, float y, float a) {
      if (a <= 0.02f)
        return;
      std::vector<float>& b = mBucket[a > 0.5f ? 1 : 0];
      b.push_back(x);
      b.push_back(y);
      b.push_back(2.5f);
    };
    const float live = 16.f + 24.f * E;
    for (int i = 0; i < kAmbientSpores; ++i)
    {
      const float vis = std::clamp(live - static_cast<float>(i), 0.f, 1.f);
      if (vis <= 0.f)
        break;
      const uint32_t id = static_cast<uint32_t>(i);
      const uint32_t key = id * 2654435761u;
      const double cyc = m.clock / kSporeLife + Hash01(key ^ 0x51u);
      const double cn = std::floor(cyc);
      const float u = static_cast<float>(cyc - cn);
      const Tip& tp = tipAt(Hash01(static_cast<uint32_t>(static_cast<int64_t>(cn)) * 747796405u ^ key));
      const float dx = mA[tp.band] + mB[tp.band] * tp.y;
      const float x = tp.x + dx * (1.f - u) + h * (0.06f * u * u + 0.02f * u * Noise1(tf * 0.9f, 300u + id));
      const float y = tp.y - h * (0.075f * u + 0.012f * u * Noise1(tf * 0.7f, 400u + id));
      const float env = Smoothstep(0.f, 0.12f, u) * (1.f - Smoothstep(0.5f, 1.f, u));
      emit(x, y, vis * wake * (0.45f + 0.55f * E) * env * (0.75f + 0.5f * Noise1(tf * 3.3f, 500u + id)));
    }
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      if (a >= kMaxEventAge)
        continue;
      const uint32_t pick = m.onsets - static_cast<uint32_t>(k);
      const float f = 1.f - std::exp(-2.8f * a);
      const float fade = PickVariation(m, k) * Smoothstep(0.f, 0.04f, a) * (1.f - Smoothstep(0.35f, 1.45f, a));
      for (int q = 0; q < kPuffSpores; ++q)
      {
        const uint32_t key = (pick * static_cast<uint32_t>(kPuffSpores) + static_cast<uint32_t>(q)) * 2654435761u;
        const Tip& tp = tipAt(Hash01(key ^ 0x77u));
        const float dx = mA[tp.band] + mB[tp.band] * tp.y;
        const float x = tp.x + dx * (1.f - f) + h * f * (0.05f + 0.12f * Hash01(key ^ 0x99u));
        const float y = tp.y - h * (f * (0.03f + 0.08f * Hash01(key ^ 0xabu)) + 0.02f * a);
        emit(x, y, fade * (0.8f + 0.4f * Noise1(tf * 4.f, 600u + static_cast<uint32_t>(q))));
      }
    }
    const IColor spore = Mix(kTeal, kGoldHi, 0.7f);
    static const IColor kSporeHot(255, 255, 244, 200);
    DrawGlints(g, mBucket[0], spore, kSporeHot, 0.9f, 0.35f, 0.55f);
    DrawGlints(g, mBucket[1], spore, kSporeHot, 0.9f, 0.6f, 0.95f);
  }

  ArtLayer mRest;
  ArtLayer mBack;
  ArtLayer mBand[kBrunettiBands];
  ArtLaneFit mFit;
  IRECT mSeen;
  std::vector<Leaf> mLeaves;
  std::vector<Tip> mTips;
  std::vector<float> mBucket[4];
  float mA[kBrunettiBands] = {}, mB[kBrunettiBands] = {};
  float mBackA = 0.f, mBackB = 0.f;
  float mFrontBy = 0.f, mFrontH = 1.f, mBackBy = 0.f, mBackH = 1.f;
  float mFrontX = 0.f, mBackX = 0.f, mFrondLeft = 0.f;
};

inline std::unique_ptr<ArtAnimator> MakeBrunettiAnimator()
{
  return std::make_unique<BrunettiAnimator>();
}
} // namespace volumart::art
