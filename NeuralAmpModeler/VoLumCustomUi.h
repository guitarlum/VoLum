#pragma once

// VoLum 1.2.0 BYO + presets UI shells (unified dropdown + Manage design).
//
// Controls, rendering against the in-memory VoLumCustomContentMock:
//   - VoLumPresetBarControl    : F5 preset strip centred in the top header
//                                (prev/name/next + opens the preset dropdown).
//   - VoLumListMenuControl     : reusable anchored dropdown (item list + a single
//                                "Manage..." action). Used for presets and IR cabs.
//   - VoLumCustomOverlayControl: one full-window overlay with two screens -
//       * Manage  : a shared CRUD list for presets / custom IRs / custom pedals
//                   (Rename / Delete + add: Save-as-new/Update for presets, or
//                   Import via OS file dialog for IRs/pedals).
//       * Builder : file-first custom amp create/edit (DIRECT + 3 stock cabs,
//                   numbered channels) with a live (speaker x channel) grid.
//
// The PRE pedal dropdown (custom pedals + "Manage custom pedals...") lives in
// VoLumTriptychMenus.h. This is the UI-shell phase: navigation, layout, and the
// live coverage are real; load/save/import are stubs.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentMock.h"
#include "VoLumFractalArt.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// F5: preset bar (AMP header strip)
// ---------------------------------------------------------------------------
class VoLumPresetBarControl : public IControl
{
public:
  using OpenCallback = std::function<void()>;

  VoLumPresetBarControl(const IRECT& bounds, OpenCallback openCb)
  : IControl(bounds)
  , mOpen(std::move(openCb))
  {
  }

  // Set the active amp's preset bank (mock). Empty list => "(unsaved)" + inert arrows.
  void SetList(const std::vector<std::string>& names)
  {
    mList = names;
    mIdx = -1;
    mName.clear();
    mEmpty = true;
    mDirtyEdit = false;
    SetDirty(false);
  }

  // Mark a preset (by name) as the active one, e.g. after a recall from the
  // browser. A fresh recall is clean (matches the stored snapshot).
  void SelectName(const char* name)
  {
    mName = name ? name : "";
    mEmpty = mName.empty();
    mDirtyEdit = false;
    mIdx = -1;
    for (int i = 0; i < (int)mList.size(); i++)
      if (mList[(size_t)i] == mName)
        mIdx = i;
    SetDirty(false);
  }

  void SetName(const char* name)
  {
    mName = name ? name : "";
    SetDirty(false);
  }
  void SetEmpty(bool empty)
  {
    mEmpty = empty;
    SetDirty(false);
  }

  // The live signal chain diverged from (or matches) the recalled snapshot.
  // Drives the "(unsaved)" suffix. Cleared on recall / save.
  void SetDirtyState(bool dirty)
  {
    if (mDirtyEdit != dirty)
    {
      mDirtyEdit = dirty;
      SetDirty(false);
    }
  }

  // Active preset index in the current bank, or -1 when none is selected.
  int ActiveIndex() const { return mIdx; }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mMouseIsOver ? VoLumColors::ITEM_SEL_BG : VoLumColors::BTN_OFF_BG, mRECT);
    g.DrawRect(VoLumColors::FRAME, mRECT);

    const IText arrow(15.f, mList.empty() ? VoLumColors::CREAM_DIM : VoLumColors::GOLD, "Josefin-Bold", EAlign::Center,
                      EVAlign::Middle);
    g.DrawText(arrow, "<", PrevRect());
    g.DrawText(arrow, ">", NextRect());

    // No caret / no "PRESET" caption: only the two cycle arrows reserve side
    // space. Empty -> "No Preset". Clean -> name. Dirty -> "<name> (unsaved)".
    const IRECT mid = mRECT.GetReducedFromLeft(22.f).GetReducedFromRight(22.f);
    std::string label;
    IColor col;
    if (mEmpty || mName.empty())
    {
      label = "No Preset";
      col = VoLumColors::CREAM_DIM;
    }
    else if (mDirtyEdit)
    {
      label = mName + "  (unsaved)";
      col = VoLumColors::AMBER;
    }
    else
    {
      label = mName;
      col = VoLumColors::TEXT_BRIGHT;
    }
    // Clip to the centre area so a long name can never spill over the arrows.
    const IRECT nameR = mid.GetPadded(-2.f, 0.f, -2.f, 0.f);
    g.PathClipRegion(nameR);
    g.DrawText(IText(13.f, col, "Josefin-Bold", EAlign::Center, EVAlign::Middle), label.c_str(), nameR);
    g.PathClipRegion();
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!mList.empty() && PrevRect().Contains(x, y))
    {
      Step(-1);
      return;
    }
    if (!mList.empty() && NextRect().Contains(x, y))
    {
      Step(1);
      return;
    }
    if (mOpen)
      mOpen();
  }

  void OnMouseOver(float, float, const IMouseMod&) override
  {
    mMouseIsOver = true;
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    SetDirty(false);
  }

