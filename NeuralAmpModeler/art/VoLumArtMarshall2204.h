#pragma once

// Marshall 2204 (amp 7, fractal case 6): "Windswept Glow".
// Hero art: Marshall 2204 - Windswept Glow (tree leaned into a gust over a dawn glow, glowing embers)
// PLAY motion (class B): the wind rises with the level. The tree sways further downwind (a shear
// about the trunk foot) while slow gust fronts roll through the crown and fan its tip embers;
// a pick is a gust: a lean pulse, a bright front racing downwind and a spray of sparks. Embers
// break off the tips and blow away, cooling gold to teal. Silence: the static art, exactly.
// A PLAY Dual lane is too narrow for the leaning crown: there the whole art is fitted into it.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
struct Marshall2204Branch
{
  float x, y, a, l;
  int d;
};

// The tree leaned 12 degrees into the gust, depth first: cb(branch, ex, ey) once per branch.
template <class Cb>
inline void Marshall2204Grow(const IRECT& rect, int maxD, Cb&& cb)
{
  const float cx = rect.MW(), w = rect.W(), h = rect.H();
  // clang-format off
  const float R = std::min(w, h);
  const float lean = 12.f;
  using Br = Marshall2204Branch;
  std::vector<Br> stk; stk.push_back({cx, rect.T + h * 0.94f, -90.f, R * 0.34f, 0});
  while (!stk.empty())
  {
    Br b = stk.back(); stk.pop_back();
    if (b.d > maxD || b.l < 2.5f) continue;
    const float rad = b.a * 3.14159f / 180.f, ex = b.x + b.l * cosf(rad), ey = b.y + b.l * sinf(rad);
    cb(b, ex, ey);
    const float sp = 22.f + b.d * 4.f;
    stk.push_back({ex, ey, b.a - sp + lean, b.l * 0.67f, b.d + 1});
    stk.push_back({ex, ey, b.a + sp + lean, b.l * 0.67f, b.d + 1});
  }
  // clang-format on
}

// The dawn glow and the teal haze behind the tree.
inline void DrawMarshall2204Glow(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, rect.B, R * 0.8f, kGold, 0.12f);
  Bloom(g, cx, cy, R * 0.6f, kTeal, 0.06f);
  // clang-format on
}

// Branches, then the tip embers.
inline void DrawMarshall2204Tree(IGraphics& g, const IRECT& rect)
{
  // clang-format off
  using namespace volumart;
  using Br = Marshall2204Branch;
  Marshall2204Grow(rect, 9, [&](const Br& b, float ex, float ey) {
    const IColor c = WithA(Mix(kDim, kTeal, std::max(0.f, 1.f - b.d / 6.f)), 0.9f);
    if (b.d < 3) GlowLine(g, kTeal, c, b.x, b.y, ex, ey, 2.6f, 4.f);
    else g.DrawLine(c, b.x, b.y, ex, ey, nullptr, 1.f);
  });
  Marshall2204Grow(rect, 9, [&](const Br& b, float ex, float ey) {
    if (b.d >= 7) GlowDot(g, kGold, (b.d % 2) ? kGoldHi : kTeal, ex, ey, 1.7f, 4.f);
  });
  // clang-format on
}

// The 26 floating embers downwind of the crown.
inline void DrawMarshall2204Embers(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  {
    unsigned s = 9u;
    for (int i = 0; i < 26; i++)
    {
      const float px = rect.L + w * (0.55f + 0.4f * Frand(s)), py = rect.T + h * (0.1f + 0.5f * Frand(s));
      GlowDot(g, kGold, Frand(s) > 0.5f ? kGoldHi : kTeal, px, py, Frand(s) * 1.6f + 0.5f, 4.f);
    }
  }
  // clang-format on
}

// The farthest the PLAY wind leans the tree downwind, in degrees.
inline constexpr float kMarshall2204MaxLean = 3.2f;

