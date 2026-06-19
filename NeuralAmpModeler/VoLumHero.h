#pragma once

// VoLum hero panel controls.
//
// - VoLumHeroImageControl: the procedural "fractal vista" backplate plus the
//   AMP title strip, Dual Amp chip, lane PAN dots, and click-to-focus dispatch
//   for the SUPPORT lane. Owns most of the visual identity of the AMP section.
// - VoLumSupportPolarityControl: small overlay toggle that flips the SUPPORT
//   lane's polarity (180-degree phase flip) when Dual Amp is engaged.
//
// Extracted from VoLumCoreControls.h on the 1.0-bugs-hygiene branch (file-size
// hygiene split). Source-string locks for the "Switch to Single/Dual Amp"
// tooltip and the polarity glyph live in test_volum_ui_regressions.cpp and now
// read this file.

#include "VoLumColorHelpers.h"
#include "VoLumFractalArt.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <functional>

class VoLumHeroImageControl : public IControl
{
public:
  using FocusCallback = std::function<void(bool supportFocused)>;
  using PickerCallback = std::function<void(const IRECT& anchor)>;
  using DualToggleCallback = std::function<void()>;
  using DismissPickerCallback = std::function<void()>;
  using IsPickerOpenCallback = std::function<bool()>;

  VoLumHeroImageControl(const IRECT& bounds,
                        FocusCallback focusCb = nullptr,
                        PickerCallback pickerCb = nullptr,
                        DualToggleCallback dualToggleCb = nullptr,
                        DismissPickerCallback dismissPickerCb = nullptr,
                        IsPickerOpenCallback isPickerOpenCb = nullptr)
  : IControl(bounds)
  , mFocusCallback(std::move(focusCb))
  , mPickerCallback(std::move(pickerCb))
  , mDualToggleCallback(std::move(dualToggleCb))
  , mDismissPickerCallback(std::move(dismissPickerCb))
  , mIsPickerOpenCallback(std::move(isPickerOpenCb))
  {
    mIgnoreMouse = (mFocusCallback == nullptr && mPickerCallback == nullptr
                    && mDualToggleCallback == nullptr && mDismissPickerCallback == nullptr);
  }

  void Draw(IGraphics& g) override
  {
    if (mDualAmpActive && !mCustomMode)
    {
      DrawDualHero(g);
    }
    else if (mHasBitmap)
    {
      const float imgW = (float)mBitmap.W();
      const float imgH = (float)mBitmap.H() / (float)std::max(1, mBitmap.N());
      const float scale = std::min(mRECT.W() / imgW, mRECT.H() / imgH);
      const float drawW = imgW * scale;
      const float drawH = imgH * scale;
      const IRECT centered(mRECT.MW() - drawW / 2.f, mRECT.MH() - drawH / 2.f,
                           mRECT.MW() + drawW / 2.f, mRECT.MH() + drawH / 2.f);
      g.DrawFittedBitmap(mBitmap, centered);
    }
    else
    {
      DrawMonoHero(g);
    }
  }

  void SetPlaceholder(const char* text, int ampIdx = 0)
  {
    mPlaceholder = text;
    if (mAmpIdx != ampIdx || mCustomMode)
    {
      mAmpIdx = ampIdx;
      mMonoArtLayer = nullptr;
      mMainArtLayer = nullptr;
    }
    mCustomMode = false; // SetPlaceholder is the factory-amp entry point
    mHasBitmap = false;
    SetDirty(false);
  }

  void SetName(const char* name)
  {
    mName = name;
    SetDirty(false);
  }

  // Custom amps render an assigned procedural art instead of the factory amp
  // fractal, and hide the dual/stereo affordance (not supported for custom amps
  // in 1.2.0). Pass isCustom=false to return to the factory amp art.
  void SetCustomArt(bool isCustom, int artId)
  {
    if (mCustomMode != isCustom || mCustomArt != artId)
    {
      mCustomMode = isCustom;
      mCustomArt = artId;
      mMonoArtLayer = nullptr; // force art layer rebuild
    }
    SetDirty(false);
  }

