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

  VoLumTriptychControl(const IRECT& bounds, StateCallback cb)
  : IControl(bounds)
  , mCallback(std::move(cb))
  {}

  void Draw(IGraphics& g) override
  {
    const float stripW = 30.f;
    const EVoLumSection displaySection = mExpandedSection;
    const float expandedW = (displaySection == EVoLumSection::AMP) ? 400.f : 460.f;
    const float gap = 10.f;
    const float cx = mRECT.MW();

    // Calculate layout
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

    // Draw the sections
    _DrawStrip(g, preRect, "PRE", displaySection == EVoLumSection::PRE, mPreActive, false);
    
    // AMP: we draw the strip, or we draw the hero frame
    if (displaySection == EVoLumSection::AMP) {
      // Just the frame, the VoLumHeroImageControl will draw on top (it's attached)
      // Actually we must resize the HeroImageControl!
    } else {
      _DrawAmpStrip(g, ampRect);
    }

    _DrawStrip(g, postRect, "POST", displaySection == EVoLumSection::POST, mPostActive, true);
  }
  
  void SetState(bool preActive, bool postActive, int ampIdx, const char* ampName)
  {
    (void) preActive;
    mPreActive = preActive;
    mPostActive = postActive;
    mAmpIdx = ampIdx;
    mAmpName = ampName;
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
  void _DrawStrip(IGraphics& g, const IRECT& r, const char* label, bool expanded, bool active, bool isPost)
  {
    if (expanded)
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
    else
    {
      bool dormant = !active;
      if (dormant)
      {
        g.FillRect(VoLumColors::HERO_BG, r);
        g.DrawRect(IColor(153, 43, 47, 55), r);
        // Diagonal crosshatch (clipped to rect)
        IColor hatch(25, 100, 180, 200);
        for (float d = -r.H(); d < r.W(); d += 8.f) {
          float x1 = r.L + d, y1 = r.T, x2 = r.L + d + r.H(), y2 = r.B;
          if (x1 < r.L) { y1 += (r.L - x1); x1 = r.L; }
          if (x2 > r.R) { y2 -= (x2 - r.R); x2 = r.R; }
          if (y1 < r.B && y2 > r.T) g.DrawLine(hatch, x1, y1, x2, y2, nullptr, 0.7f);
          x1 = r.R - d; y1 = r.T; x2 = r.R - d - r.H(); y2 = r.B;
          if (x1 > r.R) { y1 += (x1 - r.R); x1 = r.R; }
          if (x2 < r.L) { y2 -= (r.L - x2); x2 = r.L; }
          if (y1 < r.B && y2 > r.T) g.DrawLine(hatch, x1, y1, x2, y2, nullptr, 0.7f);
        }
        const float cs = 6.f;
        DrawCornerAccent(g, r.L + 3.f, r.T + 3.f, cs, false, false, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.R - 3.f, r.T + 3.f, cs, true, false, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.L + 3.f, r.B - 3.f, cs, false, true, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
        DrawCornerAccent(g, r.R - 3.f, r.B - 3.f, cs, true, true, VoLumColors::TEAL_DIM.WithOpacity(0.45f));
      }
      else
      {
        g.FillRect(VoLumColors::HERO_BG, r);
        g.DrawRect(VoLumColors::FRAME, r);
      }

      if (active)
        g.FillCircle(VoLumColors::TEAL, r.MW(), r.T + 12.f, 3.5f);

      IText t(11.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      float charH = 12.f;
      float totalH = strlen(label) * charH;
      float ty = r.MH() - totalH / 2.0f;
      for (size_t i = 0; i < strlen(label); i++) {
        char ch[2] = { label[i], 0 };
        g.DrawText(t, ch, IRECT(r.L, ty + i * charH, r.R, ty + (i + 1) * charH));
      }
    }
  }

  void _DrawAmpStrip(IGraphics& g, const IRECT& r)
  {
    g.FillRect(VoLumColors::HERO_BG, r);
    g.DrawRect(VoLumColors::FRAME, r);

    IColor grid(28, 100, 180, 200);
    float step = 12.f;
    for (float y = r.T + step; y < r.B - 1.f; y += step)
      g.DrawLine(grid, r.L + 4.f, y, r.R - 4.f, y, nullptr, 0.6f);
    for (float x = r.L + step / 2.f; x < r.R - 1.f; x += step)
      g.DrawLine(grid, x, r.T + 4.f, x, r.B - 4.f, nullptr, 0.6f);
    IColor node(35, 120, 210, 220);
    for (float y = r.T + step; y < r.B - 1.f; y += step * 2.f)
      for (float x = r.L + step / 2.f; x < r.R - 1.f; x += step * 2.f)
        if (x > r.L + 2.f && x < r.R - 2.f && y > r.T + 2.f && y < r.B - 2.f)
          g.FillCircle(node, x, y, 1.5f);

    IText t(11.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const char* name = mAmpName.empty() ? "AMP" : mAmpName.c_str();
    float charH = 12.f;
    int len = (int)strlen(name);
    int maxChars = (int)(r.H() / charH) - 2;
    if (len > maxChars) len = maxChars;
    float totalH = len * charH;
    float ty = r.MH() - totalH / 2.f;
    for (int i = 0; i < len; i++) {
      char ch[2] = { name[i], 0 };
      g.DrawText(t, ch, IRECT(r.L, ty + i * charH, r.R, ty + (i + 1) * charH));
    }
  }


  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mPreRect.Contains(x, y) && mExpandedSection != EVoLumSection::PRE)
    {
      mExpandedSection = EVoLumSection::PRE;
      if (mCallback) mCallback(mExpandedSection, EVoLumEffectFocus::COMP);
      SetDirty(false);
    }
    else if (mAmpRect.Contains(x, y) && mExpandedSection != EVoLumSection::AMP)
    {
      mExpandedSection = EVoLumSection::AMP;
      if (mCallback) mCallback(mExpandedSection, EVoLumEffectFocus::AMP);
      SetDirty(false);
    }
    else if (mPostRect.Contains(x, y) && mExpandedSection != EVoLumSection::POST)
    {
      mExpandedSection = EVoLumSection::POST;
      if (mCallback) mCallback(mExpandedSection, EVoLumEffectFocus::DELAY);
      SetDirty(false);
    }
  }
  
  StateCallback mCallback;
  bool mPreActive = false;
  bool mPostActive = false;
  int mAmpIdx = 0;
  std::string mAmpName;
  EVoLumSection mExpandedSection = EVoLumSection::AMP;

  IRECT mPreRect;
  IRECT mAmpRect;
  IRECT mPostRect;
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
    float cy = r.MH();
    IColor bright(dimmed ? 70 : 150, 120, 210, 220);
    IColor mid(dimmed ? 40 : 80, 100, 180, 200);
    IColor dim(dimmed ? 20 : 45, 80, 150, 170);

    if (mEffect == EVoLumEffectFocus::COMP)
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
    else if (mEffect == EVoLumEffectFocus::PRE_NAM1)
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
    else if (mEffect == EVoLumEffectFocus::PRE_NAM2)
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
    else if (mEffect == EVoLumEffectFocus::DELAY)
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
      int count = 12000;
      for (int i = 0; i < count; i++) {
        rng = rng * 1103515245 + 12345;
        int parentIdx = rng % pts.size();
        float angle = (float)(rng % 360);
        float rad = angle * 3.14159f / 180.f;
        float len = 2.f + (float)(rng % 4);
        Pt next = {pts[parentIdx].x + len * cosf(rad), pts[parentIdx].y + len * sinf(rad)};
        if (next.x >= r.L && next.x <= r.R && next.y >= r.T && next.y <= r.B) {
          IColor col = (i < 300) ? bright : ((i < 1500) ? mid : dim);
          float tk = (i < 200) ? 2.f : ((i < 800) ? 1.5f : 1.f);
          g.DrawLine(col, pts[parentIdx].x, pts[parentIdx].y, next.x, next.y, nullptr, tk);
          pts.push_back(next);
        }
      }
    }
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
