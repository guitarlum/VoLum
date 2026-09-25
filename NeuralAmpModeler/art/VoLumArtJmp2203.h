#pragma once

// Marshall JMP 2203 (amp 8, fractal case 7): "Depth Triforce".
// Hero art: Marshall JMP 2203 - Depth Triforce (stacked offset triforce outlines receding teal->gold)
// PLAY motion (class B, live stack): the Triforce charges up. The level telescopes the stack
// deeper and lets its far end circle slowly round the gold front (parallax), while a band of
// light climbs from the gold front to the teal back. A pick punches the depth, flares the front
// and sends a gold echo receding through the stack, ringing each layer it passes.

#include "VoLumArtAnimator.h"
#include "VoLumArtCommon.h"

namespace volumart::art
{
// One triforce outline: three sub-triangles, three edges each.
inline void DrawJmp2203TriOutline(IGraphics& g, float ccx, float ccy, float S, const IColor& col, float thick,
                                  bool glow, float glowW = 5.f)
{
  // clang-format off
  using namespace volumart;
  auto M = [](float x1, float y1, float x2, float y2) { return std::make_pair((x1 + x2) / 2.f, (y1 + y2) / 2.f); };
  const float Ax = ccx, Ay = ccy - S, Bx = ccx - S * 0.866f, By = ccy + S * 0.5f, Cx = ccx + S * 0.866f, Cy = ccy + S * 0.5f;
  const auto mAB = M(Ax, Ay, Bx, By), mAC = M(Ax, Ay, Cx, Cy), mBC = M(Bx, By, Cx, Cy);
  struct Tri { float x1, y1, x2, y2, x3, y3; };
  const Tri T[3] = {{Ax, Ay, mAB.first, mAB.second, mAC.first, mAC.second},
                    {mAB.first, mAB.second, Bx, By, mBC.first, mBC.second},
                    {mAC.first, mAC.second, mBC.first, mBC.second, Cx, Cy}};
  for (auto& t : T)
  {
    if (glow)
    {
      GlowLine(g, kGold, col, t.x1, t.y1, t.x2, t.y2, thick, glowW);
      GlowLine(g, kGold, col, t.x2, t.y2, t.x3, t.y3, thick, glowW);
      GlowLine(g, kGold, col, t.x3, t.y3, t.x1, t.y1, thick, glowW);
    }
    else
    {
      g.DrawLine(col, t.x1, t.y1, t.x2, t.y2, nullptr, thick);
      g.DrawLine(col, t.x2, t.y2, t.x3, t.y3, nullptr, thick);
      g.DrawLine(col, t.x3, t.y3, t.x1, t.y1, nullptr, thick);
    }
  }
  // clang-format on
}

inline void DrawJmp2203Bloom(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float cy2 = cy + h * 0.02f, R = std::min(w, h);
  Bloom(g, cx, cy2, R * 0.5f, kGold, 0.10f);
  // clang-format on
}

inline void DrawJmp2203Stack(IGraphics& g, const IRECT& rect)
{
  [[maybe_unused]] const float cx = rect.MW(), cy = rect.MH(), w = rect.W(), h = rect.H();
  // clang-format off
  using namespace volumart;
  const float cy2 = cy + h * 0.02f, baseS = std::min(w * 0.34f, h * 0.42f), R = std::min(w, h);
  const int K = 6;
  for (int i = K - 1; i >= 0; --i) // back (teal, small, high) -> front (gold, full, glowing)
  {
    const float dt = (float)i / (float)(K - 1);
    const float S = baseS * (1.f - dt * 0.14f), off = dt * R * 0.07f;
    const IColor col = WithA(Mix(kGold, kTeal, dt), 0.85f - dt * 0.5f);
    DrawJmp2203TriOutline(g, cx, cy2 - off, S, col, i == 0 ? 2.2f : 1.2f, i == 0);
  }
  // clang-format on
}

inline void DrawJmp2203Hero(IGraphics& g, const IRECT& rect)
{
  DrawJmp2203Bloom(g, rect);
  DrawJmp2203Stack(g, rect);
}

// The same outline as one additive path (three closed sub-triangles); adds about col * amount.
inline void AddJmp2203Triforce(IGraphics& g, float ccx, float ccy, float S, const IColor& col, float width,
                               float amount)
{
  if (amount <= 0.005f)
    return;
  const float Ax = ccx, Ay = ccy - S, Bx = ccx - S * 0.866f, By = ccy + S * 0.5f, Cx = ccx + S * 0.866f,
              Cy = ccy + S * 0.5f;
  const float abx = (Ax + Bx) / 2.f, aby = (Ay + By) / 2.f, acx = (Ax + Cx) / 2.f, acy = (Ay + Cy) / 2.f;
  const float bcx = (Bx + Cx) / 2.f, bcy = (By + Cy) / 2.f;
  const float tri[3][6] = {{Ax, Ay, abx, aby, acx, acy}, {abx, aby, Bx, By, bcx, bcy}, {acx, acy, bcx, bcy, Cx, Cy}};
  g.PathClear();
  for (const auto& t : tri)
  {
    g.PathMoveTo(t[0], t[1]);
    g.PathLineTo(t[2], t[3]);
    g.PathLineTo(t[4], t[5]);
    g.PathClose();
  }
  const IBlend add(EBlend::Add, std::sqrt(std::min(1.f, amount)));
  g.PathStroke(col, width, RoundStroke(), &add);
}

// Where depth d (0 = front, 1 = back layer, > 1 behind it) sits this frame. spread 1 and
// sway 0 reproduce DrawJmp2203Stack's float expressions exactly.
struct Jmp2203Depth
{
  float cx, cy2, baseS, R, spread, sway;

