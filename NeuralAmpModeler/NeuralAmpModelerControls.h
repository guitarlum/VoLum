#pragma once

#include <algorithm> // std::max, std::min
#include <cmath> // std::round
#include <sstream> // std::stringstream
#include <unordered_map> // std::unordered_map
#include "config.h"
#include "IControls.h"
// VoLum: custom controls and keyboard stepping (upstream-equivalent file fence)
#include "VoLumControls.h"
#include "VoLumKeyboardModel.h"
#include "VoLumLatencyReport.h"
#include "VoLumOutputMode.h"

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())
#define NAM_KNOB_HEIGHT 120.0f
#define NAM_SWTICH_HEIGHT 50.0f

using namespace iplug;
using namespace igraphics;

enum class NAMBrowserState
{
  Empty, // when no file loaded, show "Get" button
  Loaded // when file loaded, show "Clear" button
};

// Where the corner button on the plugin (settings, close settings) goes
// :param rect: Rect for the whole plugin's UI
IRECT CornerButtonArea(const IRECT& rect)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
};

class NAMSquareButtonControl : public ISVGButtonControl
{
public:
  NAMSquareButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

    ISVGButtonControl::Draw(g);
  }
};

class NAMCircleButtonControl : public ISVGButtonControl
{
public:
  NAMCircleButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

class VoLumUpdateBadgeControl : public IControl
{
public:
  explicit VoLumUpdateBadgeControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillEllipse(VoLumColors::GOLD, mRECT);
    g.DrawEllipse(VoLumColors::HERO_BG, mRECT, nullptr, 1.f);
  }
};

class NAMKnobControl : public IVKnobControl, public IBitmapBase
{
public:
  NAMKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.55;
    if (label)
      mKeyboardLabel = label;

    mText = IText(16.f, COLOR_WHITE, "Josefin-Bold", EAlign::Center, EVAlign::Middle, 0.f, IColor(235, 18, 20, 28),
                  IColor(255, 255, 248, 238));
    SetTextEntryLength(12);
  }

  void SetSelectedForKeyboard(bool selected)
  {
    if (mKeyboardSelected != selected)
    {
      mKeyboardSelected = selected;
      mWheelAccum.Reset();
      SetDirty(false);
    }
  }

  bool IsSelectedForKeyboard() const { return mKeyboardSelected; }
  const char* GetKeyboardLabel() const { return mKeyboardLabel.c_str(); }
  IRECT GetKeyboardEntryBounds() const { return mValueBounds.GetCentredInside(132.f, 36.f).GetVShifted(8.f); }

  void PromptExactValueEntry()
  {
    if (auto* pPlugin = PLUG())
      pPlugin->_PromptVoLumKnobExactEntry(GetParamIdx(), GetKeyboardLabel());
  }

  bool HandleKeyboardInput(const IKeyPress& key)
  {
    if (!mKeyboardSelected)
      return false;

    switch (key.VK)
    {
      case kVK_LEFT:
        if (auto* pPlugin = PLUG())
          return pPlugin->_SelectAdjacentVoLumKnob(GetParamIdx(), -1);
        return false;
      case kVK_RIGHT:
        if (auto* pPlugin = PLUG())
          return pPlugin->_SelectAdjacentVoLumKnob(GetParamIdx(), 1);
        return false;
      case kVK_DOWN: return Nudge(false, key.S);
      case kVK_UP: return Nudge(true, key.S);
      case kVK_RETURN: PromptExactValueEntry(); return true;
      case kVK_DELETE:
      case kVK_BACK: SetValueToDefault(); return true;
      case kVK_ESCAPE: SetSelectedForKeyboard(false); return true;
      default: return false;
    }
  }

  bool Nudge(bool increase, bool fine)
  {
    const IParam* pParam = GetParam();
    if (!pParam)
      return false;

    const double current = pParam->FromNormalized(GetValue());
    const double delta = (increase ? 1.0 : -1.0) * GetKeyboardStep(fine);
    const double next = std::clamp(current + delta, pParam->GetMin(), pParam->GetMax());
    const double normalized = pParam->ToNormalized(next);

    if (normalized == GetValue())
      return false;

    SetValueFromUserInput(normalized);
    return true;
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!IsDisabled())
    {
      SetSelectedForKeyboard(true);

      if (auto* pPlugin = PLUG())
        pPlugin->_SelectVoLumKnob(GetParamIdx());
    }

    IVKnobControl::OnMouseDown(x, y, mod);
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    (void)x;
    (void)y;
    return HandleKeyboardInput(key);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    (void)x;
    (void)y;

    if (IsDisabled())
      return;

    const int steps = mWheelAccum.OnDelta(d);
    if (steps == 0)
      return;

    const bool increase = steps > 0;
    const int count = increase ? steps : -steps;
    for (int i = 0; i < count; ++i)
      Nudge(increase, mod.S);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void DrawWidget(IGraphics& g) override
  {
    float widgetRadius = GetRadius() * 0.73;
    auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    DrawIndicatorTrack(g, angle, cx + 0.5, cy, widgetRadius);
    g.DrawFittedBitmap(mBitmap, knobRect);
    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f,
                                              {{GetColor(mMouseIsOver ? kX3 : kX1), 0.f},
                                               {GetColor(mMouseIsOver ? kX3 : kX1), 0.8f},
                                               {COLOR_TRANSPARENT, 1.0f}}),
               {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);

    if (mKeyboardSelected)
    {
      const auto selectionColor = GetColor(kX1).WithOpacity(0.8f);
      g.DrawCircle(selectionColor, cx, cy, widgetRadius + 5.f, nullptr, 1.5f);
    }
  }