  void SetDualAmpState(bool active, bool supportFocused, int supportAmpIdx)
  {
    if (mSupportAmpIdx != supportAmpIdx)
      mSupportArtLayer = nullptr;
    mDualAmpActive = active;
    mSupportFocused = supportFocused;
    mSupportAmpIdx = supportAmpIdx;
    SetDirty(false);
  }

  void SetBitmap(const IBitmap& bitmap)
  {
    mBitmap = bitmap;
    mHasBitmap = true;
    mMonoArtLayer = nullptr;
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;

    // Custom amps have no dual/stereo affordance in 1.2.0 — swallow clicks.
    if (mCustomMode)
      return;

    // 1. DUAL toggle chip — top-right of mono, top-right of MAIN panel in dual.
    if (mDualToggleCallback && DualChipRect().Contains(x, y))
    {
      if (mDismissPickerCallback)
        mDismissPickerCallback();
      mDualToggleCallback();
      return;
    }

    // 2. Lane focus + support-amp picker (dual mode only).
    if (!mDualAmpActive)
      return;

    const bool clickedSupport = (x >= mRECT.MW());
    const bool wasSupportFocused = mSupportFocused;

    // IMPORTANT: capture the picker's visibility BEFORE running the focus callback. The focus
    // callback rebuilds the layout and explicitly hides the menu — if we sampled it afterwards
    // we'd always see "closed" and re-open it on the same click.
    const bool pickerOpen = mIsPickerOpenCallback ? mIsPickerOpenCallback() : false;

    mSupportFocused = clickedSupport;
    if (mFocusCallback)
      mFocusCallback(mSupportFocused);

    // Picker behaviour on the SUPPORT panel:
    //   - If the picker is already open: any click on the support panel dismisses it
    //     (regardless of focus state) so a second click is always "close".
    //   - Empty state (no support amp picked): the panel is the "ADD AMP" CTA — open immediately.
    //   - Support already focused (and picker closed): clicks open the picker.
    //   - First click that shifts focus from MAIN to support (with a support amp already picked):
    //     focus only, no picker. The user has to click again to open the dropdown.
    if (clickedSupport)
    {
      if (pickerOpen)
      {
        if (mDismissPickerCallback) mDismissPickerCallback();
      }
      else if (mPickerCallback)
      {
        const bool empty = (mSupportAmpIdx < 0 || mSupportAmpIdx >= volum::kAmpCount);
        if (wasSupportFocused || empty)
        {
          const float gap = 8.f;
          const float mid = mRECT.MW();
          const IRECT supportPanel(mid + gap / 2.f, mRECT.T, mRECT.R, mRECT.B);
          mPickerCallback(supportPanel);
        }
      }
    }
    else if (mDismissPickerCallback)
    {
      // Click on MAIN while the support menu is open should dismiss it. (Outside-the-hero clicks
      // are handled by the global VoLumKnobSelectionClearControl.)
      mDismissPickerCallback();
    }

    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    const bool chipHovered = mDualToggleCallback && DualChipRect().Contains(x, y);
    if (chipHovered != mDualChipHovered)
    {
      mDualChipHovered = chipHovered;
      SetTooltip(mDualChipHovered ? (mDualAmpActive ? "Switch to Single Amp" : "Switch to Dual Amp") : "");
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mDualChipHovered)
    {
      mDualChipHovered = false;
      SetTooltip("");
      SetDirty(false);
    }
  }

  // Reserve a square slot in the bottom-right of each lane for a PAN knob attached at the IGraphics
  // level (NAMKnobControl). Only the dual-mode lanes need it; mono mode has no PAN.
  IRECT GetMainPanKnobSlot() const
  {
    const float gap = 8.f;
    const float mid = mRECT.MW();
    const IRECT lane(mRECT.L, mRECT.T, mid - gap / 2.f, mRECT.B);
    return PanKnobSlotInLane(lane);
  }

  IRECT GetSupportPanKnobSlot() const
  {
    const float gap = 8.f;
    const float mid = mRECT.MW();
    const IRECT lane(mid + gap / 2.f, mRECT.T, mRECT.R, mRECT.B);
    return PanKnobSlotInLane(lane);
  }

