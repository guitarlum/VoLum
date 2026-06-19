#pragma once

// VoLum 1.2.0 BYO + presets UI shells (refined D8).
//
// Two controls, rendering against the in-memory VoLumCustomContentMock:
//   - VoLumPresetBarControl    : F5 preset strip in the AMP header (prev/name/next + open browser).
//   - VoLumCustomOverlayControl: one full-window overlay with two screens -
//       * Presets : per-amp named presets (recall / save-as / rename / delete).
//       * Builder : free-form, file-first custom amp create/edit with a live
//                   (speaker x channel) coverage grid and auto-snap demo.
//
// Custom IR cabs and imported pedals are NOT here - they live inline in the
// speaker-row dropdown (VoLumSpeakerRow.h) and the PRE capture dropdown
// (VoLumTriptychMenus.h) respectively. This is the UI-shell phase: navigation,
// layout, and the live snap/coverage are real; load/save/import are stubs.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentMock.h"

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
    SetDirty(false);
  }

  // Mark a preset (by name) as the active one, e.g. after a recall from the browser.
  void SelectName(const char* name)
  {
    mName = name ? name : "";
    mEmpty = mName.empty();
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

  void Draw(IGraphics& g) override
  {
    g.FillRect(mMouseIsOver ? VoLumColors::ITEM_SEL_BG : VoLumColors::BTN_OFF_BG, mRECT);
    g.DrawRect(VoLumColors::FRAME, mRECT);

    const IText arrow(15.f, mList.empty() ? VoLumColors::CREAM_DIM : VoLumColors::GOLD, "Josefin-Bold", EAlign::Center,
                      EVAlign::Middle);
    g.DrawText(arrow, "<", PrevRect());
    g.DrawText(arrow, ">", NextRect());

    const IRECT mid = mRECT.GetReducedFromLeft(22.f).GetReducedFromRight(40.f);
    g.DrawText(IText(8.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Top), "PRESET",
               mid.GetFromTop(11.f));
    const char* label = mEmpty ? "(unsaved)" : (mName.empty() ? "(unsaved)" : mName.c_str());
    g.DrawText(IText(12.f, mEmpty ? VoLumColors::CREAM_DIM : VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Center,
                     EVAlign::Bottom),
               label, mid.GetReducedFromTop(9.f));

    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "v", CaretRect());
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
    SetDirty(false);
  }

  IRECT PrevRect() const { return mRECT.GetFromLeft(22.f); }
  IRECT NextRect() const { return mRECT.GetFromRight(22.f); }
  IRECT CaretRect() const { return mRECT.GetFromRight(40.f).GetFromLeft(18.f); }

  std::string mName;
  bool mEmpty = true;
  std::vector<std::string> mList;
  int mIdx = -1;
  OpenCallback mOpen;
};

// ---------------------------------------------------------------------------
// F7: Custom IR cabinet dropdown (anchored under the speaker-row IR button).
// Callback-based so it needs no plugin methods; the plugin positions + shows it.
// ---------------------------------------------------------------------------
class VoLumIrMenuControl : public IControl
{
public:
  // code: >=0 picks IR index; -1 = Import; -2 = Manage; -3 = None (no custom IR).
  using SelectCallback = std::function<void(int code)>;