private:
  double GetKeyboardStep(bool fine) const { return volum::keyboard::StepForParam(GetParamIdx(), fine); }

  bool mKeyboardSelected = false;
  volum::keyboard::WheelAccumulator mWheelAccum;
  std::string mKeyboardLabel;
};

class NAMSwitchControl : public IVSlideSwitchControl, public IBitmapBase
{
public:
  NAMSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVSlideSwitchControl(bounds, paramIdx, label,
                         style.WithRoundness(0.666f)
                           .WithShowValue(false)
                           .WithEmboss(true)
                           .WithShadowOffset(1.5f)
                           .WithDrawShadows(false)
                           .WithColor(kFR, COLOR_BLACK)
                           .WithFrameThickness(0.5f)
                           .WithWidgetFrac(0.5f)
                           .WithLabelOrientation(EOrientation::South))
  , IBitmapBase(bitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void DrawTrack(IGraphics& g, const IRECT& bounds) override
  {
    IRECT handleBounds = GetAdjustedHandleBounds(bounds);
    handleBounds = IRECT(handleBounds.L, handleBounds.T, handleBounds.R, handleBounds.T + mBitmap.H());
    IRECT centreBounds = handleBounds.GetPadded(-mStyle.shadowOffset);
    IRECT shadowBounds = handleBounds.GetTranslated(mStyle.shadowOffset, mStyle.shadowOffset);
    //    const float contrast = mDisabled ? -GRAYED_ALPHA : 0.f;
    float cR = 7.f;
    const float tlr = cR;
    const float trr = cR;
    const float blr = cR;
    const float brr = cR;

    // outer shadow
    if (mStyle.drawShadows)
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr, &mBlend);

    // Embossed style unpressed
    if (mStyle.emboss)
    {
      // Positive light
      g.FillRoundRect(GetColor(kPR), handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Negative light
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr /*, &blend*/);

      // Fill in foreground
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, centreBounds, tlr, trr, blr, brr, &mBlend);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), centreBounds, tlr, trr, blr, brr, &mBlend);
    }
    else
    {
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), handleBounds, tlr, trr, blr, brr, &mBlend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), handleBounds, tlr, trr, blr, brr, &mBlend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    IRECT r;
    if (GetSelectedIdx() == 0)
    {
      r = filledArea.GetFromLeft(mBitmap.W());
    }
    else
    {
      r = filledArea.GetFromRight(mBitmap.W());
    }

    g.DrawBitmap(mBitmap, r, 0, 0, nullptr);
  }
};

class VoLumPowerSwitchControl : public IControl
{
public:
  VoLumPowerSwitchControl(const IRECT& bounds, int paramIdx)
  : IControl(bounds, paramIdx)
  {
  }

  void Draw(IGraphics& g) override
  {
    const bool on = GetValue() > 0.5;
    const IColor track = on ? VoLumColors::GOLD.WithOpacity(0.35f) : VoLumColors::FRAME;
    const IColor thumb = on ? VoLumColors::GOLD : VoLumColors::TEXT_DIM;
    const IColor text = on ? VoLumColors::GOLD : VoLumColors::TEXT_DIM;

    g.DrawText(IText(18.f, text, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "⏻",
               IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + 18.f));

    const IRECT trackR = IRECT(mRECT.MW() - 6.f, mRECT.T + 22.f, mRECT.MW() + 6.f, mRECT.B - 2.f);
    g.FillRoundRect(IColor(255, 8, 10, 14), trackR, 6.f);
    g.DrawRoundRect(track, trackR, 6.f);

    const float cy = on ? trackR.T + 8.f : trackR.B - 8.f;
    g.FillEllipse(thumb, IRECT(mRECT.MW() - 8.f, cy - 8.f, mRECT.MW() + 8.f, cy + 8.f));
    if (mMouseIsOver)
      g.DrawEllipse(VoLumColors::TEXT_BRIGHT, IRECT(mRECT.MW() - 10.f, cy - 10.f, mRECT.MW() + 10.f, cy + 10.f));
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    SetValueFromUserInput(GetValue() > 0.5 ? 0.0 : 1.0);
  }
};

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  {
  }

  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }

  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert(maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str{filePath};

      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };

    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }
};