  void At(float d, float& x, float& y, float& S) const
  {
    S = baseS * (1.f - d * 0.14f);
    y = cy2 - d * R * 0.07f * spread;
    x = cx + d * sway;
  }
};

class Jmp2203Animator final : public ArtAnimator
{
public:
  // mFull is the static art (the exact rest frame); mBloom is the gold bloom under the live stack.
  bool Prepare(IGraphics& g, IControl* owner, const IRECT& r) override
  {
    if (!mFull.Ok(g))
      mFull.Build(g, owner, r, [&] { DrawJmp2203Hero(g, r); });
    if (!mBloom.Ok(g))
      mBloom.Build(g, owner, r, [&] { DrawJmp2203Bloom(g, r); });
    return true;
  }

  void Draw(IGraphics& g, const IRECT& r, const ArtMotion& m) override
  {
    if (m.IsRest())
    {
      mFull.DrawLit(g, r, m.bloom);
      return;
    }
    mBloom.DrawLit(g, r, m.bloom);
    static const IColor kWarm(255, 255, 240, 205);
    const float E = m.energy, P = m.pick;
    const double t = m.clock;
    const float w = r.W(), h = r.H();
    Jmp2203Depth depth;
    depth.cx = r.MW();
    depth.cy2 = r.MH() + h * 0.02f;
    depth.baseS = std::min(w * 0.34f, h * 0.42f);
    depth.R = std::min(w, h);
    // Kept inside the frame: the back layer's apex clears the top edge at spread 1.92.
    depth.spread = 1.f + E * (0.5f + 0.12f * static_cast<float>(std::cos(kOrbit * t))) + 0.3f * P;
    depth.sway = depth.R * 0.06f * E * static_cast<float>(std::sin(kOrbit * t));

    float crest[kLayers], ring[kLayers];
    for (int i = 0; i < kLayers; ++i)
    {
      const float s = std::max(0.f, static_cast<float>(std::sin(kClimb * t - 0.9 * i)));
      crest[i] = E * s * s;
      ring[i] = 0.f;
    }

    struct Echo
    {
      float u, fade;
    };
    Echo echoes[kOnsetSlots];
    int nEcho = 0;
    for (int k = 0; k < kOnsetSlots; ++k)
    {
      const float age = m.onsetAge[k];
      if (age >= kMaxEventAge)
        continue;
      const float var = 0.8f + 0.4f * Hash01((m.onsets - static_cast<uint32_t>(k)) * 747796405u + m.seed);
      const float fade = var * std::exp(-1.1f * age) * (1.f - Smoothstep(0.95f, 1.45f, age));
      if (fade <= 0.f)
        continue;
      const float u = age / kEchoTravel;
      echoes[nEcho++] = {u, fade};
      for (int i = 0; i < kLayers; ++i)
      {
        const float d = static_cast<float>(i) / static_cast<float>(kLayers - 1);
        ring[i] += fade * std::max(0.f, 1.f - std::abs(u - d) * 4.f);
      }
    }

    // At E = P = 0 these are DrawJmp2203Stack's exact arguments.
    for (int i = kLayers - 1; i >= 0; --i)
    {
      const float dt = (float)i / (float)(kLayers - 1);
      float x = 0.f, y = 0.f, S = 0.f;
      depth.At(dt, x, y, S);
      const float a = std::min(1.f, (0.85f - dt * 0.5f) * (1.f + 0.8f * crest[i]));
      const IColor col = WithA(Mix(kGold, kTeal, dt), a);
      DrawJmp2203TriOutline(g, x, y, S, col, i == 0 ? 2.2f : 1.2f, i == 0, 5.f + 6.f * P);
    }

    // Light on top of the whole stack, so a lit back layer shines through the ones before it.
    const float lift = m.bloom * m.bloom;
    for (int i = 0; i < kLayers; ++i)
    {
      const float dt = (float)i / (float)(kLayers - 1);
      float x = 0.f, y = 0.f, S = 0.f;
      depth.At(dt, x, y, S);
      const IColor ink = Mix(kGold, kTeal, dt);
      const float thick = i == 0 ? 2.2f : 1.2f;
      const float front = i == 0 ? P : 0.f;
      const float glowAmt =
        std::min(0.8f, 0.35f * lift + 0.28f * crest[i] + 0.3f * std::min(1.f, ring[i]) + 0.3f * front);
      AddJmp2203Triforce(g, x, y, S, ink, thick + 4.5f + 6.f * front, glowAmt);
      AddJmp2203Triforce(g, x, y, S, Mix(ink, kWarm, 0.3f), thick, std::min(0.9f, 0.55f * crest[i]));
    }

    for (int e = 0; e < nEcho; ++e)
    {
      float x = 0.f, y = 0.f, S = 0.f;
      depth.At(echoes[e].u, x, y, S);
      const IColor ink = Mix(kGold, kTeal, echoes[e].u);
      AddJmp2203Triforce(g, x, y, S, ink, 6.f, 0.3f * echoes[e].fade);
      AddJmp2203Triforce(g, x, y, S, Mix(ink, kWarm, 0.3f), 1.6f, 0.5f * echoes[e].fade);
    }

    // BloomAdd adds (peak * w)^2 at the centre, so w = sqrt(amount) / peak.
    const float heart = std::min(0.3f, 0.08f * E + 0.14f * P);
    BloomAdd(g, depth.cx, depth.cy2, depth.R * (0.5f + 0.06f * P), kGold, 0.6f, std::sqrt(heart) / 0.6f);
  }

  void DropLayers() override
  {
    mFull.Reset();
    mBloom.Reset();
  }

private:
  static constexpr int kLayers = 6;
  static constexpr double kClimb = 2.4;
  static constexpr double kOrbit = 0.45;
  static constexpr float kEchoTravel = 1.05f; // onset age (s) at which an echo reaches the back layer

  ArtLayer mFull;
  ArtLayer mBloom;
};

inline std::unique_ptr<ArtAnimator> MakeJmp2203Animator()
{
  return std::make_unique<Jmp2203Animator>();
}
} // namespace volumart::art
