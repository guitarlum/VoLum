#pragma once

// Lichtlaerm Prometheus (amp 6, fractal case 5): "Koch Deep".
// Hero art: Lichtlaerm Prometheus - Koch Deep (snowflake over bloom+dust, gradient stroke into gold, glowing core)
// PLAY motion (class A): the heart burns and its fire climbs the flake. Embers run out
// from the heart along all six lobe sides at once, reaching further toward the tips as the
// level rises; a pick throws a bright spark up every side, and the three tips flare with a
// six-ray crystal glint when it arrives. Silence: the static flake, no fire.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawLichtlaermHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w * 0.46f, h * 0.5f);
  Bloom(g, cx, cy, R * 1.2f, kTeal, 0.15f);
  unsigned ds = 7u; Dust(g, ds, cx, cy, R * 1.1f, 60);
  struct Seg { float x1, y1, x2, y2; };
  std::vector<Seg> segs;
  for (int i = 0; i < 3; i++)
  {
    const float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
    segs.push_back({cx + R * cosf(a1), cy + R * sinf(a1), cx + R * cosf(a2), cy + R * sinf(a2)});
  }
  for (int depth = 0; depth < 5; depth++)
  {
    std::vector<Seg> next;
    for (auto& s : segs)
    {
      const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
      const float ax = s.x1 + dx / 3.f, ay = s.y1 + dy / 3.f, bx = s.x1 + dx * 2.f / 3.f, by = s.y1 + dy * 2.f / 3.f;
      const float px2 = (s.x1 + s.x2) / 2.f - dy * 0.2887f, py2 = (s.y1 + s.y2) / 2.f + dx * 0.2887f;
      next.push_back({s.x1, s.y1, ax, ay}); next.push_back({ax, ay, px2, py2});
      next.push_back({px2, py2, bx, by}); next.push_back({bx, by, s.x2, s.y2});
    }
    segs = next;
  }
  std::vector<std::pair<float, float>> P; P.push_back({segs[0].x1, segs[0].y1});
  for (auto& s : segs) P.push_back({s.x2, s.y2});
  GradStroke(g, P, kTeal, kGoldHi, 1.9f, true);
  GlowDot(g, kGold, kGoldHi, cx, cy, 3.5f, 10.f);
  // clang-format on
}

// The curve is the anti-snowflake: each edge's first notch reaches the centre, so vertices
// 512, 1536 and 2560 sit on the heart and 0, 1024, 2048 are the tips. A "side" runs 512
// vertices from a heart crossing to a tip; the six sides are the three lobes' two flanks.
class LichtlaermAnimator final : public AdditiveArtAnimator<&DrawLichtlaermHero>
{
protected:
  static constexpr int kSegs = 3072; // 3 * 4^5, the hero's depth-5 curve
  static constexpr int kHalf = 512;
  static constexpr int kSides = 6;
  static constexpr int kTiers = 5;
  static constexpr int kEmbers = 3;
  static constexpr double kEmberSpeed = 0.3; // ember cycles per motion-second
  static constexpr float kEmberRun = 1.14f; // heads run past the tip until the trail (<= 0.13) has drained
  static constexpr float kSparkRise = 0.5f; // s, heart to tip
  static constexpr float kSparkLen = 0.2f;

  // Same curve as DrawLichtlaermHero; vertex k is its P[k].
  void PrepareExtras(const IRECT& r) override
  {
    const float cx = r.MW(), cy = r.MH(), w = r.W(), h = r.H();
    const float R = std::min(w * 0.46f, h * 0.5f);
    struct Seg
    {
      float x1, y1, x2, y2;
    };
    std::vector<Seg> segs;
    for (int i = 0; i < 3; i++)
    {
      const float a1 = (i * 120.f - 90.f) * 3.14159f / 180.f, a2 = ((i + 1) * 120.f - 90.f) * 3.14159f / 180.f;
      segs.push_back({cx + R * cosf(a1), cy + R * sinf(a1), cx + R * cosf(a2), cy + R * sinf(a2)});
    }
    for (int depth = 0; depth < 5; depth++)
    {
      std::vector<Seg> next;
      for (auto& s : segs)
      {
        const float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
        const float ax = s.x1 + dx / 3.f, ay = s.y1 + dy / 3.f, bx = s.x1 + dx * 2.f / 3.f, by = s.y1 + dy * 2.f / 3.f;
        const float px2 = (s.x1 + s.x2) / 2.f - dy * 0.2887f, py2 = (s.y1 + s.y2) / 2.f + dx * 0.2887f;
        next.push_back({s.x1, s.y1, ax, ay});
        next.push_back({ax, ay, px2, py2});
        next.push_back({px2, py2, bx, by});
        next.push_back({bx, by, s.x2, s.y2});
      }
      segs = next;
    }
    mV.clear();
    mV.reserve(2 * (kSegs + 1));
    mV.push_back(segs[0].x1);
    mV.push_back(segs[0].y1);
    for (auto& s : segs)
    {
      mV.push_back(s.x2);
      mV.push_back(s.y2);
    }
    mCx = cx;
    mCy = cy;
    mR = R;
  }

