#pragma once

// Soldano SLO100 (amp 13, fractal case 12): "Beacon Sweep".
// Hero art: Soldano SLO100 - Beacon Sweep (Burning Ship "sea" lit by a lighthouse's rotating double beam)
// PLAY motion (class B, crossfade to live): the lighthouse wakes and turns. The
// beams sweep round the beacon (one turn per ~7.5 motion-seconds, so a light
// touch turns it slowly), widen and lengthen as they swing toward the viewer,
// and light the Burning Ship's wave crests they pass over. A pick flares the
// lantern. The sea is cached; only the beams, glints and lantern are live.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
struct SoldanoBeacon
{
  float bx, by, R;
};

inline SoldanoBeacon SoldanoBeaconAt(const IRECT& rect)
{
  const float w = rect.W(), h = rect.H();
  return {rect.MW(), rect.T + h * 0.44f, std::min(w, h)};
}

// The Burning Ship sea under the beams (everything the beams are drawn over).
inline void DrawSoldanoSea(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  const float bx = cx, by = rect.T + h * 0.44f; // beacon
  Bloom(g, bx, by, R * 0.5f, kTeal, 0.08f);
  const float step = 2.f, pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
  for (float px = 0; px < pw; px += step)
    for (float py = 0; py < ph; py += step)
    {
      double cr = -1.75 + (px / pw) * 0.15, ci = -0.08 + (py / ph) * 0.12;
      double zr = 0, zi = 0; int it = 0;
      while (zr * zr + zi * zi < 4.0 && it < 64) { double t = zr * zr - zi * zi + cr; zi = fabs(2.0 * zr * zi) + ci; zr = t; it++; }
      if (it < 64 && it > 3)
      {
        // Deeper dark contrast on the "sea": squared curve pushes mids/darks toward near-black
        // ink while the teal highlight peak stays where it was (not brighter).
        const float f = (float)it / 64.f, fc = f * f;
        static const IColor kInk(255, 10, 20, 28);
        g.FillCircle(WithA(Mix(kInk, kTeal, fc), 0.32f + 0.52f * fc), pl + px, pt + py, step * 0.8f);
      }
    }
  // clang-format on
}

// Volumetric light cone: gradient triangle (beam colour -> transparent) from the beacon.
inline void DrawSoldanoBeamCone(IGraphics& g, float bx, float by, float ang, float spread, float len, float a,
                                const IBlend* blend = nullptr)
{
  const IColor beam = kGold; // hero is always large -> gold light
  // clang-format off
  const float ex = bx + cosf(ang) * len, ey = by + sinf(ang) * len;
  g.PathTriangle(bx, by, bx + cosf(ang - spread) * len, by + sinf(ang - spread) * len,
                 bx + cosf(ang + spread) * len, by + sinf(ang + spread) * len);
  g.PathFill(IPattern::CreateLinearGradient(bx, by, ex, ey,
                                            {{WithA(beam, a), 0.f},
                                             {WithA(beam, a * 0.25f), 0.7f},
                                             {IColor(0, beam.R, beam.G, beam.B), 1.f}}), IFillOptions(), blend);
  // clang-format on
}

// The resting beams and lantern; `fade` < 1 only while the live beams take over.
inline void DrawSoldanoBeams(IGraphics& g, const IRECT& rect, float fade = 1.f, bool lantern = true)
{
  const SoldanoBeacon b = SoldanoBeaconAt(rect);
  const float bx = b.bx, by = b.by, R = b.R;
  const IColor beam = kGold;
  const float PIf = 3.14159f;
  DrawSoldanoBeamCone(g, bx, by, PIf * 0.14f, 0.055f, R * 1.0f, 0.085f * fade); // main beam (dimmed)
  DrawSoldanoBeamCone(g, bx, by, PIf * 0.14f + PIf, 0.055f, R * 0.9f, 0.038f * fade); // opposing back beam
  DrawSoldanoBeamCone(g, bx, by, PIf * 0.86f, 0.05f, R * 0.85f, 0.048f * fade); // secondary sweep
  if (lantern)
    GlowDot(g, beam, kGoldHi, bx, by, 2.2f, 6.f);
}

inline void DrawSoldanoHero(IGraphics& g, const IRECT& rect)
{
  DrawSoldanoSea(g, rect);
  DrawSoldanoBeams(g, rect);
}

