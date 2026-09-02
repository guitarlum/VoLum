#pragma once

// VoLumListMenuControl: reusable anchored dropdown for managed lists.
// Extracted from VoLumCustomUi.h for file-size hygiene.

#include "VoLumColorHelpers.h"
#include "VoLumScroll.h"
#include "VoLumCustomContentApi.h"
#include "VoLumFractalArt.h"
#include "VoLumIrFileGuard.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Reusable anchored dropdown for the managed lists (presets, custom IR cabs).
// Rows are built by the plugin so one control serves several menus; selecting a
// row fires the callback with that row's code. Special codes by convention:
//   >= 0  : pick item at this index
//   kNone : "no selection" row (e.g. IR -> use baked cab)
//   kManage: open the shared Manage panel
// Action rows (Manage) render teal and never show the selection dot.
// ---------------------------------------------------------------------------
class VoLumListMenuControl : public IControl
{
public:
  static constexpr int kNone = -3;
  static constexpr int kManage = -2;
  static constexpr int kDefault = -4; // reset amp to factory defaults (preset menu)
  static constexpr int kOverwrite = -5; // overwrite the active dirty preset (preset menu)
  static constexpr int kSaveAsNew = -6; // save the current dirty rig as a new preset (preset menu)

  struct Row
  {
    std::string label;
    int code = 0;
    bool action = false; // teal action row (Manage / Default)
    bool dim = false; // non-interactive plain hint row (e.g. "No presets yet")
    bool dividerBelow = false; // draw a separator under this row (e.g. pinned Default)
    bool group = false; // item belongs to a group (e.g. CUSTOM) -> left accent bar + indent
    bool header = false; // group header row (amber tick + label, PRE-pedal style)
  };

  using SelectCallback = std::function<void(int code)>;
  using HeaderCallback = std::function<void(int code)>;

  // Attached at full-window bounds so a click anywhere outside the menu can
  // dismiss it (the menu paints only into mMenuRect). Position the visible menu
  // with SetMenuRect before un-hiding.
  explicit VoLumListMenuControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallback(SelectCallback cb) { mCb = std::move(cb); }
  void SetHeaderCallback(HeaderCallback cb) { mHeaderCb = std::move(cb); }

  void SetRows(const std::vector<Row>& rows, int selectedCode)
  {
    mRows = rows;
    mSelectedCode = selectedCode;
    mHovered = -1;
    mScroll = 0.f;
    SetDirty(false);
  }

  // Anchor the visible menu panel. Caller caps the height to the screen.
  void SetMenuRect(const IRECT& r)
  {
    mMenuRect = r;
    mScroll = 0.f;
  }