// URL control for the "Get" models/irs links
class NAMGetButtonControl : public NAMSquareButtonControl
{
public:
  NAMGetButtonControl(const IRECT& bounds, const char* label, const char* url, const ISVG& globeSVG)
  : NAMSquareButtonControl(
      bounds,
      [url](IControl* pCaller) {
        WDL_String fullURL(url);
        pCaller->GetUI()->OpenURL(fullURL.Get());
      },
      globeSVG)
  {
    SetTooltip(label);
  }
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style, const ISVG& loadSVG,
                        const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG, const IBitmap& bitmap,
                        const ISVG& globeSVG, const char* getButtonLabel, const char* getButtonURL)
  : IDirBrowseControlBase(bounds, fileExtension, false, false)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mBitmap(bitmap)
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mGlobeSVG(globeSVG)
  , mGetButtonLabel(getButtonLabel)
  , mGetButtonURL(getButtonURL)
  , mBrowserState(NAMBrowserState::Empty)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.DrawFittedBitmap(mBitmap, mRECT); }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedItemIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [&](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
    };

    auto chooseFileFunc = [&, loadFileFunc](IControl* pCaller) {
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();

        if (!mMainMenu.HasSubMenus())
        {
          mMainMenu.SetChosenItemIdx(mSelectedItemIndex);
        }
        pCaller->GetUI()->CreatePopupMenu(*this, mMainMenu, pCaller->GetRECT());
      }
    };

    IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
    const auto buttonWidth = padded.H();
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;

    AddChildControl(new NAMSquareButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG))
      ->SetAnimationEndActionFunction(loadFileFunc);
    AddChildControl(new NAMSquareButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG))
      ->SetAnimationEndActionFunction(prevFileFunc);
    AddChildControl(new NAMSquareButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG))
      ->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle))
      ->SetAnimationEndActionFunction(chooseFileFunc);

    // creates both right-side controls but only show one based on state
    mClearButton = new NAMSquareButtonControl(clearAndGetButtonBounds, DefaultClickActionFunc, mClearSVG);
    mClearButton->SetAnimationEndActionFunction(clearFileFunc);
    AddChildControl(mClearButton);

    mGetButton = new NAMGetButtonControl(clearAndGetButtonBounds, mGetButtonLabel, mGetButtonURL, mGlobeSVG);
    AddChildControl(mGetButton);

    // initialize control visibility
    SetBrowserState(NAMBrowserState::Empty);
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedItemIndex > -1 && mSelectedItemIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    switch (msgTag)
    {
      case kMsgTagLoadFailed:
        // Honestly, not sure why I made a big stink of it before. Why not just say it failed and move on? :)
        {
          std::string label(std::string("(FAILED) ") + std::string(mFileNameControl->GetLabelStr()));
          mFileNameControl->SetLabelAndTooltip(label.c_str());
          SetBrowserState(NAMBrowserState::Empty);
        }
        break;
      case kMsgTagLoadedModel:
      case kMsgTagLoadedIR:
      {
        WDL_String fileName, directory;
        fileName.Set(reinterpret_cast<const char*>(pData));
        directory.Set(reinterpret_cast<const char*>(pData));
        directory.remove_filepart(true);

        ClearPathList();
        AddPath(directory.Get(), "");
        SetupMenu();
        SetSelectedFile(fileName.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
        SetBrowserState(NAMBrowserState::Loaded);
      }
      break;
      default: break;
    }
  }

private:
  void SelectFirstFile() { mSelectedItemIndex = mFiles.GetSize() ? 0 : -1; }

  void GetSelectedFileDirectory(WDL_String& path)
  {
    GetSelectedFile(path);
    path.remove_filepart();
    return;
  }

  // set the state of the browser and the visibility of the "Get" vs. "Clear" buttons
  void SetBrowserState(NAMBrowserState newState)
  {
    mBrowserState = newState;

    switch (mBrowserState)
    {
      case NAMBrowserState::Empty:
        mClearButton->Hide(true);
        mGetButton->Hide(false);
        break;
      case NAMBrowserState::Loaded:
        mClearButton->Hide(false);
        mGetButton->Hide(true);
        break;
    }
  }

  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  IBitmap mBitmap;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG, mGlobeSVG;
  int mClearMsgTag;

  // new members for the "Get" button
  const char* mGetButtonLabel;
  const char* mGetButtonURL;
  NAMBrowserState mBrowserState;
  NAMSquareButtonControl* mClearButton = nullptr;
  NAMGetButtonControl* mGetButton = nullptr;
};

class NAMMeterControl : public IVPeakAvgMeterControl<>, public IBitmapBase
{
  static constexpr float KMeterMin = -70.0f;
  static constexpr float KMeterMax = -0.01f;

public:
  NAMMeterControl(const IRECT& bounds, const IBitmap& bitmap, const IVStyle& style)
  : IVPeakAvgMeterControl<>(bounds, "", style.WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.8),
                            EDirection::Vertical, {}, 0, KMeterMin, KMeterMax, {})
  , IBitmapBase(bitmap)
  {
    SetPeakSize(1.0f);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void SetSafetyActive(bool active)
  {
    if (mSafetyActive == active)
      return;
    mSafetyActive = active;
    SetDirty(false);
  }

  virtual void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    mWidgetBounds = mWidgetBounds.GetMidHPadded(5).GetVPadded(10);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override { g.DrawFittedBitmap(mBitmap, r); }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    if (r.H() > 2)
      g.FillRect(mSafetyActive ? GetColor(kX2) : GetColor(kX1), r, &mBlend);
  }

  void DrawPeak(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    g.DrawGrid(COLOR_BLACK, mTrackBounds.Get()[chIdx], 10, 2);
    g.FillRect(mSafetyActive ? GetColor(kX2) : GetColor(kX3), r, &mBlend);
  }

private:
  bool mSafetyActive = false;
};

// Container where we can refer to children by names instead of indices
class IContainerBaseWithNamedChildren : public IContainerBase
{
public:
  IContainerBaseWithNamedChildren(const IRECT& bounds)
  : IContainerBase(bounds) {};
  ~IContainerBaseWithNamedChildren() = default;

protected:
  IControl* AddNamedChildControl(IControl* control, std::string name, int ctrlTag = kNoTag, const char* group = "")
  {
    // Make sure we haven't already used this name
    assert(mChildNameIndexMap.find(name) == mChildNameIndexMap.end());
    mChildNameIndexMap[name] = NChildren();
    return AddChildControl(control, ctrlTag, group);
  };

  IControl* GetNamedChild(std::string name)
  {
    const int index = mChildNameIndexMap[name];
    return GetChild(index);
  };


private:
  std::unordered_map<std::string, int> mChildNameIndexMap;
}; // class IContainerBaseWithNamedChildren


struct PossiblyKnownParameter
{
  bool known = false;
  double value = 0.0;
};

struct ModelInfo
{
  PossiblyKnownParameter sampleRate;
  PossiblyKnownParameter inputCalibrationLevel;
  PossiblyKnownParameter outputCalibrationLevel;
};

class ModelInfoControl : public IContainerBaseWithNamedChildren
{
public:
  ModelInfoControl(const IRECT& bounds, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mStyle(style) {};

  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };

