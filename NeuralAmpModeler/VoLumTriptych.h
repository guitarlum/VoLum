#pragma once

#include "VoLumColorHelpers.h"
#include "VoLumTriptychState.h"
#include "NeuralAmpModeler.h"

#include <algorithm>

using namespace iplug;
using namespace igraphics;

//==============================================================================
// Reverb & Delay Extension Controls (PRE / AMP / POST)
//==============================================================================

// Per-effect "motif" drawn in pedal cards and in the Quiet PRE/POST slots.
// `dimmed` collapses alpha when the effect is bypassed.
inline void DrawEffectMotif(IGraphics& g, const IRECT& r, EVoLumEffectFocus effect, bool dimmed)
{
  float cy = r.MH();
  IColor bright(dimmed ? 70 : 150, 120, 210, 220);
  IColor mid(dimmed ? 40 : 80, 100, 180, 200);
  IColor dim(dimmed ? 20 : 45, 80, 150, 170);

  if (effect == EVoLumEffectFocus::COMP)
  {
    const float activeMul = dimmed ? 0.25f : 1.0f;
    const IColor teal((int)(125.f * activeMul), 90, 205, 220);
    const IColor blue((int)(135.f * activeMul), 145, 220, 245);
    const float cx = r.MW();
    const float radii[][2] = {{r.W() * 0.24f, r.H() * 0.11f}, {r.W() * 0.33f, r.H() * 0.16f},
                              {r.W() * 0.42f, r.H() * 0.21f}, {r.W() * 0.50f, r.H() * 0.26f}};
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
          g.DrawRect(dim.WithOpacity(0.65f), IRECT(px + sub, py + sub, px + cell - sub, py + cell - sub), nullptr, 0.9f);
        }
      }
    }
    g.DrawRect(teal.WithOpacity(0.55f), IRECT(left - 2.f, top - 2.f, left + cell * 6.8f, top + cell * 4.3f), nullptr, 1.2f);
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
    for (int t = 0; t < taps; t++) {
      float decay = 1.f - (float)t / (float)taps * 0.7f;
      float ampY = r.H() * 0.35f * decay;
      int alpha = (int)(140.f * decay * activeMul);
      float baseX = r.L + t * tapW;
      int segs = 40;
      for (int j = 0; j < segs; j++) {
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
      if (t > 0) {
        float tx = baseX;
        g.DrawLine(IColor(alpha / 3, 100, 190, 210), tx, r.T + 4.f, tx, r.B - 4.f, nullptr, 0.5f);
      }
    }
    g.DrawLine(IColor((int)(35.f * activeMul), 100, 190, 210), r.L, cy, r.R, cy, nullptr, 0.5f);
  }
  else
  {
    // Reverb (default): DLA-style branching dust grown from baseline + mid
    // seeds. We scale the iteration count and step length to the rect size so
    // the same pattern stays lush at pedal-card size and readable at the
    // PRE/POST thumbnail.
    const float scale = std::min(r.W(), r.H());           // ~22 thumb, ~150 card
    const float sizeFactor = std::clamp(scale / 150.f, 0.18f, 1.0f);
    const int   count       = std::max(450, (int)(12000.f * sizeFactor * sizeFactor));
    const float lenScale    = std::max(0.55f, sizeFactor); // shorter strokes at small size
    const int   brightCount = (int)(count * 0.025f);
    const int   midCount    = (int)(count * 0.125f);

    struct Pt { float x, y; };
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
    for (int i = 0; i < count; i++) {
      rng = rng * 1103515245 + 12345;
      int parentIdx = rng % pts.size();
      float angle = (float)(rng % 360);
      float rad = angle * 3.14159f / 180.f;
      float len = (2.f + (float)(rng % 4)) * lenScale;
      Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
      if (next.x >= r.L && next.x <= r.R && next.y >= r.T && next.y <= r.B) {
        IColor col = (i < brightCount) ? bright : ((i < midCount) ? mid : dim);
        float tk = (i < (int)(brightCount * 0.7f)) ? 2.f
                                                    : ((i < midCount) ? 1.5f : 1.f);
        g.DrawLine(col, pts[parentIdx].x, pts[parentIdx].y, next.x, next.y, nullptr, tk);
        pts.push_back(next);
      }
    }
  }
}

class VoLumChainConnectorControl : public IControl
{
public:
  VoLumChainConnectorControl(const IRECT& bounds) : IControl(bounds) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    g.DrawLine(VoLumColors::TEAL.WithOpacity(0.55f), mRECT.L, mRECT.MH(), mRECT.R, mRECT.MH(), nullptr, 1.f);
    g.FillCircle(VoLumColors::TEAL, mRECT.MW(), mRECT.MH(), 2.f);
  }
};

