#pragma once

// VoLum per-effect motif drawing.
//
// One inline helper, DrawEffectMotif(), shared between the expanded
// triptych Quiet slots and the focused pedal cards (VoLumPedalCardControl).
// Lives in its own header so VoLumTriptych.h stays focused on the
// VoLumTriptychControl class. Extracted from VoLumTriptych.h on the
// 1.0-bugs-hygiene branch (file-size hygiene split).

#include "VoLumTriptychState.h"
#include "VoLumFractalArt.h" // volumart:: atmosphere helpers (bloom/dust/glow/mix) for the 1.2.0 motif glow-up

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

using namespace iplug;
using namespace igraphics;

// DBZ Fusion-Dance figure for a single NAM capture slot. NAM1 leans right
// (dir=+1), NAM2 leans left (dir=-1); their raised arms sweep up into one big
// rounded ARCH that meets at the shared inner edge - fists at the TOP plus a
// second pair at CENTRE, each on a gold "fusion spark". Wide crouched A-stance,
// spiky hair + yelling mouth. Gold sparks/fists appear only at card size (blue thumbs).
inline void DrawFusionFigureLegacy(IGraphics& g, const IRECT& r, int dir, bool dimmed)
{
  using namespace volumart;
  const float activeMul = dimmed ? 0.3f : 1.0f;
  const bool big = std::min(r.W(), r.H()) > 40.f;
  const float W = r.W(), H = r.H(), ox = r.L, oy = r.T;
  auto X = [&](float f) { return ox + (dir > 0 ? f : 1.f - f) * W; }; // f=1 => shared inner edge (arch spine)
  auto Y = [&](float f) { return oy + f * H; };
  const float gnd = Y(0.90f);
  const float topCx = X(1.0f), topCy = Y(0.15f), midCx = X(1.0f), midCy = Y(0.52f); // shared contact points
  const float hipX = X(0.36f), hipY = Y(0.60f), shX = X(0.58f), shY = Y(0.38f);
  const float headX = X(0.66f), headY = Y(0.29f), headR = H * 0.075f;
  const float oKneeX = X(0.18f), oKneeY = Y(0.74f), oFootX = X(0.05f);
  const float iKneeX = X(0.54f), iKneeY = Y(0.75f), iFootX = X(0.58f);
  const float inElX = X(0.82f), inElY = Y(0.47f), outCtlX = X(0.60f), outCtlY = Y(0.05f);
  const float lw = big ? 2.4f : 1.3f;
  const IColor teal = WithA(kTeal, 0.9f * activeMul);
  auto L = [&](float x1, float y1, float x2, float y2, float a) {
    g.DrawLine(WithA(kTeal, a * activeMul), x1, y1, x2, y2, nullptr, lw);
  };
  g.DrawCircle(teal, headX, headY, headR, nullptr, lw);
  if (big)
    g.FillEllipse(WithA(kTeal, 0.85f * activeMul), headX + dir * headR * 0.42f, headY + headR * 0.35f, headR * 0.32f,
                  headR * 0.28f); // wide-open yelling mouth
  L(shX, shY, headX - dir * W * 0.01f, headY + headR * 0.85f, 0.9f); // neck
  L(shX, shY, hipX, hipY, 0.95f); // torso (lean-in)
  L(shX, shY, inElX, inElY, 0.95f); // inner upper arm
  L(inElX, inElY, midCx, midCy, 0.95f); // inner forearm -> centre fist
  for (int i = 0; i < 8; i++) // outer arm arch (quadratic sh -> outCtl -> topC, sampled)
  {
    const float t0 = (float)i / 8.f, t1 = (float)(i + 1) / 8.f;
    auto qx = [&](float t) { return (1 - t) * (1 - t) * shX + 2 * (1 - t) * t * outCtlX + t * t * topCx; };
    auto qy = [&](float t) { return (1 - t) * (1 - t) * shY + 2 * (1 - t) * t * outCtlY + t * t * topCy; };
    L(qx(t0), qy(t0), qx(t1), qy(t1), 0.95f);
  }
  L(hipX, hipY, oKneeX, oKneeY, 0.9f);
  L(oKneeX, oKneeY, oFootX, gnd, 0.9f); // outer leg
  L(hipX, hipY, iKneeX, iKneeY, 0.9f);
  L(iKneeX, iKneeY, iFootX, gnd, 0.9f); // inner leg
  L(oFootX, gnd, oFootX - dir * W * 0.06f, gnd, 0.85f);
  L(iFootX, gnd, iFootX + dir * W * 0.06f, gnd, 0.85f); // feet
  if (big)
  {
    g.FillCircle(teal, topCx, topCy, lw * 1.3f);
    g.FillCircle(teal, midCx, midCy, lw * 1.3f); // fists at meeting points
    Bloom(g, topCx, topCy, std::min(W, H) * 0.4f, kGold, 0.14f * activeMul);
    GlowDot(g, kGold, kGoldHi, topCx, topCy, 4.5f, 10.f);
    Bloom(g, midCx, midCy, std::min(W, H) * 0.28f, kGold, 0.12f * activeMul);
    GlowDot(g, kGold, kGoldHi, midCx, midCy, 3.2f, 8.f);
    for (int i = 0; i < 10; i++)
    {
      const float a = (float)i / 10.f * 6.28318f;
      g.DrawLine(WithA(kGoldHi, 0.75f), topCx, topCy, topCx + cosf(a) * (9.f + (i % 2) * 5.f),
                 topCy + sinf(a) * (9.f + (i % 2) * 5.f), nullptr, 1.6f);
    }
  }
  else
  {
    g.FillCircle(WithA(kTeal, 0.8f * activeMul), topCx, topCy, 2.f);
    g.FillCircle(WithA(kTeal, 0.8f * activeMul), midCx, midCy, 1.6f);
  }
}