  void SetCurrentLatency(const volum::LatencyReport& report)
  {
#if defined(APP_API)
    constexpr bool kStandalone = true;
#else
    constexpr bool kStandalone = false;
#endif
    const volum::LatencyLines lines = volum::FormatLatencyLines(report, kStandalone);
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.currentLatency))->SetStr(lines.headline.c_str());
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.latencyDetail))->SetStr(lines.detail.c_str());
  }

  void Hide(bool hide) override
  {
    // Keep heading visible in settings footer; only the sample line populates when a model loads.
    IContainerBase::Hide(hide);
  };

  void OnAttached() override
  {
    IRECT r(GetRECT());
    const float footerCapH = 20.f;
    const IVStyle headingStyle = mStyle.WithDrawFrame(false).WithValueText(
      IText(15.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Top));
    AddChildControl(new IVLabelControl(r.ReduceFromTop(footerCapH), "Model information", headingStyle));
    const float rowH = 14.f;
    AddNamedChildControl(new IVLabelControl(r.ReduceFromTop(rowH), "", mStyle), mControlNames.sampleRate);
    AddNamedChildControl(new IVLabelControl(r.ReduceFromTop(rowH), "", mStyle), mControlNames.currentLatency);
    // The latency caveat needs more characters than the 15 px rows fit - the single
    // combined line used to run past this box and clip its own closing bracket.
    const IVStyle detailStyle = mStyle.WithDrawFrame(false).WithValueText(
      IText(12.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top));
    AddNamedChildControl(new IVLabelControl(r.ReduceFromTop(rowH), "", detailStyle), mControlNames.latencyDetail);
  };

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto SetControlStr = [&](const std::string& name, const PossiblyKnownParameter& p, const std::string& units,
                             const std::string& childName) {
      std::stringstream ss;
      ss << name << ": ";
      if (p.known)
      {
        ss << p.value << " " << units;
      }
      else
      {
        ss << "(Unknown)";
      }
      static_cast<IVLabelControl*>(GetNamedChild(childName))->SetStr(ss.str().c_str());
    };

    // "Model rate", not "Sample rate": this is the rate the capture was trained at,
    // which is 48 kHz for every rig VoLum ships and for almost every NAM capture in
    // circulation. Under the old label it sat directly above the round-trip line,
    // which is derived from the audio device - so a constant 48000 next to a
    // device-dependent figure read as a device rate that was ignoring the interface.
    SetControlStr("Model rate", modelInfo.sampleRate, "Hz", mControlNames.sampleRate);

    mHasInfo = true;
  };

private:
  const IVStyle mStyle;
  struct
  {
    const std::string sampleRate = "sampleRate";
    const std::string currentLatency = "currentLatency";
    const std::string latencyDetail = "latencyDetail";
  } mControlNames;
  // Do I have info?
  bool mHasInfo = false;
};

class OutputModeControl : public IVRadioButtonControl
{
public:
  OutputModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(bounds, paramIdx, {}, "", style, EVShape::Ellipse, EDirection::Vertical, buttonSize) {};

  void DrawWidget(IGraphics& g) override
  {
    const int hit = GetSelectedIdx();
    const float textGap = 9.f;
    float maxContentW = mButtonAreaWidth + textGap + 8.f;
    for (int i = 0; i < mNumStates; i++)
    {
      const IRECT rowR = mButtons.Get()[i];
      WDL_String* pLab = mTabLabels.Get(i);
      const char* s = (pLab && pLab->Get() && pLab->Get()[0]) ? pLab->Get() : "";
      IRECT textMeas(0.f, 0.f, 0.f, 0.f);
      if (s[0] != '\0')
        g.MeasureText(mStyle.valueText, s, textMeas);
      const float tw = std::max(8.f, textMeas.W());
      const float contentW = mButtonAreaWidth + textGap + tw;
      maxContentW = std::max(maxContentW, contentW);
    }
    for (int i = 0; i < mNumStates; i++)
    {
      const IRECT rowR = mButtons.Get()[i];
      WDL_String* pLab = mTabLabels.Get(i);
      const char* s = (pLab && pLab->Get() && pLab->Get()[0]) ? pLab->Get() : "";
      const float x0 = rowR.MW() - maxContentW * 0.5f;
      IRECT btnSlot(x0, rowR.T, x0 + mButtonAreaWidth, rowR.B);
      DrawButton(g, btnSlot.GetFromLeft(mButtonAreaWidth).GetCentredInside(mButtonSize), i == hit,
                 mMouseOverButton == i, IVTabSwitchControl::ETabSegment::Mid, IsDisabled() || GetStateDisabled(i));
      if (s[0] != '\0')
      {
        // A disabled mode (e.g. Calibrated when the model carries no output-level
        // calibration) reads as a dimmed "(n/a)" row instead of full-strength text.
        const bool stateDisabled = IsDisabled() || GetStateDisabled(i);
        IColor fg = (i == hit) ? GetColor(kON) : GetColor(kX1);
        if (stateDisabled)
          fg = VoLumColors::TEXT_DIM.WithOpacity(0.42f);
        IRECT textR(x0 + mButtonAreaWidth + textGap, rowR.T, x0 + maxContentW + 2.f, rowR.B);
        g.DrawText(mStyle.valueText.WithFGColor(fg), s, textR, &mBlend);
      }
    }
  }

  void SetNormalizedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Normalized";
    if (disable)
    {
      ss << " (n/a)";
    }
    mTabLabels.Get(1)->Set(ss.str().c_str());
  };
  void SetCalibratedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Calibrated";
    if (disable)
    {
      ss << " (n/a)";
    }
    mTabLabels.Get(2)->Set(ss.str().c_str());
  };
};

