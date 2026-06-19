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

#include <algorithm>
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

  void SetCustomAmps(const std::vector<std::string>& names)
  {
    mCustomNames = names;
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
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    const float itemH = ItemHeight();
    const float iconSize = 22.f;
    const float pad = 8.f;
    const float contentH = ContentHeight();
    const bool scrollable = contentH > mRECT.H() + 0.5f;
    const float scrollbarW = scrollable ? 5.f : 0.f;
    const float rowR = mRECT.R - scrollbarW;

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
        g.FillRect(VoLumColors::ITEM_SEL_BG, paddedRow);
        g.DrawRect(VoLumColors::ITEM_SEL_BORDER, paddedRow);
      }
      else if (i == mHovered && mHoveredDomain == EDomain::Factory)
      {
        g.FillRect(VoLumColors::ITEM_HOVER, paddedRow);
        g.DrawRect(IColor(20, 200, 162, 78), paddedRow);
      }

      IRECT iconArea(paddedRow.L + pad, paddedRow.MH() - iconSize / 2.f,
                     paddedRow.L + pad + iconSize, paddedRow.MH() + iconSize / 2.f);

      g.DrawRect(IColor(selected ? 80 : 58, 120, 195, 210), iconArea);
      // Blit the cached thumbnail at the row's CURRENT position. DrawLayer would
      // paint at the bounds the layer was first built with, which freezes
      // thumbnails in place once the list scrolls.
      if (mIconLayers[i] && g.CheckLayer(mIconLayers[i]))
        g.DrawFittedBitmap(mIconLayers[i]->GetBitmap(), iconArea);

      IRECT nameArea = paddedRow.GetReducedFromLeft(pad + iconSize + 8.f);
      IColor nameCol = selected ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
      IText nameText(13.f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
      g.DrawText(nameText, mAmpNames[i].c_str(), nameArea);
    }

    DrawCustomSection(g, itemH, rowR);

    g.PathClipRegion();

    if (scrollable)
      DrawScrollbar(g, contentH, scrollbarW);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    ClearVoLumKnobSelection(this);

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

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
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
    if (idx != mHovered || dom != mHoveredDomain)
    {
      mHovered = idx;
      mHoveredDomain = dom;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    mHovered = -1;
    mHoveredDomain = EDomain::None;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    (void) mod;
    const float contentH = ContentHeight();
    if (contentH <= mRECT.H())
      return;

    mScrollOffset -= d * ItemHeight();
    ClampScroll();

    OnMouseOver(x, y, mod);
    SetDirty(false);
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

  enum class EDomain { None, Factory, Custom };
  enum class EZone { None, Factory, CustomHeader, CustomAdd, CustomRow, CustomEdit, CustomDelete };

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
      const float scrollbarW = (ContentHeight() > mRECT.H() + 0.5f) ? 5.f : 0.f;
      const float plusL = mRECT.R - scrollbarW - 26.f;
      if (x >= plusL)
        return {EZone::CustomAdd, -1};
      return {EZone::CustomHeader, -1};
    }

    const int cidx = (int)((rel - CustomRowsTopContent()) / itemH);
    if (cidx >= 0 && cidx < (int)mCustomNames.size())
    {
      const float scrollbarW = (ContentHeight() > mRECT.H() + 0.5f) ? 5.f : 0.f;
      const float binL = mRECT.R - scrollbarW - 24.f;
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
      if (mCustomNames.empty())
      {
        g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "+ CUSTOM",
                   header.GetPadded(-8.f, 0.f, -8.f, 0.f));
      }
      else
      {
        g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "CUSTOM",
                   header.GetPadded(-8.f, 0.f, -8.f, 0.f));
        // + button at the right
        const IRECT plus(header.R - 26.f, header.T, header.R - 6.f, header.B);
        DrawPlusGlyph(g, plus, VoLumColors::TEAL);
      }
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
        g.FillRect(VoLumColors::ITEM_SEL_BG, paddedRow);
        g.DrawRect(VoLumColors::ITEM_SEL_BORDER, paddedRow);
      }
      else if (hovered)
      {
        g.FillRect(VoLumColors::ITEM_HOVER, paddedRow);
        g.DrawRect(IColor(20, 200, 162, 78), paddedRow);
      }

      // small diamond marker instead of a fractal icon
      DrawDiamond(g, paddedRow.L + 19.f, paddedRow.MH(), 7.f, selected ? VoLumColors::GOLD : VoLumColors::TEAL);

      IRECT nameArea = paddedRow.GetReducedFromLeft(38.f).GetReducedFromRight(hovered || selected ? 48.f : 6.f);
      g.DrawText(IText(13.f, selected ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Near,
                       EVAlign::Middle),
                 mCustomNames[c].c_str(), nameArea);

      if (hovered || selected)
      {
        const IRECT bin(paddedRow.R - 22.f, paddedRow.T, paddedRow.R - 2.f, paddedRow.B);
        const IRECT pen(bin.L - 22.f, paddedRow.T, bin.L - 2.f, paddedRow.B);
        DrawPenGlyph(g, pen, VoLumColors::CREAM_DIM);
        DrawBinGlyph(g, bin, VoLumColors::CREAM_DIM);
      }
    }
  }

  void DrawScrollbar(IGraphics& g, float contentH, float scrollbarW)
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

  static void DrawPlusGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH(), s = 6.f;
    g.DrawLine(col, cx - s, cy, cx + s, cy, nullptr, 1.6f);
    g.DrawLine(col, cx, cy - s, cx, cy + s, nullptr, 1.6f);
  }

  static void DrawPenGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float x0 = r.MW() - 5.f, y0 = r.MH() + 5.f, x1 = r.MW() + 5.f, y1 = r.MH() - 5.f;
    g.DrawLine(col, x0, y0, x1, y1, nullptr, 1.4f); // pen body
    g.DrawLine(col, x0, y0, x0 + 3.f, y0 - 1.f, nullptr, 1.4f); // nib
    g.DrawLine(col, x0, y0, x0 + 1.f, y0 - 3.f, nullptr, 1.4f);
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
  float mScrollOffset = 0.f;
  std::vector<std::string> mAmpNames;
  std::vector<std::string> mAmpAbbrs;
  SelectionCallback mCallback;
  std::vector<ILayerPtr> mIconLayers;

  // Custom section (1.2.0)
  std::vector<std::string> mCustomNames;
  int mCustomSelected = -1;
  CustomSelectCallback mCustomSelectCb;
  AddCallback mCustomAddCb;
  CustomActionCallback mCustomEditCb;
  CustomActionCallback mCustomDeleteCb;
};
