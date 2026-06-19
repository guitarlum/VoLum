#pragma once

// VoLum dropdown-menu controls used by the triptych.
//
// - VoLumPreCaptureMenuItem: row-model struct describing each PRE-capture
//   pick (label + capture index + optional group header).
// - VoLumPreCaptureMenuControl: drop-down picker for the PRE-NAM-1 and
//   PRE-NAM-2 pedal-card slots.
// - VoLumSupportAmpMenuControl: drop-down picker for the Dual Amp
//   support-amp slot. Item 0 is "(none)" and maps to support index -1.
//
// Both controls render themselves with VoLumColors and DrawCornerAccent and
// route selection back to the plugin via PLUG_CLASS_NAME helpers. Extracted
// from VoLumTriptych.h on the 1.0-bugs-hygiene branch.

#include "VoLumColorHelpers.h"
#include "VoLumTriptychState.h"
#include "NeuralAmpModeler.h"

#include <cstddef>
#include <string>
#include <vector>

using namespace iplug;
using namespace igraphics;

// The PRE dropdown hosts a single "Manage custom pedals..." entry that opens the
// shared Manage panel (CRUD + import via file dialog). No inline import row.
enum class PreMenuAction
{
  None = 0,
  Manage
};

struct VoLumPreCaptureMenuItem
{
  std::string label;
  int captureIdx = 0;
  bool isHeader = false;
  volum::PrePedalCaptureGroup group = volum::PrePedalCaptureGroup::None;
  PreMenuAction action = PreMenuAction::None; // Manage row (opens the Manage panel)
  bool custom = false; // imported capture row (dismiss-only in the shell)
};

class VoLumPreCaptureMenuControl : public IControl
{
public:
  VoLumPreCaptureMenuControl(const IRECT& bounds) : IControl(bounds) { mIgnoreMouse = false; }

  void SetItems(int slot, const std::vector<VoLumPreCaptureMenuItem>& items, int selectedIdx)
  {
    mSlot = slot;
    mItems = items;
    mSelectedIdx = selectedIdx;
    mHovered = -1;
    SetDirty(false);
  }