// Branches and tip embers at rest and at the full lean, and the floating embers; the
// glow is soft light and may run past the lane.
inline ArtBox Marshall2204Extent(const IRECT& rect)
{
  const float footY = rect.T + rect.H() * 0.94f;
  const float tLean = std::tan(kMarshall2204MaxLean * 0.017453293f);
  ArtBox box{rect.MW(), footY, rect.MW(), footY};
  auto add = [&](float x, float y, float rad) {
    box.Add(x, y, rad);
    box.Add(x + tLean * (footY - y), y, rad);
  };
  Marshall2204Grow(rect, 9, [&](const Marshall2204Branch& b, float ex, float ey) {
    const float rad = b.d < 3 ? 3.3f : (b.d >= 7 ? 5.7f : 0.5f);
    add(b.x, b.y, rad);
    add(ex, ey, rad);
  });
  box.Add(rect.L + rect.W() * 0.55f, rect.T + rect.H() * 0.1f, 6.1f);
  box.Add(rect.L + rect.W() * 0.95f, rect.T + rect.H() * 0.6f, 6.1f);
  return box;
}

inline ArtLaneFit Marshall2204Fit(const IRECT& rect)
{
  if (!IsNarrowLane(rect.W(), rect.H()))
    return {};
  return LaneFit(ArtBoxOf(rect), Marshall2204Extent(rect), kLaneFitMargin, 1.f);
}

inline void DrawMarshall2204Hero(IGraphics& g, const IRECT& rect)
{
  LaneFitted(g, Marshall2204Fit(rect), [&] {
    DrawMarshall2204Glow(g, rect);
    DrawMarshall2204Tree(g, rect);
    DrawMarshall2204Embers(g, rect);
  });
}

class Marshall2204Animator final : public ArtAnimator
{
  struct Tip
  {
    float x, y, u;
    uint32_t id;
  };
  struct Pt
  {
    float x, y;
  };
  struct Lit
  {
    float x, y, a;
  };

public:
  // Rest is the whole static art in one layer (bit-exact); sounding draws the passes
  // separately so the tree can sway over an unmoved glow.
  // In a Dual lane every pass is drawn under the lane fit: the layers are built fitted
  // and blitted 1:1, the live sparks drawn fitted, mSeen is the art space in view.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    mFit = Marshall2204Fit(r);
    mSeen = IRectOf(mFit.Unmap(ArtBoxOf(r)));
    if (!mFull.Ok(g))
      mFull.Build(g, owner, r, [&] { DrawMarshall2204Hero(g, r); });
    if (!mGlow.Ok(g))
      mGlow.Build(g, owner, r, [&] { LaneFitted(g, mFit, [&] { DrawMarshall2204Glow(g, r); }); });
    if (!mTree.Ok(g))
      mTree.Build(g, owner, r, [&] { LaneFitted(g, mFit, [&] { DrawMarshall2204Tree(g, r); }); });
    if (!mEmbers.Ok(g))
      mEmbers.Build(g, owner, r, [&] { LaneFitted(g, mFit, [&] { DrawMarshall2204Embers(g, r); }); });
    FindTips(r);
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest())
    {
      mFull.DrawLit(g, r, m.bloom);
      return;
    }
    const float wake = m.Wake();
    const float lean = WindLean(m);
    const float tS = std::tan(lean * 0.017453293f);
    const float px = r.MW(), py = r.T + r.H() * 0.94f; // trunk foot
    // Negative x-skew about the foot pushes everything above it right, into the lean.
    const IMatrix sway = AboutPoint(mFit.X(px), mFit.Y(py), 0.f, 1.f, 1.f, -lean);
    mGlow.DrawLit(g, r, m.bloom);
    const float breath = 0.1f * m.energy * static_cast<float>(std::sin(0.45 * m.clock));
    LaneFitted(g, mFit, [&] {
      BloomAdd(g, px, r.B, mR * (0.8f + 0.1f * m.energy), kGold, 0.42f,
               std::min(1.f, wake * (0.4f + 0.5f * m.energy + breath) + 0.35f * m.pick));
    });
    mTree.DrawXform(g, r, sway);
    mTree.DrawXform(g, r, sway, m.bloom, true);
    if (wake < 1.f)
    {
      mEmbers.Draw(g, r, 1.f - wake);
      mEmbers.DrawAdd(g, r, m.bloom * (1.f - wake));
    }
    LaneFitted(g, mFit, [&] {
      FanTips(m, tS, py);
      DrawFan(g);
      BlowEmbers(mSeen, m, wake, tS, py);
      DrawEmbers(g);
    });
  }

  void DropLayers() override
  {
    mFull.Reset();
    mGlow.Reset();
    mTree.Reset();
    mEmbers.Reset();
  }

