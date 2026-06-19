#pragma once

// VoLum cab/speaker row picker (VoLumSpeakerRowControl).
//
// Renders the four speaker-cab buttons (1x12 / 2x12 / 4x10 / 4x12) under the
// hero panel and forwards selection changes to the plugin. Extracted from
// VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

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
  void SetIrCab(bool active, const char* name)
  {
    mIrCabActive = active;
    mIrName = name ? name : "";
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const char* labels[] = {"AMP", "G12", "G65", "V30"};
    const float btnW = 52.f;
    const float irBtnW = 78.f;
    const float btnH = 26.f;
    const float gap = 6.f;
    const float divGap = 12.f;
    const float labelGap = 6.f;

    // Single line: DIRECT [AMP] | CABINET [G12] [G65] [V30] | [IR v]
    IText sectionText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float directLblW = 50.f;
    const float cabLblW = 66.f;

    float totalW = directLblW + labelGap + btnW + divGap + cabLblW + labelGap + 3 * btnW + 2 * gap + divGap + irBtnW;
    float x = mRECT.MW() - totalW / 2.f;
    float btnY = mRECT.MH() - btnH / 2.f;
    // IV/Josefin bold caps sit visually high in short rects â€” nudge text area down for optical centering
    const float btnTextNudgeY = 3.f;

    // "DIRECT" label
    IRECT directLabel(x, btnY + btnTextNudgeY, x + directLblW, btnY + btnH);
    g.DrawText(sectionText, "DIRECT", directLabel);
    x += directLblW + labelGap;

    // AMP button (index 0) -- teal/cyan accent when active
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (0 == mSelected) && !mIrCabActive;
      bool isHovered = (0 == mHovered);
      g.FillRoundRect(ButtonFill(isOn, isHovered, true), btn, 3.f);
      g.DrawRoundRect(ButtonBorder(isOn, isHovered, true), btn, 3.f, nullptr, isHovered ? 1.35f : 1.f);
      IText btnText(14.f, ButtonText(isOn, isHovered), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnText, labels[0], IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      mBtnRects[0] = btn;
      x += btnW;
    }

    // Divider
    float divX = x + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX, btnY + 4.f, divX, btnY + btnH - 4.f);
    x += divGap;

    // "CABINET" label
    IRECT cabLabel(x, btnY + btnTextNudgeY, x + cabLblW, btnY + btnH);
    g.DrawText(sectionText, "CABINET", cabLabel);
    x += cabLblW + labelGap;

    // G12, G65, V30 buttons (indices 1-3) — always gold; lane belonging is communicated by the
    // LED dot on amp-row knob labels, not by tinting the cab buttons.
    for (int i = 1; i < 4; i++)
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (i == mSelected) && !mIrCabActive;
      bool isHovered = (i == mHovered);
      g.FillRoundRect(isOn ? ButtonFill(true, isHovered, false) : ButtonFill(false, isHovered, false), btn, 3.f);
      g.DrawRoundRect(isOn ? ButtonBorder(true, isHovered, false) : ButtonBorder(false, isHovered, false),
                      btn, 3.f, nullptr, isHovered ? 1.35f : 1.f);
      IColor cabTextCol = ButtonText(isOn, isHovered);
      IText btnTextCab(14.f, cabTextCol, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnTextCab, labels[i], IRECT(btn.L, btn.T + btnTextNudgeY, btn.R, btn.B));
      mBtnRects[i] = btn;
      x += btnW + gap;
    }

    // Divider before the custom IR cab dropdown
    float divX2 = (x - gap) + divGap / 2.f;
    g.DrawLine(IColor(60, 200, 162, 78), divX2, btnY + 4.f, divX2, btnY + btnH - 4.f);
    x += divGap - gap;

    // Custom IR cab dropdown button (F7). When active it is the selected cab and
    // shows the IR name; otherwise a quiet "IR v" affordance.
    {
      IRECT btn(x, btnY, x + irBtnW, btnY + btnH);
      const bool isHovered = (4 == mHovered);
      g.FillRoundRect(mIrCabActive ? ButtonFill(true, isHovered, false) : ButtonFill(false, isHovered, false), btn, 3.f);
      g.DrawRoundRect(mIrCabActive ? ButtonBorder(true, isHovered, false) : ButtonBorder(false, isHovered, false), btn,
                      3.f, nullptr, isHovered ? 1.35f : 1.f);
      std::string label = mIrCabActive && !mIrName.empty() ? TruncatedIr() : "IR";
      const IColor txt = ButtonText(mIrCabActive, isHovered);
      g.DrawText(IText(mIrCabActive ? 11.f : 14.f, txt, "Josefin-Bold", EAlign::Center, EVAlign::Middle), label.c_str(),
                 IRECT(btn.L, btn.T + btnTextNudgeY, btn.R - 12.f, btn.B));
      g.DrawText(IText(10.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "v",
                 IRECT(btn.R - 14.f, btn.T, btn.R - 2.f, btn.B));
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
      if (mBtnRects[i].Contains(x, y) && (i != mSelected || mIrCabActive))
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
  std::string TruncatedIr() const
  {
    if (mIrName.size() <= 9)
      return mIrName;
    return mIrName.substr(0, 8) + "\u2026";
  }

  int HitTestButton(float x, float y) const
  {
    for (int i = 0; i < 4; ++i)
      if (mBtnRects[i].Contains(x, y))
        return i;
    return -1;
  }

  static IColor ButtonFill(bool active, bool hovered, bool ampButton)
  {
    if (active)
      return ampButton ? VoLumColors::BTN_AMP_ON_BG : VoLumColors::BTN_CAB_ON_BG;
    if (hovered)
      return IColor(58, 33, 46, 50);
    return VoLumColors::BTN_OFF_BG;
  }

  static IColor ButtonBorder(bool active, bool hovered, bool ampButton)
  {
    if (hovered)
      return ampButton ? VoLumColors::TEAL : VoLumColors::GOLD_DIM;
    if (active)
      return ampButton ? VoLumColors::BTN_AMP_ON_BORDER : VoLumColors::BTN_CAB_ON_BORDER;
    return VoLumColors::BTN_OFF_BORDER;
  }

  static IColor ButtonText(bool active, bool hovered)
  {
    return (active || hovered) ? VoLumColors::BTN_AMP_ON_TEXT : VoLumColors::BTN_OFF_TEXT;
  }

  int mSelected = 3;
  int mHovered = -1;
  IRECT mBtnRects[4];
  IRECT mIrBtnRect;
  bool mIrCabActive = false;
  std::string mIrName;
  ChangeCallback mCallback;
  IrMenuCallback mIrMenuCb;
};