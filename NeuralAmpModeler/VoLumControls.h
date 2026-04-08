#pragma once

#include "IControls.h"
#include "VoLumAmpeteCatalog.h"
#include <functional>
#include <string>
#include <vector>

using namespace iplug;
using namespace igraphics;

namespace VoLumColors
{
const IColor BG(255, 14, 14, 20);
const IColor SIDEBAR_BG(255, 16, 16, 24);
const IColor SIDEBAR_BORDER(255, 30, 30, 40);
const IColor ITEM_HOVER(40, 255, 255, 255);
const IColor ITEM_SEL_BG(20, 91, 141, 239);
const IColor ITEM_SEL_BORDER(50, 91, 141, 239);
const IColor TEXT_DIM(255, 130, 130, 145);
const IColor TEXT_MED(255, 175, 175, 190);
const IColor TEXT_BRIGHT(255, 225, 225, 235);
const IColor ACCENT_BLUE(255, 142, 197, 255);
const IColor ACCENT_AMBER(255, 212, 168, 74);
const IColor METER_GREEN(255, 42, 138, 42);
const IColor KNOB_BG(255, 21, 21, 24);
const IColor KNOB_BORDER(12, 255, 255, 255);
const IColor DIVIDER(30, 255, 255, 255);
const IColor SPK_BTN_OFF_BG(5, 255, 255, 255);
const IColor SPK_BTN_OFF_BORDER(12, 255, 255, 255);
const IColor SPK_BTN_OFF_TEXT(255, 140, 140, 155);
const IColor SPK_CAB_ON_BG(30, 91, 141, 239);
const IColor SPK_CAB_ON_BORDER(100, 91, 141, 239);
const IColor SPK_AMP_ON_BG(25, 200, 150, 60);
const IColor SPK_AMP_ON_BORDER(100, 200, 150, 60);
} // namespace VoLumColors

class VoLumBackgroundControl : public IControl
{
public:
  VoLumBackgroundControl(const IRECT& bounds, float sidebarWidth)
  : IControl(bounds)
  , mSidebarWidth(sidebarWidth)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(VoLumColors::BG, mRECT);
    IRECT sidebar(mRECT.L, mRECT.T, mRECT.L + mSidebarWidth, mRECT.B);
    g.FillRect(VoLumColors::SIDEBAR_BG, sidebar);
    g.DrawLine(VoLumColors::SIDEBAR_BORDER, sidebar.R, sidebar.T, sidebar.R, sidebar.B);
  }

private:
  float mSidebarWidth;
};

class VoLumLogoControl : public IControl
{
public:
  VoLumLogoControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText logoText(26.f, VoLumColors::ACCENT_BLUE, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    IRECT logoArea = mRECT.GetFromTop(30.f).GetVShifted(2.f);
    g.DrawText(logoText, "VoLum", logoArea);

    IText subText(10.f, VoLumColors::TEXT_MED, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    IRECT subArea = mRECT.GetFromBottom(14.f);
    g.DrawText(subText, "NAM PLAYER", subArea);
  }
};

class VoLumAmpListControl : public IControl
{
public:
  using SelectionCallback = std::function<void(int ampIdx)>;

  VoLumAmpListControl(const IRECT& bounds, int numAmps, const char** ampNames, SelectionCallback cb)
  : IControl(bounds)
  , mNumAmps(numAmps)
  , mCallback(cb)
  {
    for (int i = 0; i < numAmps; i++)
      mAmpNames.push_back(ampNames[i]);
  }

