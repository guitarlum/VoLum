#pragma once

// VoLum left-sidebar amp picker (VoLumAmpListControl).
//
// Vertically scrollable list of 15 bundled amp profiles with cached SVG icons,
// hover lift, and click-to-select dispatch back to the plugin. Extracted from
// VoLumCoreControls.h on the 1.0-bugs-hygiene branch (file-size hygiene split).
// Source-string lock for the layer-cache idiom (`if (!g.CheckLayer(mIconLayers[i]))`)
// lives in test_volum_ui_regressions.cpp and reads this file.

#include "VoLumColorHelpers.h"

#include <algorithm>
#include <cstdlib>
#include <functional>

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