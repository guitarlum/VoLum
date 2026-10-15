#pragma once

// Diezel Herbert Mk1 (amp 3, fractal case 14): "Lichtenberg Glow".
// Hero art: Diezel Herbert Mk1 - Lichtenberg Glow (dual-seed discharge over a teal bloom, glowing terminals)
// PLAY motion (class B): charge climbs the burned-in discharge from both terminals. The level sets its
// reach (the two trees see-saw, one extending while the other draws back) with a white-hot front, tip
// sparks and pulses running outward; a pick strikes it out to the tips and fires one main bolt along a
// root-to-tip channel that leaps out and draws back into its terminal. Silence: the static art.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

#include <unordered_set>

namespace volumart::art
{
struct DiezelSeg
{
  float x0, y0, x1, y1;
  float d0, d1; // path length from the segment's seed to each end
  int up; // the segment ending where this one starts; -1 at a seed
  uint8_t tier; // 0 bright, 1 mid, 2 dim
  uint8_t root; // 0 bottom seed, 1 top seed
};

// The discharge's growth, recorded in draw order instead of drawn. The RNG calls,
// rejections and float expressions are the static art's own.
inline void GrowDiezelDischarge(const IRECT& rect, std::vector<DiezelSeg>& segs)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  segs.clear();
  // clang-format off
  // Two opposing seeds (top and bottom), DLA-style branching toward random
  // attractor points distributed across the panel. Result: dual branching
  // electric arcs evoking high-voltage transformer discharge.
  struct Pt { float x, y; };
  std::vector<Pt> pts;
  pts.reserve(1500);
  pts.push_back({cx, rect.B - 16.f});
  pts.push_back({cx, rect.T + 16.f});

  // Spatial hash to skip near-duplicate placements (keeps the drawing crisp)
  const float cellSz = 6.f;
  std::unordered_set<long long> grid;
  auto cellKey = [&](float x, float y) -> long long {
    long long ix = (long long)std::floor(x / cellSz);
    long long iy = (long long)std::floor(y / cellSz);
    return (ix << 32) ^ (long long)(unsigned long long)iy;
  };
  grid.insert(cellKey(pts[0].x, pts[0].y));
  grid.insert(cellKey(pts[1].x, pts[1].y));

  // Randomly distributed attractors - bias toward column near center
  constexpr int kAttractors = 360;
  std::vector<Pt> targets;
  targets.reserve(kAttractors);
  unsigned rng = 0xBEEFu;
  auto frand = [&]() -> float {
    rng = rng * 1664525u + 1013904223u;
    return (float)rng / (float)0xFFFFFFFFu;
  };
  for (int i = 0; i < kAttractors; ++i)
  {
    targets.push_back({cx + (frand() - 0.5f) * w * 0.72f,
                       rect.T + 18.f + frand() * (h - 36.f)});
  }

  constexpr int kIters = 1400;
  for (int i = 0; i < kIters; ++i)
  {
    const Pt& target = targets[(int)(frand() * kAttractors) % kAttractors];

    // Sample-based nearest search (fast enough; pts grows to ~1.4k)
    int trials = std::min(48, (int)pts.size());
    int nearestIdx = 0;
    float bestD = 1e30f;
    for (int k = 0; k < trials; ++k)
    {
      int idx2 = (int)(frand() * pts.size()) % (int)pts.size();
      float dx = pts[idx2].x - target.x;
      float dy = pts[idx2].y - target.y;
      float d = dx * dx + dy * dy;
      if (d < bestD) { bestD = d; nearestIdx = idx2; }
    }

    const Pt& parent = pts[nearestIdx];
    float dx = target.x - parent.x;
    float dy = target.y - parent.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.001f) continue;
    float ang = std::atan2(dy, dx) + (frand() - 0.5f) * 0.6f;
    float len = 9.f + frand() * 5.f;
    Pt next{parent.x + std::cos(ang) * len, parent.y + std::sin(ang) * len};
    if (next.x < rect.L + 14.f || next.x > rect.R - 14.f
        || next.y < rect.T + 12.f || next.y > rect.B - 12.f) continue;

    long long key = cellKey(next.x, next.y);
    if (!grid.insert(key).second) continue;

