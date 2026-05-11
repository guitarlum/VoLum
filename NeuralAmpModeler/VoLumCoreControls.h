#pragma once

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"
#include "VoLumTunerDSP.h"
#include "VoLumMetronomeDSP.h"
#include "VoLumHero.h"
#include "VoLumTunerMetronomeOverlay.h"

#include <algorithm>
#include <cstdlib>
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
    g.FillRect(VoLumColors::BG, mRECT);

    // Sidebar
    IRECT sidebar(mRECT.L, mRECT.T, mRECT.L + mSidebarWidth, mRECT.B);
    g.FillRect(VoLumColors::SIDEBAR_BG, sidebar);
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

class VoLumAmpListControl : public IControl
{
public:
  using SelectionCallback = std::function<void(int ampIdx)>;

  VoLumAmpListControl(const IRECT& bounds, int numAmps, const char** ampNames, const char** ampAbbrs,
                      SelectionCallback cb)
  : IControl(bounds)
  , mNumAmps(numAmps)
  , mCallback(cb)
  {
    for (int i = 0; i < numAmps; i++)
    {
      mAmpNames.push_back(ampNames[i]);
      mAmpAbbrs.push_back(ampAbbrs[i]);
    }
  }

  void Draw(IGraphics& g) override
  {
    const float itemH = ItemHeight();
    const float iconSize = 22.f;
    const float pad = 8.f;
    const float contentH = itemH * mNumAmps;
    const bool scrollable = contentH > mRECT.H() + 0.5f;
    const float scrollbarW = scrollable ? 5.f : 0.f;

    ClampScroll();

    if ((int)mIconLayers.size() != mNumAmps)
      mIconLayers.resize(mNumAmps);

    // First pass: ensure all visible icon layers are built. StartLayer/EndLayer
    // mutates the clip region, so do this before clipping the row drawing pass.
    for (int i = 0; i < mNumAmps; i++)
    {
      const float top = mRECT.T + i * itemH - mScrollOffset;
      const float bot = top + itemH;
      if (bot < mRECT.T - 1.f || top > mRECT.B + 1.f)
        continue;

      IRECT row(mRECT.L, top, mRECT.R - scrollbarW, bot);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);
      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f,
                     paddedRow.L + pad + iconSize, paddedRow.MH() + iconSize / 2.f);