  void Draw(IGraphics& g) override
  {
    const float itemH = mRECT.H() / (float)mNumAmps;
    const float dotSize = 10.f;
    const float pad = 6.f;

    static const IColor dotColors[] = {
      IColor(255, 26, 42, 74), IColor(255, 58, 26, 26), IColor(255, 26, 42, 26), IColor(255, 42, 42, 26),
      IColor(255, 26, 26, 58), IColor(255, 42, 26, 42), IColor(255, 42, 42, 26), IColor(255, 42, 26, 26),
      IColor(255, 26, 42, 42), IColor(255, 58, 42, 26), IColor(255, 58, 58, 26), IColor(255, 26, 42, 42),
      IColor(255, 42, 26, 42), IColor(255, 26, 42, 42), IColor(255, 42, 42, 26), IColor(255, 26, 58, 42),
    };

    for (int i = 0; i < mNumAmps; i++)
    {
      IRECT row(mRECT.L, mRECT.T + i * itemH, mRECT.R, mRECT.T + (i + 1) * itemH);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);

      if (i == mSelected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, paddedRow, 6.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, paddedRow, 6.f);
      }
      else if (i == mHovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, paddedRow, 6.f);
      }

      IRECT dotArea = paddedRow.GetFromLeft(dotSize + 2 * pad).GetCentredInside(dotSize, dotSize);
      dotArea.Translate(pad, 0.f);
      int colorIdx = i % (sizeof(dotColors) / sizeof(dotColors[0]));
      g.FillRoundRect(dotColors[colorIdx], dotArea, 3.f);

      IRECT nameArea = paddedRow.GetReducedFromLeft(dotSize + 2 * pad + 8.f);
      IText nameText(13.f, (i == mSelected) ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Roboto-Regular",
                     EAlign::Near, EVAlign::Middle);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    int idx = HitTestItem(y);
    if (idx >= 0 && idx < mNumAmps)
    {
      mSelected = idx;
      if (mCallback)
        mCallback(idx);
      SetDirty(false);
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    int idx = HitTestItem(y);
    if (idx != mHovered)
    {
      mHovered = idx;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHovered = -1;
    SetDirty(false);
  }

  void SetSelected(int idx)
  {
    mSelected = idx;
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }

private:
  int HitTestItem(float y)
  {
    float itemH = mRECT.H() / (float)mNumAmps;
    int idx = (int)((y - mRECT.T) / itemH);
    return (idx >= 0 && idx < mNumAmps) ? idx : -1;
  }

  int mNumAmps = 0;
  int mSelected = 0;
  int mHovered = -1;
  std::vector<std::string> mAmpNames;
  SelectionCallback mCallback;
};

class VoLumSpeakerRowControl : public IControl
{
public:
  using ChangeCallback = std::function<void(int speakerIdx)>;

  VoLumSpeakerRowControl(const IRECT& bounds, ChangeCallback cb = nullptr)
  : IControl(bounds)
  , mCallback(cb)
  {
    mSelected = 3; // V30 default
  }