  int GetSlot() const { return mSlot; }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mRECT, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mRECT, 4.f, nullptr, 1.5f);
    DrawCornerAccent(g, mRECT.L + 5.f, mRECT.T + 5.f, 8.f, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, mRECT.R - 5.f, mRECT.B - 5.f, 8.f, true, true, VoLumColors::TEAL_DIM);

    const IText text(12.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const IText dimText(12.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const IText headerText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    float rowT = mRECT.T + 6.f;
    for (int i = 0; i < static_cast<int>(mItems.size()); ++i)
    {
      const auto& item = mItems[static_cast<size_t>(i)];
      const float rowH = RowHeight(item);
      const IRECT row(mRECT.L + 8.f, rowT, mRECT.R - 8.f, rowT + rowH);
      rowT += rowH;
      if (item.isHeader)
      {
        const IRECT line(row.L, row.MH(), row.L + 10.f, row.MH() + 1.f);
        g.FillRect(GroupColor(item.group).WithOpacity(0.75f), line);
        g.DrawText(headerText, item.label.c_str(), IRECT(row.L + 14.f, row.T, row.R, row.B));
        continue;
      }

      const bool selected = item.captureIdx == mSelectedIdx;
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      else if (i == mHovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(IColor(20, 200, 162, 78), row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      if (item.action != PreMenuAction::None)
      {
        // Import/Manage affordance: teal, left-aligned, no selection dot.
        g.DrawText(IText(12.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Middle), item.label.c_str(),
                   IRECT(row.L + 14.f, row.T, row.R, row.B));
        continue;
      }
      if (item.group != volum::PrePedalCaptureGroup::None)
        g.FillRoundRect(GroupColor(item.group).WithOpacity(0.32f), IRECT(row.L, row.T + 4.f, row.L + 4.f, row.B - 4.f), 2.f);
      if (selected)
        g.FillCircle(VoLumColors::TEAL, row.L + 8.f, row.MH(), 3.f);
      g.DrawText(selected ? text : dimText, item.label.c_str(), IRECT(row.L + 20.f, row.T, row.R, row.B));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) mod;
    (void) x;
    const int idx = ItemIndexAtY(y);
    if (idx < 0 || idx >= static_cast<int>(mItems.size()) || mItems[static_cast<size_t>(idx)].isHeader)
      return;

    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (!plugin)
      return;

    const auto& item = mItems[static_cast<size_t>(idx)];
    // "Manage custom pedals..." opens the shared Manage panel; imported pedal
    // rows are dismiss-only in the shell (selecting/applying lands with backend).
    if (item.action == PreMenuAction::Manage)
    {
      plugin->_VolumHidePreCaptureMenu();
      plugin->_VolumShowManageCustomPedals();
      return;
    }
    if (item.custom)
    {
      plugin->_VolumHidePreCaptureMenu();
      return;
    }
    plugin->_VolumSetPreNamCapture(mSlot, item.captureIdx);
    plugin->_VolumHidePreCaptureMenu();
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) mod;
    const int idx = ItemIndexAtY(y);
    const int next =
      (idx >= 0 && idx < static_cast<int>(mItems.size()) && !mItems[static_cast<size_t>(idx)].isHeader) ? idx : -1;
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

  static constexpr float ItemHeight() { return 21.f; }
  static constexpr float HeaderHeight() { return 15.f; }

  static float MenuHeight(const std::vector<VoLumPreCaptureMenuItem>& items)
  {
    float height = 12.f;
    for (const auto& item : items)
      height += RowHeight(item);
    return height;
  }

private:
  static float RowHeight(const VoLumPreCaptureMenuItem& item)
  {
    return item.isHeader ? HeaderHeight() : ItemHeight();
  }

  int ItemIndexAtY(float y) const
  {
    float rowT = mRECT.T + 6.f;
    for (int i = 0; i < static_cast<int>(mItems.size()); ++i)
    {
      const float rowB = rowT + RowHeight(mItems[static_cast<size_t>(i)]);
      if (y >= rowT && y < rowB)
        return i;
      rowT = rowB;
    }
    return -1;
  }

  static IColor GroupColor(volum::PrePedalCaptureGroup group)
  {
    switch (group)
    {
      case volum::PrePedalCaptureGroup::Klon:
        return IColor(255, 235, 181, 78);
      case volum::PrePedalCaptureGroup::TsBoost:
        return IColor(255, 80, 210, 150);
      case volum::PrePedalCaptureGroup::Distortion:
        return IColor(255, 230, 120, 72);
      case volum::PrePedalCaptureGroup::Fuzz:
        return IColor(255, 190, 100, 230);
      default:
        return VoLumColors::TEAL_DIM;
    }
  }

  int mSlot = 0;
  int mSelectedIdx = 0;
  int mHovered = -1;
  std::vector<VoLumPreCaptureMenuItem> mItems;
};

// Dual Amp support-amp dropdown picker. Visually matches VoLumPreCaptureMenuControl but
// operates on the support-amp index. Item 0 represents "(none)" so the user can clear the
// support amp without disabling Dual Amp.
class VoLumSupportAmpMenuControl : public IControl
{
public:
  VoLumSupportAmpMenuControl(const IRECT& bounds) : IControl(bounds) { mIgnoreMouse = false; }

  void SetItems(const std::vector<std::string>& labels, int selectedIdx)
  {
    mLabels = labels;
    mSelectedIdx = selectedIdx;
    mHovered = -1;
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mRECT, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mRECT, 4.f, nullptr, 1.5f);
    DrawCornerAccent(g, mRECT.L + 5.f, mRECT.T + 5.f, 8.f, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, mRECT.R - 5.f, mRECT.B - 5.f, 8.f, true, true, VoLumColors::TEAL_DIM);

    const IText text(12.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const IText dimText(12.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    for (int i = 0; i < static_cast<int>(mLabels.size()); ++i)
    {
      const IRECT row(mRECT.L + 8.f, mRECT.T + 6.f + i * mItemH, mRECT.R - 8.f, mRECT.T + 6.f + (i + 1) * mItemH);
      const bool selected = i == mSelectedIdx;
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      else if (i == mHovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(IColor(20, 200, 162, 78), row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      if (selected)
        g.FillCircle(VoLumColors::TEAL, row.L + 8.f, row.MH(), 3.f);
      g.DrawText(selected ? text : dimText, mLabels[static_cast<size_t>(i)].c_str(), IRECT(row.L + 20.f, row.T, row.R, row.B));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    (void) x; (void) mod;
    auto* plugin = dynamic_cast<PLUG_CLASS_NAME*>(GetDelegate());
    if (!plugin) return;

    const int idx = static_cast<int>((y - (mRECT.T + 6.f)) / mItemH);
    if (idx < 0 || idx >= static_cast<int>(mLabels.size()))
    {
      // Click on the menu's padding (top/bottom border or below the last item) - dismiss without
      // changing the selection, same as clicking outside the menu rect.
      plugin->_VolumHideSupportAmpMenu();
      return;
    }

    // Item 0 is "(none)" -> map to support-amp index -1; subsequent items map to amp index 0..N-1.
    plugin->_VolumSetSupportAmp(idx - 1);
    plugin->_VolumHideSupportAmpMenu();
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    (void) x;
    (void) mod;
    const int idx = static_cast<int>((y - (mRECT.T + 6.f)) / mItemH);
    const int next = (idx >= 0 && idx < static_cast<int>(mLabels.size())) ? idx : -1;
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

  // 17px keeps the full 16-amp list (15 amps + "(none)") inside the standalone window without
  // bottom clipping. Combined with the 6+6 px padding handled in Draw, total menu height stays
  // under ~290 px, leaving headroom even when the standalone toolbar shrinks the canvas.
  static constexpr float ItemHeight() { return 17.f; }

private:
  int mSelectedIdx = 0;
  int mHovered = -1;
  float mItemH = ItemHeight();
  std::vector<std::string> mLabels;
};