private:
  static constexpr int kEmbers = 40;
  static constexpr int kBurstSparks = 7;
  static constexpr int kFanTiers = 4;
  static constexpr int kHeatBins = 3;
  static constexpr float kLife = 2.6f;
  static constexpr float kBurstLife = 1.2f;
  static constexpr float kFanKeep = 0.3f;

  static float PickHash(const ArtMotion& m, int slot, uint32_t salt)
  {
    const uint32_t pickId = m.onsets - static_cast<uint32_t>(slot);
    return Hash01((pickId * 747796405u + m.seed) ^ (salt * 2654435761u));
  }

  // Degrees of extra lean downwind, never upwind: Marshall2204Extent fits a Dual lane
  // to the tree over exactly this range.
  static float WindLean(const ArtMotion& m)
  {
    const float sway = m.energy
                       * (1.3f + 0.7f * static_cast<float>(std::sin(0.9 * m.clock))
                          + 0.3f * static_cast<float>(std::sin(2.3 * m.clock + 1.0)));
    float gust = 0.f;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      if (a >= kMaxEventAge)
        continue;
      const float env = (1.f - std::exp(-a / 0.07f)) * std::exp(-a / 0.45f) * (1.f - a / kMaxEventAge);
      gust += (0.75f + 0.5f * PickHash(m, k, 3u)) * env * (1.f + 0.3f * std::sin(11.f * a));
    }
    gust = 2.6f * (1.f - std::exp(-gust * 3.2f / 2.6f));
    return std::clamp(sway + gust, 0.f, kMarshall2204MaxLean);
  }

  static float EdgeFade(const IRECT& r, float x, float y)
  {
    return Smoothstep(r.R, r.R - 28.f, x) * Smoothstep(r.T, r.T + 14.f, y);
  }

  // Tip embers (depth >= 7) at their exact base positions, from the same grow replay.
  // Only tips in view move.
  void FindTips(const IRECT& r)
  {
    mR = std::min(r.W(), r.H());
    mTips.clear();
    mSpawn.clear();
    float xLo = 1e9f, xHi = -1e9f;
    uint32_t id = 0;
    const IRECT& v = mSeen;
    Marshall2204Grow(r, 9, [&](const Marshall2204Branch& b, float ex, float ey) {
      if (b.d < 7)
        return;
      ++id;
      xLo = std::min(xLo, ex);
      xHi = std::max(xHi, ex);
      if (ex < v.L + 4.f || ex > v.R - 4.f || ey < v.T + 4.f || ey > v.B - 4.f)
        return;
      if (b.d >= 8 && Hash01(id * 2654435761u ^ 61u) < kFanKeep)
        mTips.push_back({ex, ey, 0.f, id});
      if (ex < v.R - 36.f)
        mSpawn.push_back({ex, ey});
    });
    const float span = std::max(1.f, xHi - xLo);
    for (Tip& t : mTips)
      t.u = (t.x - xLo) / span;
    mLit.reserve(mTips.size());
    mDots.reserve(3 * (mTips.size() + kEmbers + kOnsetSlots * kBurstSparks));
    mCores.reserve(3 * mTips.size());
    for (std::vector<float>& bin : mBin)
      bin.reserve(3 * (kEmbers + kOnsetSlots * kBurstSparks));
  }

  int SpawnAt(float h01) const
  {
    const int n = static_cast<int>(mSpawn.size());
    return std::min(n - 1, static_cast<int>(h01 * static_cast<float>(n)));
  }

  // Wind fanning the coals: slow fronts roll downwind through the crown with the level,
  // each pick sends a fast one, and the strike itself kindles a random scatter.
  void FanTips(const ArtMotion& m, float tS, float py)
  {
    mLit.clear();
    const float wavePh = static_cast<float>(std::fmod(m.clock * 0.4, 1.0));
    const float tw = static_cast<float>(m.clock * 3.5);
    float front[kOnsetSlots], frontGain[kOnsetSlots];
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      const float f = std::max(0.f, 1.f - a / kMaxEventAge);
      front[k] = -0.15f + a / 0.5f;
      frontGain[k] = a < kMaxEventAge ? f * f * (0.8f + 0.4f * PickHash(m, k, 5u)) : 0.f;
    }
    const uint32_t strike = m.onsets * 747796405u;
    for (const Tip& t : mTips)
    {
      const float c = 0.5f + 0.5f * std::cos(6.2831853f * (1.3f * t.u - wavePh));
      float a = 0.85f * m.energy * c * c * c * c;
      for (int k = 0; k < kOnsetSlots; ++k)
        if (frontGain[k] > 0.f)
        {
          const float d = (t.u - front[k]) / 0.14f;
          a += frontGain[k] * std::exp(-d * d);
        }
      a += 0.3f * m.pick * Hash01(t.id * 2654435761u ^ strike);
      a *= 0.6f + 0.8f * Noise1(tw, t.id);
      if (a > 0.06f)
        mLit.push_back({t.x + tS * (py - t.y), t.y, std::min(a, 1.f)});
    }
  }

  void DrawFan(IGraphics& g)
  {
    const IColor halo = WithA(kGold, 0.55f);
    for (int k = 0; k < kFanTiers; ++k)
    {
      const float lo = static_cast<float>(k) / kFanTiers, hi = static_cast<float>(k + 1) / kFanTiers;
      mDots.clear();
      mCores.clear();
      for (const Lit& l : mLit)
        if (l.a > lo && l.a <= hi)
        {
          const float s = 0.6f + 0.4f * l.a / hi;
          mDots.push_back(l.x);
          mDots.push_back(l.y);
          mDots.push_back(4.5f * s);
          mCores.push_back(l.x);
          mCores.push_back(l.y);
          mCores.push_back(1.6f * s);
        }
      const int n = static_cast<int>(mDots.size() / 3);
      if (n == 0)
        continue;
      const IBlend glow(EBlend::Add, hi), hot(EBlend::Add, std::min(1.f, 0.35f + 0.65f * hi));
      BatchDots(g, halo, mDots.data(), n, &glow);
      BatchDots(g, kGoldHi, mCores.data(), n, &hot);
    }
  }

  // Size carries each ember's fade (it burns down); the bin carries its heat.
  void Push(float x, float y, float s, float heat)
  {
    if (s < 0.05f)
      return;
    std::vector<float>& bin = mBin[heat < 0.3f ? 0 : (heat < 0.65f ? 1 : 2)];
    bin.push_back(x);
    bin.push_back(y);
    bin.push_back(s);
  }

  void BlowEmbers(const IRECT& r, const ArtMotion& m, float wake, float tS, float py)
  {
    for (std::vector<float>& bin : mBin)
      bin.clear();
    if (mSpawn.empty())
      return;
    const float reach = std::clamp(r.W() / 620.f, 0.45f, 1.f);
    const float density = 0.3f + 0.7f * m.energy;
    const float flickT = static_cast<float>(m.clock * 7.0);
    const float driftX = static_cast<float>(m.clock * 0.7), driftY = static_cast<float>(m.clock * 0.9);
    // Embers torn off the tips, one life per cycle; a new cycle starts from a new tip
    // while the ember is invisible.
    for (int i = 0; i < kEmbers; ++i)
    {
      const uint32_t ui = static_cast<uint32_t>(i);
      const float h0 = Hash01(ui * 2654435761u ^ 0x2204u);
      const float gate = Smoothstep(0.85f * h0, 0.85f * h0 + 0.12f, density) * wake;
      if (gate <= 0.f)
        continue;
      const double cyc = m.clock / kLife + Hash01(ui * 2654435761u ^ 0x51u);
      const double cycF = std::floor(cyc);
      const float phase = static_cast<float>(cyc - cycF);
      const uint32_t c = static_cast<uint32_t>(static_cast<int64_t>(cycF));
      const Pt& s = mSpawn[SpawnAt(Hash01((ui * 97u + c) * 2654435761u ^ 0x7au))];
      const float age = phase * kLife;
      const float vx = (20.f + 44.f * m.energy + 14.f * h0) * reach;
      const float vy = 8.f + 12.f * Hash01(ui * 2654435761u ^ 0x9du);
      const float drift = Smoothstep(0.f, 0.8f, age);
      const float x =
        s.x + tS * (py - s.y) + vx * age + 4.f * m.energy * reach * age * age + 16.f * drift * Noise1(driftX, ui + 40u);
      const float y = s.y - vy * age - 2.5f * age * age + 10.f * drift * Noise1(driftY, ui + 90u);
      const float env = Smoothstep(0.f, 0.06f, phase) * std::pow(1.f - phase, 1.3f);
      const float flick = 0.75f + 0.5f * Noise1(flickT, ui + 7u);
      Push(x, y, gate * env * flick * EdgeFade(r, x, y), phase);
    }
    // A pick shakes a spray of sparks out of one tip of the crown.
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      if (a >= kBurstLife)
        continue;
      const Pt& o = mSpawn[SpawnAt(PickHash(m, k, 11u))];
      const float ox = o.x + tS * (py - o.y);
      const float left = 1.f - a / kBurstLife;
      const float fade = left * left * Smoothstep(0.f, 0.03f, a);
      const float drag = (1.f - std::exp(-3.f * a)) / 3.f;
      const float wind = (18.f + 30.f * m.energy) * reach * a;
      const uint32_t pickId = m.onsets - static_cast<uint32_t>(k);
      for (int j = 0; j < kBurstSparks; ++j)
      {
        const uint32_t sid = (pickId * 8u + static_cast<uint32_t>(j)) * 2654435761u;
        const float ang = -1.25f + 1.45f * Hash01(sid ^ 0xb1u);
        const float speed = (55.f + 70.f * Hash01(sid ^ 0xc3u)) * reach;
        const float x = ox + std::cos(ang) * speed * drag + wind;
        const float y = o.y + std::sin(ang) * speed * drag - 8.f * a + 14.f * a * a * Hash01(sid ^ 0xd5u);
        const float flick = 0.75f + 0.5f * Noise1(flickT * 1.3f, sid ^ 0x3u);
        Push(x, y, 0.9f * fade * flick * EdgeFade(r, x, y), a / kBurstLife);
      }
    }
  }

  void Rings(IGraphics& g, const std::vector<float>& src, float radius, const IColor& c, float w)
  {
    mDots.clear();
    for (size_t i = 0; i + 2 < src.size(); i += 3)
    {
      mDots.push_back(src[i]);
      mDots.push_back(src[i + 1]);
      mDots.push_back(radius * src[i + 2]);
    }
    const IBlend add(EBlend::Add, w);
    BatchDots(g, c, mDots.data(), static_cast<int>(mDots.size() / 3), &add);
  }

  // Hot embers burn gold-white; cooling ones keep the static embers' teal core.
  void DrawEmbers(IGraphics& g)
  {
    const IColor core[kHeatBins] = {kGoldHi, Mix(kGoldHi, kTeal, 0.45f), kTeal};
    const IColor halo = WithA(kGold, 0.6f), mid = WithA(kGold, 0.8f);
    for (int b = 0; b < kHeatBins; ++b)
    {
      if (mBin[b].empty())
        continue;
      const float cool = static_cast<float>(b);
      Rings(g, mBin[b], 5.5f - cool, halo, 0.85f - 0.2f * cool);
      Rings(g, mBin[b], 2.6f - 0.3f * cool, mid, 0.8f - 0.2f * cool);
      Rings(g, mBin[b], 1.25f - 0.3f * cool, core[b], 1.f - 0.25f * cool);
    }
  }

  ArtLayer mFull;
  ArtLayer mGlow;
  ArtLayer mTree;
  ArtLayer mEmbers;
  ArtLaneFit mFit;
  IRECT mSeen;
  float mR = 0.f;
  std::vector<Tip> mTips;
  std::vector<Pt> mSpawn;
  std::vector<Lit> mLit;
  std::vector<float> mDots;
  std::vector<float> mCores;
  std::vector<float> mBin[kHeatBins];
};

inline std::unique_ptr<ArtAnimator> MakeMarshall2204Animator()
{
  return std::make_unique<Marshall2204Animator>();
}
} // namespace volumart::art