  void Draw(IGraphics& g) override
  {
    const char* labels[] = {"AMP", "G12", "G65", "V30"};
    const float btnW = 58.f;
    const float btnH = 26.f;
    const float gap = 6.f;
    const float divGap = 14.f;

    // Total: AMP + divGap + G12 + gap + G65 + gap + V30
    float totalW = btnW + divGap + 3 * btnW + 2 * gap;
    float startX = mRECT.MW() - totalW / 2.f;
    float btnY = mRECT.MH() - btnH / 2.f + 4.f;

    // "SPEAKER MODE" label centered above G65 (middle of cab group)
    float cabGroupL = startX + btnW + divGap;
    float cabGroupR = cabGroupL + 3 * btnW + 2 * gap;
    IText headerText(10.f, VoLumColors::TEXT_DIM, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    IRECT headerArea(cabGroupL, mRECT.T, cabGroupR, btnY - 2.f);
    g.DrawText(headerText, "SPEAKER MODE", headerArea);

    for (int i = 0; i < 4; i++)
    {
      float x;
      if (i == 0)
        x = startX;
      else
        x = startX + btnW + divGap + (i - 1) * (btnW + gap);

      // Vertical divider between AMP and cab group
      if (i == 1)
      {
        float divX = startX + btnW + divGap / 2.f;
        g.DrawLine(VoLumColors::DIVIDER, divX, btnY + 3.f, divX, btnY + btnH - 3.f);
      }

      IRECT btn(x, btnY, x + btnW, btnY + btnH);
      bool isOn = (i == mSelected);
      bool isAmp = (i == 0);

      if (isOn)
      {
        if (isAmp)
        {
          g.FillRoundRect(VoLumColors::SPK_AMP_ON_BG, btn, 6.f);
          g.DrawRoundRect(VoLumColors::SPK_AMP_ON_BORDER, btn, 6.f);
        }
        else
        {
          g.FillRoundRect(VoLumColors::SPK_CAB_ON_BG, btn, 6.f);
          g.DrawRoundRect(VoLumColors::SPK_CAB_ON_BORDER, btn, 6.f);
        }
      }
      else
      {
        g.FillRoundRect(VoLumColors::SPK_BTN_OFF_BG, btn, 6.f);
        g.DrawRoundRect(VoLumColors::SPK_BTN_OFF_BORDER, btn, 6.f);
      }

      IColor textCol = isOn ? (isAmp ? VoLumColors::ACCENT_AMBER : VoLumColors::ACCENT_BLUE)
                            : VoLumColors::SPK_BTN_OFF_TEXT;
      IText btnText(14.f, textCol, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
      g.DrawText(btnText, labels[i], btn);

      mBtnRects[i] = btn;
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
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

private:
  int mSelected = 3;
  IRECT mBtnRects[4];
  ChangeCallback mCallback;
};

class VoLumHeroImageControl : public IControl
{
public:
  VoLumHeroImageControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mHasBitmap)
    {
      // Compute centered rect that preserves the image's aspect ratio
      float imgW = (float)mBitmap.W();
      float imgH = (float)mBitmap.H() / (float)std::max(1, mBitmap.N());
      float scale = std::min(mRECT.W() / imgW, mRECT.H() / imgH);
      float drawW = imgW * scale;
      float drawH = imgH * scale;
      IRECT centered(mRECT.MW() - drawW / 2.f, mRECT.MH() - drawH / 2.f,
                      mRECT.MW() + drawW / 2.f, mRECT.MH() + drawH / 2.f);
      g.DrawFittedBitmap(mBitmap, centered);
    }
    else
    {
      g.FillRoundRect(IColor(5, 255, 255, 255), mRECT, 12.f);
      g.DrawRoundRect(IColor(10, 255, 255, 255), mRECT, 12.f);
      IText phText(48.f, IColor(20, 142, 197, 255), "Roboto-Regular", EAlign::Center, EVAlign::Middle);
      g.DrawText(phText, mPlaceholder.c_str(), mRECT);
    }
  }

  void SetPlaceholder(const char* text)
  {
    mPlaceholder = text;
    mHasBitmap = false;
    SetDirty(false);
  }

  void SetBitmap(const IBitmap& bitmap)
  {
    mBitmap = bitmap;
    mHasBitmap = true;
    SetDirty(false);
  }

private:
  bool mHasBitmap = false;
  IBitmap mBitmap;
  std::string mPlaceholder = "A1";
};

class VoLumAmpNameControl : public IControl
{
public:
  VoLumAmpNameControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText nameText(24.f, IColor(255, 230, 230, 235), "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(nameText, mName.c_str(), mRECT);
  }

  void SetName(const char* name)
  {
    mName = name;
    SetDirty(false);
  }

private:
  std::string mName = "Ampete One";
};

class VoLumMeterControl : public IControl
{
public:
  VoLumMeterControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(IColor(5, 255, 255, 255), mRECT, 3.f);
    g.DrawRoundRect(IColor(8, 255, 255, 255), mRECT, 3.f);

    float fillH = mRECT.H() * mLevel;
    if (fillH > 1.f)
    {
      IRECT fill(mRECT.L + 1.f, mRECT.B - fillH, mRECT.R - 1.f, mRECT.B - 1.f);
      g.FillRoundRect(VoLumColors::METER_GREEN, fill, 2.f);
    }
  }

  void SetLevel(float level)
  {
    mLevel = std::clamp(level, 0.f, 1.f);
    SetDirty(false);
  }

private:
  float mLevel = 0.35f;
};

class VoLumKnobLabelControl : public IControl
{
public:
  VoLumKnobLabelControl(const IRECT& bounds, const char* label, bool isChannel = false)
  : IControl(bounds)
  , mLabel(label)
  , mIsChannel(isChannel)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(14.f, VoLumColors::TEXT_MED, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mLabel.c_str(), mRECT);
  }

private:
  std::string mLabel;
  bool mIsChannel;
};

class VoLumDividerControl : public IControl
{
public:
  VoLumDividerControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.FillRect(VoLumColors::DIVIDER, mRECT); }
};

