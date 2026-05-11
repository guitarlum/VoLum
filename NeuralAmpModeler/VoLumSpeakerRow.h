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

class VoLumSpeakerRowControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int speakerIdx)>;

  VoLumSpeakerRowControl(const IRECT& bounds, ChangeCallback cb = nullptr)
  : IControl(bounds)
  , mCallback(cb)
  {
    mSelected = 3;
  }

  void Draw(IGraphics& g) override
  {
    const char* labels[] = {"AMP", "G12", "G65", "V30"};
    const float btnW = 52.f;
    const float btnH = 26.f;
    const float gap = 6.f;
    const float divGap = 12.f;
    const float labelGap = 6.f;

    // Single line: DIRECT [AMP] | CABINET [G12] [G65] [V30]
    IText sectionText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const float directLblW = 50.f;
    const float cabLblW = 66.f;

    float totalW = directLblW + labelGap + btnW + divGap + cabLblW + labelGap + 3 * btnW + 2 * gap;
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
      bool isOn = (0 == mSelected);
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
    const IColor cabOnBg = VoLumColors::BTN_CAB_ON_BG;
    const IColor cabOnBorder = VoLumColors::BTN_CAB_ON_BORDER;
    for (int i = 1; i < 4; i++)
    {
      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (i == mSelected);
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
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    const int next = HitTestButton(x, y);
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
    ClearVoLumKnobSelection(this);

    for (int i = 0; i < 4; i++)
    {
      if (mBtnRects[i].Contains(x, y) && i != mSelected)
      {
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
  ChangeCallback mCallback;
};