    uint8_t tier;
    if (i < 90)        tier = 0;
    else if (i < 380)  tier = 1;
    else               tier = 2;
    // Point p >= 2 is the end of segment p - 2: both are pushed together.
    const int up = nearestIdx < 2 ? -1 : nearestIdx - 2;
    const float d0 = up < 0 ? 0.f : segs[up].d1;
    const uint8_t root = up < 0 ? static_cast<uint8_t>(nearestIdx) : segs[up].root;
    segs.push_back({parent.x, parent.y, next.x, next.y, d0, d0 + len, up, tier, root});
    pts.push_back(next);
  }
  // clang-format on
}

inline void DrawDiezelHerbertBloom(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  volumart::Bloom(g, cx, cy, std::min(w, h) * 0.55f, volumart::kTeal, 0.16f);
  // clang-format on
}

// The recorded discharge and the terminals: everything drawn over the bloom.
inline void DrawDiezelHerbertDischarge(IGraphics& g, const IRECT& rect, const std::vector<DiezelSeg>& segs)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  const IColor bright = kHeroBright, mid = kHeroMid, dim = kHeroDim;
  // clang-format off
  for (const DiezelSeg& s : segs)
  {
    IColor col;
    float thickness;
    if (s.tier == 0)      { col = bright; thickness = 2.4f; }
    else if (s.tier == 1) { col = mid;    thickness = 1.5f; }
    else                  { col = dim;    thickness = 1.0f; }
    g.DrawLine(col, s.x0, s.y0, s.x1, s.y1, nullptr, thickness);
  }

  // Glowing terminals (a whisper of gold only at hero size).
  volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.B - 16.f, 2.6f, 5.f);
  volumart::GlowDot(g, volumart::kTeal, volumart::kTeal, cx, rect.T + 16.f, 2.6f, 5.f);
  g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.B - 16.f, 1.2f);
  g.FillCircle(volumart::WithA(volumart::kGoldHi, 0.85f), cx, rect.T + 16.f, 1.2f);
  // clang-format on
}

inline void DrawDiezelHerbertHero(IGraphics& g, const IRECT& rect)
{
  std::vector<DiezelSeg> segs;
  GrowDiezelDischarge(rect, segs);
  DrawDiezelHerbertBloom(g, rect);
  DrawDiezelHerbertDischarge(g, rect, segs);
}

static const IColor kDiezelWhite(255, 235, 245, 250);

// xyxy = {x0, y0, x1, y1, ...} as one additive stroke. Butt caps: additive round
// caps would bead at every joint of the tree.
inline void DiezelStrokeAdd(IGraphics& g, const IColor& c, float width, const std::vector<float>& xyxy, float w)
{
  if (xyxy.size() < 4 || w <= 0.f)
    return;
  IStrokeOptions butt;
  butt.mCapOption = ELineCap::Butt;
  const IBlend add(EBlend::Add, std::min(1.f, w));
  g.PathClear();
  for (size_t i = 0; i + 3 < xyxy.size(); i += 4)
  {
    g.PathMoveTo(xyxy[i], xyxy[i + 1]);
    g.PathLineTo(xyxy[i + 2], xyxy[i + 3]);
  }
  g.PathStroke(c, width, butt, &add);
}