class VoLumFooterControl : public IControl
{
public:
  VoLumFooterControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(12.f, VoLumColors::TEXT_DIM, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mText.c_str(), mRECT);
  }

  void SetText(const char* text)
  {
    mText = text;
    SetDirty(false);
  }

private:
  std::string mText = "(no rig loaded)";
};

// Callback-based discrete channel stepper: [<] CH 1 [>]
// Arrows are always visible. Click left/right thirds to step.
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

  void Draw(IGraphics& g) override
  {
    const int n = (int)mLabels.size();

    g.FillRoundRect(IColor(10, 255, 255, 255), mRECT, mRECT.H() / 2.f);
    g.DrawRoundRect(IColor(20, 142, 197, 255), mRECT, mRECT.H() / 2.f);

    const float arrowW = mRECT.H();
    const float btnSize = arrowW - 2.f;

    // Left arrow button
    IRECT leftArea = mRECT.GetFromLeft(arrowW).GetCentredInside(btnSize, btnSize);
    g.FillEllipse(mMouseOverLeft ? IColor(30, 142, 197, 255) : IColor(15, 255, 255, 255), leftArea);
    IColor leftCol = mMouseOverLeft ? VoLumColors::ACCENT_BLUE : VoLumColors::TEXT_MED;
    IText arrowText(12.f, leftCol, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowText, "\xe2\x97\x80", leftArea);

    // Right arrow button
    IRECT rightArea = mRECT.GetFromRight(arrowW).GetCentredInside(btnSize, btnSize);
    g.FillEllipse(mMouseOverRight ? IColor(30, 142, 197, 255) : IColor(15, 255, 255, 255), rightArea);
    IColor rightCol = mMouseOverRight ? VoLumColors::ACCENT_BLUE : VoLumColors::TEXT_MED;
    IText arrowTextR(12.f, rightCol, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(arrowTextR, "\xe2\x96\xb6", rightArea);

    // Center label
    IRECT center = mRECT.GetReducedFromLeft(arrowW).GetReducedFromRight(arrowW);
    const char* label = (n > 0 && mSelected >= 0 && mSelected < n) ? mLabels[mSelected].c_str() : "---";
    IText labelText(14.f, VoLumColors::TEXT_BRIGHT, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(labelText, label, center);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const int n = (int)mLabels.size();
    if (n < 1)
      return;

    const float hitW = mRECT.H();
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
    const float hitW = mRECT.H();
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
      pParam->GetDisplay(str);
      if (mSuffix[0])
      {
        str.Append(" ");
        str.Append(mSuffix);
      }
    }

    IText text(12.f, VoLumColors::TEXT_DIM, "Roboto-Regular", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, str.Get(), mRECT);
  }

  void SetValueFromDelegate(double value, int valIdx) override
  {
    IControl::SetValueFromDelegate(value, valIdx);
    SetDirty(false);
  }

private:
  const char* mSuffix;
};
