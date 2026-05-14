#pragma once

// VoLum inline exact-value entry + numeric param display.
//
// - VoLumExactEntryControl: small text-entry overlay anchored next to the
//   focused knob; lets the user type an exact value instead of dragging.
// - VoLumParamValueControl: read-only formatted-value label that mirrors the
//   knob position (replaces the IPlug2 default tooltip readout).
//
// Extracted from VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <functional>

class VoLumExactEntryControl : public IControl
{
public:
  VoLumExactEntryControl(const IRECT& bounds, int paramIdx, const char* label = "")
  : IControl(bounds, paramIdx)
  , mLabel(label ? label : "")
  {
    mIgnoreMouse = false;
    mText = IText(22.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle, 0.f,
                  IColor(245, 14, 16, 22), VoLumColors::TEXT_BRIGHT);
    SetTextEntryLength(12);
  }

  void Draw(IGraphics& g) override
  {
    if (mHide)
      return;

    g.FillRect(IColor(182, 8, 10, 14), mRECT);

    const IRECT panel = GetPanelRect();
    const IRECT frame = panel.GetPadded(10.f);
    const IRECT entry = GetEntryRect();
    const IRECT titleRect(panel.L + 24.f, panel.T + 18.f, panel.R - 24.f, panel.T + 38.f);
    const IRECT rangeRect(panel.L + 24.f, titleRect.B + 8.f, panel.R - 24.f, titleRect.B + 26.f);
    const IRECT hintRect(panel.L + 24.f, entry.B + 10.f, panel.R - 24.f, entry.B + 26.f);

    g.FillRoundRect(IColor(255, 22, 22, 30), panel, 10.f);
    g.DrawRoundRect(VoLumColors::FRAME, panel, 10.f, nullptr, 1.2f);
    g.DrawRoundRect(IColor(90, 200, 180, 100), frame, 8.f, nullptr, 1.f);

    DrawCornerAccent(g, panel.L + 11.f, panel.T + 11.f, 16.f, false, false);
    DrawCornerAccent(g, panel.R - 11.f, panel.T + 11.f, 16.f, true, false);
    DrawCornerAccent(g, panel.L + 11.f, panel.B - 11.f, 16.f, false, true);
    DrawCornerAccent(g, panel.R - 11.f, panel.B - 11.f, 16.f, true, true);

    g.DrawText(IText(14.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mLabel.empty() ? "EXACT VALUE" : mLabel.c_str(), titleRect);
    g.DrawText(IText(11.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               mRangeText.c_str(), rangeRect);

    g.FillRoundRect(IColor(235, 14, 16, 22), entry, 6.f);
    g.DrawRoundRect(IColor(72, 200, 162, 78), entry, 6.f, nullptr, 1.f);
    g.DrawRoundRect(IColor(36, 200, 162, 78), entry.GetPadded(2.f), 5.f, nullptr, 1.f);

    g.DrawText(IText(10.5f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
               "Type a number, press Enter to apply, Esc to cancel", hintRect);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mHide)
      return;

    auto* ui = GetUI();
    if (!ui)
      return;

    if (!GetPanelRect().Contains(x, y))
    {
      Hide(true);
      if (auto* textEntry = ui->GetTextEntryControl())
        textEntry->DismissEdit();
      return;
    }

    if (!mEditing && GetEntryRect().Contains(x, y))
      StartEntry();
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    mEditing = false;
    Hide(true);
    IControl::OnTextEntryCompletion(str, valIdx);
  }

  void SetValueFromUserInput(double value, int valIdx) override
  {
    mEditing = false;
    Hide(true);
    IControl::SetValueFromUserInput(value, valIdx);
  }

  void SetLabel(const char* label)
  {
    mLabel = label ? label : "";
    SetDirty(false);
  }

  void ShowForParam(int paramIdx, const char* label = nullptr)
  {
    SetParamIdx(paramIdx);
    SetLabel(label);
    BuildRangeText();
    Hide(false);
    mEditing = false;
    SetDirty(false);
  }

  void StartEntry()
  {
    if (mHide)
      return;

    auto* ui = GetUI();
    if (!ui)
      return;

    WDL_String currentText;
    if (const auto* pParam = GetParam())
      pParam->GetDisplay(currentText, false);

    mEditing = true;
    BuildRangeText();
    ui->CreateTextEntry(*this, mText, GetEntryRect(), currentText.Get());
    SetDirty(false);
  }

  bool IsEditing() const { return mEditing; }

  void CancelEntry()
  {
    if (auto* ui = GetUI())
    {
      if (ui->GetControlInTextEntry() == this)
        if (auto* textEntry = ui->GetTextEntryControl())
          textEntry->DismissEdit();
    }

    mEditing = false;
    Hide(true);
    SetDirty(false);
  }

  void SyncTextEntryState()
  {
    auto* ui = GetUI();
    if (!ui)
      return;

    if (mEditing && ui->GetControlInTextEntry() != this)
    {
      mEditing = false;
      Hide(true);
      SetDirty(false);
    }
  }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    if (hide)
      mEditing = false;
  }

private:
  IRECT GetPanelRect() const
  {
    return mRECT.GetCentredInside(340.f, 156.f);
  }

  IRECT GetEntryRect() const
  {
    const IRECT panel = GetPanelRect();
    return IRECT(panel.L + 28.f, panel.T + 64.f, panel.R - 28.f, panel.T + 106.f);
  }

  void BuildRangeText()
  {
    mRangeText.clear();
    if (const auto* pParam = GetParam())
    {
      WDL_String minText;
      WDL_String maxText;
      pParam->GetDisplay(pParam->GetMin(), false, minText, false);
      pParam->GetDisplay(pParam->GetMax(), false, maxText, false);
      mRangeText = "Range ";
      mRangeText += minText.Get();
      mRangeText += " to ";
      mRangeText += maxText.Get();
      if (const char* label = pParam->GetLabel(); label && label[0])
      {
        mRangeText += " ";
        mRangeText += label;
      }
    }
  }

  std::string mLabel;
  std::string mRangeText;
  bool mEditing = false;
};

class VoLumParamValueControl : public IControl
{
public:
  VoLumParamValueControl(const IRECT& bounds, int paramIdx, const char* suffix = "")
  : IControl(bounds, paramIdx)
  , mSuffix(suffix)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    WDL_String str;
    const auto* pParam = GetParam();
    if (pParam)
    {
      if (std::strcmp(mSuffix, "%") == 0)
      {
        const double percent = pParam->Value() * 100.0;
        if (std::fabs(percent - std::round(percent)) < 0.005)
          str.SetFormatted(16, "%.0f%%", percent);
        else
          str.SetFormatted(16, "%.1f%%", percent);
      }
      else
      {
        pParam->GetDisplay(str);
        if (mSuffix[0])
        {
          str.Append(" ");
          str.Append(mSuffix);
        }
      }
    }

    IText text(13.5f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, str.Get(), mRECT);
  }

  void SetValueFromDelegate(double value, int valIdx) override
  {
    IControl::SetValueFromDelegate(value, valIdx);
    SetDirty(false);
  }

  // Lane belonging is conveyed by the knob pointer dot turning teal, not by tinting the value
  // text. Kept as a no-op for ABI parity with prior dual-amp UX iterations.
  void SetSupportAccent(bool /*support*/) {}

private:
  const char* mSuffix;
};