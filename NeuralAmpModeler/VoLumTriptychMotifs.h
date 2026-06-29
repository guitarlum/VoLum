#pragma once

// VoLum per-effect motif drawing.
//
// One inline helper, DrawEffectMotif(), shared between the expanded
// triptych Quiet slots and the focused pedal cards (VoLumPedalCardControl).
// Lives in its own header so VoLumTriptych.h stays focused on the
// VoLumTriptychControl class. Extracted from VoLumTriptych.h on the
// 1.0-bugs-hygiene branch (file-size hygiene split).

#include "VoLumTriptychState.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace iplug;
using namespace igraphics;

//==============================================================================
// Reverb & Delay Extension Controls (PRE / AMP / POST)
//==============================================================================

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

  if (effect == EVoLumEffectFocus::PITCH)
  {
    const float activeMul = dimmed ? 0.25f : 1.0f;
    const IColor teal((int)(135.f * activeMul), 90, 205, 220);
    const IColor blue((int)(145.f * activeMul), 150, 200, 245);
    const float cx = r.MW();
    if (variant == 1)
    {
      // Octaver: a centre note with an octave-UP chevron above and octave-DOWN
      // chevron below, reading instantly as "two octaves" vs the helix.
      const float w = r.W() * 0.30f;
      const float h = r.H() * 0.22f;
      const float gap = r.H() * 0.20f;
      g.FillCircle(blue, cx, cy, std::max(1.6f, r.H() * 0.05f));
      // Up chevron (octave up).
      g.DrawLine(teal, cx - w, cy - gap, cx, cy - gap - h, nullptr, 1.8f);
      g.DrawLine(teal, cx + w, cy - gap, cx, cy - gap - h, nullptr, 1.8f);
      g.DrawLine(teal.WithOpacity(0.6f), cx - w * 0.55f, cy - gap - h * 0.55f, cx, cy - gap - h * 1.15f, nullptr, 1.4f);
      g.DrawLine(teal.WithOpacity(0.6f), cx + w * 0.55f, cy - gap - h * 0.55f, cx, cy - gap - h * 1.15f, nullptr, 1.4f);
      // Down chevron (octave down).
      g.DrawLine(blue, cx - w, cy + gap, cx, cy + gap + h, nullptr, 1.8f);
      g.DrawLine(blue, cx + w, cy + gap, cx, cy + gap + h, nullptr, 1.8f);
      g.DrawLine(blue.WithOpacity(0.6f), cx - w * 0.55f, cy + gap + h * 0.55f, cx, cy + gap + h * 1.15f, nullptr, 1.4f);
      g.DrawLine(blue.WithOpacity(0.6f), cx + w * 0.55f, cy + gap + h * 0.55f, cx, cy + gap + h * 1.15f, nullptr, 1.4f);
    }
    else
    {
      // Transpose: vertical double helix - two phase-shifted sine strands running
      // top-to-bottom with cross "rungs". Scales from PRE thumbnail to focused card.
      const float amp = r.W() * 0.28f;
      const float turns = 2.2f;
      const int segs = 64;
      const float y0 = r.T + r.H() * 0.12f;
      const float y1 = r.B - r.H() * 0.12f;
      float pAx = 0.f, pAy = 0.f, pBx = 0.f, pBy = 0.f;
      for (int s = 0; s <= segs; ++s)
      {
        const float t = (float)s / (float)segs;
        const float y = y0 + (y1 - y0) * t;
        const float ph = t * turns * 6.28318f;
        const float ax = cx + sinf(ph) * amp;
        const float bx = cx + sinf(ph + 3.14159f) * amp;
        if (s > 0)
        {
          g.DrawLine(teal, pAx, pAy, ax, y, nullptr, 1.6f);
          g.DrawLine(blue, pBx, pBy, bx, y, nullptr, 1.6f);
          if (s % 6 == 0)
            g.DrawLine(blue.WithOpacity(0.5f), ax, y, bx, y, nullptr, 1.0f);
        }
        pAx = ax;
        pAy = y;
        pBx = bx;
        pBy = y;
      }
    }
  }
  else if (effect == EVoLumEffectFocus::COMP)
  {
    const float activeMul = dimmed ? 0.25f : 1.0f;
    const IColor teal((int)(125.f * activeMul), 90, 205, 220);
    const IColor blue((int)(135.f * activeMul), 145, 220, 245);
    const float cx = r.MW();
    const float radii[][2] = {{r.W() * 0.24f, r.H() * 0.11f},
                              {r.W() * 0.33f, r.H() * 0.16f},
                              {r.W() * 0.42f, r.H() * 0.21f},
                              {r.W() * 0.50f, r.H() * 0.26f}};
    for (int i = 0; i < 4; ++i)
    {
      const float rotation = i * 23.f * 3.14159f / 180.f;
      float prevX = cx + radii[i][0] * cosf(rotation);
      float prevY = cy + radii[i][0] * sinf(rotation) * 0.28f;
      for (int s = 1; s <= 72; ++s)
      {
        const float a = (float)s / 72.f * 6.28318f;
        const float x = cx + radii[i][0] * cosf(a) * cosf(rotation) - radii[i][1] * sinf(a) * sinf(rotation);
        const float y = cy + radii[i][0] * cosf(a) * sinf(rotation) + radii[i][1] * sinf(a) * cosf(rotation);
        g.DrawLine((i % 2) ? blue : teal, prevX, prevY, x, y, nullptr, 1.2f);
        prevX = x;
        prevY = y;
      }
    }
    g.FillCircle(teal, cx - r.W() * 0.32f, cy + r.H() * 0.13f, 3.5f);
    g.FillCircle(blue, cx - r.W() * 0.12f, cy - r.H() * 0.10f, 3.f);
    g.FillCircle(teal, cx + r.W() * 0.15f, cy + r.H() * 0.12f, 3.5f);
    g.FillCircle(blue, cx + r.W() * 0.36f, cy - r.H() * 0.09f, 3.f);
  }
  else if (effect == EVoLumEffectFocus::PRE_NAM1)
  {
    const float activeMul = dimmed ? 0.25f : 1.0f;
    const IColor teal((int)(135.f * activeMul), 90, 205, 220);
    const IColor blue((int)(145.f * activeMul), 145, 220, 245);
    const float cell = std::min(r.W() * 0.12f, r.H() * 0.18f);
    const float gridW = cell * 6.8f;
    const float gridH = cell * 4.3f;
    const float left = r.MW() - gridW * 0.5f;
    const float top = cy - gridH * 0.5f;
    for (int y = 0; y < 4; ++y)
    {
      for (int x = 0; x < 6; ++x)
      {
        const float px = left + x * cell * 1.16f;
        const float py = top + y * cell * 1.10f;
        const bool hole = (x == 2 || x == 3) && (y == 1 || y == 2);
        const IColor col = ((x + y) % 2) ? blue.WithOpacity(0.72f) : teal;
        if (hole)
          g.FillRect(IColor((int)(28.f * activeMul), 24, 42, 58), IRECT(px, py, px + cell, py + cell));
        else
          g.DrawRect(col, IRECT(px, py, px + cell, py + cell), nullptr, 1.3f);

        if (!hole && (x + y) % 3 == 0)
        {
          const float sub = cell * 0.32f;
          g.DrawRect(
            dim.WithOpacity(0.65f), IRECT(px + sub, py + sub, px + cell - sub, py + cell - sub), nullptr, 0.9f);
        }
      }
    }
    g.DrawRect(
      teal.WithOpacity(0.55f), IRECT(left - 2.f, top - 2.f, left + cell * 6.8f, top + cell * 4.3f), nullptr, 1.2f);
  }
  else if (effect == EVoLumEffectFocus::PRE_NAM2)
  {
    const float activeMul = dimmed ? 0.25f : 1.0f;
    const IColor teal((int)(135.f * activeMul), 90, 205, 220);
    const IColor blue((int)(145.f * activeMul), 145, 220, 245);
    const float cxA = r.MW() - r.W() * 0.11f;
    const float cxB = r.MW() + r.W() * 0.11f;
    for (int i = 0; i < 7; ++i)
    {
      const float radius = r.H() * (0.12f + i * 0.045f);
      float prevAX = cxA + radius;
      float prevAY = cy;
      float prevBX = cxB + radius;
      float prevBY = cy;
      for (int s = 1; s <= 48; ++s)
      {
        const float a = (float)s / 48.f * 6.28318f;
        const float ax = cxA + cosf(a) * radius;
        const float ay = cy + sinf(a) * radius;
        const float bx = cxB + cosf(a + 0.16f) * radius;
        const float by = cy + sinf(a + 0.16f) * radius;
        g.DrawLine(teal.WithOpacity(0.28f + i * 0.06f), prevAX, prevAY, ax, ay, nullptr, 1.0f);
        g.DrawLine(blue.WithOpacity(0.24f + i * 0.05f), prevBX, prevBY, bx, by, nullptr, 1.0f);
        prevAX = ax;
        prevAY = ay;
        prevBX = bx;
        prevBY = by;
      }
    }
  }
  else if (effect == EVoLumEffectFocus::DELAY)
  {
    const float activeMul = dimmed ? 0.28f : 1.0f;
    int taps = 5;
    float tapW = r.W() / (float)taps;
    for (int t = 0; t < taps; t++)
    {
      float decay = 1.f - (float)t / (float)taps * 0.7f;
      float ampY = r.H() * 0.35f * decay;
      int alpha = (int)(140.f * decay * activeMul);
      float baseX = r.L + t * tapW;
      int segs = 40;
      for (int j = 0; j < segs; j++)
      {
        float t1 = (float)j / segs;
        float t2 = (float)(j + 1) / segs;
        float x1 = baseX + t1 * tapW;
        float x2 = baseX + t2 * tapW;
        float env1 = sinf(t1 * 3.14159f);
        float env2 = sinf(t2 * 3.14159f);
        float y1 = cy + sinf(t1 * 6.28318f * 3.f) * ampY * env1;
        float y2 = cy + sinf(t2 * 6.28318f * 3.f) * ampY * env2;
        g.DrawLine(IColor(alpha, 100, 190, 210), x1, y1, x2, y2, nullptr, 1.5f * decay);
      }
      if (t > 0)
      {
        float tx = baseX;
        g.DrawLine(IColor(alpha / 3, 100, 190, 210), tx, r.T + 4.f, tx, r.B - 4.f, nullptr, 0.5f);
      }
    }
    g.DrawLine(IColor((int)(35.f * activeMul), 100, 190, 210), r.L, cy, r.R, cy, nullptr, 0.5f);
  }
  else if (effect == EVoLumEffectFocus::TREMOLO)
  {
    // Amplitude modulation: a fast carrier whose envelope swells and dips under
    // a slow LFO, plus the dashed LFO envelope outline. The classic tremolo look.
    const float activeMul = dimmed ? 0.28f : 1.0f;
    const IColor teal((int)(135.f * activeMul), 90, 205, 220);
    const IColor blue((int)(140.f * activeMul), 150, 205, 245);
    const float x0 = r.L + r.W() * 0.08f;
    const float x1 = r.R - r.W() * 0.08f;
    const float maxAmp = r.H() * 0.34f;
    const int segs = 96;
    float pcx = 0.f, pTop = 0.f, pBot = 0.f, pcy = 0.f;
    for (int s = 0; s <= segs; ++s)
    {
      const float t = (float)s / (float)segs;
      const float x = x0 + (x1 - x0) * t;
      const float env = 0.28f + 0.72f * (0.5f + 0.5f * sinf(t * 6.28318f * 1.5f - 1.5708f));
      const float amp = maxAmp * env;
      const float carrier = cy + sinf(t * 6.28318f * 9.f) * amp;
      const float top = cy - amp;
      const float bot = cy + amp;
      if (s > 0)
      {
        g.DrawLine(teal, pcx, pcy, x, carrier, nullptr, 1.5f);
        g.DrawLine(blue.WithOpacity(0.45f), pcx, pTop, x, top, nullptr, 1.0f);
        g.DrawLine(blue.WithOpacity(0.45f), pcx, pBot, x, bot, nullptr, 1.0f);
      }
      pcx = x;
      pcy = carrier;
      pTop = top;
      pBot = bot;
    }
  }
  else
  {
    // Reverb (default): DLA-style branching dust grown from baseline + mid
    // seeds. We scale the iteration count and step length to the rect size so
    // the same pattern stays lush at pedal-card size and readable at the
    // PRE/POST thumbnail.
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