// VoLum: non-parameter A2 Lite-mode toggle for the Settings overlay. Reads and
// writes the machine-global Lite/Full choice on the plugin (persisted to
// volum-settings.json, NOT the plugin chunk). Off = Full (best quality,
// default); On = Lite (smaller A2 slice, lower CPU). No effect on rigs that are
// not slimmable containers.
class VoLumLiteModeSwitchControl : public IControl
{
public:
  VoLumLiteModeSwitchControl(const IRECT& bounds, const IText& labelText)
  : IControl(bounds)
  , mLabelText(labelText.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle))
  {
    SetTooltip(
      "Lite mode runs the smaller A2-Lite slice of A2 amp/pedal captures to lower CPU.\n"
      "Off = full quality (default). Applies to all NAM lanes; no effect on non-A2 rigs.");
  }

  bool IsLite() const
  {
    if (auto* plugin = static_cast<PLUG_CLASS_NAME*>(const_cast<VoLumLiteModeSwitchControl*>(this)->GetDelegate()))
      return plugin->_VolumIsLiteMode();
    return false;
  }

  // Geometry of the two-segment FULL | LITE switch within our rect (shared by
  // Draw and OnMouseDown so the hit-test always matches what is drawn).
  IRECT SegmentTrack() const
  {
    const float segW = std::min(176.f, mRECT.W());
    const float segH = 26.f;
    return mRECT.GetFromTop(segH).GetCentredInside(segW, segH);
  }

  void Draw(IGraphics& g) override
  {
    // Explicit two-state switch so the active quality mode is unambiguous:
    // FULL (best quality, default) on the left, LITE (lower CPU) on the right.
    const bool lite = IsLite();
    const IRECT seg = SegmentTrack();
    const float cr = seg.H() * 0.5f;
    const IRECT leftR = seg.GetFromLeft(seg.W() * 0.5f);
    const IRECT rightR = seg.GetFromRight(seg.W() * 0.5f);
    const IRECT activeR = lite ? rightR : leftR;

    // Recessed track, then a brass slug behind the active segment.
    g.FillRoundRect(IColor(255, 9, 9, 14), seg, cr);
    g.FillRoundRect(VoLumColors::GOLD.WithOpacity(0.30f), activeR, cr);
    g.DrawRoundRect(VoLumColors::GOLD, activeR, cr, &mBlend, 1.25f);
    g.DrawRoundRect(VoLumColors::FRAME, seg, cr, &mBlend, 1.f);

    const IText onText(12.f, VoLumColors::SEL_TEXT, "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    const IText offText(
      12.f, VoLumColors::TEXT_DIM.WithOpacity(0.55f), "Josefin-Bold", EAlign::Center, EVAlign::Middle);
    g.DrawText(lite ? offText : onText, "FULL", leftR);
    g.DrawText(lite ? onText : offText, "LITE", rightR);

    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, seg, cr);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)y;
    (void)mod;
    if (auto* plugin = static_cast<PLUG_CLASS_NAME*>(GetDelegate()))
    {
      const IRECT seg = SegmentTrack();
      const bool wantLite = x >= seg.MW();
      plugin->_VolumSetLiteMode(wantLite);
    }
    SetDirty(false);
  }

private:
  IText mLabelText;
};

class NAMSettingsPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMSettingsPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& inputLevelBackgroundBitmap,
                         const IBitmap& switchBitmap, ISVG closeSVG, const IVStyle& style,
                         const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mAnimationTime(0)
  , mBitmap(bitmap)
  , mInputLevelBackgroundBitmap(inputLevelBackgroundBitmap)
  , mSwitchBitmap(switchBitmap)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
  , mCloseSVG(closeSVG)
  {
    mIgnoreMouse = false;
  }

  void ClearModelInfo()
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->ClearModelInfo();
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      HideAnimated(true);
      return true;
    }

    return false;
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;

    if (hide == false)
    {
      mHide = false;
      // Children were hidden on close; unhide immediately so they paint during the fade-in
      ForAllChildrenFunc([](int childIdx, IControl* pChild) { pChild->Hide(false); });
    }
    else // hide subcontrols immediately
    {
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });
    }

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());

        if (mWillHide)
          SetBlend(IBlend(EBlend::Default, 1.0f - progress));
        else
          SetBlend(IBlend(EBlend::Default, progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
          return;
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const float pad = 22.0f;
    const IRECT rootB = GetRECT();
    const float panelW = rootB.W() * 0.84f;
    const float panelH = rootB.H() * 0.84f;
    const IRECT panel = rootB.GetCentredInside(static_cast<int>(panelW), static_cast<int>(panelH));
    // Poiret reads too light/small for an overlay title; use Josefin-Bold for a clear “panel” headline.
    const IVStyle titleStyle = mStyle.WithDrawFrame(false).WithShowValue(false).WithValueText(
      IText(44.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top));
    const auto text = IText(15.f, EAlign::Center, VoLumColors::TEXT_BRIGHT);
    const auto leftText = text.WithAlign(EAlign::Near).WithFGColor(VoLumColors::TEXT_BRIGHT);
    const auto style = mStyle.WithDrawFrame(false).WithValueText(text);
    const IVStyle leftStyle = style.WithValueText(leftText);

    AddNamedChildControl(new VoLumSettingsBackdropControl(GetRECT(), panel), mControlNames.bitmap);

    IRECT inner = panel.GetPadded(-pad);

    auto headerRow = inner.ReduceFromTop(58.0f);
    AddNamedChildControl(new IVLabelControl(headerRow, "SETTINGS", titleStyle), mControlNames.title);
    (void)inner.ReduceFromTop(2.0f); // tight gap under title row (matches mockup)

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMSettingsPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    const IRECT closeR(headerRow.R - 36.f, headerRow.T + 4.f, headerRow.R - 6.f, headerRow.T + 32.f);
    AddNamedChildControl(new VoLumSettingsCloseControl(closeR, closeAction), mControlNames.close);

    // Shared section-cap style for the three cards (gold, bold, centred).
    const IVStyle sectionCapStyle = mStyle.WithDrawFrame(false).WithShowValue(false).WithValueText(
      IText(13.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top));

    // ---- Footer, reserved bottom-up so the card row absorbs any extra slack ----
    // Bottom: model information (left) + about (right). The About block needs four
    // lines plus the update reminder and controls, so keep this row tall enough.
    const IRECT metaRow = inner.ReduceFromBottom(92.f).GetPadded(-2.f, 0.f, -2.f, 0.f);
    const float modelColW = metaRow.W() * 0.46f;
    const auto modelInfoArea = metaRow.GetFromLeft(modelColW);
    const auto aboutArea = metaRow.GetFromRight(metaRow.W() - modelColW);

    // Above it: the "real audio settings live elsewhere" hint line.
    (void)inner.ReduceFromBottom(6.f);
    const IRECT hintRow = inner.ReduceFromBottom(16.f);

    // Above that: the keyboard shortcut cheat-sheet box (tall enough for the
    // four-row Edit column).
    (void)inner.ReduceFromBottom(12.f);
    const IRECT shortcutArea = inner.ReduceFromBottom(94.f).GetPadded(-2.f, 0.f, -2.f, 0.f);

    // Hairline divider separating the interactive cards from the footer block.
    (void)inner.ReduceFromBottom(12.f);
    const IRECT sepRow = inner.ReduceFromBottom(1.f);
    AddNamedChildControl(new VoLumSettingsFooterSepControl(sepRow), mControlNames.footerSep);
    (void)inner.ReduceFromBottom(14.f);

    // ---- Card row: Input calibration | Output mode | Performance ----
    // Three equal, framed cards in one balanced row so the Performance / Lite
    // toggle gets a real home instead of floating on the divider.
    {
      const IRECT cardsBand = inner.GetPadded(-2.f, 0.f, -2.f, 0.f);
      const float cardGap = 14.f;
      const float cardW = (cardsBand.W() - 2.f * cardGap) / 3.f;
      const float cardH = std::min(cardsBand.H(), 176.f);
      const IRECT cardsRow = cardsBand.GetCentredInside(cardsBand.W(), cardH);
      const float x0 = cardsRow.L;
      const IRECT inputCard(x0, cardsRow.T, x0 + cardW, cardsRow.B);
      const IRECT outputCard(x0 + cardW + cardGap, cardsRow.T, x0 + 2.f * cardW + cardGap, cardsRow.B);
      const IRECT perfCard(x0 + 2.f * (cardW + cardGap), cardsRow.T, cardsRow.R, cardsRow.B);

      const float capH = 18.f;
      const float capGap = 8.f;
      auto cardInnerOf = [](const IRECT& c) { return c.GetPadded(-14.f); };
      auto capOf = [&](const IRECT& c) { return cardInnerOf(c).GetFromTop(capH); };
      auto bodyOf = [&](const IRECT& c) { return cardInnerOf(c).GetReducedFromTop(capH + capGap); };

      // Card frames first so all interactive content paints on top of them.
      AddNamedChildControl(new VoLumSettingsGroupFrameControl(inputCard), mControlNames.inputGroupFrame);
      AddNamedChildControl(new VoLumSettingsGroupFrameControl(outputCard), mControlNames.outputGroupFrame);
      AddNamedChildControl(new VoLumSettingsGroupFrameControl(perfCard), mControlNames.perfGroupFrame);

      // --- Input calibration: dBu field, Calibrate switch, and a status line ---
      // The status line is what turns a grayed-out card into an explanation; see
      // volum::InputCalibrationHelpText.
      {
        const IRECT body = bodyOf(inputCard);
        const float fieldH = 30.f;
        const float fieldW = std::min(120.f, body.W());
        const float switchH = NAM_SWTICH_HEIGHT;
        const float gap = 6.f;
        const float helpH = 16.f;
        const float stackH = fieldH + gap + switchH + gap + helpH;
        IRECT stack = body.GetCentredInside(body.W(), std::min(body.H(), stackH));
        const IRECT fieldR = stack.ReduceFromTop(fieldH).GetCentredInside(fieldW, fieldH);
        (void)stack.ReduceFromTop(gap);
        const IRECT switchR = stack.ReduceFromTop(switchH).GetCentredInside(std::min(110.f, body.W()), switchH);
        (void)stack.ReduceFromTop(gap);
        const IRECT helpR = stack.ReduceFromTop(helpH);

        auto* inputLevelControl =
          AddNamedChildControl(new InputLevelControl(fieldR, kInputCalibrationLevel, mInputLevelBackgroundBitmap, text),
                               mControlNames.inputCalibrationLevel, kCtrlTagInputCalibrationLevel);
        inputLevelControl->SetTooltip(
          "The analog level, in dBu RMS, that corresponds to digital level of 0 dBFS peak in the host as its signal "
          "enters this plugin.");
        AddNamedChildControl(new NAMSwitchControl(switchR, kCalibrateInput, "Calibrate input", mStyle, mSwitchBitmap),
                             mControlNames.calibrateInput, kCtrlTagCalibrateInput);
        const IVStyle inputHelpStyle = mStyle.WithDrawFrame(false).WithShowValue(false).WithValueText(
          IText(11.f, VoLumColors::TEXT_DIM.WithOpacity(0.7f), "Josefin-Sans", EAlign::Center, EVAlign::Top));
        AddNamedChildControl(
          new IVLabelControl(helpR, volum::InputCalibrationHelpText(false), inputHelpStyle), mControlNames.inputHelp);
      }

      // --- Output mode: Raw / Normalized / Calibrated radios ---
      {
        const IRECT body = bodyOf(outputCard);
        const float radioBandH = 56.f;
        const IRECT radioArea = body.GetCentredInside(body.W(), std::min(body.H(), radioBandH));
        const float buttonSize = 11.0f;
        auto* outputModeControl =
          AddNamedChildControl(new OutputModeControl(radioArea, kOutputMode, mRadioButtonStyle, buttonSize),
                               mControlNames.outputMode, kCtrlTagOutputMode);
        outputModeControl->SetTooltip(
          "How to adjust the level of the output.\nRaw=No adjustment.\nNormalized=Adjust the level so that all models "
          "are about the same loudness.\nCalibrated=Match the input's digital-analog calibration (needs a model with "
          "output-level data).");
      }

      // --- Performance: centred Lite-mode toggle + helper line ---
      {
        const IRECT body = bodyOf(perfCard);
        const float liteH = 30.f;
        const float helpH = 16.f;
        const float gap = 10.f;
        const float groupH = liteH + gap + helpH;
        IRECT group = body.GetCentredInside(body.W(), std::min(body.H(), groupH));
        const IRECT liteR = group.ReduceFromTop(liteH);
        (void)group.ReduceFromTop(gap);
        const IRECT helpR = group.ReduceFromTop(helpH);
        AddNamedChildControl(new VoLumLiteModeSwitchControl(liteR, leftText), mControlNames.liteMode);
        const IVStyle helpStyle = mStyle.WithDrawFrame(false).WithShowValue(false).WithValueText(
          IText(11.f, VoLumColors::TEXT_DIM.WithOpacity(0.7f), "Josefin-Sans", EAlign::Center, EVAlign::Top));
        AddNamedChildControl(
          new IVLabelControl(helpR, "Lite: smaller A2 slice, lower CPU", helpStyle), mControlNames.perfHelp);
      }

      // Section caps on top of each card frame.
      AddNamedChildControl(
        new IVLabelControl(capOf(inputCard), "Input calibration", sectionCapStyle), mControlNames.inputSection);
      AddNamedChildControl(
        new IVLabelControl(capOf(outputCard), "Output mode", sectionCapStyle), mControlNames.outputSection);
      AddNamedChildControl(
        new IVLabelControl(capOf(perfCard), "Performance", sectionCapStyle), mControlNames.perfSection);
    }

    // Footer block: build-aware audio-settings hint, shortcut box, model info, about.
#if defined(APP_API)
    const char* audioHintStr = "Audio device / sample rate / buffer: open File > Preferences";
#else
    const char* audioHintStr = "Audio device / sample rate / buffer: use your host's audio settings";
#endif
    const IVStyle hintStyle = mStyle.WithDrawFrame(false).WithShowValue(false).WithValueText(
      IText(11.f, VoLumColors::TEXT_DIM.WithOpacity(0.62f), "Josefin-Sans", EAlign::Center, EVAlign::Middle));
    AddNamedChildControl(new IVLabelControl(hintRow, audioHintStr, hintStyle), mControlNames.audioHint);

    const IVStyle modelInfoStyle = leftStyle.WithValueText(leftText.WithVAlign(EVAlign::Top));
    AddNamedChildControl(new VoLumSettingsShortcutInfoControl(shortcutArea), mControlNames.shortcutInfo);
    AddNamedChildControl(new ModelInfoControl(modelInfoArea, modelInfoStyle), mControlNames.modelInfo);
    {
      const auto aboutLineText = leftText.WithAlign(EAlign::Far).WithVAlign(EVAlign::Top);
      const IVStyle aboutStyle = leftStyle.WithValueText(aboutLineText);
      const auto urlText = aboutLineText;
      AddNamedChildControl(new AboutControl(aboutArea, aboutStyle, urlText), mControlNames.about);
    }

    OnResize();
  }

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->SetModelInfo(modelInfo);
  };

  void SetCurrentLatency(const volum::LatencyReport& report)
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->SetCurrentLatency(report);
  }

  // Keep the input-calibration card's status line and tooltip in step with whether
  // the loaded model can actually be calibrated against.
  void SetInputCalibrationAvailable(bool available)
  {
    if (auto* help = GetNamedChild(mControlNames.inputHelp))
    {
      static_cast<ITextControl*>(help)->SetStr(volum::InputCalibrationHelpText(available));
      help->SetDirty(false);
    }
    if (auto* sw = GetNamedChild(mControlNames.calibrateInput))
      sw->SetTooltip(volum::InputCalibrationTooltip(available));
  }