class DiezelHerbertAnimator final : public ArtAnimator
{
public:
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    Index(r);
    if (!mFull.Ok(g))
      mFull.Build(g, owner, r, [&] {
        DrawDiezelHerbertBloom(g, r);
        DrawDiezelHerbertDischarge(g, r, mSegs);
      });
    if (!mBloom.Ok(g))
      mBloom.Build(g, owner, r, [&] { DrawDiezelHerbertBloom(g, r); });
    if (!mTree.Ok(g))
      mTree.Build(g, owner, r, [&] { DrawDiezelHerbertDischarge(g, r, mSegs); });
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest() || mSegs.empty())
    {
      mFull.DrawLit(g, r, m.bloom);
      return;
    }
    const float wake = m.Wake();
    const float t = static_cast<float>(m.clock);
    const float PIf = 3.14159265f;
    // The burned-in figure steps back where the charge is not; a pick flashes all of it.
    const float keep = 1.f - 0.55f * m.energy;
    const float bloomW = m.bloom * keep, flashW = 0.55f * m.pick;
    mBloom.DrawLit(g, r, m.bloom);
    mTree.Draw(g, r, keep);
    mTree.DrawAdd(g, r, std::sqrt(bloomW * bloomW + flashW * flashW));

    const IColor arc = Mix(kTeal, kDiezelWhite, 0.35f);
    float charge[2] = {0.f, 0.f};
    const float lift = wake * (0.65f + 0.35f * m.energy) * (0.9f + 0.2f * Noise1(t * 13.f, 50u));
    if (lift > 0.f)
    {
      for (std::vector<float>& v : mTierXY)
        v.clear();
      mHaloXY.clear();
      mPulseXY.clear();
      mFrontXY.clear();
      mSparks.clear();
      const float seesaw = static_cast<float>(std::fmod(m.clock * kSeesawHz, 1.0)) * 2.f * PIf;
      const float pulse = static_cast<float>(std::fmod(m.clock * kPulseHz, 1.0)) * 2.f * PIf;
      for (int root = 0; root < 2; ++root)
      {
        const float frac =
          std::clamp(0.12f + 0.63f * m.energy + 0.4f * m.pick + 0.09f * m.energy * std::sin(seesaw + root * PIf)
                       + 0.05f * wake * Noise1(t * 9.f, static_cast<uint32_t>(40 + root)),
                     0.f, 1.f);
        charge[root] = frac;
        Reveal(root, frac * mMaxDist[root] * 1.01f, pulse);
      }
      DiezelStrokeAdd(g, kTeal, 5.5f, mHaloXY, 0.45f * lift);
      DiezelStrokeAdd(g, arc, 1.6f, mTierXY[2], 0.5f * lift);
      DiezelStrokeAdd(g, arc, 2.1f, mTierXY[1], 0.65f * lift);
      DiezelStrokeAdd(g, arc, 3.f, mTierXY[0], 0.9f * lift);
      DiezelStrokeAdd(g, Mix(kTeal, kDiezelWhite, 0.6f), 2.2f, mPulseXY, (0.3f + 0.5f * m.energy) * lift);
      DiezelStrokeAdd(g, kDiezelWhite, 2.6f, mFrontXY, 0.85f * lift);
    }
    float flare[2] = {0.f, 0.f};
    DrawBolts(g, m, arc, flare);
    if (lift > 0.f)
      DrawSparks(g, t, lift);
    DrawTerminals(g, m, wake, charge, flare);
  }

  void DropLayers() override
  {
    mFull.Reset();
    mBloom.Reset();
    mTree.Reset();
  }