  IRECT GetSupportPolarityToggleSlot() const
  {
    const float gap = 8.f;
    const float mid = mRECT.MW();
    const IRECT lane(mid + gap / 2.f, mRECT.T, mRECT.R, mRECT.B);
    return PolarityToggleSlotInLane(lane);
  }

private:
  /* ---------- geometry helpers ---------- */

  static constexpr float kChipW = 24.f;
  static constexpr float kChipH = 16.f;
  // Sized to share the amp-name title strip's vertical center with the label. 24 px straddles
  // the 22 px strip (1 px above, 1 px below) so the knob reads as visually centered with the
  // label text rather than floating in the art area above it.
  static constexpr float kPanKnobSize = 24.f;

  IRECT DualChipRect() const
  {
    if (mDualAmpActive)
    {
      const float gap = 8.f;
      const float mid = mRECT.MW();
      const float right = mid - gap / 2.f; // right edge of MAIN panel
      return IRECT(right - kChipW - 6.f, mRECT.T + 6.f, right - 6.f, mRECT.T + 6.f + kChipH);
    }
    return IRECT(mRECT.R - kChipW - 6.f, mRECT.T + 6.f, mRECT.R - 6.f, mRECT.T + 6.f + kChipH);
  }

  static IRECT PanKnobSlotInLane(const IRECT& lane)
  {
    // The amp-name title strip lives at IRECT(lane.L + 8, lane.B - 30, lane.R - 8, lane.B - 8)
    // (see DrawLane). Center the knob's vertical midpoint on the strip's vertical midpoint so
    // it visually shares a baseline with the amp-name label. Right edge inset 8 px from the
    // lane edge.
    const float rightInset = 8.f;
    const float stripCenterY = lane.B - 19.f; // (lane.B - 30 + lane.B - 8) / 2
    const float right = lane.R - rightInset;
    const float top = stripCenterY - kPanKnobSize * 0.5f;
    return IRECT(right - kPanKnobSize, top, right, top + kPanKnobSize);
  }

  static IRECT PolarityToggleSlotInLane(const IRECT& lane)
  {
    const float size = 24.f;
    const float right = lane.R - 8.f;
    const float top = lane.T + 8.f;
    return IRECT(right - size, top, right, top + size);
  }

  /* ---------- draw helpers ---------- */

  // Two-rect "split" glyph (◫): two small rectangles representing two amp panels side-by-side.
  // Filled when Dual Amp is engaged, outlined-only when off.
  void DrawDualChip(IGraphics& g, const IColor& accent)
  {
    const IRECT chip = DualChipRect();
    const bool active = mDualAmpActive;
    const IColor border = (active || mDualChipHovered) ? accent : VoLumColors::GOLD_DIM;
    const IColor fill = active ? IColor(mDualChipHovered ? 95 : 70, accent.R, accent.G, accent.B)
                               : (mDualChipHovered ? IColor(34, accent.R, accent.G, accent.B)
                                                   : IColor(0, 0, 0, 0));
    g.FillRect(fill, chip);
    g.DrawRect(border, chip, nullptr, mDualChipHovered ? 1.4f : 1.f);
    const float cy = chip.MH();
    const float h = 8.f;
    const float w = 4.f;
    const float gx = chip.MW();
    g.DrawRect(border, IRECT(gx - w - 1.f, cy - h / 2.f, gx - 1.f, cy + h / 2.f));
    g.DrawRect(border, IRECT(gx + 1.f, cy - h / 2.f, gx + 1.f + w, cy + h / 2.f));
  }