class VoLumTriptychControl : public IControl
{
public:
  using StateCallback = std::function<void(EVoLumSection, EVoLumEffectFocus)>;

  struct QuietSlot { EVoLumEffectFocus focus; const char* label; int paramIdx; };

  VoLumTriptychControl(const IRECT& bounds, StateCallback cb)
  : IControl(bounds)
  , mCallback(std::move(cb))
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    const float stripW = 100.f;
    const EVoLumSection displaySection = mExpandedSection;
    const float expandedW = (displaySection == EVoLumSection::AMP) ? 400.f : 430.f;
    const float gap = 10.f;
    const float cx = mRECT.MW();

    IRECT preRect, ampRect, postRect;

    if (displaySection == EVoLumSection::AMP)
    {
      const float totalW = stripW + gap + expandedW + gap + stripW;
      const float left = cx - totalW / 2.f;
      preRect = IRECT(left, mRECT.T, left + stripW, mRECT.B);
      ampRect = IRECT(preRect.R + gap, mRECT.T, preRect.R + gap + expandedW, mRECT.B);
      postRect = IRECT(ampRect.R + gap, mRECT.T, ampRect.R + gap + stripW, mRECT.B);
    }
    else if (displaySection == EVoLumSection::POST)
    {
      const float ampStripW = 70.f;
      const float preStripW = stripW;
      const float totalW = preStripW + gap + ampStripW + gap + expandedW;
      const float left = cx - totalW / 2.f;
      preRect = IRECT(left, mRECT.T, left + preStripW, mRECT.B);
      ampRect = IRECT(preRect.R + gap, mRECT.T, preRect.R + gap + ampStripW, mRECT.B);
      postRect = IRECT(ampRect.R + gap, mRECT.T, ampRect.R + gap + expandedW, mRECT.B);
    }
    else // PRE
    {
      const float ampStripW = 70.f;
      const float postStripW = stripW;
      const float totalW = expandedW + gap + ampStripW + gap + postStripW;
      const float left = cx - totalW / 2.f;
      preRect = IRECT(left, mRECT.T, left + expandedW, mRECT.B);
      ampRect = IRECT(preRect.R + gap, mRECT.T, preRect.R + gap + ampStripW, mRECT.B);
      postRect = IRECT(ampRect.R + gap, mRECT.T, ampRect.R + gap + postStripW, mRECT.B);
    }

    mPreRect = preRect;
    mAmpRect = ampRect;
    mPostRect = postRect;

    // Reset hit-zone tracking before re-populating during slot draw.
    mSlotNavRects.clear();
    mSlotToggleRects.clear();
    mSlotParams.clear();
    mSlotFocuses.clear();
    mSlotSection.clear();
    mPreHeaderRect = IRECT();
    mPostHeaderRect = IRECT();
    mAmpBlockRect = IRECT();

    if (displaySection == EVoLumSection::PRE)
      _DrawExpandedFrame(g, preRect, "PRE");
    else
      _DrawQuietBlock(g, preRect, EVoLumSection::PRE);

    if (displaySection == EVoLumSection::AMP) {
      // VoLumHeroImageControl draws the AMP frame on top.
    } else {
      _DrawAmpStrip(g, ampRect);
    }

    if (displaySection == EVoLumSection::POST)
      _DrawExpandedFrame(g, postRect, "POST");
    else
      _DrawQuietBlock(g, postRect, EVoLumSection::POST);
  }

  void SetState(bool preActive, bool postActive, int ampIdx, const char* ampName)
  {
    (void) preActive;
    mPreActive = preActive;
    mPostActive = postActive;
    mAmpIdx = ampIdx;
    if (mAmpName != ampName)
    {
      mAmpName = ampName;
      // Invalidate the spine font-size cache so the next redraw re-measures
      // for the new name.
      mCachedSpineSize = 0.f;
      mCachedSpineMaxLen = -1.f;
      mCachedSpineName.clear();
    }
    SetDirty(false);
  }

  void SetExpandedSection(EVoLumSection s)
  {
    mExpandedSection = s;
    SetDirty(false);
  }

  IRECT GetPostExpandedRect() const { return mPostRect; }
  IRECT GetPreExpandedRect() const { return mPreRect; }