private:
  void Step(int dir)
  {
    if (mList.empty())
      return;
    const int n = (int)mList.size();
    mIdx = ((mIdx < 0 ? 0 : mIdx) + dir % n + n) % n;
    mName = mList[(size_t)mIdx];
    mEmpty = false;
    mDirtyEdit = false; // cycling to a stored preset is a clean recall
    SetDirty(false);
  }

  IRECT PrevRect() const { return mRECT.GetFromLeft(22.f); }
  IRECT NextRect() const { return mRECT.GetFromRight(22.f); }

  std::string mName;
  bool mEmpty = true;
  bool mDirtyEdit = false;
  std::vector<std::string> mList;
  int mIdx = -1;
  OpenCallback mOpen;
};

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

  struct Row
  {
    std::string label;
    int code = 0;
    bool action = false; // teal action row (Manage / Default)
    bool dim = false; // non-interactive hint row
    bool dividerBelow = false; // draw a separator under this row (e.g. pinned Default)
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
      if (selected)
        g.FillCircle(VoLumColors::TEAL, row.L + 8.f, row.MH(), 3.f);
      const IColor col = r.dim ? VoLumColors::CREAM_DIM
                               : (r.action ? VoLumColors::TEAL : (selected ? VoLumColors::CREAM : VoLumColors::CREAM_DIM));
      g.DrawText(IText(12.f, col, "Josefin-Bold", EAlign::Near, EVAlign::Middle), r.label.c_str(),
                 IRECT(row.L + (selected ? 18.f : 12.f), row.T, row.R, row.B));
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
    if (idx >= 0 && idx < (int)mRows.size() && !mRows[(size_t)idx].dim && mCb)
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

// ---------------------------------------------------------------------------
// Shared "Are you sure?" confirmation modal, used for every destructive delete
// (Manage panel + sidebar trash). Attached full-window above other surfaces;
// click-outside or Cancel dismisses, Delete runs the stored callback.
// ---------------------------------------------------------------------------
class VoLumConfirmDialogControl : public IControl
{
public:
  explicit VoLumConfirmDialogControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  void Show(const std::string& title, const std::string& message, std::function<void()> onConfirm)
  {
    mTitle = title;
    mMessage = message;
    mOnConfirm = std::move(onConfirm);
    Hide(false);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(190, 8, 10, 14), mRECT); // dimming scrim
    const IRECT box = BoxRect();
    g.FillRoundRect(IColor(255, 26, 26, 34), box, 6.f);
    g.DrawRoundRect(VoLumColors::AMBER, box, 6.f, nullptr, 1.6f);

    g.DrawText(IText(15.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top), mTitle.c_str(),
               box.GetPadded(-16.f).GetFromTop(22.f));
    const IRECT msgR = box.GetPadded(-16.f, -38.f, -16.f, -52.f);
    g.PathClipRegion(msgR);
    g.DrawText(IText(11.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Center, EVAlign::Middle), mMessage.c_str(), msgR);
    g.PathClipRegion();

    DrawBtn(g, CancelRect(), "Cancel", false);
    DrawBtn(g, DeleteRect(), "Delete", true);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (DeleteRect().Contains(x, y))
    {
      auto cb = mOnConfirm;
      Hide(true);
      if (cb)
        cb();
      return;
    }
    // Cancel button or any click outside the box dismisses without acting.
    if (CancelRect().Contains(x, y) || !BoxRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
  }

private:
  IRECT BoxRect() const
  {
    const float w = 444.f, h = 150.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }
  IRECT CancelRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.L + 18.f, box.B - 42.f, box.MW() - 6.f, box.B - 14.f);
  }
  IRECT DeleteRect() const
  {
    const IRECT box = BoxRect();
    return IRECT(box.MW() + 6.f, box.B - 42.f, box.R - 18.f, box.B - 14.f);
  }
  void DrawBtn(IGraphics& g, const IRECT& r, const char* label, bool danger)
  {
    g.FillRect(danger ? IColor(70, 232, 130, 92) : VoLumColors::BTN_OFF_BG, r);
    g.DrawRect(danger ? VoLumColors::AMBER : VoLumColors::FRAME, r);
    g.DrawText(IText(12.f, danger ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               label, r);
  }

  std::string mTitle, mMessage;
  std::function<void()> mOnConfirm;
};

// ---------------------------------------------------------------------------
// Overlay: Presets + Builder
// ---------------------------------------------------------------------------
class VoLumCustomOverlayControl : public IControl
{
public:
  // Which managed list the (shared) Manage screen is editing.
  enum class ManageKind
  {
    Presets,
    IR,
    Pedals
  };

  using BuilderSavedCallback = std::function<void(const volum::custom::CustomAmp& amp)>;
  using ChangedCallback = std::function<void()>; // a managed list was mutated

