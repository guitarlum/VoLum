#pragma once

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"
#include "VoLumTunerDSP.h"
#include "VoLumMetronomeDSP.h"
#include "VoLumHero.h"
#include "VoLumTunerMetronomeOverlay.h"
#include "VoLumAmpList.h"
#include "VoLumSpeakerRow.h"
#include "VoLumKeyboardNav.h"
#include "VoLumExactEntry.h"
#include "VoLumSettingsOverlay.h"
#include "VoLumSettingsTabs.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <functional>

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
    // Main canvas: gentle top-lit vertical gradient + soft edge vignette so the
    // large dark field reads as a lit panel instead of a flat fill.
    FillVGradient(g, mRECT, IColor(255, 21, 21, 29), IColor(255, 12, 12, 18));
    DrawVignette(g, mRECT, 72);

    // Sidebar (recedes via a slightly darker vertical gradient).
    IRECT sidebar(mRECT.L, mRECT.T, mRECT.L + mSidebarWidth, mRECT.B);
    FillVGradient(g, sidebar, VoLumColors::SIDEBAR_BG, VoLumColors::SIDEBAR_BG2);
    g.DrawLine(VoLumColors::SIDEBAR_BORDER, sidebar.R, sidebar.T, sidebar.R, sidebar.B);

    // Outer gold frame
    g.DrawRect(VoLumColors::FRAME, mRECT);

    // L-shaped corner accents
    const float cs = 22.f;
    const float m = 6.f;
    DrawCornerAccent(g, mRECT.L + m, mRECT.T + m, cs, false, false);
    DrawCornerAccent(g, mRECT.R - m, mRECT.T + m, cs, true, false);
    DrawCornerAccent(g, mRECT.L + m, mRECT.B - m, cs, false, true);
    DrawCornerAccent(g, mRECT.R - m, mRECT.B - m, cs, true, true);
  }

private:
  float mSidebarWidth;
};

class VoLumKnobSelectionClearControl : public IControl
{
public:
  using ClearCallback = std::function<void()>;

  VoLumKnobSelectionClearControl(const IRECT& bounds, ClearCallback callback)
  : IControl(bounds)
  , mCallback(callback)
  {
  }

  void Draw(IGraphics& g) override {}

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mCallback)
      mCallback();
  }

private:
  ClearCallback mCallback;
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
    IText logoText(30.f, VoLumColors::GOLD, "Poiret-One", EAlign::Center, EVAlign::Middle);
    IRECT logoArea = mRECT.GetFromTop(34.f).GetVShifted(2.f);
    g.DrawText(logoText, "VoLum", logoArea);

    IText subText(10.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    IRECT subArea = mRECT.GetFromTop(32.f).GetVShifted(22.f);
    g.DrawText(subText, "NAM PLAYER", subArea);

    // Stepped Art Deco decoration
    float cy = subArea.B + 8.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(38, 200, 162, 78), cx - 30.f, cy, cx + 30.f, cy);
    g.DrawLine(IColor(38, 200, 162, 78), cx - 18.f, cy + 3.f, cx + 18.f, cy + 3.f);
    DrawDiamond(g, cx, cy + 9.f, 3.f, VoLumColors::GOLD_DIM);
  }
};



// VoLumHeroImageControl + VoLumSupportPolarityControl live in VoLumHero.h.

class VoLumModePickerControl : public IControl
{
public:
  VoLumModePickerControl(const IRECT& bounds, int paramIdx, const std::vector<std::string>& modes)
  : IControl(bounds, paramIdx)
  , mModes(modes)
  {}