// The Fusion Dance "HA!" beat as a stylized silhouette. NAM1 (dir=+1) stands on
// the left and leans right, NAM2 (dir=-1) is its mirror. Each figure stands on
// its inner leg with the outer leg kicked out level, the outer arm arched over
// the head and the inner arm straight, both index fingers touching the shared
// inner edge where the partner's meet them; a gold flare fires there at card size.
// The whole body is one path: NanoVG fills overlapping subpaths as a nonzero
// union, so the dimmed alpha stays flat where limbs overlap. The pose keeps its
// aspect and hugs the inner edge in the portrait card, the landscape PLAY well
// and the ~20 px Quiet slot alike.
inline void DrawFusionFigure(IGraphics& g, const IRECT& r, int dir, bool dimmed)
{
  using namespace volumart;
  const float am = dimmed ? 0.28f : 1.0f;
  const bool big = std::min(r.W(), r.H()) > 40.f;
  const float aspect = 0.82f;
  const float bh = std::min(r.H() * (big ? 0.94f : 1.f), r.W() / aspect);
  const float bw = bh * aspect;
  const float seam = dir > 0 ? r.R : r.L;
  const float top = r.MH() - bh * 0.54f;
  const float fd = (float)dir;
  struct Pt
  {
    float x, y;
  };
  // u runs from the outer edge (0) to the seam (1), v from the top (0) to the floor (1).
  auto P = [&](float u, float v) { return Pt{seam - fd * (1.f - u) * bw, top + v * bh}; };
  const float minRad = big ? 0.9f : 0.85f;
  auto R = [&](float f) { return std::max(minRad, f * bh); };
  auto along = [](Pt a, Pt b, float t) { return Pt{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; };

  auto segment = [&](Pt a, float ra, Pt b, float rb) {
    const float dx = b.x - a.x, dy = b.y - a.y, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-3f)
      return;
    const float nx = -dy / len, ny = dx / len;
    g.PathMoveTo(a.x + nx * ra, a.y + ny * ra);
    g.PathLineTo(b.x + nx * rb, b.y + ny * rb);
    g.PathLineTo(b.x - nx * rb, b.y - ny * rb);
    g.PathLineTo(a.x - nx * ra, a.y - ny * ra);
    g.PathClose();
  };
  auto joint = [&](Pt p, float rad) { g.PathCircle(p.x, p.y, rad); };
  // Quadratic limb a -> c -> b tapering ra -> rb, rounded at every sample.
  auto limb = [&](Pt a, Pt c, Pt b, float ra, float rb, int steps) {
    Pt prev = a;
    float prevR = ra;
    joint(a, ra);
    for (int i = 1; i <= steps; ++i)
    {
      const float t = (float)i / (float)steps, s = 1.f - t;
      const Pt p{s * s * a.x + 2.f * s * t * c.x + t * t * b.x, s * s * a.y + 2.f * s * t * c.y + t * t * b.y};
      const float rad = ra + (rb - ra) * t;
      segment(prev, prevR, p, rad);
      joint(p, rad);
      prev = p;
      prevR = rad;
    }
  };
  auto straight = [&](Pt a, Pt b, float ra, float rb) { limb(a, along(a, b, 0.5f), b, ra, rb, 1); };

  // Skeleton.
  const Pt pelvis = P(0.44f, 0.60f), neck = P(0.70f, 0.40f);
  const float ax = neck.x - pelvis.x, ay = neck.y - pelvis.y, alen = std::sqrt(ax * ax + ay * ay);
  const Pt axis{ax / alen, ay / alen};
  const Pt out{axis.y * fd, -axis.x * fd}; // perpendicular to the spine, pointing outer-up
  auto off = [](Pt p, Pt d, float k) { return Pt{p.x + d.x * k, p.y + d.y * k}; };
  // At Quiet size the arms open wider and thinner, or head and arms clot into one blob.
  const float headR = std::max(big ? 3.f : 1.7f, 0.056f * bh);
  const float armR = big ? R(0.022f) : 0.62f, wristR = big ? R(0.014f) : 0.55f;
  const Pt head = off(neck, axis, R(0.028f) + headR);
  const Pt shOut = off(neck, out, R(0.062f)), shIn = off(neck, out, -R(0.062f));
  const Pt touchTop = P(1.f, big ? 0.195f : 0.14f), touchLow = P(1.f, big ? 0.50f : 0.54f);
  const Pt hipOut = off(pelvis, out, R(0.026f)), hipIn = off(pelvis, out, -R(0.026f));
  const Pt elbowTop = P(0.67f, big ? 0.09f : 0.f), wristTop = P(0.935f, big ? 0.19f : 0.135f);
  const Pt wristLow = P(0.935f, big ? 0.495f : 0.535f);
  const Pt kneeOut = P(0.25f, 0.555f), ankleOut = P(0.06f, 0.495f), toe = P(0.f, 0.48f);
  const Pt kneeIn = P(0.56f, 0.78f), ankleIn = P(0.53f, 0.955f);
  const float tipR = std::max(0.35f, 0.003f * bh);

  // grow > 0 fattens every part for the halo pass.
  auto body = [&](float grow) {
    g.PathClear();
    const Pt waist = along(pelvis, neck, 0.38f);
    const Pt torso[6] = {off(neck, out, R(0.062f) + grow),   off(waist, out, R(0.042f) + grow),
                         off(pelvis, out, R(0.048f) + grow), off(pelvis, out, -R(0.048f) - grow),
                         off(waist, out, -R(0.042f) - grow), off(neck, out, -R(0.062f) - grow)};
    g.PathMoveTo(torso[0].x, torso[0].y);
    for (int i = 1; i < 6; ++i)
      g.PathLineTo(torso[i].x, torso[i].y);
    g.PathClose();
    joint(pelvis, R(0.044f) + grow);
    straight(neck, head, R(0.022f) + grow, R(0.02f) + grow);
    joint(head, headR + grow);
    // Outer arm bends over the head, inner arm reaches straight; both end in a pointed finger.
    limb(shOut, elbowTop, wristTop, armR + grow, wristR + grow, big ? 10 : 5);
    segment(wristTop, wristR + grow, touchTop, tipR + grow);
    straight(shIn, wristLow, armR + grow, wristR + grow);
    segment(wristLow, wristR + grow, touchLow, tipR + grow);
    // Outer leg kicked out level with a pointed toe; inner leg bent, foot flat toward the partner.
    straight(hipOut, kneeOut, R(0.032f) + grow, R(0.022f) + grow);
    straight(kneeOut, ankleOut, R(0.022f) + grow, R(0.013f) + grow);
    segment(ankleOut, R(0.013f) + grow, toe, tipR + grow);
    straight(hipIn, kneeIn, R(0.034f) + grow, R(0.024f) + grow);
    straight(kneeIn, ankleIn, R(0.024f) + grow, R(0.016f) + grow);
    straight(P(0.51f, 0.98f), P(0.62f, 0.99f), R(0.013f) + grow, R(0.008f) + grow);
  };

  if (big)
  {
    Bloom(g, along(pelvis, neck, 0.5f).x, along(pelvis, neck, 0.5f).y, bh * 0.5f, kTeal, 0.07f * am);
    // Swing trails: the outer arm's sweep from out wide to over the head.
    for (int k = 0; k < 3; ++k)
    {
      const float rad = bh * (0.31f + 0.055f * (float)k);
      const float a0 = 3.35f, a1 = 4.3f - 0.08f * (float)k;
      const int segs = 18;
      for (int i = 0; i < segs; ++i)
      {
        const float t0 = (float)i / (float)segs, t1 = (float)(i + 1) / (float)segs;
        const float p0 = a0 + (a1 - a0) * t0, p1 = a0 + (a1 - a0) * t1;
        g.DrawLine(WithA(kTeal, (0.05f + 0.3f * t1 * t1) * (1.f - 0.28f * (float)k) * am),
                   neck.x + fd * std::cos(p0) * rad, neck.y + std::sin(p0) * rad, neck.x + fd * std::cos(p1) * rad,
                   neck.y + std::sin(p1) * rad, nullptr, 1.3f);
      }
    }
    body(5.f);
    g.PathFill(WithA(kTeal, 0.05f * am));
    body(2.2f);
    g.PathFill(WithA(kTeal, 0.1f * am));
  }
  body(0.f);
  g.PathFill(IPattern::CreateLinearGradient(toe.x, toe.y, touchTop.x, touchTop.y,
                                            {{WithA(Mix(kDim, kBlue, 0.4f), 0.45f * am), 0.f},
                                             {WithA(kTeal, 0.9f * am), 0.5f},
                                             {WithA(Mix(kTeal, kBlue, 0.35f), 0.98f * am), 1.f}}));

  if (big)
  {
    const float span = std::min(r.W(), r.H());
    Bloom(g, touchTop.x, touchTop.y, span * 0.5f, kGold, 0.2f * am);
    Bloom(g, touchLow.x, touchLow.y, span * 0.26f, kGold, 0.12f * am);
    auto flare = [&](Pt c, float len, float wid, float a) {
      g.PathClear();
      g.PathMoveTo(c.x - len, c.y);
      g.PathLineTo(c.x, c.y - wid);
      g.PathLineTo(c.x + len, c.y);
      g.PathLineTo(c.x, c.y + wid);
      g.PathClose();
      g.PathMoveTo(c.x, c.y - len);
      g.PathLineTo(c.x + wid, c.y);
      g.PathLineTo(c.x, c.y + len);
      g.PathLineTo(c.x - wid, c.y);
      g.PathClose();
      g.PathFill(WithA(kGoldHi, a));
    };
    g.FillCircle(WithA(kGold, 0.26f * am), touchTop.x, touchTop.y, 11.f);
    g.FillCircle(WithA(kGold, 0.45f * am), touchTop.x, touchTop.y, 6.5f);
    flare(touchTop, bh * 0.22f, 2.2f, 0.95f * am);
    g.FillCircle(WithA(kGoldHi, am), touchTop.x, touchTop.y, 3.2f);
    g.FillCircle(WithA(kGold, 0.35f * am), touchLow.x, touchLow.y, 6.f);
    flare(touchLow, bh * 0.1f, 1.4f, 0.8f * am);
    g.FillCircle(WithA(kGoldHi, am), touchLow.x, touchLow.y, 2.2f);
  }
  else
  {
    g.FillCircle(WithA(kTeal, 0.9f * am), touchTop.x, touchTop.y, 1.4f);
  }
}