class SoldanoAnimator final : public ArtAnimator
{
public:
  // The rest layer is the sea copied 1:1 plus the beams drawn inside the layer,
  // the same pixels and commands as the static art, so silence is bit-exact.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mSea.Ok(g))
      mSea.Build(g, owner, r, [&] { DrawSoldanoSea(g, r); });
    if (!mRest.Ok(g))
      mRest.Build(g, owner, r, [&] {
        mSea.Draw(g, r);
        DrawSoldanoBeams(g, r);
      });
    FindCrests(r);
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    const float wake = m.Wake();
    if (m.IsRest() || wake <= 0.f)
    {
      mRest.DrawLit(g, r, m.bloom);
      return;
    }
    mSea.DrawLit(g, r, m.bloom);
    const SoldanoBeacon b = SoldanoBeaconAt(r);
    const float PIf = 3.14159f;
    const float rotor = PIf * 0.14f + static_cast<float>(std::fmod(m.clock * kTurnPerSecond, 6.283185307179586));
    const float lift = 1.f + 0.9f * m.energy + 0.6f * m.pick;
    struct Beam
    {
      float ang, spread, len, a, light;
    };
    Beam beams[3] = {{rotor, 0.055f, b.R * 1.0f, 0.085f, 1.f},
                     {rotor + PIf, 0.055f, b.R * 0.9f, 0.038f, 0.6f},
                     {rotor + 0.72f * PIf, 0.05f, b.R * 0.85f, 0.048f, 0.7f}};
    mLit.clear();
    for (Beam& beam : beams)
    {
      // A beam swinging down the frame points at the viewer across the water: wider, and
      // shorter where it runs up the frame into the distance.
      const float toward = std::max(0.f, std::sin(beam.ang));
      beam.spread *= 1.f + 0.6f * toward + 0.25f * m.pick;
      beam.len *= (0.74f + 0.26f * std::abs(std::cos(beam.ang))) * (1.f + 0.12f * m.energy);
      LightCrests(beam.ang, beam.spread * 3.2f, beam.len, wake * beam.light * (0.55f + 0.45f * m.energy), m.clock);
    }
    DrawGlints(g);
    if (wake < 1.f)
      DrawSoldanoBeams(g, r, 1.f - wake, false);
    for (const Beam& beam : beams)
      DrawSoldanoBeamCone(g, b.bx, b.by, beam.ang, beam.spread, beam.len, std::min(0.3f, beam.a * lift) * wake);
    // Each beam's bright core is light, so it is added: it brightens the water it
    // crosses instead of veiling it.
    for (const Beam& beam : beams)
    {
      const IBlend core(EBlend::Add, wake * beam.light * (0.6f + 0.4f * m.energy));
      DrawSoldanoBeamCone(g, b.bx, b.by, beam.ang, beam.spread * 0.35f, beam.len * 0.95f, 0.5f + 0.2f * m.pick, &core);
    }
    // The lantern flashes when the main beam swings toward the viewer, and on a pick.
    const float facing = std::pow(std::max(0.f, std::sin(beams[0].ang)), 6.f);
    BloomAdd(g, b.bx, b.by, b.R * (0.16f + 0.08f * m.pick), kGoldHi, 0.45f * facing + 0.5f * m.pick + 0.15f, wake);
    GlowDot(g, kGold, kGoldHi, b.bx, b.by, 2.2f + 1.6f * m.pick * wake, 6.f + (3.f * m.energy + 4.f * m.pick) * wake);
  }

  void DropLayers() override
  {
    mSea.Reset();
    mRest.Reset();
  }