private:
  static constexpr QuietSlot kPreSlots[3] = {
    { EVoLumEffectFocus::COMP,     "COMP",  kPreCompActive },
    { EVoLumEffectFocus::PRE_NAM1, "NAM 1", kPreNam1Active },
    { EVoLumEffectFocus::PRE_NAM2, "NAM 2", kPreNam2Active },
  };
  static constexpr QuietSlot kPostSlots[2] = {
    { EVoLumEffectFocus::DELAY,  "DELAY",  kDelayActive },
    { EVoLumEffectFocus::REVERB, "REVERB", kReverbActive },
  };

  void _DrawExpandedFrame(IGraphics& g, const IRECT& r, const char* label)
  {
    g.FillRect(VoLumColors::HERO_BG, r);
    g.DrawRect(VoLumColors::FRAME, r);
    const float cs = 8.f;
    DrawCornerAccent(g, r.L + 4.f, r.T + 4.f, cs, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, r.R - 4.f, r.T + 4.f, cs, true, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, r.L + 4.f, r.B - 4.f, cs, false, true, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, r.R - 4.f, r.B - 4.f, cs, true, true, VoLumColors::TEAL_DIM);
    IText txt(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    g.DrawText(txt, label, IRECT(r.L + 10.f, r.T + 4.f, r.L + 80.f, r.T + 22.f));
  }

  bool _GetParamBool(int paramIdx) const
  {
    auto* del = const_cast<VoLumTriptychControl*>(this)->GetDelegate();
    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del))
      return plugin->GetParam(paramIdx)->Bool();
    return false;
  }

  void _DrawMiniPill(IGraphics& g, const IRECT& r, bool on, bool dimmed)
  {
    // 24x11 horizontal pill. Uses the same gold-track / teal-LED idiom as
    // VoLumPowerSwitchControl + the toggles below the amp panel.
    const float radius = r.H() * 0.5f;
    const IColor track = on ? VoLumColors::GOLD.WithOpacity(dimmed ? 0.18f : 0.35f)
                            : VoLumColors::FRAME;
    const IColor knob = on ? VoLumColors::GOLD : VoLumColors::TEXT_DIM;

    g.FillRoundRect(IColor(255, 8, 10, 14), r, radius);
    g.DrawRoundRect(track, r, radius);

    const float knobR = radius - 1.5f;
    const float knobCY = r.MH();
    const float knobCX = on ? r.R - radius : r.L + radius;
    g.FillEllipse(knob, IRECT(knobCX - knobR, knobCY - knobR, knobCX + knobR, knobCY + knobR));
    if (on)
    {
      const IColor led = VoLumColors::TEAL.WithOpacity(dimmed ? 0.55f : 1.0f);
      g.FillCircle(led, knobCX, knobCY, 1.4f);
    }
  }

  void _DrawQuietBlock(IGraphics& g, const IRECT& r, EVoLumSection section)
  {
    // Center an 80W x 140H block vertically inside the strip rect (strip is
    // already 80 wide, so we only adjust height).
    const float blockH = 140.f;
    const float blockTop = r.MH() - blockH / 2.f;
    const IRECT block(r.L, blockTop, r.R, blockTop + blockH);

    g.FillRect(VoLumColors::HERO_BG, block);
    g.DrawRect(VoLumColors::FRAME, block);
    const float cs = 6.f;
    DrawCornerAccent(g, block.L + 3.f, block.T + 3.f, cs, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.T + 3.f, cs, true,  false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.L + 3.f, block.B - 3.f, cs, false, true,  VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.B - 3.f, cs, true,  true,  VoLumColors::TEAL_DIM);

    // Header: label + chevron + 1px teal underline.
    const float headerH = 22.f;
    const IRECT header(block.L + 4.f, block.T + 2.f, block.R - 4.f, block.T + 2.f + headerH);
    const char* hdrLabel = (section == EVoLumSection::PRE) ? "PRE" : "POST";

    IText hdrText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float chevronW = 7.f;
    const IRECT hdrTextRect(header.L, header.T, header.R - chevronW - 2.f, header.B);
    g.DrawText(hdrText, hdrLabel, hdrTextRect);

    // Chevron (>) drawn from two short lines, just to the right of the label.
    const float cxC = header.R - chevronW;
    const float cyC = header.MH();
    const float chs = 3.5f;
    const IColor chevronCol = VoLumColors::GOLD_DIM;
    g.DrawLine(chevronCol, cxC, cyC - chs, cxC + chs, cyC, nullptr, 1.2f);
    g.DrawLine(chevronCol, cxC + chs, cyC, cxC, cyC + chs, nullptr, 1.2f);

    // Hairline under the header.
    const float underlineY = header.B + 1.f;
    g.DrawLine(VoLumColors::TEAL_DIM.WithOpacity(0.55f),
               block.L + 6.f, underlineY, block.R - 6.f, underlineY, nullptr, 1.f);

    if (section == EVoLumSection::PRE)
      mPreHeaderRect = IRECT(block.L, block.T, block.R, header.B + 2.f);
    else
      mPostHeaderRect = IRECT(block.L, block.T, block.R, header.B + 2.f);

    // Slot iteration.
    const QuietSlot* slots = (section == EVoLumSection::PRE) ? kPreSlots : kPostSlots;
    const int slotCount = (section == EVoLumSection::PRE) ? 3 : 2;
    const float innerTop = underlineY + 2.f;
    const float innerBot = block.B - 4.f;
    const float slotH = (innerBot - innerTop) / (float)slotCount;

    for (int i = 0; i < slotCount; ++i)
    {
      const IRECT slotR(block.L + 2.f, innerTop + i * slotH,
                        block.R - 2.f, innerTop + (i + 1) * slotH);
      _DrawQuietSlot(g, slotR, slots[i], section, i, i < slotCount - 1);
    }
  }

  void _DrawQuietSlot(IGraphics& g, const IRECT& slotR, const QuietSlot& slot,
                      EVoLumSection section, int slotIdx, bool drawDivider)
  {
    const bool active = _GetParamBool(slot.paramIdx);
    const bool bypassed = !active;
    const int globalIdx = (int)mSlotNavRects.size();
    const bool hovered = (mHoveredSlot == globalIdx);

    if (hovered)
    {
      // Subtle hover lift (~5% brighter background) + faint teal inner edge.
      g.FillRect(IColor(20, 80, 140, 160), slotR);
      g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.35f),
                 slotR.GetPadded(-1.f, -1.f, -1.f, -1.f));
    }

    // Active indicator: 3 px teal edge bar pinned to the left of the slot.
    if (active)
    {
      const IRECT edge(slotR.L + 1.f, slotR.T + 2.f, slotR.L + 4.f, slotR.B - 2.f);
      g.FillRect(VoLumColors::TEAL, edge);
    }

    // Layout inside the slot. Block is 100 wide, slot inner ~96 wide:
    //   leftPad(6) + motif(20) + gap(4) + label(36) + gap(4) + pill(20) + rightPad(6)
    const float leftPad = 6.f;
    const float motifSize = std::min(20.f, slotR.H() - 6.f);
    const float motifL = slotR.L + leftPad;
    const float motifT = slotR.MH() - motifSize / 2.f;
    const IRECT motifR(motifL, motifT, motifL + motifSize, motifT + motifSize);

    // Mini pill on the right edge. Bumped to 22x14 for a friendlier hit target
    // while still leaving label space for "REVERB".
    const float pillW = 22.f;
    const float pillH = 14.f;
    const IRECT pillR(slotR.R - pillW - 6.f, slotR.MH() - pillH / 2.f,
                      slotR.R - 6.f, slotR.MH() + pillH / 2.f);

    // Label region between motif and pill.
    const IRECT labelR(motifR.R + 4.f, slotR.T + 2.f,
                       pillR.L - 4.f, slotR.B - 2.f);

    DrawEffectMotif(g, motifR, slot.focus, bypassed);
    IText labelText(9.f, bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM,
                    "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    g.DrawText(labelText, slot.label, labelR);

    _DrawMiniPill(g, pillR, active, bypassed && !active);

    // Hairline divider between slots (skip after the last).
    if (drawDivider)
    {
      g.DrawLine(VoLumColors::FRAME.WithOpacity(0.5f),
                 slotR.L + 6.f, slotR.B, slotR.R - 6.f, slotR.B, nullptr, 1.f);
    }

    // Track interactive zones. Toggle hit-rect is padded a few px beyond the
    // visible pill for forgiving hit-testing; nav zone covers everything to
    // the left of that toggle hit-rect.
    const IRECT toggleHitR = pillR.GetPadded(4.f, 3.f, 4.f, 3.f);
    IRECT navR = slotR;
    navR.R = toggleHitR.L - 1.f;
    mSlotNavRects.push_back(navR);
    mSlotToggleRects.push_back(toggleHitR);
    mSlotParams.push_back(slot.paramIdx);
    mSlotFocuses.push_back(slot.focus);
    mSlotSection.push_back(section);
    (void)slotIdx;
  }

  void _DrawAmpStrip(IGraphics& g, const IRECT& r)
  {
    // Quiet block in the same frame language as the PRE/POST collapsed blocks
    // but taller (180 H vs 140 H) so the AMP reads as the centerpiece of the
    // row and the rotated amp name has room to breathe. No thumbnail motif -
    // the spine is the whole point. Click anywhere in the strip navigates to
    // the AMP-expanded view (handled by OnMouseDown via mAmpRect).
    const float blockH = 180.f;
    const float blockTop = r.MH() - blockH / 2.f;
    const IRECT block(r.L, blockTop, r.R, blockTop + blockH);
    mAmpBlockRect = block;

    g.FillRect(VoLumColors::HERO_BG, block);
    if (mAmpHovered)
    {
      g.FillRect(IColor(20, 80, 140, 160), block);
      g.DrawRect(VoLumColors::TEAL_DIM.WithOpacity(0.55f),
                 block.GetPadded(-1.f, -1.f, -1.f, -1.f));
    }
    g.DrawRect(VoLumColors::FRAME, block);
    const float cs = 6.f;
    DrawCornerAccent(g, block.L + 3.f, block.T + 3.f, cs, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.T + 3.f, cs, true,  false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.L + 3.f, block.B - 3.f, cs, false, true,  VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, block.R - 3.f, block.B - 3.f, cs, true,  true,  VoLumColors::TEAL_DIM);

    // Header: "AMP" + chevron + 1px teal hairline (mirrors PRE/POST exactly).
    const float headerH = 22.f;
    const IRECT header(block.L + 4.f, block.T + 2.f, block.R - 4.f, block.T + 2.f + headerH);
    IText hdrText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float chevronW = 7.f;
    const IRECT hdrTextRect(header.L, header.T, header.R - chevronW - 2.f, header.B);
    g.DrawText(hdrText, "AMP", hdrTextRect);

    const float cxC = header.R - chevronW;
    const float cyC = header.MH();
    const float chs = 3.5f;
    const IColor chevronCol = VoLumColors::GOLD_DIM;
    g.DrawLine(chevronCol, cxC, cyC - chs, cxC + chs, cyC, nullptr, 1.2f);
    g.DrawLine(chevronCol, cxC + chs, cyC, cxC, cyC + chs, nullptr, 1.2f);

    const float underlineY = header.B + 1.f;
    g.DrawLine(VoLumColors::TEAL_DIM.WithOpacity(0.55f),
               block.L + 6.f, underlineY, block.R - 6.f, underlineY, nullptr, 1.f);

    // Rotated spine fills the rest of the block. With ~150 px of vertical
    // budget the auto-shrink picks ~14pt for "Diezel Herbert Mk1" and ~16pt
    // for short names like "AMP" or "Marshall JMP", floor at 8pt.
    const IRECT spineR(block.L + 4.f, underlineY + 4.f,
                       block.R - 4.f, block.B - 6.f);
    const char* name = mAmpName.empty() ? "AMP" : mAmpName.c_str();
    const float maxLen = spineR.H() - 2.f;
    const float chosenSize = _ResolveSpineFontSize(g, name, maxLen);
    IText spineText(chosenSize, VoLumColors::CREAM, "Josefin-Bold",
                    EAlign::Center, EVAlign::Middle);
    spineText.mAngle = -90.f; // bottom-to-top: head tilts left to read.
    g.DrawText(spineText, name, spineR);
  }

  // Picks the largest font size from a fixed descending table whose rendered
  // width fits in maxLen (the rotated text's vertical extent). MeasureText
  // hits GDI/DirectWrite on Windows so cache the answer keyed by amp name +
  // spine height; the cache is invalidated in SetState when the amp name
  // changes and is also implicitly refreshed if the layout changes maxLen.
  float _ResolveSpineFontSize(IGraphics& g, const char* name, float maxLen)
  {
    if (mCachedSpineSize > 0.f
        && mCachedSpineMaxLen == maxLen
        && mCachedSpineName == name)
      return mCachedSpineSize;

    static const float kSpineSizes[] = {16.f, 14.f, 12.f, 11.f, 10.f, 9.f, 8.f};
    float chosenSize = 8.f;
    for (float s : kSpineSizes)
    {
      IText probe(s, VoLumColors::CREAM, "Josefin-Bold");
      IRECT measured;
      g.MeasureText(probe, name, measured);
      if (measured.W() <= maxLen)
      {
        chosenSize = s;
        break;
      }
    }

    mCachedSpineName = name;
    mCachedSpineMaxLen = maxLen;
    mCachedSpineSize = chosenSize;
    return chosenSize;
  }


  EVoLumEffectFocus _FirstActiveOrFirst(EVoLumSection section) const
  {
    const QuietSlot* slots = (section == EVoLumSection::PRE) ? kPreSlots : kPostSlots;
    const int count = (section == EVoLumSection::PRE) ? 3 : 2;
    for (int i = 0; i < count; ++i)
      if (_GetParamBool(slots[i].paramIdx))
        return slots[i].focus;
    return slots[0].focus;
  }

  void _ToggleParam(int paramIdx)
  {
    auto* del = GetDelegate();
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(del);
    if (!plugin) return;
    const double cur = plugin->GetParam(paramIdx)->Value();
    const double next = (cur > 0.5) ? 0.0 : 1.0;
    del->BeginInformHostOfParamChangeFromUI(paramIdx);
    del->SendParameterValueFromUI(paramIdx, next);
    del->EndInformHostOfParamChangeFromUI(paramIdx);
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;

    // 1) Toggle pills first (highest precedence so users can flip bypass
    // without leaving the AMP view).
    for (size_t i = 0; i < mSlotToggleRects.size(); ++i)
    {
      if (mSlotToggleRects[i].Contains(x, y))
      {
        _ToggleParam(mSlotParams[i]);
        return;
      }
    }

    // 2) Slot navigation zones: jump to that section and focus the slot.
    for (size_t i = 0; i < mSlotNavRects.size(); ++i)
    {
      if (mSlotNavRects[i].Contains(x, y))
      {
        const EVoLumSection sec = mSlotSection[i];
        const EVoLumEffectFocus focus = mSlotFocuses[i];
        mExpandedSection = sec;
        if (mCallback) mCallback(sec, focus);
        SetDirty(false);
        return;
      }
    }

    // 3) Header click: navigate, focus first active or first slot.
    if (mPreHeaderRect.W() > 0 && mPreHeaderRect.Contains(x, y))
    {
      mExpandedSection = EVoLumSection::PRE;
      if (mCallback) mCallback(EVoLumSection::PRE, _FirstActiveOrFirst(EVoLumSection::PRE));
      SetDirty(false);
      return;
    }
    if (mPostHeaderRect.W() > 0 && mPostHeaderRect.Contains(x, y))
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback) mCallback(EVoLumSection::POST, _FirstActiveOrFirst(EVoLumSection::POST));
      SetDirty(false);
      return;
    }

    // 4) Whole-block fallback (covers gaps between rects, e.g. block frame
    // padding). Behaviour matches a header click on that block.
    if (mPreRect.Contains(x, y) && mExpandedSection != EVoLumSection::PRE)
    {
      mExpandedSection = EVoLumSection::PRE;
      if (mCallback) mCallback(EVoLumSection::PRE, _FirstActiveOrFirst(EVoLumSection::PRE));
      SetDirty(false);
      return;
    }
    if (mAmpRect.Contains(x, y) && mExpandedSection != EVoLumSection::AMP)
    {
      mExpandedSection = EVoLumSection::AMP;
      if (mCallback) mCallback(EVoLumSection::AMP, EVoLumEffectFocus::AMP);
      SetDirty(false);
      return;
    }
    if (mPostRect.Contains(x, y) && mExpandedSection != EVoLumSection::POST)
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback) mCallback(EVoLumSection::POST, _FirstActiveOrFirst(EVoLumSection::POST));
      SetDirty(false);
      return;
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    int next = -1;
    for (size_t i = 0; i < mSlotNavRects.size(); ++i)
    {
      if (mSlotNavRects[i].Contains(x, y))
      {
        next = (int)i;
        break;
      }
    }
    // AMP strip hover only matters when AMP is collapsed (i.e. PRE or POST is
    // expanded), so a subtle hover invites a click back to AMP view. Hover is
    // gated on the visible block rect so the empty whitespace around it does
    // not light up.
    const bool ampHov = (mExpandedSection != EVoLumSection::AMP)
                        && mAmpBlockRect.W() > 0
                        && mAmpBlockRect.Contains(x, y);
    bool dirty = false;
    if (next != mHoveredSlot) { mHoveredSlot = next; dirty = true; }
    if (ampHov != mAmpHovered) { mAmpHovered = ampHov; dirty = true; }
    if (dirty) SetDirty(false);
  }

  void OnMouseOut() override
  {
    bool dirty = false;
    if (mHoveredSlot != -1) { mHoveredSlot = -1; dirty = true; }
    if (mAmpHovered)        { mAmpHovered = false; dirty = true; }
    if (dirty) SetDirty(false);
  }

  StateCallback mCallback;
  bool mPreActive = false;
  bool mPostActive = false;
  int mAmpIdx = 0;
  std::string mAmpName;
  EVoLumSection mExpandedSection = EVoLumSection::AMP;

  IRECT mPreRect;
  IRECT mAmpRect;
  // Visible AMP block (180 H, centered inside the 196 H strip rect) - used for
  // hover hit-testing so the hover lift only fires when the cursor is over the
  // block, not the empty whitespace above/below it. Click hit-testing still
  // uses mAmpRect (the full strip) so clicks near the block still register.
  IRECT mAmpBlockRect;

  // Cached auto-shrink result for the rotated spine font size. MeasureText
  // hits GDI/DirectWrite on Windows; caching by (name, maxLen) skips up to
  // 7 measure calls per redraw in the steady state.
  std::string mCachedSpineName;
  float mCachedSpineMaxLen = -1.f;
  float mCachedSpineSize = 0.f;
  IRECT mPostRect;

  // Quiet block hit-zones, repopulated each Draw().
  std::vector<IRECT> mSlotNavRects;
  std::vector<IRECT> mSlotToggleRects;
  std::vector<int> mSlotParams;
  std::vector<EVoLumEffectFocus> mSlotFocuses;
  std::vector<EVoLumSection> mSlotSection;
  IRECT mPreHeaderRect;
  IRECT mPostHeaderRect;
  int mHoveredSlot = -1;
  bool mAmpHovered = false;
};