      if (!g.CheckLayer(mIconLayers[i]))
      {
        g.StartLayer(this, iconArea);
        IColor thmBright(200, 120, 210, 220);
        IColor thmDim(100, 100, 180, 200);
        DrawMiniFractal(g, iconArea, i, thmBright, thmDim);
        mIconLayers[i] = g.EndLayer();
      }
    }

    g.PathClipRegion(mRECT);

    for (int i = 0; i < mNumAmps; i++)
    {
      const float top = mRECT.T + i * itemH - mScrollOffset;
      const float bot = top + itemH;
      if (bot < mRECT.T - 1.f || top > mRECT.B + 1.f)
        continue;

      IRECT row(mRECT.L, top, mRECT.R - scrollbarW, bot);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);

      if (i == mSelected)
      {
        g.FillRect(VoLumColors::ITEM_SEL_BG, paddedRow);
        g.DrawRect(VoLumColors::ITEM_SEL_BORDER, paddedRow);
      }
      else if (i == mHovered)
      {
        g.FillRect(VoLumColors::ITEM_HOVER, paddedRow);
        g.DrawRect(IColor(20, 200, 162, 78), paddedRow);
      }

      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f,
                     paddedRow.L + pad + iconSize, paddedRow.MH() + iconSize / 2.f);

      g.DrawRect(IColor(i == mSelected ? 80 : 58, 120, 195, 210), iconArea);
      g.DrawLayer(mIconLayers[i]);

      IRECT nameArea = paddedRow.GetReducedFromLeft(pad + iconSize + 8.f);
      IColor nameCol = (i == mSelected) ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
      IText nameText(13.f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
    }

    g.PathClipRegion();

    if (scrollable)
    {
      const float trackX = mRECT.R - scrollbarW + 1.f;
      IRECT track(trackX, mRECT.T + 2.f, mRECT.R - 1.f, mRECT.B - 2.f);
      g.FillRect(IColor(40, 200, 162, 78), track);

      const float maxScroll = contentH - mRECT.H();
      const float thumbH = std::max(18.f, track.H() * (mRECT.H() / contentH));
      const float t = (maxScroll > 0.f) ? (mScrollOffset / maxScroll) : 0.f;
      const float thumbY = track.T + (track.H() - thumbH) * t;
      IRECT thumb(track.L, thumbY, track.R, thumbY + thumbH);
      g.FillRect(VoLumColors::GOLD_DIM, thumb);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    ClearVoLumKnobSelection(this);

    int idx = HitTestItem(y);
    if (idx >= 0 && idx < mNumAmps)
    {
      mSelected = idx;
      if (mCallback)
        mCallback(idx);
      ScrollToReveal(idx);
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

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    const float contentH = ItemHeight() * mNumAmps;
    if (contentH <= mRECT.H())
      return;

    const float step = ItemHeight();
    mScrollOffset -= d * step;
    ClampScroll();

    int idx = HitTestItem(y);
    if (idx != mHovered)
      mHovered = idx;
    SetDirty(false);
  }

  void SetSelected(int idx)
  {
    mSelected = idx;
    ScrollToReveal(idx);
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }

private:
  static constexpr float kMinItemH = 30.f;

  float ItemHeight() const
  {
    if (mNumAmps <= 0)
      return mRECT.H();
    const float fitH = mRECT.H() / (float)mNumAmps;
    return std::max(kMinItemH, fitH);
  }

  void ClampScroll()
  {
    const float contentH = ItemHeight() * mNumAmps;
    const float maxScroll = std::max(0.f, contentH - mRECT.H());
    mScrollOffset = std::clamp(mScrollOffset, 0.f, maxScroll);
  }

  void ScrollToReveal(int idx)
  {
    if (idx < 0 || idx >= mNumAmps)
      return;
    const float itemH = ItemHeight();
    const float itemTop = idx * itemH;
    const float itemBot = itemTop + itemH;
    if (itemTop < mScrollOffset)
      mScrollOffset = itemTop;
    else if (itemBot > mScrollOffset + mRECT.H())
      mScrollOffset = itemBot - mRECT.H();
    ClampScroll();
  }

  int HitTestItem(float y)
  {
    if (mNumAmps <= 0)
      return -1;
    const float itemH = ItemHeight();
    const float relY = (y - mRECT.T) + mScrollOffset;
    int idx = (int)(relY / itemH);
    return (idx >= 0 && idx < mNumAmps) ? idx : -1;
  }

  void DrawMiniFractal(IGraphics& g, const IRECT& r, int ampIdx, const IColor& bright, const IColor& dim)
  {
    DrawSidebarMiniFractal(g, r, FractalCaseForAmp(ampIdx), bright, dim);
  }

  int mNumAmps = 0;
  int mSelected = 0;
  int mHovered = -1;
  float mScrollOffset = 0.f;
  std::vector<std::string> mAmpNames;
  std::vector<std::string> mAmpAbbrs;
  SelectionCallback mCallback;
  std::vector<ILayerPtr> mIconLayers;
};

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

      if (isSelected) {
        g.FillRect(VoLumColors::AMBER, itemArea.GetPadded(-1.f));
      }
      else if (static_cast<int>(i) == mHovered)
      {
        // Soft amber wash to telegraph the click target without competing with the
        // current selection's solid amber fill.
        g.FillRect(IColor(48, 226, 165, 78), itemArea.GetPadded(-1.f));
      }

      IColor textCol = isSelected ? IColor(255, 26, 18, 8) : VoLumColors::TEXT_BRIGHT;
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

    IColor col = VoLumColors::GOLD;
    g.DrawText(IText(21.f, col, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mName.c_str(), nameArea);

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
    g.DrawText(IText(21.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               mName.c_str(), nameArea);

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
    g.FillRect(IColor(255, 10, 10, 16), mRECT);
    g.DrawRect(IColor(20, 200, 162, 78), mRECT);

    float fillH = mRECT.H() * mLevel;
    if (fillH > 1.f)
    {
      IRECT fill(mRECT.L + 1.f, mRECT.B - fillH, mRECT.R - 1.f, mRECT.B - 1.f);
      g.FillRect(VoLumColors::METER_GREEN, fill);
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

  void Draw(IGraphics& g) override
  {
    const int n = static_cast<int>(mLabels.size());
    if (n <= 0)
      return;
    const int selected = std::clamp(static_cast<int>(GetValue() * (n - 1) + 0.5), 0, n - 1);
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

      if (isSelected)
        g.FillRoundRect(VoLumColors::AMBER, itemArea.GetPadded(-1.5f), 3.f);
      else if (i == mHovered)
        g.FillRoundRect(IColor(48, 226, 165, 78), itemArea.GetPadded(-1.5f), 3.f);

      if (i > 0)
        g.DrawLine(IColor(96, 200, 162, 78), itemArea.L, mRECT.T + 3.f, itemArea.L, mRECT.B - 3.f, nullptr, 1.f);

      const IColor textCol = isSelected ? IColor(255, 26, 18, 8) : VoLumColors::TEXT_BRIGHT;
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
  std::vector<std::string> mLabels;
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
    IText text(12.5f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle);
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

// Art Deco channel stepper: gold-themed [<] Ch 1 [>]
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

    g.FillRoundRect(IColor(168, 14, 16, 22), mRECT, 7.f);
    g.DrawRoundRect(IColor(72, 200, 162, 78), mRECT, 7.f, nullptr, 1.f);

    const IText titleText(12.5f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const IText detailText(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Center, EVAlign::Middle);

    const IRECT inner = mRECT.GetPadded(-18.f, -6.f, -18.f, -6.f);
    const IRECT top = inner.GetFromTop(17.f);
    const IRECT bottom = IRECT(inner.L, top.B + 2.f, inner.R, inner.B);
    g.DrawText(titleText, mHintTitle.c_str(), top);
    g.DrawText(detailText, mHintDetail.c_str(), bottom);
  }

  void SetHintText(const char* hintText)
  {
    mHintText = (hintText && hintText[0]) ? hintText : "";
    mHintTitle.clear();
    mHintDetail.clear();

    if (!mHintText.empty())
    {
      const std::string divider = "  |  ";
      const auto firstSplit = mHintText.find(divider);
      if (firstSplit == std::string::npos)
      {
        mHintTitle = mHintText;
      }
      else
      {
        mHintTitle = mHintText.substr(0, firstSplit);
        mHintDetail = mHintText.substr(firstSplit + divider.size());
      }
    }

    SetDirty(false);
  }

private:
  std::string mHintText;
  std::string mHintTitle;
  std::string mHintDetail;
};

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
      pParam->GetDisplay(str);
      if (mSuffix[0])
      {
        str.Append(" ");
        str.Append(mSuffix);
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

/** Full-window dim + explicit panel rect (must match layout math in NAMSettingsPageControl). */
class VoLumSettingsBackdropControl : public IControl
{
public:
  VoLumSettingsBackdropControl(const IRECT& fullBounds, const IRECT& panelRect)
  : IControl(fullBounds)
  , mPanel(panelRect)
  {
    // Must receive hits: if ignored, dim/panel â€œemptyâ€ pixels fall through to main UI and can steal
    // mouse up/down when the cursor moves quickly (settings appears to close at random).
    mIgnoreMouse = false;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    // Consume clicks on overlay shell (dim + filler); interactive children sit above in z-order.
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(185, 8, 10, 14), mRECT);
    const IRECT& p = mPanel;
    // Slightly lifted panel so it reads clearly over the dim layer
    g.FillRect(IColor(255, 22, 22, 30), p);
    g.DrawRect(VoLumColors::FRAME, p);
    g.DrawRect(IColor(90, 200, 180, 100), p.GetPadded(2.f));
    const float cs = 18.f;
    const float m = 8.f;
    DrawCornerAccent(g, p.L + m, p.T + m, cs, false, false);
    DrawCornerAccent(g, p.R - m, p.T + m, cs, true, false);
    DrawCornerAccent(g, p.L + m, p.B - m, cs, false, true);
    DrawCornerAccent(g, p.R - m, p.B - m, cs, true, true);
  }

private:
  IRECT mPanel;
};

/** Subtle frame behind grouped settings controls (ignores mouse so widgets on top still hit-test). */
class VoLumSettingsGroupFrameControl : public IControl
{
public:
  explicit VoLumSettingsGroupFrameControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    // Match ui-mockup/settings-overlay-mockup.html output card (group-fill + gold border + inner hairline).
    g.FillRect(IColor(235, 20, 20, 26), mRECT);
    g.DrawRect(IColor(89, 200, 162, 78), mRECT);
    g.DrawRect(IColor(31, 200, 162, 78), mRECT.GetPadded(3.f));
  }
};

/** Thin horizontal rule above settings footer (mouse passes through). */
class VoLumSettingsFooterSepControl : public IControl
{
public:
  explicit VoLumSettingsFooterSepControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const IColor c(31, 200, 162, 78);
    g.FillRect(c, mRECT);
  }
};

/** Thin vertical rule between settings columns (mouse passes through). */
class VoLumSettingsVertRuleControl : public IControl
{
public:
  explicit VoLumSettingsVertRuleControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float t = mRECT.H() * 0.12f;
    const IColor mid(90, 200, 162, 78);
    const IColor end(35, 200, 162, 78);
    g.DrawLine(end, cx, mRECT.T, cx, mRECT.T + t, nullptr, 1.f);
    g.DrawLine(mid, cx, mRECT.T + t, cx, mRECT.B - t, nullptr, 1.f);
    g.DrawLine(end, cx, mRECT.B - t, cx, mRECT.B, nullptr, 1.f);
  }
};

/** Gold â€œÃ—â€ close control (no grey SVG) for the settings overlay. */
class VoLumSettingsCloseControl : public IControl
{
public:
  VoLumSettingsCloseControl(const IRECT& bounds, IActionFunction actionFunc)
  : IControl(bounds, kNoParameter, nullptr)
  , mCloseAction(std::move(actionFunc))
  {
  }

  void Draw(IGraphics& g) override
  {
    const IColor c = mMouseIsOver ? VoLumColors::GOLD : VoLumColors::GOLD_DIM;
    const float inset = 8.f;
    const float t = mMouseIsOver ? 2.25f : 1.75f;
    const float x0 = mRECT.L + inset;
    const float y0 = mRECT.T + inset;
    const float x1 = mRECT.R - inset;
    const float y1 = mRECT.B - inset;
    g.DrawLine(c, x0, y0, x1, y1, nullptr, t);
    g.DrawLine(c, x1, y0, x0, y1, nullptr, t);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    if (mCloseAction)
      mCloseAction(this);
  }

private:
  IActionFunction mCloseAction;
};

// VoLumTunerControl + VoLumMetronomeButtonControl + VoLumMetronomeControl
// live in VoLumTunerMetronomeOverlay.h.