// VOLUM_NAM_ART_LEGACY=1 draws the NAM1 / NAM2 cards with DrawFusionFigureLegacy
// so the old and new fusion art can be compared side by side. Unset, empty or any
// other value draws DrawFusionFigure.
inline bool ParseNamArtLegacy(const char* value)
{
  return value && value[0] == '1' && value[1] == '\0';
}

inline void DrawNamFusionMotif(IGraphics& g, const IRECT& r, int dir, bool dimmed)
{
  static const bool legacy = ParseNamArtLegacy(std::getenv("VOLUM_NAM_ART_LEGACY"));
  if (legacy)
    DrawFusionFigureLegacy(g, r, dir, dimmed);
  else
    DrawFusionFigure(g, r, dir, dimmed);
}

//==============================================================================
// Reverb & Delay Extension Controls (PRE / AMP / POST)
//==============================================================================

// Neural-network node-graph motif shared by the two NAM capture blocks. Each
// block passes its own layer shape + seed so PRE_NAM1 and PRE_NAM2 read as a
// related pair while staying distinguishable (NAM2 is deeper/denser). Scales
// from the ~20 px collapsed slot to the ~150 px focused card.
inline void DrawNeuralNetMotif(IGraphics& g, const IRECT& r, const int* layers, int L, unsigned seed, bool dimmed)
{
  const float activeMul = dimmed ? 0.25f : 1.0f;
  // Output-layer nodes glow gold at card size but stay teal in the collapsed
  // thumbnail so the ~20 px slot reads blue/teal only.
  const bool big = std::min(r.W(), r.H()) > 40.f;
  const IColor node((int)(150.f * activeMul), 120, 210, 220);
  const IColor out =
    big ? IColor((int)(235.f * activeMul), 252, 222, 145) : IColor((int)(235.f * activeMul), 120, 210, 220);
  unsigned rng = seed;
  auto frand = [&]() -> float {
    rng = rng * 1664525u + 1013904223u;
    return (float)rng / (float)0xFFFFFFFFu;
  };
  struct Node
  {
    float x, y;
  };
  std::vector<std::vector<Node>> pos(L);
  for (int i = 0; i < L; i++)
  {
    const float x = r.L + r.W() * (L > 1 ? (0.2f + 0.6f * (float)i / (float)(L - 1)) : 0.5f);
    const int n = layers[i];
    for (int j = 0; j < n; j++)
    {
      const float y = r.T + r.H() * (n > 1 ? (0.22f + 0.56f * (float)j / (float)(n - 1)) : 0.5f);
      pos[i].push_back({x, y});
    }
  }
  for (int i = 0; i < L - 1; i++)
    for (auto& a : pos[i])
      for (auto& b : pos[i + 1])
      {
        const int al = (int)(255.f * (0.16f + 0.16f * frand()) * activeMul);
        g.DrawLine(IColor(al, 100, 180, 200), a.x, a.y, b.x, b.y, nullptr, 0.8f);
      }
  const float scale = std::min(r.W(), r.H());
  const float nodeR = std::max(1.6f, scale * 0.045f);
  for (int i = 0; i < L; i++)
  {
    const bool isOut = (i == L - 1);
    const IColor& c = isOut ? out : node;
    for (auto& p : pos[i])
    {
      g.FillCircle(IColor((int)(60.f * activeMul), c.R, c.G, c.B), p.x, p.y, nodeR * 1.7f);
      g.FillCircle(c, p.x, p.y, isOut ? nodeR * 1.15f : nodeR);
    }
  }
}

