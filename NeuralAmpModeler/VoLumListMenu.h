#pragma once

// VoLumListMenuControl: reusable anchored dropdown for managed lists.
// Extracted from VoLumCustomUi.h for file-size hygiene.

#include "VoLumColorHelpers.h"
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

  // Attached at full-window bounds so a click anywhere outside the menu can
  // dismiss it (the menu paints only into mMenuRect). Position the visible menu
  // with SetMenuRect before un-hiding.
  explicit VoLumListMenuControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallback(SelectCallback cb) { mCb = std::move(cb); }

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
        const IRECT tick(row.L, row.MH(), row.L + 10.f, row.MH() + 1.f);
        g.FillRect(VoLumColors::AMBER.WithOpacity(0.75f), tick);
        g.DrawText(IText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), r.label.c_str(),
                   IRECT(row.L + 14.f, row.T, row.R, row.B));
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
      const float trackX = mMenuRect.R - sbW - 1.f;
      IRECT track(trackX, mMenuRect.T + 4.f, mMenuRect.R - 2.f, mMenuRect.B - 4.f);
      g.FillRect(IColor(40, 200, 162, 78), track);
      const float maxScroll = ContentH() - mMenuRect.H();
      const float thumbH = std::max(18.f, track.H() * (mMenuRect.H() / ContentH()));
      const float t = (maxScroll > 0.f) ? (mScroll / maxScroll) : 0.f;
      const float thumbY = track.T + (track.H() - thumbH) * t;
      g.FillRect(VoLumColors::GOLD_DIM, IRECT(track.L, thumbY, track.R, thumbY + thumbH));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!mMenuRect.Contains(x, y))
    {
      Hide(true); // click-outside dismisses
      return;
    }
    const int idx = RowAtY(y);
    if (idx >= 0 && idx < (int)mRows.size() && !mRows[(size_t)idx].dim && !mRows[(size_t)idx].header && mCb)
      mCb(mRows[(size_t)idx].code);
    Hide(true);
  }

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
};