  // Empty-state placeholder for an unselected support amp: large "+" plus a hint, no fractal.
  void DrawEmptySupportArt(IGraphics& g, const IRECT& artRect)
  {
    const IColor dim = IColor(140, VoLumColors::TEAL.R, VoLumColors::TEAL.G, VoLumColors::TEAL.B);
    const IColor faint = IColor(60, VoLumColors::TEAL.R, VoLumColors::TEAL.G, VoLumColors::TEAL.B);

    const float cx = artRect.MW();
    const float cy = artRect.MH() - 6.f;
    const float r1 = 22.f;
    g.DrawCircle(faint, cx, cy, r1);
    const float thick = 2.f;
    const float arm = 14.f;
    g.FillRect(dim, IRECT(cx - arm, cy - thick, cx + arm, cy + thick));
    g.FillRect(dim, IRECT(cx - thick, cy - arm, cx + thick, cy + arm));

    g.DrawText(IText(10.f, dim, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "ADD AMP", IRECT(artRect.L, cy + r1 + 4.f, artRect.R, cy + r1 + 18.f));
  }

  void DrawMonoHero(IGraphics& g)
  {
    g.FillRect(VoLumColors::HERO_BG, mRECT);
    g.DrawRect(VoLumColors::HERO_BORDER, mRECT);
    const float cs = 16.f;
    const float m = 6.f;
    DrawCornerAccent(g, mRECT.L + m, mRECT.T + m, cs, false, false, VoLumColors::HERO_CORNER);
    // Top-right corner skipped — DUAL chip lives there
    DrawCornerAccent(g, mRECT.L + m, mRECT.B - m, cs, false, true, VoLumColors::HERO_CORNER);
    DrawCornerAccent(g, mRECT.R - m, mRECT.B - m, cs, true, true, VoLumColors::HERO_CORNER);

    const IRECT artRect = mRECT.GetPadded(-6.f, -6.f, -6.f, -6.f);
    if (!g.CheckLayer(mMonoArtLayer) || mCachedMonoAmpIdx != mAmpIdx)
    {
      g.StartLayer(this, artRect);
      if (mCustomMode)
        DrawCustomAmpArt(g, artRect, mCustomArt, IColor(170, 120, 210, 220), IColor(70, 100, 180, 200));
      else
        DrawHeroFractalArt(g, artRect, FractalCaseForAmp(mAmpIdx));
      mMonoArtLayer = g.EndLayer();
      mCachedMonoAmpIdx = mAmpIdx;
    }
    g.DrawLayer(mMonoArtLayer);

    // No dual/stereo affordance for custom amps in 1.2.0.
    if (!mCustomMode)
      DrawDualChip(g, VoLumColors::AMBER);
  }

  void DrawLane(IGraphics& g, const IRECT& r, int ampIdx, const char* role, const char* name, bool focused,
                const IColor& accent, bool drawChip, bool empty)
  {
    g.FillRect(VoLumColors::HERO_BG, r);
    g.DrawRect(focused ? accent : VoLumColors::HERO_BORDER, r);
    DrawCornerAccent(g, r.L + 6.f, r.T + 6.f, 12.f, false, false, focused ? accent : VoLumColors::HERO_CORNER);
    if (!drawChip)
      DrawCornerAccent(g, r.R - 6.f, r.T + 6.f, 12.f, true, false, focused ? accent : VoLumColors::HERO_CORNER);
    DrawCornerAccent(g, r.L + 6.f, r.B - 6.f, 12.f, false, true, focused ? accent : VoLumColors::HERO_CORNER);
    DrawCornerAccent(g, r.R - 6.f, r.B - 6.f, 12.f, true, true, focused ? accent : VoLumColors::HERO_CORNER);

    // Fractal (or empty-state placeholder) clipped above the title strip.
    const IRECT artRect = r.GetPadded(-10.f, -28.f, -10.f, -38.f);
    const bool mainLane = std::strcmp(role, "MAIN") == 0;
    ILayerPtr& layer = mainLane ? mMainArtLayer : mSupportArtLayer;
    int& cachedAmpIdx = mainLane ? mCachedMainAmpIdx : mCachedSupportAmpIdx;
    bool& cachedEmpty = mainLane ? mCachedMainEmpty : mCachedSupportEmpty;
    const int clampedAmpIdx = std::clamp(ampIdx, 0, volum::kAmpCount - 1);
    if (!g.CheckLayer(layer) || cachedAmpIdx != clampedAmpIdx || cachedEmpty != empty)
    {
      g.StartLayer(this, artRect);
      if (empty)
        DrawEmptySupportArt(g, artRect);
      else
        DrawHeroFractalArt(g, artRect, FractalCaseForAmp(clampedAmpIdx));
      layer = g.EndLayer();
      cachedAmpIdx = clampedAmpIdx;
      cachedEmpty = empty;
    }
    g.DrawLayer(layer);

    // Title strip with amp name
    const IRECT titleStrip(r.L + 8.f, r.B - 30.f, r.R - 8.f, r.B - 8.f);
    g.FillRect(IColor(190, 12, 12, 18), titleStrip);
    g.DrawText(IText(10.f, accent, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
               role, IRECT(r.L + 12.f, r.T + 8.f, r.R - 12.f, r.T + 24.f));
    g.DrawText(IText(12.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               name, titleStrip);
  }

  void DrawDualHero(IGraphics& g)
  {
    const float gap = 8.f;
    const float mid = mRECT.MW();
    const IRECT left(mRECT.L, mRECT.T, mid - gap / 2.f, mRECT.B);
    const IRECT right(mid + gap / 2.f, mRECT.T, mRECT.R, mRECT.B);
    DrawLane(g, left, mAmpIdx, "MAIN", mName.c_str(), !mSupportFocused, VoLumColors::AMBER,
             /*drawChip=*/true, /*empty=*/false);
    const bool hasSupport = mSupportAmpIdx >= 0 && mSupportAmpIdx < volum::kAmpCount;
    DrawLane(g, right, hasSupport ? mSupportAmpIdx : 0, "SUPPORT",
             hasSupport ? volum::kAmps[mSupportAmpIdx].displayName : "Choose support amp",
             mSupportFocused, VoLumColors::TEAL,
             /*drawChip=*/false, /*empty=*/!hasSupport);
    g.FillRect(VoLumColors::BG, IRECT(mid - 1.f, mRECT.T, mid + 1.f, mRECT.B));
    DrawDualChip(g, VoLumColors::AMBER);
  }

  bool mHasBitmap = false;
  IBitmap mBitmap;
  ILayerPtr mMonoArtLayer;
  ILayerPtr mMainArtLayer;
  ILayerPtr mSupportArtLayer;
  int mCachedMonoAmpIdx = -1;
  int mCachedMainAmpIdx = -1;
  int mCachedSupportAmpIdx = -1;
  bool mCachedMainEmpty = false;
  bool mCachedSupportEmpty = false;
  std::string mPlaceholder = "A1";
  std::string mName = "Ampete One";
  int mAmpIdx = 0;
  int mSupportAmpIdx = -1;
  bool mDualAmpActive = false;
  bool mSupportFocused = false;
  bool mDualChipHovered = false;
  bool mCustomMode = false;
  int mCustomArt = 0;
  FocusCallback mFocusCallback;
  PickerCallback mPickerCallback;
  DualToggleCallback mDualToggleCallback;
  DismissPickerCallback mDismissPickerCallback;
  IsPickerOpenCallback mIsPickerOpenCallback;
};

class VoLumSupportPolarityControl : public IControl
{
public:
  using IsActiveCallback = std::function<bool()>;
  using ToggleCallback = std::function<void()>;

  VoLumSupportPolarityControl(const IRECT& bounds, IsActiveCallback isActiveCb, ToggleCallback toggleCb)
  : IControl(bounds)
  , mIsActiveCallback(std::move(isActiveCb))
  , mToggleCallback(std::move(toggleCb))
  {
    SetTooltip("Flip polarity");
  }

  void Draw(IGraphics& g) override
  {
    const bool active = mIsActiveCallback ? mIsActiveCallback() : false;
    const bool hot = mMouseIsOver;
    const IColor glyph = active ? (hot ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEAL)
                                : (hot ? VoLumColors::CREAM_DIM : IColor(135, 92, 98, 110));
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float outerR = active ? (hot ? 7.55f : 7.25f) : 6.8f;
    const float innerR = outerR * 0.66f;
    const float slash = outerR * 1.2f;
    const float stroke = active ? (hot ? 2.25f : 1.9f) : 1.45f;

    // Filled DAW-style polarity glyph: solid ring + diagonal bar, no surrounding button frame.
    g.FillCircle(glyph, cx, cy, outerR);
    g.FillCircle(VoLumColors::HERO_BG, cx, cy, innerR);
    g.DrawLine(glyph, cx - slash, cy + slash, cx + slash, cy - slash, nullptr, stroke);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) y;
    (void) mod;
    if (mToggleCallback)
      mToggleCallback();
    SetDirty(false);
  }

private:
  IsActiveCallback mIsActiveCallback;
  ToggleCallback mToggleCallback;
};
