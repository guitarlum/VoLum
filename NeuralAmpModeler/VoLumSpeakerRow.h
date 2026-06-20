#pragma once

// VoLum cab/speaker row picker (VoLumSpeakerRowControl).
//
// Renders the four speaker-cab buttons (1x12 / 2x12 / 4x10 / 4x12) under the
// hero panel and forwards selection changes to the plugin. Extracted from
// VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

class VoLumSpeakerRowControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int speakerIdx)>;
  using IrMenuCallback = std::function<void(const IRECT& anchor)>;

  VoLumSpeakerRowControl(const IRECT& bounds, ChangeCallback cb = nullptr)
  : IControl(bounds)
  , mCallback(cb)
  {
    mSelected = 3;
  }

  // The plugin sets this to open the anchored Custom IR dropdown (F7).
  void SetIrMenuCallback(IrMenuCallback cb) { mIrMenuCb = std::move(cb); }

  // Reflects whether a custom IR is the active cab (sourced from the DIRECT
  // capture). When active, the baked-cab buttons read as unselected.
  bool IsIrCabActive() const { return mIrCabActive; }
  const std::string& IrName() const { return mIrName; }

  void SetIrCab(bool active, const char* name)
  {
    mIrCabActive = active;
    mIrName = name ? name : "";
    SetDirty(false);
  }

  // Override the three cab-button labels (slots 1-3). Used when a custom amp is
  // active so the row shows that amp's named cabs instead of G12/G65/V30. An
  // empty name marks the slot as having no capture (rendered disabled).
  void SetCabNames(const std::string& a, const std::string& b, const std::string& c)
  {
    mCabNames[0] = a;
    mCabNames[1] = b;
    mCabNames[2] = c;
    SetDirty(false);
  }

  void SetFactoryCabs()
  {
    mCabNames[0] = "G12";
    mCabNames[1] = "G65";
    mCabNames[2] = "V30";
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const std::string labels[] = {"No Cab", mCabNames[0], mCabNames[1], mCabNames[2]};
    const float noCabW = 64.f;
    const float btnW = 52.f;
    const float irBtnW = 104.f;
    const float btnH = 26.f;
    const float gap = 6.f;
    const float divGap = 12.f;

    // Single row, no section captions: [No Cab] | [cab1] [cab2] [cab3] | [~ Custom IR]
    const float cabsW = 3.f * btnW + 2.f * gap;
    const float totalW = noCabW + divGap + cabsW + divGap + irBtnW;
    float x = mRECT.MW() - totalW / 2.f;
    const float btnY = mRECT.MH() - btnH / 2.f;
    // IV/Josefin bold caps sit visually high in short rects - nudge text area down for optical centering
    const float btnTextNudgeY = 3.f;

    // "No Cab" button (index 0) -- teal/cyan accent when active (was "AMP")
    {
      IRECT btn(x, btnY, x + noCabW, btnY + btnH);
      bool isOn = (0 == mSelected) && !mIrCabActive;
      bool isHovered = (0 == mHovered);
      if (isOn)
        g.FillRoundRect(VoLumColors::SEL_GLOW, btn.GetPadded(2.5f), 5.f);
      g.FillRoundRect(ButtonFill(isOn, isHovered, true), btn, 3.f);
      g.DrawRoundRect(ButtonBorder(isOn, isHovered, true), btn, 3.f, nullptr, isHovered ? 1.35f : 1.f);
      IText btnText(13.f, ButtonText(isOn, isHovered), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnText, labels[0].c_str(), IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      mBtnRects[0] = btn;
      x += noCabW;
    }

    // Divider
    float divX = x + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX, btnY + 4.f, divX, btnY + btnH - 4.f);
    x += divGap;

    // Cab buttons (indices 1-3) — factory G12/G65/V30, or a custom amp's named
    // cabs. An empty label means the slot has no capture and is disabled.
    for (int i = 1; i < 4; i++)
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      const bool empty = labels[i].empty();
      bool isOn = (i == mSelected) && !mIrCabActive;
      bool isHovered = (i == mHovered) && !empty;
      if (empty)
      {
        g.FillRoundRect(VoLumColors::BTN_OFF_BG, btn, 3.f);
        g.DrawRoundRect(IColor(50, 200, 162, 70), btn, 3.f, nullptr, 1.f);
        g.DrawText(IText(13.f, IColor(70, 235, 220, 200), "Josefin-Bold", EAlign::Center, EVAlign::Middle), "--",
                   IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      }
      else
      {
        if (isOn)
          g.FillRoundRect(VoLumColors::SEL_GLOW, btn.GetPadded(2.5f), 5.f);
        g.FillRoundRect(isOn ? ButtonFill(true, isHovered, false) : ButtonFill(false, isHovered, false), btn, 3.f);
        g.DrawRoundRect(isOn ? ButtonBorder(true, isHovered, false) : ButtonBorder(false, isHovered, false),
                        btn, 3.f, nullptr, isHovered ? 1.35f : 1.f);
        IColor cabTextCol = ButtonText(isOn, isHovered);
        IText btnTextCab(14.f, cabTextCol, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
        g.DrawText(btnTextCab, labels[i].c_str(), IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      }
      mBtnRects[i] = btn;
      x += btnW + gap;
    }

    // Divider before the Custom IR cab dropdown
    float divX2 = (x - gap) + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX2, btnY + 4.f, divX2, btnY + btnH - 4.f);
    x += divGap - gap;

    // Custom IR cab dropdown button (F7) with an impulse-response glyph. When
    // active it is the selected cab and shows the IR name.
    {
      IRECT btn(x, btnY, x + irBtnW, btnY + btnH);
      const bool isHovered = (4 == mHovered);
      // Custom IR gets its own copper accent when active, distinct from the teal
      // "No Cab" and the gold stock cabs.
      const IColor fill = mIrCabActive ? VoLumColors::BTN_IR_ON_BG : ButtonFill(false, isHovered, false);
      const IColor border = mIrCabActive ? VoLumColors::BTN_IR_ON_BORDER
                                         : ButtonBorder(false, isHovered, false);
      if (mIrCabActive)
        g.FillRoundRect(IColor(70, 196, 122, 80), btn.GetPadded(2.5f), 5.f);
      g.FillRoundRect(fill, btn, 3.f);
      g.DrawRoundRect(border, btn, 3.f, nullptr, isHovered ? 1.35f : 1.f);
      const IColor txt = mIrCabActive ? VoLumColors::BTN_IR_ON_TEXT : ButtonText(false, isHovered);
      DrawIrGlyph(g, IRECT(btn.L + 9.f, btn.T + 5.f, btn.L + 22.f, btn.B - 5.f), txt);
      std::string label = mIrCabActive && !mIrName.empty() ? TruncatedIr() : "Custom IR";
      g.DrawText(IText(mIrCabActive ? 11.f : 12.f, txt, "Josefin-Bold", EAlign::Center, EVAlign::Middle), label.c_str(),
                 IRECT(btn.L + 22.f, btn.T + btnTextNudgeY, btn.R - 4.f, btn.B));
      mIrBtnRect = btn;
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    int next = HitTestButton(x, y);
    if (next < 0 && mIrBtnRect.Contains(x, y))
      next = 4;
    if (next != mHovered)
    {
      mHovered = next;
      const char* tip = "";
      if (next == 0)
        tip = "No cab - raw amp, no speaker";
      else if (next >= 1 && next <= 3)
        tip = mCabNames[next - 1].empty() ? "" : "Cabinet";
      else if (next == 4)
        tip = "Custom IR - use your own impulse response";
      SetTooltip(tip);
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

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    ClearVoLumKnobSelection(this);

    if (mIrBtnRect.Contains(x, y))
    {
      if (mIrMenuCb)
        mIrMenuCb(mIrBtnRect);
      return;
    }

    for (int i = 0; i < 4; i++)
    {
      // Cab slots 1-3 with an empty label have no capture and are not selectable.
      const bool emptySlot = (i >= 1) && mCabNames[i - 1].empty();
      if (mBtnRects[i].Contains(x, y) && !emptySlot && (i != mSelected || mIrCabActive))
      {
        // Choosing a baked cab clears any active custom IR cab.
        mIrCabActive = false;
        mSelected = i;
        if (mCallback)
          mCallback(i);
        SetDirty(false);
        return;
      }
    }
  }

  int GetSelected() const { return mSelected; }

  void SetSelected(int idx)
  {
    mSelected = idx;
    SetDirty(false);
  }

  // Kept for ABI parity with prior dual-amp UX iteration; the cab row no longer changes color
  // for the support lane (lane is conveyed by knob-label LED dots instead).
  void SetSupportAccent(bool /*support*/) {}

private:
  // Tiny impulse-response icon: a spike that decays to the right over a baseline.
  static void DrawIrGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float base = r.B;
    const float x0 = r.L;
    const float w = r.W();
    g.DrawLine(IColor(col.A / 2, col.R, col.G, col.B), r.L, base, r.R, base, nullptr, 1.f);
    const int n = 5;
    for (int i = 0; i < n; i++)
    {
      const float bx = x0 + (w * i) / (float) n;
      const float h = r.H() * std::pow(0.55f, (float) i);
      g.DrawLine(col, bx, base, bx, base - h, nullptr, i == 0 ? 1.7f : 1.f);
    }
  }

  std::string TruncatedIr() const
  {
    if (mIrName.size() <= 12)
      return mIrName;
    return mIrName.substr(0, 11) + "\u2026";
  }

  int HitTestButton(float x, float y) const
  {
    for (int i = 0; i < 4; ++i)
      if (mBtnRects[i].Contains(x, y))
        return i;
    return -1;
  }

  // Codified selection language: one brass treatment marks the active cab in the
  // row (No Cab + the three cabs alike). Custom IR keeps its copper BYO identity
  // (handled inline). The ampButton flag is retained for call-site parity only.
  static IColor ButtonFill(bool active, bool hovered, bool /*ampButton*/)
  {
    if (active)
      return VoLumColors::SEL_BG;
    if (hovered)
      return VoLumColors::SEL_BG_SOFT;
    return VoLumColors::BTN_OFF_BG;
  }

  static IColor ButtonBorder(bool active, bool hovered, bool /*ampButton*/)
  {
    if (active)
      return VoLumColors::SEL_BORDER;
    if (hovered)
      return VoLumColors::GOLD_DIM;
    return VoLumColors::BTN_OFF_BORDER;
  }

  static IColor ButtonText(bool active, bool hovered)
  {
    if (active)
      return VoLumColors::SEL_TEXT;
    if (hovered)
      return VoLumColors::TEXT_BRIGHT;
    return VoLumColors::BTN_OFF_TEXT;
  }

  int mSelected = 3;
  int mHovered = -1;
  // Labels for cab slots 1-3 (index 0 is always "No Cab"). Factory default; a
  // custom amp overrides these via SetCabNames.
  std::string mCabNames[3] = {"G12", "G65", "V30"};
  IRECT mBtnRects[4];
  IRECT mIrBtnRect;
  bool mIrCabActive = false;
  std::string mIrName;
  ChangeCallback mCallback;
  IrMenuCallback mIrMenuCb;
};