class VoLumPreCaptureMenuControl : public IControl
{
public:
  VoLumPreCaptureMenuControl(const IRECT& bounds) : IControl(bounds) { mIgnoreMouse = false; }

  void SetItems(int slot, const std::vector<std::string>& labels, int selectedIdx)
  {
    mSlot = slot;
    mLabels = labels;
    mSelectedIdx = selectedIdx;
    mHovered = -1;
    SetDirty(false);
  }

  int GetSlot() const { return mSlot; }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mRECT, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mRECT, 4.f, nullptr, 1.5f);
    DrawCornerAccent(g, mRECT.L + 5.f, mRECT.T + 5.f, 8.f, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, mRECT.R - 5.f, mRECT.B - 5.f, 8.f, true, true, VoLumColors::TEAL_DIM);

    const IText text(12.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const IText dimText(12.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    for (int i = 0; i < static_cast<int>(mLabels.size()); ++i)
    {
      const IRECT row(mRECT.L + 8.f, mRECT.T + 6.f + i * mItemH, mRECT.R - 8.f, mRECT.T + 6.f + (i + 1) * mItemH);
      const bool selected = i == mSelectedIdx;
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      else if (i == mHovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(IColor(20, 200, 162, 78), row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      if (selected)
        g.FillCircle(VoLumColors::TEAL, row.L + 8.f, row.MH(), 3.f);
      g.DrawText(selected ? text : dimText, mLabels[static_cast<size_t>(i)].c_str(), IRECT(row.L + 20.f, row.T, row.R, row.B));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    const int idx = static_cast<int>((y - (mRECT.T + 6.f)) / mItemH);
    if (idx < 0 || idx >= static_cast<int>(mLabels.size()))
      return;

    if (auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate()))
    {
      plugin->_VolumSetPreNamCapture(mSlot, idx);
      plugin->_VolumHidePreCaptureMenu();
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) mod;
    const int idx = static_cast<int>((y - (mRECT.T + 6.f)) / mItemH);
    const int next = (idx >= 0 && idx < static_cast<int>(mLabels.size())) ? idx : -1;
    if (next != mHovered)
    {
      mHovered = next;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mHovered != -1)
    {
      mHovered = -1;
      SetDirty(false);
    }
  }

  static constexpr float ItemHeight() { return 22.f; }

private:
  int mSlot = 0;
  int mSelectedIdx = 0;
  int mHovered = -1;
  float mItemH = ItemHeight();
  std::vector<std::string> mLabels;
};

class VoLumPedalCardControl : public IControl
{
public:
  using ClickCallback = std::function<void(VoLumPedalCardControl*, bool isBypassClick)>;

  VoLumPedalCardControl(const IRECT& bounds, EVoLumEffectFocus effect, const char* name, int fractalCase, int activeParamIdx, ClickCallback cb)
  : IControl(bounds)
  , mEffect(effect)
  , mName(name)
  , mFractalCase(fractalCase)
  , mActiveParamIdx(activeParamIdx)
  , mCallback(std::move(cb))
  {
  }

  void Draw(IGraphics& g) override
  {
    bool focused = mIsFocused;
    bool bypassed = (GetValue() < 0.5);

    g.FillRect(VoLumColors::HERO_BG, mRECT);
    
    IColor borderCol = focused ? VoLumColors::AMBER : (bypassed ? VoLumColors::FRAME : VoLumColors::TEAL_DIM);
    g.DrawRoundRect(borderCol, mRECT, 4.f);
    const float cs = 8.f;
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.T + 4.f, cs, false, false, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.T + 4.f, cs, true, false, borderCol);
    DrawCornerAccent(g, mRECT.L + 4.f, mRECT.B - 4.f, cs, false, true, borderCol);
    DrawCornerAccent(g, mRECT.R - 4.f, mRECT.B - 4.f, cs, true, true, borderCol);

    IRECT artRect = mRECT.GetPadded(-2.f, -2.f, -2.f, -22.f);
    if (!mArtLayer || g.CheckLayer(mArtLayer) || mCachedBypassed != bypassed) {
      g.StartLayer(this, artRect);
      _DrawFractalArt(g, artRect, bypassed);
      mArtLayer = g.EndLayer();
      mCachedBypassed = bypassed;
    }
    g.DrawLayer(mArtLayer);
    if (bypassed)
      g.FillRect(VoLumColors::HERO_BG.WithOpacity(0.68f), artRect);

    IText presetTxt(11.5f, bypassed ? VoLumColors::CREAM_DIM : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const std::string presetName = _GetPresetName();
    const IRECT presetRect(mRECT.L + 10.f, mRECT.B - 22.f, mRECT.R - 22.f, mRECT.B - 4.f);
    g.DrawText(presetTxt, presetName.c_str(), presetRect);

    IRECT ledRect(mRECT.R - 20.f, mRECT.B - 20.f, mRECT.R - 8.f, mRECT.B - 8.f);
    g.FillCircle(bypassed ? IColor(255, 42, 48, 52) : VoLumColors::TEAL, ledRect.MW(), ledRect.MH(), 4.5f);
    mLedRect = ledRect;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (mIsFocused)
    {
      if (plugin && mEffect == EVoLumEffectFocus::PRE_NAM1)
      {
        plugin->_VolumShowPreCaptureMenu(0, mRECT);
        return;
      }
      if (plugin && mEffect == EVoLumEffectFocus::PRE_NAM2)
      {
        plugin->_VolumShowPreCaptureMenu(1, mRECT);
        return;
      }
    }
    if (mCallback)
      mCallback(this, false);
  }
  
  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
  }

  void SetFocused(bool focused)
  {
    mIsFocused = focused;
    SetDirty(false);
  }

  void SetActiveState(bool active)
  {
    const double value = active ? 1.0 : 0.0;
    const bool wasBypassed = GetValue() < 0.5;
    const bool willBypass = !active;
    if (GetValue() != value)
      SetValue(value);
    if (wasBypassed != willBypass || mCachedBypassed != willBypass)
      mArtLayer = nullptr;
    SetDirty(false);
  }

  EVoLumEffectFocus GetEffect() const { return mEffect; }

private:
  void _DrawFractalArt(IGraphics& g, const IRECT& r, bool dimmed)
  {
    DrawEffectMotif(g, r, mEffect, dimmed);
  }


  std::string _GetPresetName()
  {
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (!plugin)
      return (mEffect == EVoLumEffectFocus::DELAY) ? "DIGITAL . 380 ms" : "HALL . 50%";

    WDL_String modeText;
    WDL_String summary;
    switch (mEffect)
    {
      case EVoLumEffectFocus::DELAY:
        plugin->GetParam(kDelayMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f ms", modeText.Get(), plugin->GetParam(kDelayTime)->Value());
        return summary.Get();
      case EVoLumEffectFocus::REVERB:
        plugin->GetParam(kReverbMode)->GetDisplay(modeText);
        summary.SetFormatted(64, "%s . %.0f %%", modeText.Get(), plugin->GetParam(kReverbMix)->Value() * 100.0);
        return summary.Get();
      case EVoLumEffectFocus::COMP:
        summary.SetFormatted(64, "%.1f:1 . %.1f", plugin->GetParam(kPreCompRatio)->Value(), plugin->GetParam(kPreCompAmount)->Value());
        return summary.Get();
      case EVoLumEffectFocus::PRE_NAM1:
        return plugin->_VolumGetPreCaptureLabel(plugin->GetParam(kPreNam1Capture)->Int());
      case EVoLumEffectFocus::PRE_NAM2:
        return plugin->_VolumGetPreCaptureLabel(plugin->GetParam(kPreNam2Capture)->Int());
      default:
        return "BYPASS";
    }
  }

  EVoLumEffectFocus mEffect;
  std::string mName;
  int mFractalCase;
  int mActiveParamIdx;
  bool mIsFocused = false;
  ILayerPtr mArtLayer;
  bool mCachedBypassed = false;
  IRECT mLedRect;
  ClickCallback mCallback;
};