  float BaseDim(const ArtMotion& m) const override { return 0.15f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    (void)r;
    if (mV.size() < 2 * (kSegs + 1))
      return;
    const float wake = m.Wake(), E = m.energy, P = m.pick;
    const float t = static_cast<float>(m.clock);
    static const IColor kEmber(255, 178, 74, 34);

    // Embers: the level sets how bright they burn and how far up the lobes they live.
    const float emberAmp = wake * (0.35f + 0.65f * E);
    const float reach = 0.3f + E;
    const float emberLen = 0.05f + 0.08f * E;
    float tipGlow = 0.f;
    if (emberAmp > 0.f)
      for (int j = 0; j < kEmbers; ++j)
      {
        const double u = std::fmod(m.clock * kEmberSpeed + j / static_cast<double>(kEmbers), 1.0);
        const float head = static_cast<float>(u) * kEmberRun;
        const float flick = 0.85f + 0.3f * Noise1(t * 3.1f, static_cast<uint32_t>(40 + j));
        Comet(g, head, emberLen, kEmber, kGold, emberAmp * flick, reach, 7.f + 3.f * E, 3.f + 1.5f * E);
        tipGlow += 0.4f * E * E * wake * Smoothstep(0.93f, 1.f, head) * (1.f - Smoothstep(1.f, kEmberRun, head));
      }

    // Sparks: one per pick, fast off the heart and easing into the tips.
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kMaxEventAge)
        continue;
      const float x = age / kSparkRise;
      if (x < 1.4f)
      {
        const float vary = Hash01((m.onsets - static_cast<uint32_t>(k)) * 747796405u + m.seed);
        const IColor spark = Mix(kGold, kGoldHi, 0.2f + 0.4f * vary);
        Comet(g, x * (1.25f - 0.25f * x), kSparkLen, Mix(kEmber, kGold, 0.5f), spark, 0.95f, 100.f, 9.f, 4.f);
      }
      tipGlow +=
        Smoothstep(0.84f * kSparkRise, 1.04f * kSparkRise, age) * (1.f - Smoothstep(1.04f * kSparkRise, 1.4f, age));
    }
    tipGlow = std::min(1.f, tipGlow);
    if (tipGlow > 0.01f)
      DrawTips(g, tipGlow, t);

    // The heart burns with the level and flares on the pick.
    const float flick = 0.8f + 0.3f * Noise1(t * 5.3f, 11u) + 0.15f * Noise1(t * 13.7f, 23u);
    const float heat = std::min(1.f, wake * (0.4f + 0.6f * E) * flick + 0.7f * P);
    BloomAdd(g, mCx, mCy, mR * (0.2f + 0.1f * E + 0.12f * P), kGold, 0.6f, heat);
    GlowDotAdd(g, kGold, kGoldHi, mCx, mCy, 3.5f + 3.f * E + 2.f * P, 10.f + 8.f * E, heat);
  }

