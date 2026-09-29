#pragma once

// Sebago Texas Flood (amp 12, fractal case 11): "Clifford Nebula".
// Hero art: Sebago Texas Flood - Clifford Nebula (attractor over bloom+dust, teal->gold density, glowing core)
// PLAY motion (class A): the attractor's own orbit re-traced as glitter: a head walks the orbit
// back and forth, each point it passes flares and twinkles out, so sparkle follows the nebula's
// density and its hue tides teal <-> gold; with the level, slow sheets of rain drift through it.
// A pick flashes a star in the core whose light echo sweeps out through the filaments (< 1.5 s).

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
inline void DrawSebagoHero(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float R = std::min(w, h);
  Bloom(g, cx, cy, R * 0.5f, kTeal, 0.16f);
  unsigned ds = 5u; Dust(g, ds, cx, cy, R * 0.5f, 70);
  const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
  double X = 0.0, Y = 0.0; const float S = R * 0.22f; const int N = 9000;
  for (int i = 0; i < N; i++)
  {
    double nx = sin(a * Y) + c * cos(a * X), ny = sin(b * X) + d * cos(b * Y);
    X = nx; Y = ny; if (i <= 120) continue;
    const float px = cx + (float)X * S, py = cy - (float)Y * S;
    if (px < rect.L || px > rect.R || py < rect.T || py > rect.B) continue;
    const float f = (float)i / (float)N;
    g.FillCircle(WithA(Mix(kTeal, kGold, f), 0.35f), px, py, 0.9f);
  }
  // clang-format on
}

class SebagoAnimator final : public AdditiveArtAnimator<&DrawSebagoHero>
{
  struct Lit
  {
    float x, y;
    int tier;
    bool warm;
  };

protected:
  static constexpr double kGlitterRate = 700.0; // orbit points per motion-second
  static constexpr double kSheetOmega = 0.9; // rain-sheet phase, radians per motion-second
  static constexpr int kMaxGlitter = 440;
  static constexpr int kGlitterTiers = 4;
  static constexpr int kMaxEcho = 520;
  static constexpr int kEchoTiers = 3;
  static constexpr float kEchoKeep = 0.14f; // share of the orbit points an echo may light
  static constexpr float kShellPx = 14.f;

  // The hero's own orbit: same formula, same skip and clip, exact base positions and f.
  void PrepareExtras(const IRECT& r) override
  {
    const float cx = r.MW(), cy = r.MH(), R = std::min(r.W(), r.H());
    mCx = cx;
    mCy = cy;
    mR = R;
    mX.clear();
    mY.clear();
    mF.clear();
    mCore.clear();
    mEchoPts.clear();
    const double a = -1.4, b = 1.6, c = 1.0, d = 0.75;
    double X = 0.0, Y = 0.0;
    const float S = R * 0.22f;
    const int N = 9000;
    for (int i = 0; i < N; i++)
    {
      const double nx = std::sin(a * Y) + c * std::cos(a * X), ny = std::sin(b * X) + d * std::cos(b * Y);
      X = nx;
      Y = ny;
      if (i <= 120)
        continue;
      const float px = cx + (float)X * S, py = cy - (float)Y * S;
      if (px < r.L || px > r.R || py < r.T || py > r.B)
        continue;
      mX.push_back(px);
      mY.push_back(py);
      mF.push_back((float)i / (float)N);
    }
    const int n = static_cast<int>(mX.size());
    std::vector<std::pair<float, int>> ranked;
    for (int k = 0; k < n; ++k)
    {
      if (std::hypot(mX[k] - cx, mY[k] - cy) < 0.3f * R)
        mCore.push_back(k);
      const float h = Hash01(static_cast<uint32_t>(k) * 2654435761u ^ 11u);
      if (h < kEchoKeep)
        ranked.push_back({h, k});
    }
    if (mCore.empty())
      for (int k = 0; k < n; ++k)
        mCore.push_back(k);
    // Hash order: an echo that hits its dot cap has still lit an even sample of the cloud.
    std::sort(ranked.begin(), ranked.end());
    for (const auto& p : ranked)
      mEchoPts.push_back(p.second);
    mLit.reserve(kMaxGlitter + 2);
    mEcho.reserve(kMaxEcho);
    mDots.reserve(3 * (kMaxGlitter + kMaxEcho + 2));
  }