  void Draw(IGraphics& g) override
  {
    int selected = static_cast<int>(GetValue() * (mModes.size() - 1));
    float itemH = mRECT.H() / static_cast<float>(mModes.size());

    // Left border
    g.DrawLine(VoLumColors::FRAME, mRECT.L, mRECT.T, mRECT.L, mRECT.B, nullptr, 1.f);

    for (size_t i = 0; i < mModes.size(); ++i)
    {
      IRECT itemArea = IRECT(mRECT.L + 12.f, mRECT.T + i * itemH, mRECT.R, mRECT.T + (i + 1) * itemH);
      bool isSelected = (i == selected);

      // Selection chrome via the shared helper (square amber fill / soft amber
      // hover wash). See VoLumColorHelpers.h DrawVoLumSelection.
      DrawVoLumSelection(g, itemArea, isSelected, static_cast<int>(i) == mHovered,
                         VoLumSelectionStyle::AmberPicker, /*roundness=*/0.f, /*inset=*/1.f);

      IColor textCol = SelectionInkColor(VoLumSelectionStyle::AmberPicker, isSelected);
      IText text(11.f, textCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      IRECT textArea = itemArea;
      textArea.L += 6.f; // manually indent instead of GetTranslated which shifts the whole rect
      g.DrawText(text, mModes[i].c_str(), textArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    float itemH = mRECT.H() / static_cast<float>(mModes.size());
    int clickedIdx = static_cast<int>((y - mRECT.T) / itemH);
    if (clickedIdx >= 0 && clickedIdx < static_cast<int>(mModes.size()))
    {
      SetValue(static_cast<double>(clickedIdx) / (mModes.size() - 1));
      SetDirty(true);
    }
  }

  void OnMouseOver(float /*x*/, float y, const IMouseMod& /*mod*/) override
  {
    const float itemH = mRECT.H() / static_cast<float>(mModes.size());
    const int idx = std::clamp(static_cast<int>((y - mRECT.T) / itemH), 0, static_cast<int>(mModes.size()) - 1);
    if (idx != mHovered)
    {
      mHovered = idx;
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

private:
  std::vector<std::string> mModes;
  int mHovered = -1;
};

class VoLumSubRowTextControl : public IControl
{
public:
  VoLumSubRowTextControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (mName.empty()) return;
    const IRECT nameArea = IRECT(mRECT.L + 18.f, mRECT.T + 8.f, mRECT.R - 18.f, mRECT.T + 36.f);

    g.DrawText(IText(21.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle), mName.c_str(), nameArea);

    // Gold divider with diamond below the name
    float cy = nameArea.B + 8.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(51, 200, 162, 78), cx - 50.f, cy, cx - 6.f, cy);
    g.DrawLine(IColor(51, 200, 162, 78), cx + 6.f, cy, cx + 50.f, cy);
    DrawDiamond(g, cx, cy, 3.f, VoLumColors::GOLD_DIM);
  }

  void SetName(const char* name, bool isAmp)
  {
    mName = name;
    mIsAmp = isAmp;
    SetDirty(false);
  }

private:
  std::string mName;
  bool mIsAmp = true;
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
    const IRECT nameArea = IRECT(mRECT.L + 18.f, mRECT.T + 8.f, mRECT.R - 18.f, mRECT.T + 36.f);
    g.DrawText(IText(21.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle), mName.c_str(), nameArea);

    // Gold divider with diamond below the name
    float cy = nameArea.B + 8.f;
    float cx = mRECT.MW();
    g.DrawLine(IColor(51, 200, 162, 78), cx - 50.f, cy, cx - 6.f, cy);
    g.DrawLine(IColor(51, 200, 162, 78), cx + 6.f, cy, cx + 50.f, cy);
    DrawDiamond(g, cx, cy, 3.f, VoLumColors::GOLD_DIM);
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
    DrawInsetWell(g, mRECT, 2.f);

    float fillH = mRECT.H() * mLevel;
    if (fillH > 1.f)
    {
      IRECT fill(mRECT.L + 1.f, mRECT.B - fillH, mRECT.R - 1.f, mRECT.B - 1.f);
      FillVGradient(g, fill, IColor(255, 96, 200, 120), VoLumColors::METER_GREEN);
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
    const float size = mIsChannel ? 12.f : 13.f;
    const IText text(size, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(text, mLabel.c_str(), mRECT);
  }

  // Allow swapping the displayed label at runtime for controls that reuse a label slot.
  void SetLabel(const char* label)
  {
    if (label && mLabel != label)
    {
      mLabel = label;
      SetDirty(false);
    }
  }

  // Lane belonging is conveyed by the knob pointer-dot colour and value-text colour.
  // Labels stay neutral so the row reads cleanly. Methods kept as no-ops for ABI parity.
  enum class LaneAccent : int { None = 0, Main = 1, Support = 2 };
  void SetLaneAccent(LaneAccent /*accent*/) {}
  void SetSupportAccent(bool /*support*/) {}

private:
  std::string mLabel;
  bool mIsChannel;
};

// Compact horizontal 3-way pill switch bound to a parameter index.
// Effect staging currently uses this for Oktaverb sub-modes
// (Dark / Shimmer / Bloom), but the control remains generic.
// Labels can be swapped at runtime via SetLabels so a single pill can repurpose
// per-mode without re-attaching controls.
class VoLumSubModePillControl : public IControl
{
public:
  VoLumSubModePillControl(const IRECT& bounds, int paramIdx, const std::vector<std::string>& labels)
  : IControl(bounds, paramIdx)
  , mLabels(labels)
  {
  }

  void SetLabels(const std::vector<std::string>& labels)
  {
    if (mLabels == labels)
      return;
    mLabels = labels;
    SetDirty(false);
  }

  // Optionally map each pill slot to a specific PARAM integer value, for a pill
  // that exposes only a SUBSET of an enum param (e.g. show INSTANT+POLY out of a
  // {Drop,Instant,Poly} enum whose order is serialization-frozen). `denom` is the
  // param's (enum count - 1) used to normalize. When unset, the pill maps slot i
  // to normalized i/(n-1) as before.
  void SetValueMap(const std::vector<int>& values, int denom)
  {
    mValues = values;
    mValueDenom = denom;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const int n = static_cast<int>(mLabels.size());
    if (n <= 0)
      return;
    const int selected = SelectedSlot(n);
    const float itemW = mRECT.W() / static_cast<float>(n);

    g.FillRoundRect(IColor(160, 14, 16, 22), mRECT, 4.f);
    g.DrawRoundRect(VoLumColors::FRAME, mRECT, 4.f, nullptr, 1.f);

    // Font size scales with pill height. Cap is ~2pt smaller than the prior 11.5 so the
    // slim 28-tall row reads as a single horizontal lockup without the labels feeling
    // chunky next to neighbouring 34 px AMP-row toggles. Lower bound preserves legibility
    // if a host sizes the pill smaller still.
    const float fontSize = std::clamp(mRECT.H() * 0.34f, 7.5f, 9.5f);

    for (int i = 0; i < n; ++i)
    {
      const float l = mRECT.L + i * itemW;
      const float r = (i == n - 1) ? mRECT.R : (mRECT.L + (i + 1) * itemW);
      const IRECT itemArea(l, mRECT.T, r, mRECT.B);
      const bool isSelected = (i == selected);

      // Selection chrome via the shared helper (rounded amber fill / soft amber
      // hover wash). See VoLumColorHelpers.h DrawVoLumSelection.
      DrawVoLumSelection(g, itemArea, isSelected, i == mHovered, VoLumSelectionStyle::AmberPicker,
                         /*roundness=*/3.f, /*inset=*/1.5f);

      if (i > 0)
        g.DrawLine(IColor(96, 200, 162, 78), itemArea.L, mRECT.T + 3.f, itemArea.L, mRECT.B - 3.f, nullptr, 1.f);

      const IColor textCol = SelectionInkColor(VoLumSelectionStyle::AmberPicker, isSelected);
      const IText text(fontSize, textCol, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
      const IRECT textArea = itemArea.GetPadded(-3.f, 0.f, -3.f, 0.f);
      g.DrawText(text, mLabels[i].c_str(), textArea);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& /*mod*/) override
  {
    const int n = static_cast<int>(mLabels.size());
    if (n <= 0)
      return;
    const float itemW = mRECT.W() / static_cast<float>(n);
    const int idx = std::clamp(static_cast<int>((x - mRECT.L) / itemW), 0, n - 1);
    if (!mValues.empty() && mValueDenom > 0)
      SetValue(static_cast<double>(mValues[static_cast<size_t>(idx)]) / static_cast<double>(mValueDenom));
    else
      SetValue(static_cast<double>(idx) / static_cast<double>(n - 1));
    SetDirty(true);
  }

  void OnMouseOver(float x, float /*y*/, const IMouseMod& /*mod*/) override
  {
    const int n = static_cast<int>(mLabels.size());
    if (n <= 0)
      return;
    const float itemW = mRECT.W() / static_cast<float>(n);
    const int idx = std::clamp(static_cast<int>((x - mRECT.L) / itemW), 0, n - 1);
    if (idx != mHovered)
    {
      mHovered = idx;
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

private:
  // Which pill slot is currently selected. With a value-map, pick the slot whose
  // mapped param value is nearest the live param int (so a legacy value outside
  // the exposed subset - e.g. a retired Drop=0 - shows the nearest kept option).
  int SelectedSlot(int n) const
  {
    if (!mValues.empty() && mValueDenom > 0)
    {
      const int cur = static_cast<int>(GetValue() * mValueDenom + 0.5);
      int best = 0, bestDist = std::numeric_limits<int>::max();
      for (int i = 0; i < static_cast<int>(mValues.size()) && i < n; ++i)
      {
        const int d = std::abs(mValues[static_cast<size_t>(i)] - cur);
        if (d < bestDist)
        {
          bestDist = d;
          best = i;
        }
      }
      return best;
    }
    return std::clamp(static_cast<int>(GetValue() * (n - 1) + 0.5), 0, n - 1);
  }

  std::vector<std::string> mLabels;
  std::vector<int> mValues;
  int mValueDenom = 0;
  int mHovered = -1;
};

// Vertical text label (draws each character stacked)
class VoLumVerticalLabelControl : public IControl
{
public:
  VoLumVerticalLabelControl(const IRECT& bounds, const char* label)
  : IControl(bounds)
  , mLabel(label)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    IText text(11.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    int len = (int)mLabel.size();
    float charH = 12.f;
    float totalH = len * charH;
    float startY = mRECT.MH() - totalH / 2.f;
    for (int i = 0; i < len; i++)
    {
      char ch[2] = {mLabel[i], 0};
      IRECT charArea(mRECT.L, startY + i * charH, mRECT.R, startY + (i + 1) * charH);
      g.DrawText(text, ch, charArea);
    }
  }

private:
  std::string mLabel;
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

/** Thin horizontal hairline (full-width segment in layout). */
class VoLumHorizontalRuleControl : public IControl
{
public:
  explicit VoLumHorizontalRuleControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(72, 200, 162, 78), mRECT);
  }
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
    // Inset from the window's L-corners so 12.5 px Josefin has a real middle
    // in the 24 px band instead of sitting on the hairline.
    const IRECT ink = mRECT.GetPadded(-18.f, 0.f, -18.f, 0.f);
    const IText text(12.5f, mAlert ? VoLumColors::AMBER : VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center,
                     EVAlign::Middle);
    const std::string fitted = FitTextToWidth(g, text, mText.c_str(), ink.W());
    g.DrawText(text, fitted.c_str(), ink);
  }

  void SetText(const char* text) { SetStatus(text, false); }

  void SetStatus(const char* text, bool alert)
  {
    mText = text ? text : "";
    mAlert = alert;
    SetDirty(false);
  }

private:
  std::string mText = "(no rig loaded)";
  bool mAlert = false;
};

// Art Deco channel stepper: gold-themed [<] Ch 1 [>]



// VoLumTunerControl + VoLumMetronomeButtonControl + VoLumMetronomeControl
// live in VoLumTunerMetronomeOverlay.h.