private:
  static float Heat(float s, float reach)
  {
    return Smoothstep(0.f, 0.035f, s) * (1.f - Smoothstep(0.5f * reach, reach, s));
  }

  int Index(int side, int j) const { return kHalf + 2 * kHalf * (side / 2) + (side % 2 == 0 ? j : -j); }

  void At(int side, float q, float& x, float& y) const
  {
    const float qc = std::clamp(q, 0.f, static_cast<float>(kHalf));
    const int j0 = std::min(static_cast<int>(qc), kHalf - 1);
    const float f = qc - static_cast<float>(j0);
    const int i0 = Index(side, j0), i1 = Index(side, j0 + 1);
    x = mV[2 * i0] + (mV[2 * i1] - mV[2 * i0]) * f;
    y = mV[2 * i0 + 1] + (mV[2 * i1 + 1] - mV[2 * i0 + 1]) * f;
  }

  // All six sides from s0 to s1 (0 = heart, 1 = tip) as one path, one stroke.
  void StrokeSides(IGraphics& g, float s0, float s1, const IColor& c, float width, const IStrokeOptions& opts,
                   const IBlend* blend, int stride) const
  {
    s0 = std::max(0.f, s0);
    s1 = std::min(1.f, s1);
    if (s1 <= s0)
      return;
    const float q0 = s0 * kHalf, q1 = s1 * kHalf;
    g.PathClear();
    for (int side = 0; side < kSides; ++side)
    {
      float x = 0.f, y = 0.f;
      At(side, q0, x, y);
      g.PathMoveTo(x, y);
      for (int j = static_cast<int>(q0) + stride; static_cast<float>(j) < q1; j += stride)
      {
        const int i = Index(side, j);
        g.PathLineTo(mV[2 * i], mV[2 * i + 1]);
      }
      At(side, q1, x, y);
      g.PathLineTo(x, y);
    }
    g.PathStroke(c, width, opts, blend);
  }

  // A comet on every side at once: a wide glow, then tiers brightening toward the head.
  void Comet(IGraphics& g, float head, float len, const IColor& tailCol, const IColor& headCol, float amp, float reach,
             float glowW, float headR) const
  {
    const float tail = head - len;
    if (amp <= 0.f || len <= 0.f || head <= 0.f || tail >= 1.f)
      return;
    static const IColor kHot(255, 255, 244, 214);
    const IBlend glow(EBlend::Add, std::min(1.f, 0.5f * amp * Heat(std::clamp(head - 0.4f * len, 0.f, 1.f), reach)));
    StrokeSides(g, tail, head, Mix(tailCol, headCol, 0.7f), glowW, RoundStroke(), &glow, 3);
    // Butt caps: tiers meet end to end, so their joints do not add up into beads.
    IStrokeOptions tierStroke = RoundStroke();
    tierStroke.mCapOption = ELineCap::Butt;
    for (int k = 0; k < kTiers; ++k)
    {
      const float f0 = static_cast<float>(k) / kTiers, f1 = static_cast<float>(k + 1) / kTiers;
      const float a = tail + len * f0, b = tail + len * f1;
      if (b <= 0.f || a >= 1.f)
        continue;
      const float alpha = std::min(0.9f, amp * std::pow(f1, 1.4f) * Heat(std::clamp(0.5f * (a + b), 0.f, 1.f), reach));
      if (alpha < 0.01f)
        continue;
      const IBlend add(EBlend::Add, alpha);
      StrokeSides(g, a, b, Mix(tailCol, headCol, f1), 1.6f + 1.4f * f1, tierStroke, &add, 1);
    }
    const float ha = std::min(1.f, amp * Heat(head, reach) * (1.f - Smoothstep(0.96f, 1.f, head)));
    if (ha < 0.01f)
      return;
    float halo[3 * kSides], core[3 * kSides];
    for (int side = 0; side < kSides; ++side)
    {
      At(side, head * kHalf, halo[3 * side], halo[3 * side + 1]);
      halo[3 * side + 2] = headR;
      core[3 * side] = halo[3 * side];
      core[3 * side + 1] = halo[3 * side + 1];
      core[3 * side + 2] = 0.35f * headR;
    }
    const IBlend add(EBlend::Add, ha);
    BatchDots(g, WithA(headCol, 0.6f), halo, kSides, &add);
    BatchDots(g, kHot, core, kSides, &add);
  }

  // The three tips catch the fire: a warm glow inside the tip and a six-ray crystal glint.
  void DrawTips(IGraphics& g, float glow, float t) const
  {
    static const IColor kHot(255, 255, 244, 214);
    const float c30 = 0.8660254f;
    const float dirs[3][2] = {{0.f, 1.f}, {c30, 0.5f}, {-c30, 0.5f}};
    float rays[3 * 3 * 4], cores[3 * 3 * 4], halo[9], core[9];
    for (int i = 0; i < 3; ++i)
    {
      const int v = i * 2 * kHalf;
      const float tx = mV[2 * v], ty = mV[2 * v + 1];
      const float ox = tx - mCx, oy = ty - mCy, ol = std::max(1.f, std::hypot(ox, oy));
      // Pulled inward so the top tip, which sits on the paint rect's edge, is not cut mid-glow.
      const float bR = 12.f + 16.f * glow;
      BloomAdd(g, tx - ox / ol * bR * 0.45f, ty - oy / ol * bR * 0.45f, bR, kGold, 0.55f, glow);
      const float L = (4.f + 14.f * glow) * (0.85f + 0.3f * Noise1(t * 7.f, static_cast<uint32_t>(60 + i)));
      for (int d = 0; d < 3; ++d)
      {
        float* ray = rays + 4 * (3 * i + d);
        float* rc = cores + 4 * (3 * i + d);
        ray[0] = tx - dirs[d][0] * L;
        ray[1] = ty - dirs[d][1] * L;
        ray[2] = tx + dirs[d][0] * L;
        ray[3] = ty + dirs[d][1] * L;
        rc[0] = tx - dirs[d][0] * L * 0.55f;
        rc[1] = ty - dirs[d][1] * L * 0.55f;
        rc[2] = tx + dirs[d][0] * L * 0.55f;
        rc[3] = ty + dirs[d][1] * L * 0.55f;
      }
      halo[3 * i] = core[3 * i] = tx;
      halo[3 * i + 1] = core[3 * i + 1] = ty;
      halo[3 * i + 2] = 2.5f + 6.f * glow;
      core[3 * i + 2] = 1.f + 1.2f * glow;
    }
    const IBlend rayGlow(EBlend::Add, 0.6f * glow), rayCore(EBlend::Add, std::min(1.f, 0.9f * glow));
    BatchLines(g, kGold, 2.5f, rays, 9, &rayGlow);
    BatchLines(g, kHot, 1.2f, cores, 9, &rayCore);
    const IBlend dotAdd(EBlend::Add, std::min(1.f, 0.85f * glow));
    BatchDots(g, WithA(kGold, 0.7f), halo, 3, &dotAdd);
    BatchDots(g, kHot, core, 3, &dotAdd);
  }

  std::vector<float> mV;
  float mCx = 0.f, mCy = 0.f, mR = 0.f;
};

inline std::unique_ptr<ArtAnimator> MakeLichtlaermAnimator()
{
  return std::make_unique<LichtlaermAnimator>();
}
} // namespace volumart::art