  explicit VoLumIrMenuControl(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallback(SelectCallback cb) { mCb = std::move(cb); }

  void SetItems(const std::vector<std::string>& irs, int selectedIdx)
  {
    mRows.clear();
    mRows.push_back({"No custom IR (use baked cab)", -3, false});
    for (int i = 0; i < (int)irs.size(); i++)
      mRows.push_back({irs[(size_t)i], i, false});
    mRows.push_back({"+ Import IR...", -1, true});
    mRows.push_back({"Manage IRs...", -2, true});
    mSelectedCode = selectedIdx;
    mHovered = -1;
    SetDirty(false);
  }

  static constexpr float kRowH = 21.f;
  static float MenuHeight(size_t irCount) { return 12.f + (float)(irCount + 3) * kRowH; }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(VoLumColors::HERO_BG, mRECT, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mRECT, 4.f, nullptr, 1.5f);
    DrawCornerAccent(g, mRECT.L + 5.f, mRECT.T + 5.f, 8.f, false, false, VoLumColors::TEAL_DIM);
    DrawCornerAccent(g, mRECT.R - 5.f, mRECT.B - 5.f, 8.f, true, true, VoLumColors::TEAL_DIM);

    float rowT = mRECT.T + 6.f;
    for (int i = 0; i < (int)mRows.size(); i++)
    {
      const auto& r = mRows[(size_t)i];
      const IRECT row(mRECT.L + 8.f, rowT, mRECT.R - 8.f, rowT + kRowH);
      rowT += kRowH;
      const bool selected = (r.code == mSelectedCode);
      if (selected)
      {
        g.FillRoundRect(VoLumColors::ITEM_SEL_BG, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
        g.DrawRoundRect(VoLumColors::ITEM_SEL_BORDER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      else if (i == mHovered)
      {
        g.FillRoundRect(VoLumColors::ITEM_HOVER, row.GetPadded(0.f, -2.f, 0.f, -2.f), 2.f);
      }
      const IColor col = r.action ? VoLumColors::TEAL : (selected ? VoLumColors::CREAM : VoLumColors::CREAM_DIM);
      g.DrawText(IText(12.f, col, "Josefin-Bold", EAlign::Near, EVAlign::Middle), r.label.c_str(),
                 IRECT(row.L + 12.f, row.T, row.R, row.B));
    }
  }

  void OnMouseDown(float, float y, const IMouseMod&) override
  {
    const int idx = RowAtY(y);
    if (idx >= 0 && idx < (int)mRows.size() && mCb)
      mCb(mRows[(size_t)idx].code);
    Hide(true);
  }

  void OnMouseOver(float, float y, const IMouseMod&) override
  {
    const int idx = RowAtY(y);
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

private:
  struct Row
  {
    std::string label;
    int code;
    bool action; // true = Import/Manage (teal), false = selectable IR / None
  };

  int RowAtY(float y) const
  {
    const int idx = (int)((y - (mRECT.T + 6.f)) / kRowH);
    return (idx >= 0 && idx < (int)mRows.size()) ? idx : -1;
  }

  std::vector<Row> mRows;
  int mSelectedCode = -3;
  int mHovered = -1;
  SelectCallback mCb;
};

// ---------------------------------------------------------------------------
// Overlay: Presets + Builder
// ---------------------------------------------------------------------------
class VoLumCustomOverlayControl : public IControl
{
public:
  using PresetChosenCallback = std::function<void(const char* name)>;
  using BuilderSavedCallback = std::function<void(const char* name)>;
  using PresetsChangedCallback = std::function<void()>;

  explicit VoLumCustomOverlayControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
    mEntryText = IText(14.f, VoLumColors::TEXT_BRIGHT, "Josefin-Bold", EAlign::Near, EVAlign::Middle, 0.f,
                       IColor(245, 14, 16, 22), VoLumColors::TEXT_BRIGHT);
  }

  // presetsChanged is fired whenever this amp's preset bank is mutated, so the
  // header bar can re-sync its list/label.
  void SetCallbacks(PresetChosenCallback presetCb, BuilderSavedCallback builderCb,
                    PresetsChangedCallback presetsChanged = nullptr)
  {
    mPresetChosen = std::move(presetCb);
    mBuilderSaved = std::move(builderCb);
    mPresetsChanged = std::move(presetsChanged);
  }

  void ShowPresets(int ampIdx, const char* ampName)
  {
    mScreen = volum::custom::Screen::Presets;
    mAmpIdx = ampIdx;
    mAmpName = ampName ? ampName : "";
    ReloadPresets();
    mPresetSel = -1;
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
      DrawPresets(g, body);
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

  void OnTextEntryCompletion(const char* str, int) override
  {
    using namespace volum::custom;
    const std::string s = str ? str : "";
    switch (mTextTarget)
    {
      case TextTarget::NewPreset:
        if (!s.empty())
        {
          const int i = AddPreset(mAmpIdx, s);
          ReloadPresets();
          mPresetSel = i;
          NotifyPresets();
        }
        break;
      case TextTarget::RenamePreset:
        if (mPresetSel >= 0)
        {
          RenamePreset(mAmpIdx, mPresetSel, s);
          ReloadPresets();
          NotifyPresets();
        }
        break;
      case TextTarget::ProfileName:
        if (!s.empty())
          mBuilderAmp.name = s;
        break;
      case TextTarget::NewCab:
        if (mTextFileIdx >= 0 && mTextFileIdx < (int)mBuilderAmp.files.size() && !s.empty())
        {
          auto& f = mBuilderAmp.files[(size_t)mTextFileIdx];
          f.speaker = s;
          if (f.channel < 1)
            f.channel = 1;
        }
        break;
      default:
        break;
    }
    mTextTarget = TextTarget::None;
    mTextFileIdx = -1;
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
    kPresetSaveNew,
    kPresetLoad,
    kPresetUpdate,
    kPresetRename,
    kPresetDelete,
    kAddFile,
    kBuilderSave,
    kEditName,
    kPresetRowBase = 100,
    kFileSpeakerBase = 200,
    kFileChannelBase = 300,
    kFileRemoveBase = 400,
    kPopupBase = 1000
  };

  enum class TextTarget
  {
    None,
    NewPreset,
    RenamePreset,
    ProfileName,
    NewCab
  };

  enum class PopupKind
  {
    Speaker,
    Channel
  };

  IRECT PanelRect() const
  {
    const float w = 780.f, h = 524.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }

  const char* TitleForScreen() const
  {
    return mScreen == volum::custom::Screen::Presets ? "Presets" : "Custom amp";
  }

  void AddHotspot(const IRECT& r, int action) { mHotspots.emplace_back(r, action); }

  void ResetTransient()
  {
    mPopupOpen = false;
    mTextTarget = TextTarget::None;
    mTextFileIdx = -1;
  }

  void ReloadPresets()
  {
    mPresets = volum::custom::MockPresetsForAmp(mAmpIdx);
    if (mPresetSel >= (int)mPresets.size())
      mPresetSel = mPresets.empty() ? -1 : (int)mPresets.size() - 1;
  }

  void NotifyPresets()
  {
    if (mPresetsChanged)
      mPresetsChanged();
  }

  volum::custom::CustomAmp NewBuilderAmp(const char* name)
  {
    volum::custom::CustomAmp a;
    a.name = (name && *name) ? name : "New custom amp";
    a.files = {{"(drop a .nam)", "", 0}};
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
      HandlePresetAction(action, rect);
      return;
    }
    HandleBuilderAction(action, rect);
  }

  void HandlePresetAction(int action, const IRECT& rect)
  {
    using namespace volum::custom;
    if (action >= kPresetRowBase && action < kPresetRowBase + 256)
    {
      mPresetSel = action - kPresetRowBase; // select only - explicit Load recalls
      SetDirty(false);
      return;
    }
    switch (action)
    {
      case kPresetSaveNew:
      {
        char def[24];
        std::snprintf(def, sizeof(def), "My preset %d", (int)mPresets.size() + 1);
        StartTextEntry(TextTarget::NewPreset, NameEntryRect(rect), def);
        break;
      }
      case kPresetLoad:
        if (mPresetSel >= 0 && mPresetSel < (int)mPresets.size())
        {
          if (mPresetChosen)
            mPresetChosen(mPresets[(size_t)mPresetSel].c_str());
          Hide(true);
        }
        break;
      case kPresetUpdate:
        // Overwrites the snapshot with the live state. No-op in the shell (no
        // real state), but kept as an explicit, labeled action.
        NotifyPresets();
        SetDirty(false);
        break;
      case kPresetRename:
        if (mPresetSel >= 0 && mPresetSel < (int)mPresets.size())
          StartTextEntry(TextTarget::RenamePreset, NameEntryRect(rect), mPresets[(size_t)mPresetSel]);
        break;
      case kPresetDelete:
        if (mPresetSel >= 0 && mPresetSel < (int)mPresets.size())
        {
          DeletePreset(mAmpIdx, mPresetSel);
          ReloadPresets();
          mPresetSel = -1;
          NotifyPresets();
          SetDirty(false);
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
      mBuilderAmp.files.push_back({fname, "", 0});
      SetDirty(false);
      return;
    }
    if (action == kBuilderSave)
    {
      if (UnassignedCount(mBuilderAmp) == 0 && !mBuilderAmp.files.empty())
      {
        if (mBuilderSaved)
          mBuilderSaved(mBuilderAmp.name.c_str());
        Hide(true);
      }
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
    using namespace volum::custom;
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    mPopupKind = PopupKind::Speaker;
    mPopupFileIdx = fileIdx;
    mPopupItems.clear();
    mPopupItems.push_back("DIRECT");
    for (const char* p : {"G12", "G65", "V30"})
      mPopupItems.push_back(p);
    // include any cab names already used in this amp that aren't a prefix above
    for (const auto& f : mBuilderAmp.files)
    {
      if (IsDirectSpeaker(f.speaker))
        continue;
      if (std::find(mPopupItems.begin(), mPopupItems.end(), f.speaker) == mPopupItems.end())
        mPopupItems.push_back(f.speaker);
    }
    mPopupItems.push_back("+ New cab name...");
    LayoutPopup(anchor);
  }

  void OpenChannelPopup(int fileIdx, const IRECT& anchor)
  {
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    mPopupKind = PopupKind::Channel;
    mPopupFileIdx = fileIdx;
    mPopupItems.clear();
    for (int c = 1; c <= 6; c++)
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
    const IRECT anchor = mPopupRect;
    if (mPopupKind == PopupKind::Speaker)
    {
      const std::string& label = mPopupItems[(size_t)j];
      if (label.rfind("+ ", 0) == 0) // "+ New cab name..."
      {
        mPopupOpen = false;
        StartTextEntry(TextTarget::NewCab, IRECT(anchor.L, anchor.T, anchor.R, anchor.T + 24.f), "", mPopupFileIdx);
        return;
      }
      f.speaker = (label == "DIRECT") ? kDirectSpeaker : label;
      if (f.channel < 1)
        f.channel = 1;
    }
    else // Channel
    {
      f.channel = j + 1;
      if (f.speaker.empty())
        f.speaker = kDirectSpeaker; // a channel implies at least a DIRECT capture
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
      const bool isNew = mPopupItems[(size_t)j].rfind("+ ", 0) == 0;
      g.DrawText(IText(11.f, isNew ? VoLumColors::TEAL : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
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

  /* ---------------- Presets screen ---------------- */

  void DrawPresets(IGraphics& g, const IRECT& body)
  {
    char sub[96];
    std::snprintf(sub, sizeof(sub), "for  %s", mAmpName.c_str());
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Top), sub, body.GetFromTop(14.f));

    const IRECT content(body.L, body.T + 22.f, body.R, body.B - 40.f);
    const IRECT listArea(content.L, content.T, content.L + content.W() * 0.6f, content.B);
    g.FillRect(IColor(235, 20, 20, 26), listArea);
    g.DrawRect(IColor(89, 200, 162, 78), listArea);

    if (mPresets.empty())
    {
      g.DrawText(IText(12.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "No presets for this amp yet.\nUse \"Save current as new\" to make the first.", listArea);
    }
    else
    {
      float y = listArea.T + 6.f;
      for (int i = 0; i < (int)mPresets.size(); i++)
      {
        const IRECT row(listArea.L + 6.f, y, listArea.R - 6.f, y + 30.f);
        const bool sel = (i == mPresetSel);
        if (sel)
        {
          g.FillRect(VoLumColors::ITEM_SEL_BG, row);
          g.DrawRect(VoLumColors::ITEM_SEL_BORDER, row);
        }
        else if (i == mPresetHover)
        {
          g.FillRect(VoLumColors::ITEM_HOVER, row);
        }
        if (sel)
          g.FillCircle(VoLumColors::GOLD, row.L + 12.f, row.MH(), 3.f);
        g.DrawText(IText(13.f, sel ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Near,
                         EVAlign::Middle),
                   mPresets[(size_t)i].c_str(), row.GetPadded(-24.f, 0.f, -8.f, 0.f));
        AddHotspot(row, kPresetRowBase + i);
        y += 34.f;
      }
    }

    // action column
    const IRECT actions(listArea.R + 16.f, content.T, body.R, content.B);
    const bool none = mPresetSel < 0;
    DrawButton(g, RowAt(actions, 0), "Save current as new", kPresetSaveNew, true);
    g.DrawText(IText(9.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top),
               none ? "SELECT A PRESET FOR:" : "SELECTED PRESET:", IRECT(actions.L, actions.T + 40.f, actions.R, actions.T + 52.f));
    DrawButton(g, RowAt(actions, 1, 52.f), "Load (recall)", kPresetLoad, false, false, none);
    DrawButton(g, RowAt(actions, 2, 52.f), "Update with current", kPresetUpdate, false, false, none);
    DrawButton(g, RowAt(actions, 3, 52.f), "Rename", kPresetRename, false, false, none);
    DrawButton(g, RowAt(actions, 4, 52.f), "Delete", kPresetDelete, false, false, none);

    DrawFooter(g, IRECT(body.L, body.B - 34.f, body.R, body.B),
               "A preset is a full snapshot of THIS amp: cab / IR, channel, AMP knobs, PRE pedals and POST FX.",
               "Load overwrites the live state. < > on the header cycle presets without opening this window.");
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
    const IRECT left(body.L, body.T, body.L + body.W() * 0.56f, body.B);
    const IRECT right(left.R + 18.f, body.T, body.R, body.B);

    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top), "PROFILE NAME (click to edit)",
               left.GetFromTop(12.f));
    const IRECT nameBox(left.L, left.T + 16.f, left.R, left.T + 42.f);
    g.FillRect(VoLumColors::BTN_OFF_BG, nameBox);
    g.DrawRect(VoLumColors::FRAME, nameBox);
    g.DrawText(IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Near, EVAlign::Middle), mBuilderAmp.name.c_str(),
               nameBox.GetPadded(-10.f, 0.f, -28.f, 0.f));
    g.DrawText(IText(12.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Far, EVAlign::Middle), "edit",
               nameBox.GetPadded(0.f, 0.f, -8.f, 0.f));
    AddHotspot(nameBox, kEditName);

    const IRECT drop(left.L, nameBox.B + 10.f, left.R, nameBox.B + 38.f);
    g.DrawDottedRect(VoLumColors::TEAL_DIM, drop, nullptr, 1.f, 4.f);
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
               "+ Add .nam file", drop);
    AddHotspot(drop, kAddFile);

    const int unassigned = UnassignedCount(mBuilderAmp);

    // file rows: filename | speaker | channel | remove
    float y = drop.B + 10.f;
    const float rowH = 26.f;
    for (int i = 0; i < (int)mBuilderAmp.files.size(); i++)
    {
      auto& f = mBuilderAmp.files[(size_t)i];
      const bool bad = !FileAssigned(f);
      const IRECT row(left.L, y, left.R, y + rowH);
      g.FillRect(bad ? IColor(28, 232, 168, 92) : VoLumColors::BTN_OFF_BG, row);
      g.DrawRect(bad ? VoLumColors::AMBER : VoLumColors::FRAME, row);

      const IRECT rem(row.R - 22.f, row.T + 3.f, row.R - 4.f, row.B - 3.f);
      const IRECT ch(rem.L - 56.f, row.T + 3.f, rem.L - 4.f, row.B - 3.f);
      const IRECT spk(ch.L - 116.f, row.T + 3.f, ch.L - 6.f, row.B - 3.f);

      g.DrawText(IText(10.5f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), f.file.c_str(),
                 IRECT(row.L + 8.f, row.T, spk.L - 6.f, row.B));

      // speaker chip
      const char* spkLabel = f.speaker.empty() ? "speaker v" : (IsDirectSpeaker(f.speaker) ? "DIRECT v" : f.speaker.c_str());
      g.FillRect(VoLumColors::HERO_BG, spk);
      g.DrawRect(f.speaker.empty() ? VoLumColors::AMBER : VoLumColors::TEAL_DIM, spk);
      g.DrawText(IText(10.f, f.speaker.empty() ? VoLumColors::AMBER : VoLumColors::GOLD, "Josefin-Bold", EAlign::Center,
                       EVAlign::Middle),
                 spkLabel, spk);
      AddHotspot(spk, kFileSpeakerBase + i);

      // channel chip
      char chLabel[12];
      if (f.channel >= 1)
        std::snprintf(chLabel, sizeof(chLabel), "Ch %d v", f.channel);
      else
        std::snprintf(chLabel, sizeof(chLabel), "Ch v");
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

    // right: live coverage grid derived from the manifest
    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top), "COVERAGE (speaker x channel)",
               right.GetFromTop(12.f));
    const auto speakers = AmpSpeakers(mBuilderAmp);
    float gy = right.T + 22.f;
    if (speakers.empty())
    {
      g.DrawText(IText(11.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top),
                 "Assign files to see the\nspeaker x channel grid fill in.", IRECT(right.L, gy, right.R, gy + 40.f));
    }
    int maxCh = 4;
    for (const auto& s : speakers)
      for (int c : AmpSpeakerChannels(mBuilderAmp, s))
        maxCh = std::max(maxCh, c);
    for (const auto& s : speakers)
    {
      const auto chans = AmpSpeakerChannels(mBuilderAmp, s);
      const IRECT srow(right.L, gy, right.R, gy + 24.f);
      g.DrawText(IText(10.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                 IsDirectSpeaker(s) ? "DIRECT" : s.c_str(), IRECT(srow.L, srow.T, srow.L + 76.f, srow.B));
      for (int c = 1; c <= maxCh; c++)
      {
        const float cw = 24.f;
        const IRECT cell(srow.L + 80.f + (c - 1) * cw, srow.T + 2.f, srow.L + 80.f + (c - 1) * cw + cw - 4.f, srow.B - 2.f);
        if (cell.R > right.R)
          break;
        const bool on = std::find(chans.begin(), chans.end(), c) != chans.end();
        g.FillRect(on ? VoLumColors::ITEM_SEL_BG : IColor(0, 0, 0, 0), cell);
        g.DrawRect(on ? VoLumColors::ITEM_SEL_BORDER : VoLumColors::FRAME, cell);
        if (on)
          g.FillCircle(VoLumColors::GOLD, cell.MW(), cell.MH(), 2.5f);
      }
      gy += 28.f;
    }

    const char* saveLabel = mBuilderAmp.files.empty() ? "Add a file first" : (unassigned == 0 ? "Save amp" : "Assign all files");
    DrawButton(g, IRECT(right.L, right.B - 32.f, right.R, right.B - 4.f), saveLabel, kBuilderSave, true, false,
               unassigned != 0 || mBuilderAmp.files.empty());
  }

  volum::custom::Screen mScreen = volum::custom::Screen::Presets;
  int mAmpIdx = 0;
  std::string mAmpName;
  std::vector<std::string> mPresets;
  int mPresetSel = -1;
  int mPresetHover = -1;
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
  IText mEntryText;

  PresetChosenCallback mPresetChosen;
  BuilderSavedCallback mBuilderSaved;
  PresetsChangedCallback mPresetsChanged;
};