  static constexpr float kRowH = 22.f;
  static float MenuHeight(size_t rowCount) { return 12.f + (float)rowCount * kRowH; }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mMenuRect, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mMenuRect, 4.f, nullptr, 1.5f);
    DrawCornerAccent(g, mMenuRect.L + 5.f, mMenuRect.T + 5.f, 8.f, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, mMenuRect.R - 5.f, mMenuRect.B - 5.f, 8.f, true, true, VoLumColors::TEAL_DIM);

    const bool scrollable = ContentH() > mMenuRect.H() + 0.5f;
    const float sbW = scrollable ? 5.f : 0.f;
    ClampScroll();

    g.PathClipRegion(mMenuRect);
    float rowT = mMenuRect.T + 6.f - mScroll;
    for (int i = 0; i < (int)mRows.size(); i++)
    {
      const auto& r = mRows[(size_t)i];
      const IRECT row(mMenuRect.L + 8.f, rowT, mMenuRect.R - 8.f - sbW, rowT + kRowH);
      rowT += kRowH;
      if (row.B < mMenuRect.T || row.T > mMenuRect.B)
        continue;

      // Group header (e.g. "CUSTOM"): styled like the PRE-pedal menu - a short
      // amber accent tick + small amber label, no selection affordance.
      if (r.header)
      {
        const bool disclosure = !r.label.empty() && (r.label[0] == '+' || r.label[0] == '-');
        if (i == mHovered)
          g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        if (!disclosure)
        {
          const IRECT tick(row.L, row.MH(), row.L + 10.f, row.MH() + 1.f);
          g.FillRect(VoLumColors::AMBER.WithOpacity(0.75f), tick);
        }
        g.DrawText(IText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), r.label.c_str(),
                   IRECT(row.L + (disclosure ? 2.f : 14.f), row.T, row.R, row.B));
        if (r.dividerBelow)
          g.DrawLine(VoLumColors::FRAME, row.L, row.B, row.R, row.B, nullptr, 1.f);
        continue;
      }

      // Visual divider above an action (Manage) row.
      if (r.action && i > 0)
        g.DrawLine(VoLumColors::FRAME, row.L, row.T, row.R, row.T, nullptr, 1.f);

      const bool selected = (r.code == mSelectedCode) && !r.action && !r.dim;
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      else if (i == mHovered && !r.dim)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      // Grouped items (CUSTOM) get a teal left accent bar, mirroring the PRE-pedal
      // dropdown so factory vs custom read the same in both menus.
      if (r.group && !r.dim)
        g.FillRoundRect(VoLumColors::TEAL.WithOpacity(0.55f), IRECT(row.L, row.T + 4.f, row.L + 4.f, row.B - 4.f), 2.f);
      if (selected)
        g.FillCircle(VoLumColors::TEAL, row.L + 8.f, row.MH(), 3.f);
      const IColor col = r.dim
                           ? VoLumColors::CREAM_DIM
                           : (r.action ? VoLumColors::TEAL : (selected ? VoLumColors::CREAM : VoLumColors::CREAM_DIM));
      const float textL = row.L + (selected ? 18.f : (r.group ? 14.f : 12.f));
      g.DrawText(IText(12.f, col, "Josefin-Bold", EAlign::Near, EVAlign::Middle), r.label.c_str(),
                 IRECT(textL, row.T, row.R, row.B));
      if (r.dividerBelow)
        g.DrawLine(VoLumColors::FRAME, row.L, row.B, row.R, row.B, nullptr, 1.f);
    }
    g.PathClipRegion();

    if (scrollable)
    {
      const auto m = ScrollMetricsNow();
      const IRECT track(mMenuRect.R - sbW - 1.f, mMenuRect.T + 4.f, mMenuRect.R - 2.f, mMenuRect.B - 4.f);
      DrawVoLumScrollbar(g, track, IRECT(track.L, m.thumbY, track.R, m.thumbY + m.thumbH), mBar.dragging);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!mMenuRect.Contains(x, y))
    {
      Hide(true); // click-outside dismisses
      return;
    }
    const bool scrollable = ContentH() > mMenuRect.H() + 0.5f;
    if (scrollable)
    {
      const auto m = ScrollMetricsNow();
      const float trackL = mMenuRect.R - 6.f;
      if (mBar.OnDown(x, y, trackL, mMenuRect.R - 2.f, m))
      {
        mScroll = volum::scroll::ThumbYToScroll(y - mBar.grabDY, m.trackTop, m.trackH, m.thumbH, m.maxScroll);
        SetDirty(false);
        return;
      }
    }
    const int idx = RowAtY(y);
    if (idx >= 0 && idx < (int)mRows.size() && mRows[(size_t)idx].header)
    {
      if (mHeaderCb)
        mHeaderCb(mRows[(size_t)idx].code);
      return;
    }
    if (idx >= 0 && idx < (int)mRows.size() && !mRows[(size_t)idx].dim && !mRows[(size_t)idx].header && mCb)
      mCb(mRows[(size_t)idx].code);
    Hide(true);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (!mBar.dragging)
      return;
    const auto m = ScrollMetricsNow();
    const float next = mBar.OnDrag(y, m);
    if (next >= 0.f)
      mScroll = next;
    SetDirty(false);
    (void)x;
  }

  void OnMouseUp(float, float, const IMouseMod&) override { mBar.OnUp(); }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const int idx = mMenuRect.Contains(x, y) ? RowAtY(y) : -1;
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

  void OnMouseWheel(float, float, const IMouseMod&, float d) override
  {
    if (ContentH() <= mMenuRect.H() + 0.5f)
      return;
    mScroll -= d * kRowH * 1.5f;
    ClampScroll();
    SetDirty(false);
  }

private:
  float ContentH() const { return 12.f + (float)mRows.size() * kRowH; }

  volum::scroll::ScrollMetrics ScrollMetricsNow() const
  {
    return volum::scroll::ComputeScroll(mMenuRect.T + 4.f, mMenuRect.B - 4.f, mMenuRect.H(), ContentH(), mScroll);
  }

  void ClampScroll()
  {
    const float maxScroll = std::max(0.f, ContentH() - mMenuRect.H());
    mScroll = std::clamp(mScroll, 0.f, maxScroll);
  }

  int RowAtY(float y) const
  {
    const int idx = (int)((y - (mMenuRect.T + 6.f) + mScroll) / kRowH);
    return (idx >= 0 && idx < (int)mRows.size()) ? idx : -1;
  }

  IRECT mMenuRect;
  std::vector<Row> mRows;
  int mSelectedCode = kNone;
  int mHovered = -1;
  float mScroll = 0.f;
  SelectCallback mCb;
  HeaderCallback mHeaderCb;
  volum::scroll::Interaction mBar;
};