  explicit VoLumCustomOverlayControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
    mEntryText = IText(14.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Near, EVAlign::Middle, 0.f,
                       IColor(245, 14, 16, 22), VoLumColors::TEXT_BRIGHT);
  }

  // changedCb fires whenever a managed list is mutated so the host can re-sync
  // dependent UI (e.g. the preset header bar).
  // Routes a destructive delete through the shared confirm modal:
  //   confirmCb(message, onConfirm) -> shows "Are you sure?"; onConfirm runs the
  //   actual delete if the user accepts.
  using ConfirmDeleteCallback = std::function<void(const std::string& message, std::function<void()> onConfirm)>;

  // Primary action for a double-clicked Manage row (recall preset / select IR /
  // load pedal). pedalSlot is the originating PRE NAM slot for Pedals (else -1).
  using PrimaryActionCallback =
    std::function<void(ManageKind kind, int ampIdx, int pedalSlot, int index)>;

  void SetCallbacks(BuilderSavedCallback builderCb, ChangedCallback changedCb = nullptr)
  {
    mBuilderSaved = std::move(builderCb);
    mChanged = std::move(changedCb);
  }

  void SetConfirmDeleteCallback(ConfirmDeleteCallback cb) { mConfirmDelete = std::move(cb); }
  void SetPrimaryActionCallback(PrimaryActionCallback cb) { mPrimaryAction = std::move(cb); }

  // ampIdx/ampName only matter for Presets; pedalSlot only for Pedals.
  void ShowManage(ManageKind kind, int ampIdx = 0, const char* ampName = nullptr, int pedalSlot = -1)
  {
    mScreen = volum::custom::Screen::Presets; // "Presets" enum == the Manage list screen
    mManageKind = kind;
    mAmpIdx = ampIdx;
    mAmpName = ampName ? ampName : "";
    mPedalSlot = pedalSlot;
    ReloadList();
    mSel = -1;
    mManageScroll = 0.f;
    ResetTransient();
    Hide(false);
    SetDirty(false);
  }

  void ShowBuilder(bool editExisting, const char* ampName)
  {
    mScreen = volum::custom::Screen::Builder;
    mBuilderAmp = editExisting ? volum::custom::MockDemoCustomAmp() : NewBuilderAmp(ampName);
    ResetTransient();
    Hide(false);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    mHotspots.clear();
    mPopupHotspots.clear();
    g.FillRect(IColor(185, 8, 10, 14), mRECT);

    const IRECT panel = PanelRect();
    g.FillRect(IColor(255, 22, 22, 30), panel);
    g.DrawRect(VoLumColors::FRAME, panel);
    g.DrawRect(IColor(90, 200, 180, 100), panel.GetPadded(2.f));
    const float cs = 18.f, m = 8.f;
    DrawCornerAccent(g, panel.L + m, panel.T + m, cs, false, false, VoLumColors::CORNER);
    DrawCornerAccent(g, panel.R - m, panel.T + m, cs, true, false, VoLumColors::CORNER);
    DrawCornerAccent(g, panel.L + m, panel.B - m, cs, false, true, VoLumColors::CORNER);
    DrawCornerAccent(g, panel.R - m, panel.B - m, cs, true, true, VoLumColors::CORNER);

    const IRECT head = panel.GetPadded(-20.f, -16.f, -20.f, 0.f).GetFromTop(34.f);
    g.DrawText(IText(18.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), TitleForScreen(), head);
    const IRECT closeR(panel.R - 36.f, panel.T + 12.f, panel.R - 12.f, panel.T + 36.f);
    DrawClose(g, closeR);
    AddHotspot(closeR, kClose);

    const IRECT body = panel.GetPadded(-22.f, -56.f, -22.f, -18.f);
    if (mScreen == volum::custom::Screen::Presets)
      DrawManage(g, body);
    else if (mScreen == volum::custom::Screen::Builder)
      DrawBuilder(g, body);

    if (mPopupOpen)
      DrawPopup(g);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (mPopupOpen)
    {
      for (const auto& hs : mPopupHotspots)
        if (hs.first.Contains(x, y))
        {
          HandlePopup(hs.second);
          return;
        }
      mPopupOpen = false; // click elsewhere closes the popup
      SetDirty(false);
      return;
    }
    if (!PanelRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
    for (const auto& hs : mHotspots)
      if (hs.first.Contains(x, y))
      {
        HandleAction(hs.second, hs.first);
        return;
      }
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    // Scroll the Manage list. Builder has no scroll; popups consume their own wheel.
    if (mScreen == volum::custom::Screen::Builder || mPopupOpen || !PanelRect().Contains(x, y))
      return;
    mManageScroll = std::max(0.f, mManageScroll - d * 38.f);
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    // Double-clicking a Manage row runs its primary action (recall preset /
    // select IR / load pedal) and closes. Icons behave like a single click.
    if (mPopupOpen || mScreen == volum::custom::Screen::Builder)
    {
      OnMouseDown(x, y, mod);
      return;
    }
    if (!PanelRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
    for (const auto& hs : mHotspots)
      if (hs.first.Contains(x, y))
      {
        const int code = hs.second;
        if (code >= kRowBase && code < kRowBase + 256)
        {
          const int idx = code - kRowBase;
          if (mPrimaryAction && idx >= 0 && idx < (int)mItems.size())
            mPrimaryAction(mManageKind, mAmpIdx, mPedalSlot, idx);
          Hide(true);
          return;
        }
        HandleAction(code, hs.first);
        return;
      }
  }

  void OnTextEntryCompletion(const char* str, int) override
  {
    using namespace volum::custom;
    const std::string s = str ? str : "";
    switch (mTextTarget)
    {
      case TextTarget::NewItem: // presets only (IR/pedals add via file dialog)
        if (!s.empty() && mManageKind == ManageKind::Presets)
        {
          const int i = AddPreset(mAmpIdx, s);
          ReloadList();
          mSel = i;
          NotifyChanged();
        }
        break;
      case TextTarget::RenameItem:
        if (mSel >= 0)
        {
          ApplyRename(mSel, s);
          ReloadList();
          NotifyChanged();
        }
        break;
      case TextTarget::ProfileName:
        if (!s.empty())
          mBuilderAmp.name = s;
        break;
      case TextTarget::CabName:
        if (mTextCabSlot >= 0 && mTextCabSlot < kNumCabSlots)
        {
          const std::string norm = NormalizeCabName(s);
          if (!norm.empty())
            mBuilderAmp.cabNames[(size_t)mTextCabSlot] = norm;
        }
        break;
      default:
        break;
    }
    mTextTarget = TextTarget::None;
    mTextFileIdx = -1;
    mTextCabSlot = -1;
    SetDirty(false);
  }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    if (hide)
      ResetTransient();
  }

private:
  enum Action
  {
    kClose = 0,
    kAdd, // Save current as new (presets) / Import (IR, pedals)
    kUpdate, // presets only
    kRename,
    kDelete,
    kAddFile,
    kBuilderSave,
    kEditName,
    kCabNameBase = 70, // [kCabNameBase, +kNumCabSlots) cab-slot rename chips
    kArtBase = 80, // [kArtBase, kArtBase + kNumCustomArts) art swatch picks
    kRowBase = 100, // Manage row body (select / double-click primary action)
    kFileSpeakerBase = 200,
    kFileChannelBase = 300,
    kFileRemoveBase = 400,
    kRowOverwriteBase = 500, // Manage inline [overwrite] icon (presets only)
    kRowRenameBase = 600, // Manage inline [pen] icon
    kRowDeleteBase = 700, // Manage inline [trash] icon
    kPopupBase = 1000
  };

  enum class TextTarget
  {
    None,
    NewItem,
    RenameItem,
    ProfileName,
    CabName
  };

  enum class PopupKind
  {
    Speaker,
    Channel
  };

  IRECT PanelRect() const
  {
    // The Manage list is a compact dedicated panel; the builder keeps its larger
    // two-pane size.
    const bool builder = (mScreen == volum::custom::Screen::Builder);
    const float w = builder ? 780.f : 560.f;
    const float h = builder ? 524.f : 430.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }

  const char* TitleForScreen() const
  {
    if (mScreen == volum::custom::Screen::Builder)
      return "Custom amp";
    switch (mManageKind)
    {
      case ManageKind::IR: return "Manage custom IRs";
      case ManageKind::Pedals: return "Manage custom pedals";
      default: return "Manage presets";
    }
  }

  void AddHotspot(const IRECT& r, int action) { mHotspots.emplace_back(r, action); }

  void ResetTransient()
  {
    mPopupOpen = false;
    mTextTarget = TextTarget::None;
    mTextFileIdx = -1;
  }

  void ReloadList()
  {
    switch (mManageKind)
    {
      case ManageKind::IR: mItems = volum::custom::MockIRLibrary(); break;
      case ManageKind::Pedals: mItems = volum::custom::MockCustomPedals(); break;
      default: mItems = volum::custom::MockPresetsForAmp(mAmpIdx); break;
    }
    if (mSel >= (int)mItems.size())
      mSel = mItems.empty() ? -1 : (int)mItems.size() - 1;
  }

  void ApplyRename(int idx, const std::string& name)
  {
    using namespace volum::custom;
    switch (mManageKind)
    {
      case ManageKind::IR: RenameIR(idx, name); break;
      case ManageKind::Pedals: RenamePedal(idx, name); break;
      default: RenamePreset(mAmpIdx, idx, name); break;
    }
  }

  void ApplyDelete(int idx)
  {
    using namespace volum::custom;
    switch (mManageKind)
    {
      case ManageKind::IR: DeleteIR(idx); break;
      case ManageKind::Pedals: DeletePedal(idx); break;
      default: DeletePreset(mAmpIdx, idx); break;
    }
  }

  void NotifyChanged()
  {
    if (mChanged)
      mChanged();
  }

  static std::string BaseName(const char* full)
  {
    std::string s = full ? full : "";
    const size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos)
      s = s.substr(slash + 1);
    const size_t dot = s.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
      s = s.substr(0, dot);
    return s;
  }

  // IR/pedal "add" = OS file picker (.wav / .nam). The chosen filename seeds the
  // mock entry name; real loading lands with the backend.
  void StartImport()
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    WDL_String fileName, path;
    const char* ext = (mManageKind == ManageKind::IR) ? "wav" : "nam";
    ui->PromptForFile(fileName, path, EFileAction::Open, ext,
                      [this](const WDL_String& fn, const WDL_String&) {
                        if (!fn.GetLength())
                          return;
                        const std::string base = BaseName(fn.Get());
                        if (mManageKind == ManageKind::IR)
                          volum::custom::AddIR(base);
                        else
                          volum::custom::AddPedal(base);
                        ReloadList();
                        mSel = (int)mItems.size() - 1;
                        NotifyChanged();
                        SetDirty(false);
                      });
  }

  volum::custom::CustomAmp NewBuilderAmp(const char* name)
  {
    volum::custom::CustomAmp a;
    a.name = (name && *name) ? name : "New custom amp";
    a.files = {}; // start empty; Add .nam seeds DIRECT/Ch1
    return a;
  }

  void StartTextEntry(TextTarget target, const IRECT& bounds, const std::string& current, int fileIdx = -1)
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    mTextTarget = target;
    mTextFileIdx = fileIdx;
    ui->CreateTextEntry(*this, mEntryText, bounds, current.c_str());
  }

  /* ---------------- action handling ---------------- */

  void HandleAction(int action, const IRECT& rect)
  {
    using namespace volum::custom;
    if (action == kClose)
    {
      Hide(true);
      return;
    }
    if (mScreen == Screen::Presets)
    {
      HandleManageAction(action, rect);
      return;
    }
    HandleBuilderAction(action, rect);
  }

  void HandleManageAction(int action, const IRECT& rect)
  {
    // Inline per-row icons select that row, then run overwrite / rename / delete.
    if (action >= kRowOverwriteBase && action < kRowOverwriteBase + 100)
    {
      mSel = action - kRowOverwriteBase;
      HandleManageAction(kUpdate, rect);
      return;
    }
    if (action >= kRowRenameBase && action < kRowRenameBase + 100)
    {
      mSel = action - kRowRenameBase;
      HandleManageAction(kRename, rect);
      return;
    }
    if (action >= kRowDeleteBase && action < kRowDeleteBase + 100)
    {
      mSel = action - kRowDeleteBase;
      HandleManageAction(kDelete, rect);
      return;
    }
    if (action >= kRowBase && action < kRowBase + 256)
    {
      mSel = action - kRowBase;
      SetDirty(false);
      return;
    }
    switch (action)
    {
      case kAdd:
        if (mManageKind == ManageKind::Presets)
        {
          char def[24];
          std::snprintf(def, sizeof(def), "My preset %d", (int)mItems.size() + 1);
          StartTextEntry(TextTarget::NewItem, NameEntryRect(rect), def);
        }
        else
        {
          StartImport();
        }
        break;
      case kUpdate:
        // presets: overwrite snapshot with live state (no-op in shell).
        NotifyChanged();
        SetDirty(false);
        break;
      case kRename:
        if (mSel >= 0 && mSel < (int)mItems.size())
          StartTextEntry(TextTarget::RenameItem, NameEntryRect(rect), mItems[(size_t)mSel]);
        break;
      case kDelete:
        if (mSel >= 0 && mSel < (int)mItems.size())
        {
          const int idx = mSel;
          const std::string nm = mItems[(size_t)idx];
          auto doDelete = [this, idx]() {
            ApplyDelete(idx);
            ReloadList();
            mSel = -1;
            NotifyChanged();
            SetDirty(false);
          };
          if (mConfirmDelete)
            mConfirmDelete("Delete " + std::string(ItemNoun()) + " \"" + nm + "\"? This cannot be undone.", doDelete);
          else
            doDelete();
        }
        break;
      default:
        break;
    }
  }

  void HandleBuilderAction(int action, const IRECT& rect)
  {
    using namespace volum::custom;
    if (action == kEditName)
    {
      StartTextEntry(TextTarget::ProfileName, rect.GetPadded(-2.f), mBuilderAmp.name);
      return;
    }
    if (action == kAddFile)
    {
      char fname[24];
      std::snprintf(fname, sizeof(fname), "capture-%d.nam", (int)mBuilderAmp.files.size() + 1);
      // Auto-assign to DIRECT / Ch 1 so a file is never in a broken state; the
      // user re-points slot/channel via the chips.
      mBuilderAmp.files.push_back({fname, kDirectSlot, 1});
      SetDirty(false);
      return;
    }
    if (action >= kArtBase && action < kArtBase + kNumCustomArts)
    {
      mBuilderAmp.art = action - kArtBase;
      SetDirty(false);
      return;
    }
    if (action == kBuilderSave)
    {
      if (SaveDisabledReason(mBuilderAmp).empty())
      {
        if (mBuilderSaved)
          mBuilderSaved(mBuilderAmp);
        Hide(true);
      }
      return;
    }
    if (action >= kCabNameBase && action < kCabNameBase + kNumCabSlots)
    {
      mTextCabSlot = action - kCabNameBase;
      StartTextEntry(TextTarget::CabName, rect.GetPadded(-1.f), mBuilderAmp.cabNames[(size_t)mTextCabSlot]);
      return;
    }
    // Per-file action ranges are 100 apart, so each span must stay < 100 to
    // avoid one branch swallowing another's codes.
    if (action >= kFileRemoveBase && action < kFileRemoveBase + 100)
    {
      const int i = action - kFileRemoveBase;
      if (i >= 0 && i < (int)mBuilderAmp.files.size())
        mBuilderAmp.files.erase(mBuilderAmp.files.begin() + i);
      SetDirty(false);
      return;
    }
    if (action >= kFileChannelBase && action < kFileChannelBase + 100)
    {
      OpenChannelPopup(action - kFileChannelBase, rect);
      return;
    }
    if (action >= kFileSpeakerBase && action < kFileSpeakerBase + 100)
    {
      OpenSpeakerPopup(action - kFileSpeakerBase, rect);
      return;
    }
  }

  /* ---------------- in-overlay popup (speaker / channel) ---------------- */

  void OpenSpeakerPopup(int fileIdx, const IRECT& anchor)
  {
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    mPopupKind = PopupKind::Speaker;
    mPopupFileIdx = fileIdx;
    mPopupItems.clear();
    // DIRECT (amp-only) + this amp's three renameable cab slots.
    mPopupItems.push_back("DIRECT");
    for (int s = 0; s < volum::custom::kNumCabSlots; s++)
      mPopupItems.push_back(mBuilderAmp.cabNames[(size_t)s]);
    LayoutPopup(anchor);
  }

  void OpenChannelPopup(int fileIdx, const IRECT& anchor)
  {
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    mPopupKind = PopupKind::Channel;
    mPopupFileIdx = fileIdx;
    mPopupItems.clear();
    for (int c = 1; c <= volum::custom::kMaxChannels; c++)
      mPopupItems.push_back("Ch " + std::to_string(c));
    LayoutPopup(anchor);
  }

  void LayoutPopup(const IRECT& anchor)
  {
    const float rowH = 20.f;
    const float w = std::max(anchor.W(), 150.f);
    float top = anchor.B + 2.f;
    const float h = 6.f + (float)mPopupItems.size() * rowH;
    const IRECT panel = PanelRect();
    if (top + h > panel.B - 6.f)
      top = std::max(panel.T + 6.f, anchor.T - 2.f - h); // flip up if no room below
    float left = anchor.L;
    if (left + w > panel.R - 6.f)
      left = panel.R - 6.f - w;
    mPopupRect = IRECT(left, top, left + w, top + h);
    mPopupOpen = true;
    SetDirty(false);
  }

  void HandlePopup(int code)
  {
    using namespace volum::custom;
    const int j = code - kPopupBase;
    if (j < 0 || j >= (int)mPopupItems.size())
    {
      mPopupOpen = false;
      return;
    }
    if (mPopupFileIdx < 0 || mPopupFileIdx >= (int)mBuilderAmp.files.size())
    {
      mPopupOpen = false;
      return;
    }
    auto& f = mBuilderAmp.files[(size_t)mPopupFileIdx];
    if (mPopupKind == PopupKind::Speaker)
    {
      // Popup row 0 == DIRECT; rows 1..N map to cab slots 0..N-1.
      f.slot = (j == 0) ? kDirectSlot : (j - 1);
      if (f.channel < 1)
        f.channel = 1;
    }
    else // Channel
    {
      f.channel = j + 1;
      if (!SlotAssigned(f.slot))
        f.slot = kDirectSlot; // a channel implies at least a DIRECT capture
    }
    mPopupOpen = false;
    SetDirty(false);
  }

  void DrawPopup(IGraphics& g)
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mPopupRect, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mPopupRect, 4.f, nullptr, 1.5f);
    const float rowH = 20.f;
    float rowT = mPopupRect.T + 3.f;
    for (int j = 0; j < (int)mPopupItems.size(); j++)
    {
      const IRECT row(mPopupRect.L + 4.f, rowT, mPopupRect.R - 4.f, rowT + rowH);
      rowT += rowH;
      g.DrawText(IText(11.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                 mPopupItems[(size_t)j].c_str(), row.GetPadded(-8.f, 0.f, -4.f, 0.f));
      mPopupHotspots.emplace_back(row, kPopupBase + j);
    }
  }

  /* ---------------- shared draw helpers ---------------- */

  IRECT NameEntryRect(const IRECT& anchor) const
  {
    // a readable entry box near the triggering button, clamped inside the panel
    const IRECT panel = PanelRect();
    const float w = 280.f, h = 30.f;
    const float l = std::min(anchor.L, panel.R - 16.f - w);
    const float t = std::min(anchor.B + 4.f, panel.B - 16.f - h);
    return IRECT(l, t, l + w, t + h);
  }

  void DrawClose(IGraphics& g, const IRECT& r)
  {
    const IColor c = VoLumColors::GOLD_DIM;
    g.DrawLine(c, r.L + 7.f, r.T + 7.f, r.R - 7.f, r.B - 7.f, nullptr, 1.8f);
    g.DrawLine(c, r.R - 7.f, r.T + 7.f, r.L + 7.f, r.B - 7.f, nullptr, 1.8f);
  }

  void DrawButton(IGraphics& g, const IRECT& r, const char* label, int action, bool primary = false, bool on = false,
                  bool disabled = false)
  {
    const IColor bg = disabled ? IColor(8, 200, 162, 78)
                               : (primary ? IColor(70, 232, 168, 92) : (on ? VoLumColors::ITEM_SEL_BG : VoLumColors::BTN_OFF_BG));
    const IColor border = disabled ? VoLumColors::FRAME : (primary ? VoLumColors::AMBER : (on ? VoLumColors::ITEM_SEL_BORDER : VoLumColors::FRAME));
    g.FillRect(bg, r);
    g.DrawRect(border, r);
    const IColor txt = disabled ? VoLumColors::CREAM_DIM : ((on || primary) ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM);
    g.DrawText(IText(11.f, txt, "Josefin-Bold", EAlign::Center, EVAlign::Middle), label, r);
    if (action >= 0 && !disabled)
      AddHotspot(r, action);
  }

  // Footer hint clipped to the panel so long copy can never bleed past the frame.
  void DrawFooter(IGraphics& g, const IRECT& r, const char* line1, const char* line2)
  {
    g.PathClipRegion(r);
    const IText t(9.5f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle);
    g.DrawText(t, line1, r.GetFromTop(r.H() * 0.5f));
    g.DrawText(t, line2, r.GetReducedFromTop(r.H() * 0.5f));
    g.PathClipRegion(); // reset to full
  }

  /* ---------------- Manage screen (presets / IR / pedals) ---------------- */

  const char* ItemNoun() const
  {
    switch (mManageKind)
    {
      case ManageKind::IR: return "Custom IR";
      case ManageKind::Pedals: return "custom pedal";
      default: return "preset";
    }
  }

  void DrawManage(IGraphics& g, const IRECT& body)
  {
    const bool presets = (mManageKind == ManageKind::Presets);

    char sub[160];
    if (presets)
      std::snprintf(sub, sizeof(sub), "for  %s   -   double-click a row to recall", mAmpName.c_str());
    else if (mManageKind == ManageKind::IR)
      std::snprintf(sub, sizeof(sub), "shared across amps   -   double-click to use on the focused cab");
    else
      std::snprintf(sub, sizeof(sub), "shown under CUSTOM in the PRE pedal menu");
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Top), sub, body.GetFromTop(14.f));

    // Single top action button: save-as-new (presets) or import (IR / pedals).
    const char* addLabel =
      presets ? "+  Save current as new" : (mManageKind == ManageKind::IR ? "+  Import IR (.wav)" : "+  Import pedal (.nam)");
    const IRECT addBtn(body.L, body.T + 20.f, body.R, body.T + 48.f);
    DrawButton(g, addBtn, addLabel, kAdd, true);

    // Scrollable list filling the rest of the panel; per-row inline icons.
    const IRECT listArea(body.L, addBtn.B + 10.f, body.R, body.B - 26.f);
    g.FillRect(IColor(235, 20, 20, 26), listArea);
    g.DrawRect(IColor(89, 200, 162, 78), listArea);

    if (mItems.empty())
    {
      char empty[120];
      std::snprintf(empty, sizeof(empty), "No %ss yet.", ItemNoun());
      g.DrawText(IText(12.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle), empty, listArea);
    }
    else
    {
      const float rowH = 32.f;
      const float contentH = (float)mItems.size() * rowH + 8.f;
      const bool scrollable = contentH > listArea.H();
      const float sbW = scrollable ? 6.f : 0.f;
      ClampManageScroll(contentH, listArea.H());

      g.PathClipRegion(listArea);
      const float iconW = 24.f;
      const int nIcons = presets ? 3 : 2;
      float y = listArea.T + 4.f - mManageScroll;
      for (int i = 0; i < (int)mItems.size(); i++)
      {
        const IRECT row(listArea.L + 4.f, y, listArea.R - 4.f - sbW, y + rowH - 4.f);
        y += rowH;
        if (row.B < listArea.T || row.T > listArea.B)
          continue;

        const bool sel = (i == mSel);
        if (sel)
        {
          g.FillRect(VoLumColors::ITEM_SEL_BG, row);
          g.DrawRect(VoLumColors::ITEM_SEL_BORDER, row);
          g.FillCircle(VoLumColors::GOLD, row.L + 10.f, row.MH(), 3.f);
        }

        // Inline icons, right-aligned: [overwrite?] [pen] [trash]. Added to the
        // hotspot list BEFORE the row body so an icon click wins over "select".
        float ix = row.R - iconW;
        const IRECT trash(ix, row.T, ix + iconW, row.B);
        DrawBinGlyph(g, trash, VoLumColors::CREAM_DIM);
        AddHotspot(trash, kRowDeleteBase + i);
        ix -= iconW;
        const IRECT pen(ix, row.T, ix + iconW, row.B);
        DrawPenGlyph(g, pen, VoLumColors::CREAM_DIM);
        AddHotspot(pen, kRowRenameBase + i);
        ix -= iconW;
        if (presets)
        {
          const IRECT ovr(ix, row.T, ix + iconW, row.B);
          DrawOverwriteGlyph(g, ovr, VoLumColors::CREAM_DIM);
          AddHotspot(ovr, kRowOverwriteBase + i);
          ix -= iconW;
        }

        const IRECT nameR(row.L + (sel ? 20.f : 12.f), row.T, ix - 6.f, row.B);
        g.PathClipRegion(nameR);
        g.DrawText(IText(13.f, sel ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Near,
                         EVAlign::Middle),
                   mItems[(size_t)i].c_str(), nameR);
        g.PathClipRegion(listArea);

        AddHotspot(row, kRowBase + i);
      }
      g.PathClipRegion();

      if (scrollable)
      {
        const float trackX = listArea.R - sbW - 1.f;
        IRECT track(trackX, listArea.T + 2.f, listArea.R - 1.f, listArea.B - 2.f);
        g.FillRect(IColor(40, 200, 162, 78), track);
        const float maxScroll = contentH - listArea.H();
        const float thumbH = std::max(18.f, track.H() * (listArea.H() / contentH));
        const float t = (maxScroll > 0.f) ? (mManageScroll / maxScroll) : 0.f;
        g.FillRect(VoLumColors::GOLD_DIM, IRECT(track.L, track.T + (track.H() - thumbH) * t, track.R,
                                                track.T + (track.H() - thumbH) * t + thumbH));
      }
    }

    const char* hint = presets
      ? "A preset snapshots this amp (cab/IR, channel, AMP, PRE, POST). Double-click recalls; icons overwrite / rename / delete."
      : (mManageKind == ManageKind::IR
           ? "Import a .wav, then double-click to convolve it with the focused DIRECT capture. Shared across amps."
           : "Import a .nam capture of your own pre-amp gear. They appear under CUSTOM in the PRE pedal menu.");
    g.DrawText(IText(9.5f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle), hint,
               IRECT(body.L, body.B - 22.f, body.R, body.B));
  }

  void ClampManageScroll(float contentH, float viewH)
  {
    const float maxScroll = std::max(0.f, contentH - viewH);
    mManageScroll = std::clamp(mManageScroll, 0.f, maxScroll);
  }

  // Stacked action button rows in the right-hand column.
  IRECT RowAt(const IRECT& col, int idx, float topOffset = 0.f) const
  {
    const float h = 28.f, gap = 6.f;
    const float t = col.T + topOffset + (float)idx * (h + gap);
    return IRECT(col.L, t, col.R, t + h);
  }

  /* ---------------- Builder screen ---------------- */

  void DrawBuilder(IGraphics& g, const IRECT& body)
  {
    using namespace volum::custom;
    const IRECT left(body.L, body.T, body.L + body.W() * 0.53f, body.B);
    const IRECT right(left.R + 22.f, body.T, body.R, body.B);

    // Hairline column divider for an editorial two-pane structure.
    const float divX = left.R + 11.f;
    g.DrawLine(IColor(60, 200, 180, 110), divX, body.T + 2.f, divX, body.B - 2.f, nullptr, 1.f);

    // ---- LEFT: profile name + file manifest -------------------------------
    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top), "PROFILE NAME",
               left.GetFromTop(12.f));
    const IRECT nameBox(left.L, left.T + 16.f, left.R, left.T + 42.f);
    g.FillRoundRect(VoLumColors::BTN_OFF_BG, nameBox, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, nameBox, 4.f); // looks like an editable input
    const IRECT nameTextR = nameBox.GetPadded(-10.f, 0.f, -10.f, 0.f);
    g.PathClipRegion(nameTextR);
    g.DrawText(IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Near, EVAlign::Middle), mBuilderAmp.name.c_str(),
               nameTextR);
    g.PathClipRegion();
    AddHotspot(nameBox, kEditName);

    const IRECT drop(left.L, nameBox.B + 12.f, left.R, nameBox.B + 40.f);
    g.DrawDottedRect(VoLumColors::TEAL_DIM, drop, nullptr, 1.f, 4.f);
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "+ Add .nam file", drop);
    AddHotspot(drop, kAddFile);

    // file rows: filename | speaker | channel | remove
    float y = drop.B + 10.f;
    const float rowH = 26.f;
    for (int i = 0; i < (int)mBuilderAmp.files.size(); i++)
    {
      auto& f = mBuilderAmp.files[(size_t)i];
      const bool unassigned = !FileAssigned(f);
      const bool dup = FileIsDuplicate(mBuilderAmp, (size_t)i);
      const IRECT row(left.L, y, left.R, y + rowH);
      // Duplicate (slot,channel) -> red; unassigned -> amber; else neutral.
      const IColor rowFill = dup ? IColor(34, 235, 70, 70) : (unassigned ? IColor(28, 232, 168, 92) : VoLumColors::BTN_OFF_BG);
      const IColor rowBorder = dup ? IColor(220, 235, 90, 90) : (unassigned ? VoLumColors::AMBER : VoLumColors::FRAME);
      g.FillRect(rowFill, row);
      g.DrawRect(rowBorder, row);

      const IRECT rem(row.R - 22.f, row.T + 3.f, row.R - 4.f, row.B - 3.f);
      const IRECT ch(rem.L - 52.f, row.T + 3.f, rem.L - 4.f, row.B - 3.f);
      const IRECT spk(ch.L - 64.f, row.T + 3.f, ch.L - 6.f, row.B - 3.f);

      g.DrawText(IText(10.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), f.file.c_str(),
                 IRECT(row.L + 8.f, row.T, spk.L - 6.f, row.B));

      // slot (cab) chip
      const bool slotOk = SlotAssigned(f.slot);
      const std::string spkLabel = slotOk ? SlotLabel(mBuilderAmp, f.slot) : "cab";
      g.FillRect(VoLumColors::HERO_BG, spk);
      g.DrawRect(slotOk ? VoLumColors::TEAL_DIM : VoLumColors::AMBER, spk);
      g.DrawText(IText(10.f, slotOk ? VoLumColors::GOLD : VoLumColors::AMBER, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                 spkLabel.c_str(), spk);
      AddHotspot(spk, kFileSpeakerBase + i);

      // channel chip
      char chLabel[12];
      if (f.channel >= 1)
        std::snprintf(chLabel, sizeof(chLabel), "Ch %d", f.channel);
      else
        std::snprintf(chLabel, sizeof(chLabel), "Ch -");
      g.FillRect(VoLumColors::HERO_BG, ch);
      g.DrawRect(VoLumColors::TEAL_DIM, ch);
      g.DrawText(IText(10.f, VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Center, EVAlign::Middle), chLabel, ch);
      AddHotspot(ch, kFileChannelBase + i);

      // remove
      g.DrawText(IText(13.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "x", rem);
      AddHotspot(rem, kFileRemoveBase + i);

      y += rowH + 4.f;
    }

    if (mBuilderAmp.files.empty())
      g.DrawText(IText(11.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top),
                 "No files yet. Add a .nam, then pick its speaker + channel.", IRECT(left.L, y, left.R, y + 18.f));

    DrawFooter(g, IRECT(left.L, left.B - 30.f, left.R, left.B),
               "Pick a speaker (DIRECT = amp-only, pair with a custom IR) and channel per file.",
               "Sparse coverage is fine. Stored as a per-amp manifest - your files are never renamed.");

    // ---- RIGHT (top): art picker as a 3x2 gallery -------------------------
    {
      const IRECT artHdr(right.L, right.T, right.R, right.T + 13.f);
      g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "ART", artHdr);
      g.DrawLine(IColor(70, 198, 162, 90), right.L + 30.f, artHdr.MH(), right.R, artHdr.MH(), nullptr, 1.f);
    }
    const float gTop = right.T + 20.f;
    const float gGap = 8.f;
    const int artCols = 3;
    const float tileW = (right.W() - gGap * (artCols - 1)) / (float) artCols;
    const float artGridH = 150.f;
    const float tileH = (artGridH - gGap) / 2.f;
    for (int a = 0; a < kNumCustomArts; a++)
    {
      const int col = a % artCols, rowN = a / artCols;
      const IRECT t(right.L + col * (tileW + gGap), gTop + rowN * (tileH + gGap),
                    right.L + col * (tileW + gGap) + tileW, gTop + rowN * (tileH + gGap) + tileH);
      const bool selArt = (mBuilderAmp.art == a);
      if (selArt)
        g.FillRoundRect(IColor(60, 232, 196, 96), t.GetPadded(2.5f), 8.f); // soft gold glow
      g.FillRoundRect(VoLumColors::HERO_BG, t, 6.f);
      const IColor bright = selArt ? IColor(235, 196, 142, 226) : IColor(150, 120, 168, 150);
      const IColor dim = selArt ? IColor(210, 110, 150, 205) : IColor(130, 70, 110, 130);
      DrawCustomAmpArt(g, t.GetPadded(-7.f), a, bright, dim);
      g.DrawRoundRect(selArt ? VoLumColors::GOLD : VoLumColors::TEAL_DIM, t, 6.f, nullptr, selArt ? 1.8f : 1.f);
      if (selArt)
      {
        const float bx = t.R - 9.f, by = t.T + 9.f;
        g.FillCircle(VoLumColors::GOLD, bx, by, 6.f);
        g.DrawLine(IColor(255, 22, 22, 30), bx - 2.6f, by + 0.2f, bx - 0.7f, by + 2.4f, nullptr, 1.6f);
        g.DrawLine(IColor(255, 22, 22, 30), bx - 0.7f, by + 2.4f, bx + 3.f, by - 2.4f, nullptr, 1.6f);
      }
      AddHotspot(t, kArtBase + a);
    }

    // ---- RIGHT (bottom): fixed 4-row coverage grid (DIRECT + 3 cab slots) ---
    const float covTop = gTop + artGridH + 16.f;
    {
      const IRECT covHdr(right.L, covTop, right.R, covTop + 13.f);
      g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "COVERAGE", covHdr);
      g.DrawLine(IColor(70, 198, 162, 90), right.L + 64.f, covHdr.MH(), right.R, covHdr.MH(), nullptr, 1.f);
      g.DrawText(IText(8.5f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Far, EVAlign::Middle),
                 "tap a cab name to rename", covHdr);
    }

    // Columns: at least 4, grow to the highest assigned channel, hard cap 8.
    const int maxCh = std::min(volum::custom::kMaxChannels, std::max(4, MaxAssignedChannel(mBuilderAmp)));
    const float labelW = 64.f;
    const float gridL = right.L + labelW;
    const float cw = std::min(26.f, (right.R - gridL) / (float) maxCh);

    // channel header numbers
    float gy = covTop + 20.f;
    {
      const IRECT hrow(right.L, gy, right.R, gy + 14.f);
      for (int c = 1; c <= maxCh; c++)
      {
        const IRECT cell(gridL + (c - 1) * cw, hrow.T, gridL + (c - 1) * cw + cw, hrow.B);
        g.DrawText(IText(8.5f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                   std::to_string(c).c_str(), cell);
      }
      gy += 16.f;
    }

    const int slots[1 + volum::custom::kNumCabSlots] = {volum::custom::kDirectSlot, 0, 1, 2};
    const float covRowH = 26.f;
    for (int row = 0; row < (int)(sizeof(slots) / sizeof(slots[0])); row++)
    {
      const int slot = slots[row];
      const IRECT srow(right.L, gy, right.R, gy + covRowH);
      if (slot == volum::custom::kDirectSlot)
      {
        g.DrawText(IText(10.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "DIRECT",
                   IRECT(srow.L, srow.T, srow.L + labelW, srow.B));
      }
      else
      {
        // Editable cab-name chip with a pen glyph.
        const IRECT chip(srow.L, srow.MH() - 10.f, srow.L + labelW - 6.f, srow.MH() + 10.f);
        g.FillRoundRect(VoLumColors::HERO_BG, chip, 3.f);
        g.DrawRoundRect(VoLumColors::TEAL_DIM, chip, 3.f);
        g.DrawText(IText(10.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                   mBuilderAmp.cabNames[(size_t)slot].c_str(), chip.GetPadded(-7.f, 0.f, -16.f, 0.f));
        DrawPenGlyph(g, IRECT(chip.R - 15.f, chip.T, chip.R - 1.f, chip.B), VoLumColors::CREAM_DIM);
        AddHotspot(chip, kCabNameBase + slot);
      }
      for (int c = 1; c <= maxCh; c++)
      {
        const IRECT cell(gridL + (c - 1) * cw + 1.f, srow.T + 3.f, gridL + (c - 1) * cw + cw - 1.f, srow.B - 3.f);
        const int n = CellFileCount(mBuilderAmp, slot, c);
        if (n >= 2)
        {
          g.FillRect(IColor(40, 235, 70, 70), cell);
          g.DrawRect(IColor(230, 235, 90, 90), cell);
          char badge[6];
          std::snprintf(badge, sizeof(badge), "x%d", n);
          g.DrawText(IText(9.f, IColor(255, 255, 150, 150), "Josefin-Bold", EAlign::Center, EVAlign::Middle), badge, cell);
        }
        else if (n == 1)
        {
          g.FillRect(VoLumColors::ITEM_SEL_BG, cell);
          g.DrawRect(VoLumColors::ITEM_SEL_BORDER, cell);
          g.FillCircle(VoLumColors::GOLD, cell.MW(), cell.MH(), 2.5f);
        }
        else
        {
          g.DrawRect(VoLumColors::FRAME, cell);
        }
      }
      gy += covRowH + 2.f;
    }

    const std::string reason = SaveDisabledReason(mBuilderAmp);
    const std::string saveLabel = reason.empty() ? "Save amp" : reason;
    DrawButton(g, IRECT(right.L, right.B - 32.f, right.R, right.B - 4.f), saveLabel.c_str(), kBuilderSave, true, false,
               !reason.empty());
  }

  // Diagonal pencil glyph (mirrors VoLumAmpListControl::DrawPenGlyph) for the
  // editable cab-name chips.
  static void DrawPenGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const float t = 1.3f;
    g.DrawLine(col, cx - 4.f, cy + 3.f, cx + 3.f, cy - 4.f, nullptr, t);
    g.DrawLine(col, cx - 1.f, cy + 5.f, cx + 5.f, cy - 1.f, nullptr, t);
    g.DrawLine(col, cx + 3.f, cy - 4.f, cx + 5.f, cy - 1.f, nullptr, t);
    g.DrawLine(col, cx - 4.f, cy + 3.f, cx - 5.f, cy + 6.f, nullptr, t);
    g.DrawLine(col, cx - 1.f, cy + 5.f, cx - 5.f, cy + 6.f, nullptr, t);
  }

  // Trash bin (mirrors VoLumAmpListControl::DrawBinGlyph).
  static void DrawBinGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const IRECT body(cx - 5.f, cy - 3.f, cx + 5.f, cy + 6.f);
    g.DrawRect(col, body, nullptr, 1.3f);
    g.DrawLine(col, cx - 7.f, cy - 3.f, cx + 7.f, cy - 3.f, nullptr, 1.3f); // lid
    g.DrawLine(col, cx - 2.f, cy - 6.f, cx + 2.f, cy - 6.f, nullptr, 1.3f); // handle
  }

  // "Overwrite with current": a down arrow dropping into an open tray.
  static void DrawOverwriteGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const float t = 1.3f;
    g.DrawLine(col, cx, cy - 6.f, cx, cy + 1.f, nullptr, t); // shaft
    g.DrawLine(col, cx - 3.f, cy - 2.f, cx, cy + 1.f, nullptr, t); // arrow head
    g.DrawLine(col, cx + 3.f, cy - 2.f, cx, cy + 1.f, nullptr, t);
    g.DrawLine(col, cx - 6.f, cy + 3.f, cx - 6.f, cy + 6.f, nullptr, t); // tray
    g.DrawLine(col, cx + 6.f, cy + 3.f, cx + 6.f, cy + 6.f, nullptr, t);
    g.DrawLine(col, cx - 6.f, cy + 6.f, cx + 6.f, cy + 6.f, nullptr, t);
  }

  volum::custom::Screen mScreen = volum::custom::Screen::Presets;
  ManageKind mManageKind = ManageKind::Presets;
  int mAmpIdx = 0;
  std::string mAmpName;
  std::vector<std::string> mItems;
  int mSel = -1;
  volum::custom::CustomAmp mBuilderAmp;
  std::vector<std::pair<IRECT, int>> mHotspots;

  // in-overlay popup (builder speaker/channel pickers)
  bool mPopupOpen = false;
  PopupKind mPopupKind = PopupKind::Speaker;
  int mPopupFileIdx = -1;
  IRECT mPopupRect;
  std::vector<std::string> mPopupItems;
  std::vector<std::pair<IRECT, int>> mPopupHotspots;

  // pending text entry target
  TextTarget mTextTarget = TextTarget::None;
  int mTextFileIdx = -1;
  int mTextCabSlot = -1; // cab slot being renamed (TextTarget::CabName)
  IText mEntryText;

  int mPedalSlot = -1; // originating PRE NAM slot for ManageKind::Pedals
  float mManageScroll = 0.f; // Manage list scroll offset (px)

  BuilderSavedCallback mBuilderSaved;
  ChangedCallback mChanged;
  ConfirmDeleteCallback mConfirmDelete;
  PrimaryActionCallback mPrimaryAction;
};