private:
  static constexpr double kTurnPerSecond = 6.283185307179586 / 7.5;
  static constexpr int kMaxCrests = 1800;
  static constexpr int kMaxLit = 240;
  static constexpr int kTiers = 3;

  struct Crest
  {
    float theta, x, y, dist, fc;
    uint32_t id;
  };

  // The sea's brightest samples, on every other sample of the base step-2 grid,
  // sorted by angle round the beacon so a beam finds its crests by binary search.
  void FindCrests(const IRECT& rect)
  {
    mCrests.clear();
    mTheta.clear();
    const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
    const SoldanoBeacon b = SoldanoBeaconAt(rect);
    const float step = 2.f, pw = w * 0.82f, ph = h * 0.82f, pl = cx - pw / 2.f, pt = cy - ph / 2.f;
    uint32_t id = 0;
    for (float px = 0; px < pw; px += 2.f * step)
      for (float py = 0; py < ph; py += 2.f * step)
      {
        double cr = -1.75 + (px / pw) * 0.15, ci = -0.08 + (py / ph) * 0.12;
        double zr = 0, zi = 0;
        int it = 0;
        while (zr * zr + zi * zi < 4.0 && it < 64)
        {
          double t = zr * zr - zi * zi + cr;
          zi = fabs(2.0 * zr * zi) + ci;
          zr = t;
          it++;
        }
        ++id;
        if (it >= 64 || it <= 3)
          continue;
        const float f = (float)it / 64.f, fc = f * f;
        if (fc < 0.18f)
          continue;
        const float x = pl + px, y = pt + py;
        mCrests.push_back({std::atan2(y - b.by, x - b.bx), x, y, std::hypot(x - b.bx, y - b.by), fc, id});
      }
    if (static_cast<int>(mCrests.size()) > kMaxCrests)
    {
      const float keep = static_cast<float>(kMaxCrests) / static_cast<float>(mCrests.size());
      mCrests.erase(std::remove_if(mCrests.begin(), mCrests.end(),
                                   [keep](const Crest& c) { return Hash01(c.id * 2654435761u ^ 12u) > keep; }),
                    mCrests.end());
    }
    std::sort(mCrests.begin(), mCrests.end(), [](const Crest& a, const Crest& c) { return a.theta < c.theta; });
    for (const Crest& c : mCrests)
      mTheta.push_back(c.theta);
    mLit.reserve(kMaxLit);
    mDots.reserve(3 * kMaxLit);
  }

  void LightCrests(float axis, float halfWidth, float len, float gain, double clock)
  {
    if (mCrests.empty() || gain <= 0.f)
      return;
    const float PIf = 3.14159265f;
    axis = std::atan2(std::sin(axis), std::cos(axis));
    auto scan = [&](float lo, float hi) {
      const auto first = std::lower_bound(mTheta.begin(), mTheta.end(), lo);
      for (auto it = first; it != mTheta.end() && *it <= hi; ++it)
      {
        if (static_cast<int>(mLit.size()) >= kMaxLit)
          return;
        const Crest& c = mCrests[static_cast<size_t>(it - mTheta.begin())];
        if (c.dist >= len)
          continue;
        float d = c.theta - axis;
        d = std::atan2(std::sin(d), std::cos(d));
        const float across = 1.f - std::abs(d) / halfWidth;
        if (across <= 0.f)
          continue;
        const float twinkle = 0.7f + 0.6f * Noise1(static_cast<float>(clock) * 5.f, c.id);
        const float a = gain * (0.35f + 0.65f * c.fc) * std::sqrt(across) * std::sqrt(1.f - c.dist / len) * twinkle;
        if (a > 0.02f)
          mLit.push_back({c.x, c.y, std::min(a, 1.f)});
      }
    };
    const float lo = axis - halfWidth, hi = axis + halfWidth;
    scan(std::max(lo, -PIf), std::min(hi, PIf));
    if (lo < -PIf)
      scan(lo + 2.f * PIf, PIf);
    if (hi > PIf)
      scan(-PIf, hi - 2.f * PIf);
  }

  // Lit crests in three brightness tiers: one halo fill and one core fill per tier.
  void DrawGlints(IGraphics& g)
  {
    const IColor glint = Mix(kTeal, kGoldHi, 0.6f);
    static const IColor kHot(255, 255, 246, 214);
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
            mDots.push_back(pass == 0 ? 3.8f : 1.35f);
          }
        const int n = static_cast<int>(mDots.size() / 3);
        const IBlend add(EBlend::Add, pass == 0 ? 0.35f + 0.35f * hi : std::min(1.f, 0.5f + 0.5f * hi));
        BatchDots(g, pass == 0 ? glint : kHot, mDots.data(), n, &add);
      }
    }
  }

  struct Lit
  {
    float x, y, a;
  };

  ArtLayer mSea;
  ArtLayer mRest;
  std::vector<Crest> mCrests;
  std::vector<float> mTheta;
  std::vector<Lit> mLit;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeSoldanoAnimator()
{
  return std::make_unique<SoldanoAnimator>();
}
} // namespace volumart::art
