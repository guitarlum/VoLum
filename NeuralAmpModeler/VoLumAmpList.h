#pragma once

// VoLum left-sidebar amp picker (VoLumAmpListControl).
//
// Vertically scrollable list of the bundled amp profiles with cached SVG icons,
// hover lift, and click-to-select dispatch back to the plugin. Below the
// factory amps it renders a CUSTOM section (1.2.0): an "+ CUSTOM" affordance
// when empty, or a "CUSTOM" header with a + button plus one row per custom amp
// (each with edit/delete affordances). Selecting a custom amp dispatches via a
// separate callback so the factory load path stays untouched.
//
// Source-string lock for the layer-cache idiom (`if (!g.CheckLayer(mIconLayers[i]))`)
// lives in test_volum_ui_regressions.cpp and reads this file.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentMock.h"
#include "VoLumFractalArt.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

class VoLumAmpListControl : public IControl
{
public:
  using SelectionCallback = std::function<void(int ampIdx)>;
  using CustomSelectCallback = std::function<void(int customIdx)>;
  using CustomActionCallback = std::function<void(int customIdx)>;
  using AddCallback = std::function<void()>;

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

  void SetCustomAmps(const std::vector<std::string>& names, const std::vector<int>& arts = {})
  {
    mCustomNames = names;
    mCustomArts = arts;
    mCustomArts.resize(mCustomNames.size(), 0); // keep lockstep even if arts omitted
    SetDirty(false);
  }

  void SetCustomCallbacks(CustomSelectCallback sel, AddCallback add, CustomActionCallback edit,
                          CustomActionCallback del)
  {
    mCustomSelectCb = std::move(sel);
    mCustomAddCb = std::move(add);
    mCustomEditCb = std::move(edit);
    mCustomDeleteCb = std::move(del);
  }

  // -1 => a factory amp is the active selection.
  void SetCustomSelected(int idx)
  {
    mCustomSelected = idx;
    ScrollToRevealCustom(idx);
    SetDirty(false);
  }

  int GetCustomSelected() const { return mCustomSelected; }
  int GetCustomCount() const { return (int)mCustomNames.size(); }