private:
  IBitmap mBitmap;
  IBitmap mInputLevelBackgroundBitmap;
  IBitmap mSwitchBitmap;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  ISVG mCloseSVG;
  int mAnimationTime = 200;
  bool mWillHide = false;

  // Names for controls
  // Make sure that these are all unique and that you use them with AddNamedChildControl
  struct ControlNames
  {
    const std::string about = "About";
    const std::string bitmap = "Bitmap";
    const std::string inputSection = "InputSection";
    const std::string outputSection = "OutputSection";
    const std::string footerSep = "FooterSep";
    const std::string calibrateInput = "CalibrateInput";
    const std::string close = "Close";
    const std::string inputCalibrationLevel = "InputCalibrationLevel";
    const std::string modelInfo = "ModelInfo";
    const std::string outputMode = "OutputMode";
    const std::string shortcutInfo = "ShortcutInfo";
    const std::string outputGroupFrame = "OutputGroupFrame";
    const std::string inputGroupFrame = "InputGroupFrame";
    const std::string perfGroupFrame = "PerfGroupFrame";
    const std::string title = "Title";
    const std::string liteMode = "LiteMode";
    const std::string perfSection = "PerfSection";
    const std::string perfHelp = "PerfHelp";
    const std::string inputHelp = "InputHelp";
    const std::string audioHint = "AudioHint";
  } mControlNames;

  class InputLevelControl : public IEditableTextControl
  {
  public:
    InputLevelControl(const IRECT& bounds, int paramIdx, const IBitmap& bitmap, const IText& text = DEFAULT_TEXT,
                      const IColor& BGColor = DEFAULT_BGCOLOR)
    : IEditableTextControl(bounds, "", text, BGColor)
    , mBitmap(bitmap)
    {
      SetParamIdx(paramIdx);
    };

    void Draw(IGraphics& g) override
    {
      g.FillRect(VoLumColors::HERO_BG, mRECT);
      g.DrawRect(VoLumColors::FRAME, mRECT);
      g.DrawRect(IColor(50, 200, 162, 78), mRECT.GetPadded(2.f));
      ITextControl::Draw(g);
    };

    void SetValueFromUserInput(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromUserInput(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      OnTextEntryCompletion(s.c_str(), valIdx);
    };

    void SetValueFromDelegate(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromDelegate(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      SetStr(s.c_str());
      SetDirty(false);
    };

  private:
    std::string ConvertToString(const double normalizedValue)
    {
      const double naturalValue = GetParam()->FromNormalized(normalizedValue);
      // And make the value to display
      std::stringstream ss;
      ss << naturalValue << " dBu";
      std::string s = ss.str();
      return s;
    };

    IBitmap mBitmap;
  };

  class AboutControl : public IContainerBase
  {
  public:
    AboutControl(const IRECT& bounds, const IVStyle& style, const IText& text)
    : IContainerBase(bounds)
    , mStyle(style)
    , mText(text) {};

    void OnAttached() override
    {
      WDL_String verStr, buildInfoStr;
      PLUG()->GetPluginVersionStr(verStr);

      buildInfoStr.SetFormatted(100, "Version %s %s %s", verStr.Get(), PLUG()->GetArchStr(), PLUG()->GetAPIStr());

      {
        IRECT lineR(GetRECT());
        const float capH = 18.f;
        const float rowH = 14.f;
        const IText capText(15.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Far, EVAlign::Top);
        const IText rowText = mStyle.valueText.WithVAlign(EVAlign::Top);
        AddChildControl(new IVLabelControl(lineR.ReduceFromTop(capH), "VoLum · By Lum", mStyle.WithValueText(capText)));
        mUpdateButton = new IVButtonControl(
          lineR.ReduceFromTop(18.f),
          [](IControl* pCaller) {
            if (auto* plugin = static_cast<PLUG_CLASS_NAME*>(pCaller->GetDelegate()))
              plugin->_VolumUseAvailableUpdate();
          },
          "Update available", mStyle.WithDrawFrame(false).WithValueText(rowText), true);
        AddChildControl(mUpdateButton);
        mUpdateButton->Hide(true);
        AddChildControl(new IVLabelControl(lineR.ReduceFromTop(rowH), buildInfoStr.Get(), mStyle.WithValueText(rowText)));
        const IColor urlMo = VoLumColors::GOLD_DIM;
        const IColor urlClk = VoLumColors::GOLD;
        AddChildControl(new IURLControl(lineR.ReduceFromTop(rowH), "Built on the Neural Amp Modeler ecosystem",
                                        "https://github.com/guitarlum/VoLum", mText, COLOR_TRANSPARENT, urlMo, urlClk));
        AddChildControl(new IURLControl(lineR.ReduceFromTop(rowH), "github.com/guitarlum/VoLum",
                                        "https://github.com/guitarlum/VoLum", mText, COLOR_TRANSPARENT, urlMo, urlClk));

        const IRECT actionRow = lineR.ReduceFromTop(22.f);
        const float checkW = 82.f;
        mAutoCheck = new IVToggleControl(
          actionRow.GetReducedFromRight(checkW + 8.f),
          [](IControl* pCaller) {
            if (auto* plugin = static_cast<PLUG_CLASS_NAME*>(pCaller->GetDelegate()))
              plugin->_VolumSetAutoUpdateCheck(pCaller->GetValue() > 0.5);
          },
          "Check automatically", mStyle.WithDrawFrame(false).WithValueText(rowText), "OFF", "ON", true);
        AddChildControl(mAutoCheck);
        AddChildControl(new IVButtonControl(
          actionRow.GetFromRight(checkW),
          [](IControl* pCaller) {
            if (auto* plugin = static_cast<PLUG_CLASS_NAME*>(pCaller->GetDelegate()))
              plugin->_VolumCheckForUpdatesNow();
          },
          "Check now", mStyle.WithDrawFrame(true).WithValueText(rowText), true));
      }
    };

    void SetUpdateInfo(bool autoCheck, bool available, const std::string& version)
    {
      if (mAutoCheck)
      {
        mAutoCheck->SetValue(autoCheck ? 1.0 : 0.0);
        mAutoCheck->SetDirty(false);
      }
      if (mUpdateButton)
      {
        const std::string label = "Update available: " + version + " - What's new";
        mUpdateButton->SetLabelStr(label.c_str());
        mUpdateButton->Hide(!available);
        mUpdateButton->SetDirty(false);
      }
    }

  private:
    IVStyle mStyle;
    IText mText;
    IVButtonControl* mUpdateButton = nullptr;
    IVToggleControl* mAutoCheck = nullptr;
  };

public:
  void SetUpdateInfo(bool autoCheck, bool available, const std::string& version)
  {
    if (auto* about = GetNamedChild(mControlNames.about))
      static_cast<AboutControl*>(about)->SetUpdateInfo(autoCheck, available, version);
  }
};
