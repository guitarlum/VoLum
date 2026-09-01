#pragma once

// VoLumPresetBarControl: F5 preset strip in the AMP header.
// Extracted from VoLumCustomUi.h for file-size hygiene.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentApi.h"
#include "VoLumFractalArt.h"
#include "VoLumIrFileGuard.h"
#include "VoLumPresetStep.h"

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
// F5: preset bar (AMP header strip)
// ---------------------------------------------------------------------------
class VoLumPresetBarControl : public IControl
{
public:
  using OpenCallback = std::function<void()>;
  // Fired when the user steps presets with the < / > arrows; the host applies
  // that preset (settings recall) and drives the bar back via SelectName.
  using RecallCallback = std::function<void(int index)>;
  // Fired when the user picks "Save current as new..." from the dropdown and
  // types a name; the host snapshots the live rig under that name.
  using SaveAsCallback = std::function<void(const std::string&)>;

  VoLumPresetBarControl(const IRECT& bounds, OpenCallback openCb)
  : IControl(bounds)
  , mOpen(std::move(openCb))
  {
  }

  void SetRecallCallback(RecallCallback cb) { mRecall = std::move(cb); }
  void SetSaveAsCallback(SaveAsCallback cb) { mSaveAs = std::move(cb); }

  // Set the active amp's preset bank (mock). Empty list => "(unsaved)" + inert arrows.
  void SetList(const std::vector<std::string>& names)
  {
    mList = names;
    mIdx = -1;
    mName.clear();
    mEmpty = true;
    mDirtyEdit = false;
    mFactory = false;
    SetDirty(false);
  }

  // Mark a preset (by name) as the active one, e.g. after a recall from the
  // browser. A fresh recall is clean (matches the stored snapshot).
  void SelectName(const char* name)
  {
    mName = name ? name : "";
    mEmpty = mName.empty();
    mDirtyEdit = false;
    mFactory = false;
    mIdx = -1;
    for (int i = 0; i < (int)mList.size(); i++)
      if (mList[(size_t)i] == mName)
        mIdx = i;
    SetDirty(false);
  }

  void SelectAt(int index, const char* name, bool factory)
  {
    mName = name ? name : "";
    mEmpty = mName.empty();
    mDirtyEdit = false;
    mIdx = (index >= 0 && index < static_cast<int>(mList.size())) ? index : -1;
    mFactory = factory;
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
  // Active preset name (empty when none selected).
  const std::string& ActiveName() const { return mName; }
  // Whether the live rig has diverged from the recalled snapshot.
  bool IsEditDirty() const { return mDirtyEdit; }
  bool IsFactoryActive() const { return mFactory; }

  // Open an inline text entry to name a new preset; on completion fires the
  // save-as callback. Used by the dropdown's "Save current as new..." row.
  void PromptSaveAs()
  {
    if (auto* ui = GetUI())
    {
      const IRECT mid = mRECT.GetReducedFromLeft(22.f).GetReducedFromRight(22.f);
      SetTextEntryLength((int)volum::custom::kMaxPresetNameLen);
      ui->CreateTextEntry(
        *this, IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center, EVAlign::Middle), mid, "");
    }
  }

  void OnTextEntryCompletion(const char* str, int) override
  {
    std::string name = str ? str : "";
    // Trim surrounding whitespace; ignore an empty name.
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    name.erase(name.begin(), std::find_if(name.begin(), name.end(), notSpace));
    name.erase(std::find_if(name.rbegin(), name.rend(), notSpace).base(), name.end());
    name = volum::custom::ClampName(name, volum::custom::kMaxPresetNameLen);
    if (!name.empty() && mSaveAs)
      mSaveAs(name);
  }

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
      label = mDirtyEdit ? "No Preset  (unsaved)" : "No Preset";
      col = mDirtyEdit ? VoLumColors::AMBER : VoLumColors::CREAM_DIM;
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

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    mMouseIsOver = true;
    const char* tip = "Open presets";
    if (!mList.empty() && PrevRect().Contains(x, y))
      tip = "Previous preset";
    else if (!mList.empty() && NextRect().Contains(x, y))
      tip = "Next preset";
    SetTooltip(tip);
    SetDirty(false);
  }
  void OnMouseOut() override
  {
    mMouseIsOver = false;
    SetTooltip("");
    SetDirty(false);
  }

private:
  void Step(int dir)
  {
    if (mList.empty())
      return;
    const int idx = volum::StepPresetIndex(mIdx, dir, (int)mList.size());
    if (idx < 0)
      return;
    if (mRecall)
    {
      // The host applies the preset and calls SelectName() back, so we don't
      // touch the visible state here (keeps recall logic in one place).
      mRecall(idx);
      return;
    }
    mIdx = idx;
    mName = mList[(size_t)mIdx];
    mEmpty = false;
    mDirtyEdit = false; // cycling to a stored preset is a clean recall
    mFactory = false;
    SetDirty(false);
  }

  IRECT PrevRect() const { return mRECT.GetFromLeft(22.f); }
  IRECT NextRect() const { return mRECT.GetFromRight(22.f); }

  std::string mName;
  bool mEmpty = true;
  bool mDirtyEdit = false;
  bool mFactory = false;
  std::vector<std::string> mList;
  int mIdx = -1;
  OpenCallback mOpen;
  RecallCallback mRecall;
  SaveAsCallback mSaveAs;
};