  void Draw(IGraphics& g) override
  {
    const float itemH = ItemHeight();
    const float iconSize = 22.f;
    const float pad = 8.f;
    const float contentH = ContentHeight();
    const bool scrollable = Scrollable();
    const float scrollbarW = scrollable ? kScrollbarW : 0.f;
    const float rowR = RowRight();

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

      IRECT row(mRECT.L, top, rowR, bot);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);
      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f, paddedRow.L + pad + iconSize,
                     paddedRow.MH() + iconSize / 2.f);

      if (!g.CheckLayer(mIconLayers[i]))
      {
        g.StartLayer(this, iconArea);
        IColor thmBright(200, 120, 210, 220);
        IColor thmDim(100, 100, 180, 200);
        DrawMiniFractal(g, iconArea, i, thmBright, thmDim);
        mIconLayers[i] = g.EndLayer();
      }
    }

    // Build the 4 custom-amp art thumbnails once (cached, blitted per custom row)
    // while the clip region is still full, like the factory icons above.
    if (!mCustomNames.empty())
    {
      const IRECT artBuildR(mRECT.L, mRECT.T, mRECT.L + 22.f, mRECT.T + 22.f);
      for (int a = 0; a < (int)mCustomArtLayers.size(); a++)
        if (!g.CheckLayer(mCustomArtLayers[a]))
        {
          g.StartLayer(this, artBuildR);
          DrawCustomAmpArt(g, artBuildR, a, VoLumColors::CUSTOM_ART_BRIGHT, VoLumColors::CUSTOM_ART_DIM);
          mCustomArtLayers[a] = g.EndLayer();
        }
    }

    g.PathClipRegion(mRECT);

    // Factory amp rows
    for (int i = 0; i < mNumAmps; i++)
    {
      const float top = mRECT.T + i * itemH - mScrollOffset;
      const float bot = top + itemH;
      if (bot < mRECT.T - 1.f || top > mRECT.B + 1.f)
        continue;

      IRECT row(mRECT.L, top, rowR, bot);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);

      const bool selected = (i == mSelected) && (mCustomSelected < 0);
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, paddedRow, 4.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, paddedRow, 4.f);
        g.FillRect(VoLumColors::GOLD, IRECT(paddedRow.L, paddedRow.T + 3.f, paddedRow.L + 2.5f, paddedRow.B - 3.f));
      }
      else if (i == mHovered && mHoveredDomain == EDomain::Factory)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, paddedRow, 4.f);
      }

      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f, paddedRow.L + pad + iconSize,
                     paddedRow.MH() + iconSize / 2.f);

      g.DrawRect(IColor(selected ? 80 : 58, 120, 195, 210), iconArea);
      // Blit the cached thumbnail at the row's CURRENT position. Use
      // DrawFittedLayer (not DrawFittedBitmap): it translates to iconArea.L/T so
      // the thumbnail still tracks the scroll, AND it scales by the layer's
      // LOGICAL bounds rather than the cached bitmap's pixel width. The pixel
      // width changes when the layer is rebuilt at a new draw scale on window
      // resize, which made DrawFittedBitmap render the icon at its original
      // (small) size while the rest of the UI scaled up. (item: thumbnails
      // don't scale on resize.)
      if (mIconLayers[i] && g.CheckLayer(mIconLayers[i]))
        g.DrawFittedLayer(mIconLayers[i], iconArea, nullptr);

      // Keep the name inside the selection highlight (paddedRow) and clear of
      // the scrollbar gutter. The sidebar is narrow, so auto-shrink long names
      // a point or two (down to a floor) instead of hard-clipping mid-word;
      // clip is only a final guard.
      IRECT nameArea = paddedRow.GetReducedFromLeft(pad + iconSize + 8.f).GetReducedFromRight(6.f);
      IColor nameCol = selected ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
      float nameFont = 13.f;
      for (; nameFont > 10.5f; nameFont -= 0.5f)
      {
        IText measureTxt(nameFont, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
        IRECT measured;
        g.MeasureText(measureTxt, mAmpNames[i].c_str(), measured);
        if (measured.W() <= nameArea.W())
          break;
      }
      IText nameText(nameFont, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      g.PathClipRegion(nameArea);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
      g.PathClipRegion(mRECT); // restore the list clip for following rows
    }

    DrawCustomSection(g, itemH, rowR);

    g.PathClipRegion();

    if (scrollable)
      DrawScrollbar(g, contentH, scrollbarW);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    ClearVoLumKnobSelection(this);

    // Scrollbar first: a forgiving grab zone covers the gutter + visible bar so
    // the thin bar is easy to hit. Dragging the thumb scrolls; clicking the
    // track jumps the thumb to the cursor.
    const float contentH = ContentHeight();
    if (Scrollable() && x >= RowRight())
    {
      IRECT track, thumb;
      ScrollbarGeometry(contentH, track, thumb);
      if (y >= thumb.T && y <= thumb.B)
        mDragGrabDY = y - thumb.T; // grab the thumb where clicked
      else
        mDragGrabDY = thumb.H() * 0.5f; // track click -> center thumb on cursor
      mDraggingScrollbar = true;
      ScrollThumbTo(y, track, thumb.H(), contentH);
      SetDirty(false);
      return;
    }

    const Hit hit = HitTest(x, y);
    switch (hit.zone)
    {
      case EZone::Factory:
        mSelected = hit.idx;
        mCustomSelected = -1;
        if (mCallback)
          mCallback(hit.idx);
        ScrollToRevealFactory(hit.idx);
        SetDirty(false);
        break;
      case EZone::CustomAdd:
        if (mCustomAddCb)
          mCustomAddCb();
        break;
      case EZone::CustomEdit:
        if (mCustomEditCb)
          mCustomEditCb(hit.idx);
        break;
      case EZone::CustomDelete:
        if (mCustomDeleteCb)
          mCustomDeleteCb(hit.idx);
        break;
      case EZone::CustomRow:
        mCustomSelected = hit.idx;
        if (mCustomSelectCb)
          mCustomSelectCb(hit.idx);
        SetDirty(false);
        break;
      default: break;
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    (void)x;
    (void)dX;
    (void)dY;
    (void)mod;
    if (!mDraggingScrollbar)
      return;
    const float contentH = ContentHeight();
    IRECT track, thumb;
    ScrollbarGeometry(contentH, track, thumb);
    ScrollThumbTo(y, track, thumb.H(), contentH);
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    (void)x;
    (void)y;
    (void)mod;
    mDraggingScrollbar = false;
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void)mod;
    const Hit hit = HitTest(x, y);
    int idx = -1;
    EDomain dom = EDomain::None;
    if (hit.zone == EZone::Factory)
    {
      idx = hit.idx;
      dom = EDomain::Factory;
    }
    else if (hit.zone == EZone::CustomRow || hit.zone == EZone::CustomEdit || hit.zone == EZone::CustomDelete)
    {
      idx = hit.idx;
      dom = EDomain::Custom;
    }
    const char* tip = "";
    if (hit.zone == EZone::CustomEdit)
      tip = "Edit custom amp";
    else if (hit.zone == EZone::CustomDelete)
      tip = "Delete custom amp";
    else if (hit.zone == EZone::CustomAdd)
      tip = "Create a custom amp";
    if (mCurTip != tip)
    {
      mCurTip = tip;
      SetTooltip(tip);
    }
    if (idx != mHovered || dom != mHoveredDomain || hit.zone != mHoveredZone)
    {
      mHovered = idx;
      mHoveredDomain = dom;
      mHoveredZone = hit.zone;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHovered = -1;
    mHoveredDomain = EDomain::None;
    mHoveredZone = EZone::None;
    if (!mCurTip.empty())
    {
      mCurTip.clear();
      SetTooltip("");
    }
    SetDirty(false);
  }

  void OnRescale() override
  {
    // The backing pixel scale changed (window resize / DPI). Drop the cached
    // thumbnail layers so they re-render crisp at the new scale on the next Draw
    // instead of being blitted at the old resolution (looked like they "didn't
    // scale" with the rest of the UI).
    for (auto& l : mIconLayers)
      l = nullptr;
    for (auto& l : mCustomArtLayers)
      l = nullptr;
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    (void)mod;
    const float contentH = ContentHeight();
    if (contentH <= mRECT.H())
      return;

    // d is wheel-delta/120. Two input shapes need different handling:
    //  - Precision touchpads stream many fractional events (|d| < 1) as the
    //    finger moves. Map those 1:1 to pixels with NO animation so the list
    //    tracks the gesture exactly - any easing on top just feels laggy.
    //  - A classic mouse wheel / coarse-detent trackpad sends |d| >= 1 per
    //    notch. Glide toward a target so a single notch travels ~2 rows
    //    smoothly instead of snapping.
    const float maxScroll = std::max(0.f, contentH - mRECT.H());
    if (std::abs(d) < 1.f)
    {
      mScrollTarget = std::clamp(mScrollTarget - d * ItemHeight(), 0.f, maxScroll);
      mScrollOffset = mScrollTarget; // 1:1, no easing
      SetDirty(false);
    }
    else
    {
      mScrollTarget = std::clamp(mScrollTarget - d * ItemHeight() * 2.0f, 0.f, maxScroll);
      StartScrollAnim();
    }

    OnMouseOver(x, y, mod);
  }

  void SetSelected(int idx)
  {
    mSelected = idx;
    mCustomSelected = -1;
    ScrollToRevealFactory(idx);
    SetDirty(false);
  }

  int GetSelected() const { return mSelected; }

private:
  static constexpr float kMinItemH = 30.f;
  static constexpr float kHeaderH = 26.f;
  static constexpr float kScrollbarW = 6.f;  // visible bar width when scrollable
  static constexpr float kScrollGutter = 7.f; // empty gap between labels and bar

  bool Scrollable() const { return ContentHeight() > mRECT.H() + 0.5f; }

  // Right edge of the row content (labels/icons stop here). Leaves a gutter
  // before the scrollbar so labels never crowd the bar.
  float RowRight() const
  {
    return Scrollable() ? (mRECT.R - kScrollbarW - kScrollGutter) : mRECT.R;
  }

  // Visible scrollbar track + thumb, shared by drawing and drag hit-testing so
  // they can never disagree.
  void ScrollbarGeometry(float contentH, IRECT& track, IRECT& thumb) const
  {
    const float trackX = mRECT.R - kScrollbarW + 1.f;
    track = IRECT(trackX, mRECT.T + 2.f, mRECT.R - 1.f, mRECT.B - 2.f);
    const float thumbH = std::max(18.f, track.H() * (mRECT.H() / std::max(contentH, 1.f)));
    const float maxScroll = std::max(0.f, contentH - mRECT.H());
    const float t = (maxScroll > 0.f) ? (mScrollOffset / maxScroll) : 0.f;
    const float thumbY = track.T + (track.H() - thumbH) * t;
    thumb = IRECT(track.L, thumbY, track.R, thumbY + thumbH);
  }

  // Map a cursor y (minus the grab offset within the thumb) to a scroll offset.
  void ScrollThumbTo(float y, const IRECT& track, float thumbH, float contentH)
  {
    const float maxScroll = std::max(0.f, contentH - mRECT.H());
    const float usable = track.H() - thumbH;
    float t = (usable > 0.f) ? ((y - mDragGrabDY - track.T) / usable) : 0.f;
    t = std::clamp(t, 0.f, 1.f);
    mScrollOffset = t * maxScroll;
    mScrollTarget = mScrollOffset;
    ClampScroll();
  }

  enum class EDomain
  {
    None,
    Factory,
    Custom
  };
  enum class EZone
  {
    None,
    Factory,
    CustomHeader,
    CustomAdd,
    CustomRow,
    CustomEdit,
    CustomDelete
  };

  struct Hit
  {
    EZone zone = EZone::None;
    int idx = -1;
  };

  float ItemHeight() const
  {
    if (mNumAmps <= 0)
      return mRECT.H();
    // Fit the factory amps plus the (always present) custom header into the
    // visible height; custom rows beyond that are what trigger scrolling.
    const float fitH = (mRECT.H() - kHeaderH) / (float)mNumAmps;
    return std::max(kMinItemH, fitH);
  }

  float FactoryBlockH() const { return ItemHeight() * mNumAmps; }

  float ContentHeight() const
  {
    const float custom = mCustomNames.empty() ? 0.f : (ItemHeight() * (float)mCustomNames.size());
    return FactoryBlockH() + kHeaderH + custom;
  }

  void ClampScroll()
  {
    const float maxScroll = std::max(0.f, ContentHeight() - mRECT.H());
    mScrollOffset = std::clamp(mScrollOffset, 0.f, maxScroll);
    mScrollTarget = std::clamp(mScrollTarget, 0.f, maxScroll);
  }

  // Ease the rendered offset toward mScrollTarget each frame for fluent scrolling.
  void StartScrollAnim()
  {
    // Snappy catch-up: ~0.45/frame settles a notch in ~6-7 frames (~110ms),
    // smooth but without the perceived lag of a long, slow glide.
    SetAnimation(
      [this](IControl* pCaller) {
        mScrollOffset += (mScrollTarget - mScrollOffset) * 0.45f;
        if (std::abs(mScrollTarget - mScrollOffset) < 0.4f)
        {
          mScrollOffset = mScrollTarget;
          pCaller->OnEndAnimation();
        }
        SetDirty(false);
      },
      350);
  }

  void ScrollToRevealCustom(int customIdx)
  {
    if (customIdx < 0 || customIdx >= (int)mCustomNames.size())
      return;
    const float itemTop = CustomRowsTopContent() + customIdx * ItemHeight();
    const float itemBot = itemTop + ItemHeight();
    if (itemTop < mScrollOffset)
      mScrollOffset = itemTop;
    else if (itemBot > mScrollOffset + mRECT.H())
      mScrollOffset = itemBot - mRECT.H();
    mScrollTarget = mScrollOffset;
    ClampScroll();
  }

  void ScrollToRevealFactory(int idx)
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
    mScrollTarget = mScrollOffset; // keep the eased target in sync with jumps
    ClampScroll();
  }

  // Content-space top (relative to mRECT.T, before scroll) of each section.
  float HeaderTopContent() const { return FactoryBlockH(); }
  float CustomRowsTopContent() const { return FactoryBlockH() + kHeaderH; }

  Hit HitTest(float x, float y)
  {
    const float rel = (y - mRECT.T) + mScrollOffset; // content space
    const float itemH = ItemHeight();

    if (rel < FactoryBlockH())
    {
      int idx = (int)(rel / itemH);
      if (idx >= 0 && idx < mNumAmps)
        return {EZone::Factory, idx};
      return {EZone::None, -1};
    }

    const float headerTop = HeaderTopContent();
    if (rel < headerTop + kHeaderH)
    {
      // Header row. Empty => whole row adds. Non-empty => only the + button adds.
      if (mCustomNames.empty())
        return {EZone::CustomAdd, -1};
      const float plusL = RowRight() - 26.f;
      if (x >= plusL)
        return {EZone::CustomAdd, -1};
      return {EZone::CustomHeader, -1};
    }

    const int cidx = (int)((rel - CustomRowsTopContent()) / itemH);
    if (cidx >= 0 && cidx < (int)mCustomNames.size())
    {
      const float binL = RowRight() - 24.f;
      const float penL = binL - 22.f;
      if (x >= binL)
        return {EZone::CustomDelete, cidx};
      if (x >= penL)
        return {EZone::CustomEdit, cidx};
      return {EZone::CustomRow, cidx};
    }
    return {EZone::None, -1};
  }

  void DrawCustomSection(IGraphics& g, float itemH, float rowR)
  {
    // Header
    const float headerTop = mRECT.T + HeaderTopContent() - mScrollOffset;
    const IRECT header(mRECT.L, headerTop, rowR, headerTop + kHeaderH);
    if (header.B >= mRECT.T - 1.f && header.T <= mRECT.B + 1.f)
    {
      g.DrawLine(VoLumColors::FRAME, header.L + 4.f, header.T, header.R - 4.f, header.T, nullptr, 1.f);
      // Same affordance whether or not the list has entries: a dim "CUSTOM"
      // label plus a teal + glyph on the right. Previously the empty state used
      // bare "+ CUSTOM" text, which read as pale/unfinished next to a populated
      // list. (Empty still adds on a whole-row click via HitTest.)
      const bool addHovered = (mHoveredZone == EZone::CustomAdd);
      const IRECT plus(header.R - 26.f, header.T, header.R - 6.f, header.B);
      if (addHovered)
      {
        // Empty -> the whole header row is the add target; populated -> only the
        // + button. Match the row hover-fill so every clickable zone reacts.
        const IRECT hoverR = mCustomNames.empty() ? header.GetPadded(-2.f, -1.f, -2.f, -1.f)
                                                   : plus.GetPadded(-2.f, -3.f, -1.f, -3.f);
        g.FillRoundRect(VoLumColors::ITEM_HOVER, hoverR, 4.f);
      }
      g.DrawText(IText(10.f, addHovered ? VoLumColors::CREAM : VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near,
                       EVAlign::Middle),
                 "CUSTOM", header.GetPadded(-8.f, 0.f, -8.f, 0.f));
      DrawPlusGlyph(g, plus, addHovered ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEAL);
    }

    // Custom rows
    for (int c = 0; c < (int)mCustomNames.size(); c++)
    {
      const float top = mRECT.T + CustomRowsTopContent() + c * itemH - mScrollOffset;
      const float bot = top + itemH;
      if (bot < mRECT.T - 1.f || top > mRECT.B + 1.f)
        continue;

      IRECT row(mRECT.L, top, rowR, bot);
      IRECT paddedRow = row.GetPadded(-2.f, -0.5f, -2.f, -0.5f);

      const bool selected = (c == mCustomSelected);
      const bool hovered = (mHoveredDomain == EDomain::Custom && c == mHovered);
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, paddedRow, 4.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, paddedRow, 4.f);
        g.FillRect(VoLumColors::GOLD, IRECT(paddedRow.L, paddedRow.T + 3.f, paddedRow.L + 2.5f, paddedRow.B - 3.f));
      }
      else if (hovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, paddedRow, 4.f);
      }

      // Assigned procedural art thumbnail (cached), matching the factory rows.
      const float isz = 20.f;
      const IRECT iconArea(
        paddedRow.L + 7.f, paddedRow.MH() - isz / 2.f, paddedRow.L + 7.f + isz, paddedRow.MH() + isz / 2.f);
      const int art = (c < (int)mCustomArts.size()) ? (mCustomArts[c] % volum::custom::kNumCustomArts) : 0;
      // DrawFittedLayer (not DrawFittedBitmap) so the art scales with the UI on
      // window resize - see the factory-row note above.
      if (mCustomArtLayers[art] && g.CheckLayer(mCustomArtLayers[art]))
        g.DrawFittedLayer(mCustomArtLayers[art], iconArea, nullptr);

      // Clip the name so a long custom-amp name can never reach the pen/trash.
      IRECT nameArea = paddedRow.GetReducedFromLeft(38.f).GetReducedFromRight(hovered || selected ? 48.f : 6.f);
      g.PathClipRegion(nameArea);
      g.DrawText(IText(13.f, selected ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Near,
                       EVAlign::Middle),
                 mCustomNames[c].c_str(), nameArea);
      g.PathClipRegion(mRECT); // restore the list clip for the next rows/scrollbar

      if (hovered || selected)
      {
        const IRECT bin(paddedRow.R - 22.f, paddedRow.T, paddedRow.R - 2.f, paddedRow.B);
        const IRECT pen(bin.L - 22.f, paddedRow.T, bin.L - 2.f, paddedRow.B);
        // Brighten the specific icon the cursor is over so the hover target is
        // obvious (tooltip alone was too subtle).
        const bool penHot = hovered && mHoveredZone == EZone::CustomEdit;
        const bool binHot = hovered && mHoveredZone == EZone::CustomDelete;
        DrawPenGlyph(g, pen, penHot ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM_DIM);
        DrawBinGlyph(g, bin, binHot ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM_DIM);
      }
    }
  }

  void DrawScrollbar(IGraphics& g, float contentH, float scrollbarW)
  {
    (void)scrollbarW;
    IRECT track, thumb;
    ScrollbarGeometry(contentH, track, thumb);
    g.FillRect(IColor(40, 200, 162, 78), track);
    g.FillRect(mDraggingScrollbar ? VoLumColors::GOLD : VoLumColors::GOLD_DIM, thumb);
  }

  static void DrawPlusGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH(), s = 6.f;
    g.DrawLine(col, cx - s, cy, cx + s, cy, nullptr, 1.6f);
    g.DrawLine(col, cx, cy - s, cx, cy + s, nullptr, 1.6f);
  }

  // A diagonal pencil: two parallel body lines, an end cap, and a converging
  // nib at the bottom-left so it reads as a pencil even at ~14px.
  static void DrawPenGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const float t = 1.4f;
    // body (two parallels, bottom-left -> top-right)
    g.DrawLine(col, cx - 5.f, cy + 4.f, cx + 3.f, cy - 4.f, nullptr, t);
    g.DrawLine(col, cx - 2.f, cy + 6.f, cx + 5.f, cy - 1.f, nullptr, t);
    // eraser end cap (top-right)
    g.DrawLine(col, cx + 3.f, cy - 4.f, cx + 5.f, cy - 1.f, nullptr, t);
    // nib converging to a point (bottom-left)
    g.DrawLine(col, cx - 5.f, cy + 4.f, cx - 6.f, cy + 7.f, nullptr, t);
    g.DrawLine(col, cx - 2.f, cy + 6.f, cx - 6.f, cy + 7.f, nullptr, t);
  }

  static void DrawBinGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const IRECT body(cx - 5.f, cy - 3.f, cx + 5.f, cy + 6.f);
    g.DrawRect(col, body, nullptr, 1.3f);
    g.DrawLine(col, cx - 7.f, cy - 3.f, cx + 7.f, cy - 3.f, nullptr, 1.3f); // lid
    g.DrawLine(col, cx - 2.f, cy - 6.f, cx + 2.f, cy - 6.f, nullptr, 1.3f); // handle
  }

  void DrawMiniFractal(IGraphics& g, const IRECT& r, int ampIdx, const IColor& bright, const IColor& dim)
  {
    DrawSidebarMiniFractal(g, r, FractalCaseForAmp(ampIdx), bright, dim);
  }

  int mNumAmps = 0;
  int mSelected = 0;
  int mHovered = -1;
  EDomain mHoveredDomain = EDomain::None;
  EZone mHoveredZone = EZone::None; // exact zone under the cursor (for pen/trash icon highlight)
  std::string mCurTip; // last tooltip pushed via SetTooltip (de-dupe)
  float mScrollOffset = 0.f;
  float mScrollTarget = 0.f;
  bool mDraggingScrollbar = false; // thumb drag in progress
  float mDragGrabDY = 0.f;         // cursor offset within thumb at grab time
  std::vector<std::string> mAmpNames;
  std::vector<std::string> mAmpAbbrs;
  SelectionCallback mCallback;
  std::vector<ILayerPtr> mIconLayers;

  // Custom section (1.2.0)
  std::vector<std::string> mCustomNames;
  std::vector<int> mCustomArts; // assigned fractal-art id per custom amp
  std::array<ILayerPtr, volum::custom::kNumCustomArts> mCustomArtLayers; // cached art thumbnails
  int mCustomSelected = -1;
  CustomSelectCallback mCustomSelectCb;
  AddCallback mCustomAddCb;
  CustomActionCallback mCustomEditCb;
  CustomActionCallback mCustomDeleteCb;
};