// POST Chorus - Throat: a wireframe hyperboloid read as a wormhole. Signal goes
// in the near (teal) mouth and comes out the far (gold) one slightly displaced.
// Straight generators with a fixed twist plus latitude rings: a tunnel, not an
// LFO waveform, so it never reads as another Delay/Tremolo curve. One glyph for
// all four voices; the same construction at Quiet ~20 px and at card size, with
// gold and bloom gated behind the shared `min(w,h) > 40` size check.
inline void DrawChorusThroatMotif(IGraphics& g, const IRECT& r, bool dimmed)
{
  using namespace volumart;
  const float am = dimmed ? 0.28f : 1.0f;
  const float w = r.W();
  const float h = r.H();
  const bool big = std::min(w, h) > 40.f;

  struct Mouth
  {
    float cx, cy, rx, ry;
  };
  // Card-size mouths fill 0.13h..0.87h so the tunnel is centred in the card art
  // like the locked motif mock. The previous 0.235h..0.83h span left an uneven
  // band above and a dead sixth of the card below the near mouth. Quiet metrics
  // are unchanged: at ~20 px the tighter span is what keeps the mouths readable.
  const Mouth front{
    r.L + w * 0.50f, r.T + h * (big ? 0.715f : 0.68f), w * (big ? 0.44f : 0.38f), h * (big ? 0.155f : 0.14f)};
  const Mouth back{
    r.L + w * 0.50f, r.T + h * (big ? 0.185f : 0.32f), w * (big ? 0.135f : 0.14f), h * (big ? 0.055f : 0.06f)};

  auto lerpMouth = [](const Mouth& a, const Mouth& b, float t) {
    return Mouth{
      a.cx + (b.cx - a.cx) * t, a.cy + (b.cy - a.cy) * t, a.rx + (b.rx - a.rx) * t, a.ry + (b.ry - a.ry) * t};
  };
  auto strokeMouth = [&](const Mouth& e, int segs, const IColor& col, float lw) {
    float px = e.cx + e.rx;
    float py = e.cy;
    for (int i = 1; i <= segs; ++i)
    {
      const float a = (float)i / (float)segs * 6.28318f;
      const float x = e.cx + e.rx * cosf(a);
      const float y = e.cy + e.ry * sinf(a);
      g.DrawLine(col, px, py, x, y, nullptr, lw);
      px = x;
      py = y;
    }
  };

  if (big)
    Bloom(g, back.cx, back.cy, std::min(w, h) * 0.42f, kGold, 0.14f * am);

  const int nLat = big ? 5 : 3;
  const int nGen = big ? 14 : 6;
  const float twist = 0.55f;
  const int segs = big ? 48 : 16;

  for (int i = 0; i < nLat; ++i)
  {
    const float t = (float)i / (float)(nLat - 1);
    const Mouth e = lerpMouth(front, back, t);
    const IColor col = (t > 0.72f) ? (big ? kGold : kTeal) : Mix(kTeal, kBlue, t);
    strokeMouth(e, segs, WithA(col, (0.35f + 0.55f * t) * am), big ? (t > 0.8f ? 2.2f : 1.35f) : 1.15f);
  }
  for (int i = 0; i < nGen; ++i)
  {
    const float a0 = (float)i / (float)nGen * 6.28318f;
    const float a1 = a0 + twist;
    const IColor col = WithA(Mix(kDim, kTeal, (i % 2) ? 0.4f : 0.85f), 0.55f * am);
    g.DrawLine(col, front.cx + front.rx * cosf(a0), front.cy + front.ry * sinf(a0), back.cx + back.rx * cosf(a1),
               back.cy + back.ry * sinf(a1), nullptr, big ? 1.15f : 0.95f);
  }
  strokeMouth(front, segs, WithA(kTeal, 0.95f * am), big ? 2.3f : 1.4f);
  if (big)
  {
    strokeMouth(back, segs, WithA(kGoldHi, 0.95f * am), 1.8f);
    g.FillCircle(WithA(kGoldHi, 0.95f * am), back.cx, back.cy, 2.8f);
  }
  else
  {
    g.FillCircle(WithA(kTeal, 0.95f * am), back.cx, back.cy, 1.5f);
  }
}

