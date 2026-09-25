#pragma once

// H&K TriAmp Mk2 (amp 5, fractal case 4): "Lissajous Nebula".
// Hero art: H&K TriAmp Mk2 - Lissajous Nebula (knot over bloom, shadow echo, gradient stroke, glowing core)
// PLAY motion (class A): three oscilloscope beams, one per TriAmp channel (clean
// teal, crunch violet-blue, lead gold), chase each other round the knot. The level sets
// how fast they run (through the clock) and how long their phosphor trails are; a
// pick flares the trails and the beam heads. Silence: the static knot, no beams.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawHkTriampHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, cy, R * 0.55f, kTeal, 0.16f);
  unsigned ds = 4u; Dust(g, ds, cx, cy, R * 0.5f, 70);
  auto knot = [&](float sc, IColor c0, IColor c1, float lw, bool gl) {
    std::vector<std::pair<float, float>> P;
    for (int i = 0; i <= 720; i++)
    {
      const float t = i * 6.28318f / 720.f;
      P.push_back({cx + sinf(3.f * t + 0.5f) * w * sc, cy + sinf(4.f * t) * h * sc * 1.06f});
    }
    GradStroke(g, P, c0, c1, lw, gl);
  };
  knot(0.34f, IColor(255, 60, 120, 150), kMid, 4.f, false);
  knot(0.4f, kTeal, kMid, 2.1f, true);
  // clang-format on
}

class HkTriampAnimator final : public AdditiveArtAnimator<&DrawHkTriampHero>
{
protected:
  static constexpr int kSamples = 720; // the main knot's own resolution
  static constexpr int kBeams = 3;
  static constexpr int kTiers = 8;
  static constexpr float kSamplesPerSecond = 110.f;

  // Same curve as knot(0.4f, ...) above.
  void PrepareExtras(const IRECT& r) override
  {
    const float cx = r.MW(), cy = r.MH(), w = r.W(), h = r.H();
    mKnot.resize(2 * kSamples);
    for (int i = 0; i < kSamples; i++)
    {
      const float t = i * 6.28318f / 720.f;
      mKnot[2 * i] = cx + sinf(3.f * t + 0.5f) * w * 0.4f;
      mKnot[2 * i + 1] = cy + sinf(4.f * t) * h * 0.4f * 1.06f;
    }
    mTrail.reserve(2 * (kSamples + 4));
  }

  // The trace dims under the beams, as a scope's phosphor does away from the beam.
  float BaseDim(const ArtMotion& m) const override { return 0.3f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    (void)r;
    const float wake = m.Wake();
    const float amp = std::min(1.f, wake * (0.55f + 0.45f * m.energy) + 0.35f * m.pick);
    if (amp <= 0.f || mKnot.empty())
      return;
    static const IColor kWhite(255, 240, 250, 252);
    // Clean, crunch, lead. Additive light saturates to white quickly, so the inks stay deep.
    const IColor beamCol[kBeams] = {Mix(kTeal, kWhite, 0.15f), IColor(255, 120, 160, 255), kGold};
    const float trail = 36.f + 90.f * m.energy + 70.f * m.pick;
    const double head0 = std::fmod(m.clock * kSamplesPerSecond, static_cast<double>(kSamples));
    // Butt caps: tiers meet end to end, so their joints do not add up into beads.
    IStrokeOptions tierStroke = RoundStroke();
    tierStroke.mCapOption = ELineCap::Butt;
    for (int b = 0; b < kBeams; ++b)
    {
      const float head = static_cast<float>(head0) + b * (kSamples / static_cast<float>(kBeams));
      const float tail = head - trail;
      const IColor& col = beamCol[b];
      Trail(tail, head);
      const IBlend glow(EBlend::Add, 0.5f * amp);
      g.PathClear();
      PathTrail(g);
      g.PathStroke(col, 8.f + 4.f * m.pick, RoundStroke(), &glow);
      for (int k = 0; k < kTiers; ++k)
      {
        const float f0 = static_cast<float>(k) / kTiers, f1 = static_cast<float>(k + 1) / kTiers;
        Trail(tail + trail * f0, tail + trail * f1);
        const IBlend add(EBlend::Add, std::min(0.85f, amp * std::pow(f1, 1.4f) * (0.8f + 0.25f * m.pick)));
        g.PathClear();
        PathTrail(g);
        g.PathStroke(col, 2.f + 1.2f * f1, tierStroke, &add);
      }
      float hx = 0.f, hy = 0.f;
      At(head, hx, hy);
      BloomAdd(g, hx, hy, 16.f + 10.f * m.energy + 12.f * m.pick, col, 0.55f, amp);
      GlowDotAdd(g, col, kWhite, hx, hy, 1.5f + 1.8f * m.pick, 4.f + 3.f * m.energy + 4.f * m.pick, amp);
    }
  }

private:
  void At(float s, float& x, float& y) const
  {
    const float wrapped = s - std::floor(s / kSamples) * kSamples;
    const int i0 = static_cast<int>(wrapped) % kSamples;
    const int i1 = (i0 + 1) % kSamples;
    const float f = wrapped - std::floor(wrapped);
    x = mKnot[2 * i0] + (mKnot[2 * i1] - mKnot[2 * i0]) * f;
    y = mKnot[2 * i0 + 1] + (mKnot[2 * i1 + 1] - mKnot[2 * i0 + 1]) * f;
  }

  // Polyline along the knot from sample position s0 to s1 (s1 > s0, any range).
  void Trail(float s0, float s1)
  {
    mTrail.clear();
    float x = 0.f, y = 0.f;
    At(s0, x, y);
    mTrail.push_back(x);
    mTrail.push_back(y);
    for (float s = std::floor(s0) + 1.f; s < s1; s += 1.f)
    {
      At(s, x, y);
      mTrail.push_back(x);
      mTrail.push_back(y);
    }
    At(s1, x, y);
    mTrail.push_back(x);
    mTrail.push_back(y);
  }

  void PathTrail(IGraphics& g) const
  {
    g.PathMoveTo(mTrail[0], mTrail[1]);
    for (size_t i = 2; i + 1 < mTrail.size(); i += 2)
      g.PathLineTo(mTrail[i], mTrail[i + 1]);
  }

  std::vector<float> mKnot;
  std::vector<float> mTrail;
};

inline std::unique_ptr<ArtAnimator> MakeHkTriampAnimator()
{
  return std::make_unique<HkTriampAnimator>();
}
} // namespace volumart::art