  // The cloud dims a little so its own light reads, as a storm cloud does under lightning.
  float BaseDim(const ArtMotion& m) const override { return 0.2f * m.Wake(); }

  void DrawExtras(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    (void)r;
    if (mX.size() < 2)
      return;
    DrawGlitter(g, m);
    DrawEchoes(g, m);
  }

private:
  // Walks the orbit (ping-pong, so the hue tides instead of snapping gold -> teal at the wrap);
  // each passed point rises over the first 12% of its life, then twinkles out.
  void DrawGlitter(IGraphics& g, const ArtMotion& m)
  {
    const float amp = std::min(1.f, m.Wake() * (0.3f + 0.7f * m.energy) + 0.35f * m.pick);
    if (amp <= 0.f)
      return;
    static const IColor kWhite(255, 240, 250, 252);
    const long long n = static_cast<long long>(mX.size());
    const long long period = 2 * (n - 1);
    auto pointAt = [n, period](long long j) {
      long long q = j % period;
      if (q < 0)
        q += period;
      return static_cast<size_t>(q < n ? q : period - q);
    };
    const float len = std::min(static_cast<float>(kMaxGlitter), 60.f + 240.f * m.energy + 160.f * m.pick);
    const double u = m.clock * kGlitterRate;
    const long long head = static_cast<long long>(std::floor(u));
    const long long first = static_cast<long long>(std::floor(u - static_cast<double>(len))) + 1;
    // Rain sheets: brightness bands falling down-right across the cloud, deeper with the level.
    const float depth = 0.5f * m.energy;
    const float sheetK = 6.2831853f / (0.9f * mR);
    const float sheetPh = static_cast<float>(std::fmod(m.clock * kSheetOmega, 6.283185307179586));
    const float dirX = 0.33f, dirY = 0.944f;
    mLit.clear();
    for (long long j = head; j >= first; --j)
    {
      const float x = static_cast<float>(u - static_cast<double>(j)) / len;
      const size_t k = pointAt(j);
      float e = Smoothstep(0.f, 0.12f, x) * std::pow(1.f - x, 1.4f);
      if (depth > 0.f)
      {
        const float s = (mX[k] - mCx) * dirX + (mY[k] - mCy) * dirY;
        e *= 1.f - depth * (0.5f - 0.5f * std::sin(sheetK * s - sheetPh));
      }
      if (e * amp < 0.04f)
        continue;
      mLit.push_back({mX[k], mY[k], std::min(kGlitterTiers - 1, static_cast<int>(e * kGlitterTiers)), false});
    }
    const IColor tide = Mix(kTeal, kGold, mF[pointAt(head)]);
    const IColor core = Mix(tide, kWhite, 0.4f);
    for (int t = 0; t < kGlitterTiers; ++t)
    {
      const float wt = std::min(1.f, amp * std::sqrt((t + 0.6f) / kGlitterTiers));
      if (t >= kGlitterTiers - 2)
        DrawTier(g, mLit, t, false, 2.4f + 0.8f * m.pick, tide, 0.5f * wt);
      DrawTier(g, mLit, t, false, 1.05f + 0.35f * m.pick, core, wt);
    }
  }

