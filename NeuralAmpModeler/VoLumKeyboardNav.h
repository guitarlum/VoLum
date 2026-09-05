#pragma once

// VoLum keyboard-navigation controls.
//
// - VoLumChannelStepControl: dual-amp channel stepper used to cycle through
//   per-amp channel snapshots when the keyboard focus is on the support row.
// - VoLumKeyboardHintControl: one quiet line under BUILD's status row while a
//   target is focused. Empty draws nothing, so the band is just bottom pad.
//
// Extracted from VoLumCoreControls.h on the 1.0 hygiene split.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

class VoLumChannelStepControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int newChannelIdx)>;

  VoLumChannelStepControl(const IRECT& bounds, ChangeCallback cb)
  : IControl(bounds, kNoParameter)
  , mCallback(cb)
  {
  }

  void SetChannels(const std::vector<std::string>& labels, int selected)
  {
    mLabels = labels;
    mSelected = labels.empty() ? 0 : std::clamp(selected, 0, (int)labels.size() - 1);
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }
  int GetNumChannels() const { return (int)mLabels.size(); }

  // Keyboard Left/Right: advance the selection (wrapping) and fire the SAME
  // callback a mouse click would, so the keyboard path can never diverge from
  // the click path (e.g. forgetting to stage the new channel's .nam).
  void StepKeyboard(int delta)
  {
    const int n = (int)mLabels.size();
    if (n < 1)
      return;
    mSelected = (mSelected + delta + n) % n;
    if (mCallback)
      mCallback(mSelected);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const int n = (int)mLabels.size();
    const float arrowW = 18.f;

    // Minimal Art Deco: thin gold lines flanking the channel number
    float lineY = mRECT.MH();
    g.DrawLine(IColor(50, 200, 162, 78), mRECT.L, lineY, mRECT.L + arrowW - 4.f, lineY);
    g.DrawLine(IColor(50, 200, 162, 78), mRECT.R - arrowW + 4.f, lineY, mRECT.R, lineY);

    // Left arrow: <
    IRECT leftArea(mRECT.L, mRECT.T, mRECT.L + arrowW, mRECT.B);
    IColor leftCol = mMouseOverLeft ? VoLumColors::GOLD : VoLumColors::TEXT_BRIGHT;
    IText arrowText(17.f, leftCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowText, "<", leftArea);

    // Right arrow: >
    IRECT rightArea(mRECT.R - arrowW, mRECT.T, mRECT.R, mRECT.B);
    IColor rightCol = mMouseOverRight ? VoLumColors::GOLD : VoLumColors::TEXT_BRIGHT;
    IText arrowTextR(17.f, rightCol, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowTextR, ">", rightArea);

    // Center: channel number — always gold; lane belonging is conveyed by the LED dot on the
    // CHANNEL knob label above, not by tinting the number.
    IRECT center(mRECT.L + arrowW, mRECT.T, mRECT.R - arrowW, mRECT.B);
    const char* label = (n > 0 && mSelected >= 0 && mSelected < n) ? mLabels[mSelected].c_str() : "---";
    IText labelText(19.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, label, center);
  }

  // Kept for ABI parity; channel number colour no longer changes per lane.
  void SetSupportAccent(bool /*support*/) {}

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ClearVoLumKnobSelection(this);

    const int n = (int)mLabels.size();
    if (n < 1)
      return;

    const float hitW = 22.f;
    if (x < mRECT.L + hitW)
      mSelected = (mSelected - 1 + n) % n;
    else if (x > mRECT.R - hitW)
      mSelected = (mSelected + 1) % n;
    else
      return;

    if (mCallback)
      mCallback(mSelected);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    const float hitW = 22.f;
    bool left = (x < mRECT.L + hitW);
    bool right = (x > mRECT.R - hitW);
    if (left != mMouseOverLeft || right != mMouseOverRight)
    {
      mMouseOverLeft = left;
      mMouseOverRight = right;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mMouseOverLeft = false;
    mMouseOverRight = false;
    SetDirty(false);
  }

private:
  int mSelected = 0;
  bool mMouseOverLeft = false;
  bool mMouseOverRight = false;
  std::vector<std::string> mLabels;
  ChangeCallback mCallback;
};

class VoLumKeyboardHintControl : public IControl
{
public:
  explicit VoLumKeyboardHintControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mHintText.empty())
      return;

    const IRECT ink = mRECT.GetPadded(-18.f, 0.f, -18.f, 0.f);
    const IText text(10.f, VoLumColors::GOLD_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
    const std::string fitted = FitTextToWidth(g, text, mHintText.c_str(), ink.W());
    g.DrawText(text, fitted.c_str(), ink);
  }

  void SetHintText(const char* hintText)
  {
    mHintText = (hintText && hintText[0]) ? hintText : "";
    SetDirty(false);
  }

private:
  std::string mHintText;
};