private:
  struct Spark
  {
    float x, y;
    uint32_t id;
  };

  static constexpr double kSeesawHz = 0.32;
  static constexpr double kPulseHz = 1.4;
  static constexpr float kPulseK = 6.2831853f / 64.f; // one pulse per 64 px of path
  static constexpr float kPulseBand = 0.55f;
  static constexpr float kFront = 24.f;
  static constexpr float kLeaderSpeed = 3200.f; // px per second along the bolt's channel
  static constexpr size_t kMaxSparks = 48;

  // Replays the growth, then indexes it: segments per root in path-distance order,
  // each root's reach, and its far leaves (the channels a bolt can take).
  void Index(const IRECT& r)
  {
    GrowDiezelDischarge(r, mSegs);
    mTermX = r.MW();
    mTermY[0] = r.B - 16.f;
    mTermY[1] = r.T + 16.f;
    const size_t n = mSegs.size();
    std::vector<char> hasChild(n, 0);
    for (int root = 0; root < 2; ++root)
    {
      mByDist[root].clear();
      mTips[root].clear();
      mMaxDist[root] = 0.f;
    }
    for (size_t i = 0; i < n; ++i)
    {
      const DiezelSeg& s = mSegs[i];
      if (s.up >= 0)
        hasChild[static_cast<size_t>(s.up)] = 1;
      mByDist[s.root].push_back(static_cast<int>(i));
      mMaxDist[s.root] = std::max(mMaxDist[s.root], s.d1);
    }
    for (int root = 0; root < 2; ++root)
    {
      std::vector<int>& order = mByDist[root];
      std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
        return mSegs[static_cast<size_t>(a)].d0 < mSegs[static_cast<size_t>(b)].d0;
      });
      int farthest = -1;
      for (const int i : order)
      {
        const DiezelSeg& s = mSegs[static_cast<size_t>(i)];
        if (!hasChild[static_cast<size_t>(i)] && s.d1 >= 0.55f * mMaxDist[root])
          mTips[root].push_back(i);
        if (farthest < 0 || s.d1 > mSegs[static_cast<size_t>(farthest)].d1)
          farthest = i;
      }
      if (mTips[root].empty() && farthest >= 0)
        mTips[root].push_back(farthest);
    }
    for (std::vector<float>& v : mTierXY)
      v.reserve(4 * n);
    mHaloXY.reserve(4 * n);
    mPulseXY.reserve(4 * n);
    mFrontXY.reserve(4 * n);
    mSparks.reserve(n);
    mDots.reserve(3 * kMaxSparks);
    mPath.reserve(n);
    mBoltXY.reserve(2 * (n + 1));
  }

  static void Push(std::vector<float>& v, float x0, float y0, float x1, float y1)
  {
    v.push_back(x0);
    v.push_back(y0);
    v.push_back(x1);
    v.push_back(y1);
  }

  // Everything of one tree within `reach` of its terminal; the segment the front
  // is crossing ends where the front is.
  void Reveal(int root, float reach, float pulse)
  {
    const float frontFrom = reach - std::min(kFront, 0.5f * reach);
    for (const int idx : mByDist[root])
    {
      const DiezelSeg& s = mSegs[static_cast<size_t>(idx)];
      if (s.d0 >= reach)
        break;
      float x1 = s.x1, y1 = s.y1, d1 = s.d1;
      if (d1 > reach)
      {
        const float f = (reach - s.d0) / (s.d1 - s.d0);
        x1 = s.x0 + (s.x1 - s.x0) * f;
        y1 = s.y0 + (s.y1 - s.y0) * f;
        d1 = reach;
        mSparks.push_back({x1, y1, static_cast<uint32_t>(idx)});
      }
      Push(mTierXY[s.tier], s.x0, s.y0, x1, y1);
      if (s.tier < 2)
        Push(mHaloXY, s.x0, s.y0, x1, y1);
      if (d1 > frontFrom)
        Push(mFrontXY, s.x0, s.y0, x1, y1);
      else if (std::cos(kPulseK * 0.5f * (s.d0 + d1) - pulse) > kPulseBand)
        Push(mPulseXY, s.x0, s.y0, x1, y1);
    }
  }

  // Crackle at the fronts: a flickering spark on the tips the charge is crossing.
  void DrawSparks(IGraphics& g, float t, float lift)
  {
    const size_t n = mSparks.size();
    if (n == 0)
      return;
    const float keepFrac = n > kMaxSparks ? static_cast<float>(kMaxSparks) / static_cast<float>(n) : 1.f;
    const IColor halo = Mix(kTeal, kBlue, 0.5f);
    for (int pass = 0; pass < 2; ++pass)
    {
      mDots.clear();
      size_t kept = 0;
      for (const Spark& s : mSparks)
      {
        if (kept >= kMaxSparks)
          break;
        if (keepFrac < 1.f && Hash01(s.id * 2654435761u ^ 14u) >= keepFrac)
          continue;
        ++kept;
        const float f = 0.5f + Noise1(t * 20.f, s.id);
        if (f < 0.3f || (pass == 1 && f <= 0.6f))
          continue;
        mDots.push_back(s.x);
        mDots.push_back(s.y);
        mDots.push_back(pass == 0 ? 2.5f + 2.5f * f : 1.f + 0.6f * f);
      }
      const IBlend add(EBlend::Add, std::min(1.f, (pass == 0 ? 0.5f : 0.95f) * lift));
      BatchDots(g, pass == 0 ? halo : kDiezelWhite, mDots.data(), static_cast<int>(mDots.size() / 3), &add);
    }
  }

  // One main bolt per pick along a root-to-far-leaf channel: it leaps out to the
  // leaf, flickers, then draws back into its terminal. Gone by kMaxEventAge.
  void DrawBolts(IGraphics& g, const ArtMotion& m, const IColor& arc, float flare[2])
  {
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float a = m.onsetAge[k];
      if (a >= kMaxEventAge || m.onsets < static_cast<uint32_t>(k + 1))
        continue;
      const uint32_t id = m.onsets - static_cast<uint32_t>(k);
      const uint32_t hsh = id * 747796405u + m.seed;
      const int root = Hash01(hsh) < 0.5f ? 0 : 1;
      const std::vector<int>& tips = mTips[root];
      if (tips.empty())
        continue;
      const size_t pickTip = static_cast<size_t>(Hash01(hsh ^ 0x9e3779b9u) * static_cast<float>(tips.size()));
      const int tip = tips[std::min(tips.size() - 1, pickTip)];
      const float full = mSegs[static_cast<size_t>(tip)].d1;
      const float reach = std::min(full, kLeaderSpeed * a) * (1.f - Smoothstep(0.3f, 1.25f, a));
      const float env = (0.5f + 0.5f * std::exp(-4.f * a)) * (0.82f + 0.36f * Noise1(a * 24.f, id))
                        * (1.f - Smoothstep(0.85f, 1.35f, a));
      if (reach <= 0.5f || env <= 0.f)
        continue;
      mPath.clear();
      for (int s = tip; s >= 0; s = mSegs[static_cast<size_t>(s)].up)
        mPath.push_back(s);
      mBoltXY.clear();
      const DiezelSeg& first = mSegs[static_cast<size_t>(mPath.back())];
      mBoltXY.push_back(first.x0);
      mBoltXY.push_back(first.y0);
      for (auto it = mPath.rbegin(); it != mPath.rend(); ++it)
      {
        const DiezelSeg& s = mSegs[static_cast<size_t>(*it)];
        if (s.d1 <= reach)
        {
          mBoltXY.push_back(s.x1);
          mBoltXY.push_back(s.y1);
          continue;
        }
        const float f = (reach - s.d0) / (s.d1 - s.d0);
        mBoltXY.push_back(s.x0 + (s.x1 - s.x0) * f);
        mBoltXY.push_back(s.y0 + (s.y1 - s.y0) * f);
        break;
      }
      const int n = static_cast<int>(mBoltXY.size() / 2);
      const IBlend halo(EBlend::Add, std::min(1.f, 0.5f * env));
      const IBlend body(EBlend::Add, std::min(1.f, 0.75f * env));
      const IBlend core(EBlend::Add, std::min(1.f, env));
      BatchPolyline(g, kBlue, 7.f, mBoltXY.data(), n, &halo);
      BatchPolyline(g, arc, 3.f, mBoltXY.data(), n, &body);
      BatchPolyline(g, kDiezelWhite, 1.4f, mBoltXY.data(), n, &core);
      GlowDotAdd(g, kBlue, kDiezelWhite, mBoltXY[2 * (n - 1)], mBoltXY[2 * (n - 1) + 1], 1.4f, 5.f, env);
      flare[root] = std::max(flare[root], env * std::exp(-3.f * a));
    }
  }

  // The terminals charge with the level and flare on a pick and on their bolts.
  void DrawTerminals(IGraphics& g, const ArtMotion& m, float wake, const float charge[2], const float flare[2])
  {
    for (int k = 0; k < 2; ++k)
    {
      const float tx = mTermX, ty = mTermY[k];
      const float glow = std::min(1.f, wake * (0.35f + 0.65f * charge[k]) + flare[k]);
      BloomAdd(g, tx, ty, 16.f + 18.f * m.energy + 22.f * m.pick + 30.f * flare[k], kTeal, 0.5f, glow);
      GlowDotAdd(g, kTeal, kDiezelWhite, tx, ty, 2.6f + 3.f * m.pick + 1.5f * flare[k], 5.f + 4.f * m.energy,
                 std::min(1.f, wake + flare[k]));
      const float goldW = std::min(1.f, 0.85f * (0.4f + 0.6f * m.energy) * std::max(wake, flare[k]));
      if (goldW > 0.f)
      {
        const IBlend gold(EBlend::Add, goldW);
        g.FillCircle(kGoldHi, tx, ty, 1.2f + 0.8f * m.pick, &gold);
      }
    }
  }

  ArtLayer mFull;
  ArtLayer mBloom;
  ArtLayer mTree;
  std::vector<DiezelSeg> mSegs;
  std::vector<int> mByDist[2];
  std::vector<int> mTips[2];
  float mMaxDist[2] = {0.f, 0.f};
  float mTermX = 0.f;
  float mTermY[2] = {0.f, 0.f};
  std::vector<float> mTierXY[3];
  std::vector<float> mHaloXY;
  std::vector<float> mPulseXY;
  std::vector<float> mFrontXY;
  std::vector<Spark> mSparks;
  std::vector<float> mDots;
  std::vector<int> mPath;
  std::vector<float> mBoltXY;
};

inline std::unique_ptr<ArtAnimator> MakeDiezelHerbertAnimator()
{
  return std::make_unique<DiezelHerbertAnimator>();
}
} // namespace volumart::art