  // Per pick: a star flares at an orbit point in the core, and its light echo, a shell
  // running out at 0.8 R/s, lights the filaments it crosses, dimming with distance.
  void DrawEchoes(IGraphics& g, const ArtMotion& m)
  {
    if (mCore.empty())
      return;
    static const IColor kWhite(255, 240, 250, 252);
    struct Star
    {
      float x, y, flash, age;
    };
    Star stars[kOnsetSlots];
    int nStars = 0;
    mEcho.clear();
    const float speed = 0.8f * mR, reach = 0.75f * mR;
    for (int s = 0; s < kOnsetSlots; ++s)
    {
      const float age = m.onsetAge[s];
      if (age >= kMaxEventAge)
        continue;
      const float fade = 1.f - Smoothstep(0.5f, kMaxEventAge, age);
      if (fade <= 0.f)
        continue;
      const uint32_t q = m.onsets - static_cast<uint32_t>(s);
      const size_t srcIdx = std::min(
        mCore.size() - 1, static_cast<size_t>(Hash01(q * 747796405u + m.seed) * static_cast<float>(mCore.size())));
      const int src = mCore[srcIdx];
      const float sx = mX[src], sy = mY[src], front = speed * age;
      const float gain = fade * (0.75f + 0.25f * m.energy);
      stars[nStars++] = {sx, sy, std::exp(-5.f * age) * fade, age};
      const float inner = std::max(0.f, front - 3.2f * kShellPx);
      for (int k : mEchoPts)
      {
        if (static_cast<int>(mEcho.size()) >= kMaxEcho)
          break;
        const float dx = mX[k] - sx, dy = mY[k] - sy, d2 = dx * dx + dy * dy;
        if (d2 >= front * front || d2 < inner * inner)
          continue;
        const float dist = std::sqrt(d2), behind = front - dist;
        const float v = gain * Smoothstep(0.f, 4.f, behind) * std::exp(-behind / kShellPx)
                        * (1.f - Smoothstep(0.25f * mR, reach, dist));
        if (v < 0.05f)
          continue;
        mEcho.push_back({mX[k], mY[k], std::min(kEchoTiers - 1, static_cast<int>(v * kEchoTiers)), mF[k] >= 0.5f});
      }
    }
    const IColor cool = Mix(kTeal, kWhite, 0.35f), warm = Mix(kGold, kGoldHi, 0.5f);
    for (int t = 0; t < kEchoTiers; ++t)
    {
      const float wt = std::sqrt((t + 0.6f) / kEchoTiers);
      if (t > 0)
      {
        DrawTier(g, mEcho, t, false, 2.6f, kTeal, 0.5f * wt);
        DrawTier(g, mEcho, t, true, 2.6f, kGold, 0.5f * wt);
      }
      DrawTier(g, mEcho, t, false, 1.15f, cool, wt);
      DrawTier(g, mEcho, t, true, 1.15f, warm, wt);
    }
    for (int s = 0; s < nStars; ++s)
    {
      const Star& st = stars[s];
      if (st.flash < 0.02f)
        continue;
      BloomAdd(g, st.x, st.y, mR * (0.08f + 0.2f * Smoothstep(0.f, 0.6f, st.age)), kGoldHi, 0.5f, st.flash);
      GlowDotAdd(g, kGold, kWhite, st.x, st.y, 1.f + 1.4f * st.flash, 2.5f + 5.5f * st.flash, st.flash);
    }
  }

  void DrawTier(IGraphics& g, const std::vector<Lit>& lit, int tier, bool warm, float radius, const IColor& c, float w)
  {
    if (w <= 0.f)
      return;
    mDots.clear();
    for (const Lit& l : lit)
      if (l.tier == tier && l.warm == warm)
      {
        mDots.push_back(l.x);
        mDots.push_back(l.y);
        mDots.push_back(radius);
      }
    const IBlend add(EBlend::Add, std::min(w, 1.f));
    BatchDots(g, c, mDots.data(), static_cast<int>(mDots.size() / 3), &add);
  }

  float mCx = 0.f, mCy = 0.f, mR = 1.f;
  std::vector<float> mX, mY, mF;
  std::vector<int> mCore;
  std::vector<int> mEchoPts;
  std::vector<Lit> mLit;
  std::vector<Lit> mEcho;
  std::vector<float> mDots;
};

inline std::unique_ptr<ArtAnimator> MakeSebagoAnimator()
{
  return std::make_unique<SebagoAnimator>();
}
} // namespace volumart::art