// Per-effect "motif" drawn in pedal cards and in the Quiet PRE/POST slots.
// `dimmed` collapses alpha when the effect is bypassed.
// `variant` lets one effect draw a sub-mode-specific motif. Currently only PITCH
// uses it: 0 = Transpose (vertical double helix), 1 = Octaver (octave up/down
// chevrons). Other effects ignore it.
inline void DrawEffectMotif(IGraphics& g, const IRECT& r, EVoLumEffectFocus effect, bool dimmed, int variant = 0)
{
  float cy = r.MH();
  IColor bright(dimmed ? 70 : 150, 120, 210, 220);
  IColor mid(dimmed ? 40 : 80, 100, 180, 200);
  IColor dim(dimmed ? 20 : 45, 80, 150, 170);
  // Gold/glow accents are a hero/card-size treatment only. Collapsed thumbnails
  // (~20 px) stay blue/teal, so gate every gold accent behind this size check.
  const bool big = std::min(r.W(), r.H()) > 40.f;

  if (effect == EVoLumEffectFocus::PITCH)
  {
    using namespace volumart;
    const float activeMul = dimmed ? 0.28f : 1.0f;
    const float cx = r.MW();
    if (variant == 1)
    {
      // Octaver - Glow Chevrons: doubled up/down chevrons with a glowing root note
      // (gold only at card size), reading instantly as "octave up + octave down".
      const float cyy = r.MH(), ww = r.W() * 0.26f, hh = r.H() * 0.13f, gap = r.H() * 0.13f;
      if (big)
        Bloom(g, cx, cyy, r.W() * 0.45f, kTeal, 0.08f * activeMul);
      auto chev = [&](int dir, const IColor& c) {
        for (int k = 0; k < 2; k++)
        {
          const float off = gap + k * hh * 0.95f, base = cyy + dir * off, tip = cyy + dir * (off + hh);
          const IColor cc = WithA(c, (0.92f - 0.32f * k) * activeMul);
          if (big)
          {
            GlowLine(g, c, cc, cx - ww, base, cx, tip, 2.6f, 4.f);
            GlowLine(g, c, cc, cx + ww, base, cx, tip, 2.6f, 4.f);
          }
          else
          {
            g.DrawLine(cc, cx - ww, base, cx, tip, nullptr, 1.8f);
            g.DrawLine(cc, cx + ww, base, cx, tip, nullptr, 1.8f);
          }
        }
      };
      chev(-1, kTeal); // octave up
      chev(1, kBlue); // octave down
      if (big)
        GlowDot(g, kGold, kGoldHi, cx, cyy, 4.f, 6.f);
      else
        g.FillCircle(WithA(kTeal, activeMul), cx, cyy, 2.6f);
    }
    else
    {
      // Transpose - Deep Helix: two phase-shifted strands with front/back depth
      // thickness, gradient teal->blue, rungs, and gold crossover nodes at card size.
      if (big)
        Bloom(g, cx, r.MH(), r.W() * 0.5f, kTeal, 0.10f * activeMul);
      const int segs = 96;
      const float amp = r.W() * 0.27f, y0 = r.T + r.H() * 0.1f, y1 = r.B - r.H() * 0.1f;
      struct HP
      {
        float x, y, z;
      };
      std::vector<HP> A(segs + 1), B(segs + 1);
      for (int s = 0; s <= segs; ++s)
      {
        const float t = (float)s / (float)segs, y = y0 + (y1 - y0) * t, ph = t * 2.4f * 6.28318f;
        A[s] = {cx + sinf(ph) * amp, y, cosf(ph)};
        B[s] = {cx + sinf(ph + 3.14159f) * amp, y, cosf(ph + 3.14159f)};
      }
      for (int s = 0; s <= segs; s += 6)
        g.DrawLine(WithA(big ? kGold : kBlue, 0.32f * activeMul), A[s].x, A[s].y, B[s].x, B[s].y, nullptr, 1.f);
      auto strand = [&](const std::vector<HP>& P, const IColor& c0, const IColor& c1) {
        for (int i = 1; i <= segs; ++i)
        {
          const float z = P[i].z * 0.5f + 0.5f, lw = (big ? 2.5f : 1.6f) * (0.55f + 0.45f * z);
          g.DrawLine(WithA(Mix(c0, c1, (float)i / (float)(segs + 1)), (0.5f + 0.45f * z) * activeMul), P[i - 1].x,
                     P[i - 1].y, P[i].x, P[i].y, nullptr, lw);
        }
      };
      strand(A, kTeal, kBlue);
      strand(B, kBlue, kTeal);
      if (big)
        for (int s = 0; s <= segs; ++s)
          if (std::fabs(A[s].x - B[s].x) < amp * 0.12f)
            GlowDot(g, kGold, kGoldHi, cx, A[s].y, 2.4f, 6.f);
    }
  }
  else if (effect == EVoLumEffectFocus::COMP)
  {
    // Gain-reduction VU arc with a gold needle: instantly reads as "compressor".
    const float activeMul = dimmed ? 0.28f : 1.0f;
    const IColor arcCol((int)(120.f * activeMul), 100, 180, 200);
    const IColor tickTeal((int)(150.f * activeMul), 100, 180, 200);
    const IColor tickGold((int)(200.f * activeMul), 200, 165, 87);
    const IColor needleGlow((int)(120.f * activeMul), 200, 165, 87);
    const IColor needleGold((int)(235.f * activeMul), 252, 222, 145);
    const IColor needleTeal((int)(235.f * activeMul), 120, 210, 220);
    const IColor needleCol = big ? needleGold : needleTeal;
    const float cx = r.MW();
    // Size the arc from the slot so the radial ticks (which extend tickOut past R)
    // never overflow the collapsed PRE slot. Width: 0.9*(R+tickOut) <= W/2 - margin;
    // height: the band spans +/-(0.5R)+tickOut around the mid line.
    const float margin = 3.f, tickOut = big ? 8.f : 3.f;
    const float wLimit = (r.W() * 0.5f - margin) / 0.9f - tickOut;
    const float hLimit = (r.H() * 0.5f - margin - tickOut) / 0.5f;
    const float R = std::max(6.f, std::min(wLimit, hLimit));
    // Anchor so the arc+needle band [cym-R, cym] is vertically centred in the
    // card instead of hanging low near the bottom edge.
    const float cym = r.MH() + R * 0.5f;
    const float a0 = 3.14159f * 1.15f, a1 = 3.14159f * 1.85f;
    float pax = cx + R * cosf(a0), pay = cym + R * sinf(a0);
    for (int s = 1; s <= 40; ++s)
    {
      const float a = a0 + (a1 - a0) * (float)s / 40.f;
      const float X = cx + R * cosf(a), Y = cym + R * sinf(a);
      g.DrawLine(arcCol, pax, pay, X, Y, nullptr, 2.f);
      pax = X;
      pay = Y;
    }
    for (int i = 0; i <= 10; ++i)
    {
      const float t = (float)i / 10.f;
      const float a = a0 + (a1 - a0) * t;
      const float r1 = R - 6.f, r2 = R + ((i % 5 == 0) ? tickOut : tickOut * 0.5f);
      const IColor& tc = (i > 7 && big) ? tickGold : tickTeal;
      g.DrawLine(tc, cx + r1 * cosf(a), cym + r1 * sinf(a), cx + r2 * cosf(a), cym + r2 * sinf(a), nullptr,
                 (i % 5 == 0) ? 1.6f : 1.f);
    }
    const float na = a0 + (a1 - a0) * 0.68f;
    if (big)
      g.DrawLine(needleGlow, cx, cym, cx + R * 0.92f * cosf(na), cym + R * 0.92f * sinf(na), nullptr, 3.2f);
    g.DrawLine(needleCol, cx, cym, cx + R * 0.92f * cosf(na), cym + R * 0.92f * sinf(na), nullptr, 1.8f);
    g.FillCircle(needleCol, cx, cym, 3.5f);
  }
  else if (effect == EVoLumEffectFocus::PRE_NAM1)
  {
    // NAM capture 1: DBZ Fusion-Dance figure leaning right toward NAM2.
    DrawNamFusionMotif(g, r, +1, dimmed);
  }
  else if (effect == EVoLumEffectFocus::PRE_NAM2)
  {
    // NAM capture 2: mirror figure leaning left; fingers meet NAM1 at the border.
    DrawNamFusionMotif(g, r, -1, dimmed);
  }
  else if (effect == EVoLumEffectFocus::DELAY)
  {
    // Glow Taps: decaying wave-burst taps with bloom + decay glow; the dry (first)
    // tap glows gold at card size, thumbnails stay teal.
    using namespace volumart;
    const float activeMul = dimmed ? 0.28f : 1.0f;
    if (big)
      Bloom(g, r.L + r.W() * 0.14f, cy, r.W() * 0.62f, kTeal, 0.09f * activeMul);
    g.DrawLine(WithA(kDim, 0.3f * activeMul), r.L, cy, r.R, cy, nullptr, 0.5f);
    const int taps = 5;
    const float tapW = r.W() / (float)taps;
    for (int t = 0; t < taps; t++)
    {
      const float decay = 1.f - (float)t / (float)taps * 0.72f, ampY = r.H() * 0.36f * decay, baseX = r.L + t * tapW;
      const bool first = (t == 0);
      const IColor core = (first && big) ? kGoldHi : (t % 2 ? kMid : kTeal);
      float pX = baseX, pY = cy;
      for (int j = 1; j <= 40; j++)
      {
        const float t1 = (float)j / 40.f, X = baseX + t1 * tapW, env = sinf(t1 * 3.14159f);
        const float y = cy + sinf(t1 * 6.28318f * 3.f) * ampY * env;
        const IColor c = WithA(core, (0.5f + 0.4f * decay) * activeMul);
        const float lw = (big ? 1.9f : 1.4f) * decay;
        if (first && big)
          GlowLine(g, kGold, c, pX, pY, X, y, lw, 3.f * decay);
        else
          g.DrawLine(c, pX, pY, X, y, nullptr, lw);
        pX = X;
        pY = y;
      }
      if (t > 0)
        g.DrawLine(
          WithA(kDim, 0.3f * decay * activeMul), baseX, r.T + r.H() * 0.16f, baseX, r.B - r.H() * 0.16f, nullptr, 0.6f);
    }
  }
  else if (effect == EVoLumEffectFocus::CHORUS)
  {
    DrawChorusThroatMotif(g, r, dimmed);
  }
  else if (effect == EVoLumEffectFocus::TREMOLO)
  {
    // Throb bars: volume bars pulsing under a gold LFO curve. Reads unmistakably as
    // amplitude modulation and stays clearly distinct from the Delay tap-trains.
    const float activeMul = dimmed ? 0.28f : 1.0f;
    const IColor barTeal((int)(215.f * activeMul), 120, 210, 220);
    const IColor barMid((int)(215.f * activeMul), 100, 180, 200);
    const int n = 17;
    const float span = r.W() * 0.7f;
    const float x0 = r.L + r.W() * 0.15f;
    const float slot = span / (float)n;
    const float bw = slot * 0.62f;
    for (int i = 0; i < n; ++i)
    {
      const float t = (float)i / (float)(n - 1);
      const float lfo = 0.2f + 0.8f * (0.5f + 0.5f * sinf(t * 6.28318f * 2.f - 1.5708f));
      const float bh = r.H() * 0.4f * lfo;
      const float bx = x0 + i * slot + (slot - bw) * 0.5f;
      g.FillRect((i % 2) ? barTeal : barMid, IRECT(bx, cy - bh, bx + bw, cy + bh));
    }
    float pX = x0, pY = cy - r.H() * 0.4f * 0.2f;
    for (int s = 1; s <= 80; ++s)
    {
      const float t = (float)s / 80.f;
      const float x = x0 + t * span;
      const float lfo = 0.2f + 0.8f * (0.5f + 0.5f * sinf(t * 6.28318f * 2.f - 1.5708f));
      const float y = cy - r.H() * 0.4f * lfo;
      if (big)
        g.DrawLine(IColor((int)(70.f * activeMul), 200, 165, 87), pX, pY, x, y, nullptr, 3.2f);
      g.DrawLine(
        big ? IColor((int)(210.f * activeMul), 252, 222, 145) : IColor((int)(210.f * activeMul), 120, 210, 220), pX, pY,
        x, y, nullptr, 1.4f);
      pX = x;
      pY = y;
    }
  }
  else
  {
    // Reverb (default) - Deep Dust: DLA-style branching dust grown from baseline +
    // mid seeds over a soft teal bloom. Scales from the ~22 px thumbnail to the ~150 px card.
    const float am = dimmed ? 0.4f : 1.0f;
    if (big)
      volumart::Bloom(g, r.MW(), r.MH() + r.H() * 0.1f, std::min(r.W(), r.H()) * 0.6f, volumart::kTeal, 0.10f * am);
    const float scale = std::min(r.W(), r.H()); // ~22 thumb, ~150 card
    const float sizeFactor = std::clamp(scale / 150.f, 0.18f, 1.0f);
    const int count = std::max(450, (int)(12000.f * sizeFactor * sizeFactor));
    const float lenScale = std::max(0.55f, sizeFactor); // shorter strokes at small size
    const int brightCount = (int)(count * 0.025f);
    const int midCount = (int)(count * 0.125f);

    struct Pt
    {
      float x, y;
    };
    std::vector<Pt> pts;
    for (float f = 0.05f; f <= 0.95f; f += 0.15f)
      pts.push_back({r.L + r.W() * f, r.B});
    for (float f = 0.1f; f <= 0.9f; f += 0.2f)
      pts.push_back({r.L + r.W() * f, cy + r.H() * 0.15f});
    for (float f = 0.15f; f <= 0.85f; f += 0.25f)
      pts.push_back({r.L + r.W() * f, cy - r.H() * 0.15f});
    for (float f = 0.2f; f <= 0.8f; f += 0.3f)
      pts.push_back({r.L + r.W() * f, r.T + r.H() * 0.1f});
    unsigned int rng = 54321;
    for (int i = 0; i < count; i++)
    {
      rng = rng * 1103515245 + 12345;
      int parentIdx = rng % pts.size();
      float angle = (float)(rng % 360);
      float rad = angle * 3.14159f / 180.f;
      float len = (2.f + (float)(rng % 4)) * lenScale;
      Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
      if (next.x >= r.L && next.x <= r.R && next.y >= r.T && next.y <= r.B)
      {
        IColor col = (i < brightCount) ? bright : ((i < midCount) ? mid : dim);
        float tk = (i < (int)(brightCount * 0.7f)) ? 2.f : ((i < midCount) ? 1.5f : 1.f);
        g.DrawLine(col, pts[parentIdx].x, pts[parentIdx].y, next.x, next.y, nullptr, tk);
        pts.push_back(next);
      }
    }
  }
}
