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

  explicit VoLumCustomOverlayControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallbacks(PresetChosenCallback presetCb, BuilderSavedCallback builderCb)
  {
    mPresetChosen = std::move(presetCb);
    mBuilderSaved = std::move(builderCb);
  }

  void ShowPresets(int ampIdx, const char* ampName)
  {
    mScreen = volum::custom::Screen::Presets;
    mAmpIdx = ampIdx;
    mAmpName = ampName ? ampName : "";
    mPresets = volum::custom::MockPresetsForAmp(ampIdx);
    mPresetSel = mPresets.empty() ? -1 : 0;
    Hide(false);
    SetDirty(false);
  }

  void ShowBuilder(bool editExisting, const char* ampName)
  {
    mScreen = volum::custom::Screen::Builder;
    mBuilderAmp = editExisting ? volum::custom::MockDemoCustomAmp() : NewBuilderAmp(ampName);
    Hide(false);
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    mHotspots.clear();
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

    const IRECT body = panel.GetPadded(-20.f, -56.f, -20.f, -18.f);
    if (mScreen == volum::custom::Screen::Presets)
      DrawPresets(g, body);
    else if (mScreen == volum::custom::Screen::Builder)
      DrawBuilder(g, body);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (!PanelRect().Contains(x, y))
    {
      Hide(true);
      return;
    }
    for (const auto& hs : mHotspots)
      if (hs.first.Contains(x, y))
      {
        HandleAction(hs.second);
        return;
      }
  }

private:
  enum Action
  {
    kClose = 0,
    kPresetSaveNew,
    kPresetRename,
    kPresetDelete,
    kAddFile,
    kBuilderSave,
    kPresetRowBase = 100,
    kFileSpeakerBase = 200,
    kFileChannelBase = 300
  };

  IRECT PanelRect() const
  {
    const float w = 780.f, h = 500.f;
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }

  const char* TitleForScreen() const
  {
    return mScreen == volum::custom::Screen::Presets ? "Presets" : "Custom amp";
  }

  void AddHotspot(const IRECT& r, int action) { mHotspots.emplace_back(r, action); }

  volum::custom::CustomAmp NewBuilderAmp(const char* name)
  {
    volum::custom::CustomAmp a;
    a.name = (name && *name) ? name : "New custom amp";
    a.files = {{"(drop a .nam)", "", 0}};
    return a;
  }

  void HandleAction(int action)
  {
    using namespace volum::custom;
    if (action == kClose)
    {
      Hide(true);
      return;
    }
    if (mScreen == Screen::Presets)
    {
      if (action >= kPresetRowBase && action < kPresetRowBase + 64)
      {
        mPresetSel = action - kPresetRowBase;
        if (mPresetSel >= 0 && mPresetSel < (int)mPresets.size() && mPresetChosen)
          mPresetChosen(mPresets[mPresetSel].c_str());
        Hide(true); // recall closes the browser
        return;
      }
      if (action == kPresetSaveNew)
      {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "New preset %d", (int)mPresets.size() + 1);
        mPresets.push_back(buf);
        mPresetSel = (int)mPresets.size() - 1;
      }
      else if (action == kPresetDelete && mPresetSel >= 0 && mPresetSel < (int)mPresets.size())
      {
        mPresets.erase(mPresets.begin() + mPresetSel);
        mPresetSel = mPresets.empty() ? -1 : std::min(mPresetSel, (int)mPresets.size() - 1);
      }
      // Rename is a no-op stub in the shell.
      SetDirty(false);
      return;
    }

    // Builder
    if (action == kAddFile)
    {
      mBuilderAmp.files.push_back({"(new .nam)", "", 0});
    }
    else if (action == kBuilderSave)
    {
      if (UnassignedCount(mBuilderAmp) == 0)
      {
        if (mBuilderSaved)
          mBuilderSaved(mBuilderAmp.name.c_str());
        Hide(true);
        return;
      }
    }
    else if (action >= kFileSpeakerBase && action < kFileSpeakerBase + 64)
    {
      CycleSpeaker(action - kFileSpeakerBase);
    }
    else if (action >= kFileChannelBase && action < kFileChannelBase + 64)
    {
      CycleChannel(action - kFileChannelBase);
    }
    SetDirty(false);
  }

  void CycleSpeaker(int fileIdx)
  {
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    static const char* kCandidates[] = {volum::custom::kDirectSpeaker, "G65 4x12", "V30 2x12", "1960A 4x12"};
    const int n = (int)(sizeof(kCandidates) / sizeof(kCandidates[0]));
    auto& f = mBuilderAmp.files[fileIdx];
    int cur = -1;
    for (int i = 0; i < n; i++)
      if (f.speaker == kCandidates[i])
        cur = i;
    f.speaker = kCandidates[(cur + 1) % n];
    if (f.channel < 1)
      f.channel = 1; // assigning a speaker implies a channel
  }

  void CycleChannel(int fileIdx)
  {
    if (fileIdx < 0 || fileIdx >= (int)mBuilderAmp.files.size())
      return;
    auto& f = mBuilderAmp.files[fileIdx];
    f.channel = (f.channel >= 4 || f.channel < 1) ? 1 : f.channel + 1;
  }

  /* ---------------- shared draw helpers ---------------- */

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

  void DrawHint(IGraphics& g, const IRECT& r, const char* text)
  {
    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top), text, r);
  }

  /* ---------------- Presets screen ---------------- */

  void DrawPresets(IGraphics& g, const IRECT& body)
  {
    char sub[96];
    std::snprintf(sub, sizeof(sub), "for  %s", mAmpName.c_str());
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Top), sub, body.GetFromTop(14.f));

    const IRECT listArea(body.L, body.T + 22.f, body.L + body.W() * 0.62f, body.B - 40.f);
    g.FillRect(IColor(235, 20, 20, 26), listArea);
    g.DrawRect(IColor(89, 200, 162, 78), listArea);

    if (mPresets.empty())
    {
      g.DrawText(IText(12.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "No presets yet for this amp - Save current creates the first.", listArea);
    }
    else
    {
      float y = listArea.T + 6.f;
      for (int i = 0; i < (int)mPresets.size(); i++)
      {
        const IRECT row(listArea.L + 6.f, y, listArea.R - 6.f, y + 28.f);
        const bool sel = (i == mPresetSel);
        if (sel)
        {
          g.FillRect(VoLumColors::ITEM_SEL_BG, row);
          g.DrawRect(VoLumColors::ITEM_SEL_BORDER, row);
        }
        g.DrawText(IText(13.f, sel ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Near,
                         EVAlign::Middle),
                   mPresets[i].c_str(), row.GetPadded(-8.f, 0.f, -8.f, 0.f));
        g.DrawText(IText(9.f, VoLumColors::TEAL, "Josefin-Sans", EAlign::Far, EVAlign::Middle), "recall >",
                   row.GetPadded(-8.f, 0.f, -8.f, 0.f));
        AddHotspot(row, kPresetRowBase + i);
        y += 32.f;
      }
    }

    // action column
    const IRECT actions(listArea.R + 14.f, listArea.T, body.R, listArea.B);
    DrawButton(g, IRECT(actions.L, actions.T, actions.R, actions.T + 28.f), "Save current as new", kPresetSaveNew, true);
    DrawButton(g, IRECT(actions.L, actions.T + 34.f, actions.R, actions.T + 62.f), "Rename", kPresetRename, false, false,
               mPresetSel < 0);
    DrawButton(g, IRECT(actions.L, actions.T + 68.f, actions.R, actions.T + 96.f), "Delete", kPresetDelete, false, false,
               mPresetSel < 0);

    DrawHint(g, IRECT(body.L, body.B - 34.f, body.R, body.B),
             "A preset is a named snapshot of THIS amp's whole state - speaker/cab (incl. a custom IR), channel, AMP "
             "knobs, PRE pedals and POST FX. Recall overwrites the live state. < > on the header cycle these without "
             "opening the browser (footswitch-mappable). Global cross-amp banks come later.");
  }

  /* ---------------- Builder screen ---------------- */

  void DrawBuilder(IGraphics& g, const IRECT& body)
  {
    using namespace volum::custom;
    const IRECT left(body.L, body.T, body.L + body.W() * 0.56f, body.B);
    const IRECT right(left.R + 18.f, body.T, body.R, body.B);

    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top), "PROFILE NAME",
               left.GetFromTop(12.f));
    const IRECT nameBox(left.L, left.T + 16.f, left.R, left.T + 42.f);
    g.FillRect(VoLumColors::BTN_OFF_BG, nameBox);
    g.DrawRect(VoLumColors::FRAME, nameBox);
    g.DrawText(IText(12.f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Near, EVAlign::Middle), mBuilderAmp.name.c_str(),
               nameBox.GetPadded(-8.f, 0.f, -8.f, 0.f));

    const IRECT drop(left.L, nameBox.B + 10.f, left.R, nameBox.B + 38.f);
    g.DrawDottedRect(VoLumColors::FRAME, drop, nullptr, 1.f, 4.f);
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
               "  + Add .nam files...   (any speaker / channel)", drop);
    AddHotspot(drop, kAddFile);

    const int unassigned = UnassignedCount(mBuilderAmp);
    if (unassigned > 0)
    {
      char warn[64];
      std::snprintf(warn, sizeof(warn), "%d file(s) need a speaker + channel", unassigned);
      const IRECT w(left.L, drop.B + 6.f, left.R, drop.B + 22.f);
      g.DrawText(IText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), warn, w);
    }

    // file rows: filename | speaker (cycle) | channel (cycle)
    float y = drop.B + 26.f;
    for (int i = 0; i < (int)mBuilderAmp.files.size(); i++)
    {
      auto& f = mBuilderAmp.files[i];
      const bool bad = !FileAssigned(f);
      const IRECT row(left.L, y, left.R, y + 26.f);
      g.FillRect(bad ? IColor(28, 232, 168, 92) : VoLumColors::BTN_OFF_BG, row);
      g.DrawRect(bad ? VoLumColors::AMBER : VoLumColors::FRAME, row);

      g.DrawText(IText(11.f, VoLumColors::TEXT_MED, "Josefin-Sans", EAlign::Near, EVAlign::Middle), f.file.c_str(),
                 IRECT(row.L + 8.f, row.T, row.L + row.W() * 0.46f, row.B));

      // speaker chip (click cycles)
      const IRECT spk(row.L + row.W() * 0.46f, row.T + 3.f, row.R - 64.f, row.B - 3.f);
      const char* spkLabel = f.speaker.empty() ? "- speaker -" : (IsDirectSpeaker(f.speaker) ? "DIRECT" : f.speaker.c_str());
      g.DrawRect(VoLumColors::FRAME, spk);
      g.DrawText(IText(10.f, f.speaker.empty() ? VoLumColors::AMBER : VoLumColors::GOLD, "Josefin-Bold", EAlign::Center,
                       EVAlign::Middle),
                 spkLabel, spk);
      AddHotspot(spk, kFileSpeakerBase + i);

      // channel chip (click cycles)
      const IRECT ch(row.R - 58.f, row.T + 3.f, row.R - 4.f, row.B - 3.f);
      char chLabel[12];
      if (f.channel >= 1)
        std::snprintf(chLabel, sizeof(chLabel), "Ch %d", f.channel);
      else
        std::snprintf(chLabel, sizeof(chLabel), "Ch -");
      g.DrawRect(VoLumColors::FRAME, ch);
      g.DrawText(IText(10.f, VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Center, EVAlign::Middle), chLabel, ch);
      AddHotspot(ch, kFileChannelBase + i);

      y += 30.f;
    }
    DrawHint(g, IRECT(left.L, y + 2.f, left.R, y + 40.f),
             "Click the speaker/channel chips to assign. DIRECT = amp-only; pair it with a custom IR cab to cover all "
             "channels. Sparse is fine. Stored as a per-amp manifest (no file renaming).");

    // right: live coverage grid derived from the manifest
    g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Top), "COVERAGE",
               right.GetFromTop(12.f));
    const auto speakers = AmpSpeakers(mBuilderAmp);
    float gy = right.T + 22.f;
    if (speakers.empty())
    {
      g.DrawText(IText(11.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top),
                 "Assign files to see the\nspeaker x channel grid fill in.", IRECT(right.L, gy, right.R, gy + 40.f));
    }
    for (const auto& s : speakers)
    {
      const auto chans = AmpSpeakerChannels(mBuilderAmp, s);
      const IRECT srow(right.L, gy, right.R, gy + 24.f);
      g.DrawText(IText(10.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
                 IsDirectSpeaker(s) ? "DIRECT" : s.c_str(), IRECT(srow.L, srow.T, srow.L + 86.f, srow.B));
      for (int c = 1; c <= 4; c++)
      {
        const IRECT cell(srow.L + 90.f + (c - 1) * 26.f, srow.T + 2.f, srow.L + 90.f + (c - 1) * 26.f + 22.f, srow.B - 2.f);
        const bool on = std::find(chans.begin(), chans.end(), c) != chans.end();
        g.FillRect(on ? VoLumColors::ITEM_SEL_BG : IColor(0, 0, 0, 0), cell);
        g.DrawRect(on ? VoLumColors::ITEM_SEL_BORDER : VoLumColors::FRAME, cell);
        if (on)
          g.FillCircle(VoLumColors::GOLD, cell.MW(), cell.MH(), 2.5f);
      }
      gy += 28.f;
    }

    DrawButton(g, IRECT(right.L, right.B - 32.f, right.R, right.B - 4.f), unassigned == 0 ? "Save amp" : "Assign all files",
               kBuilderSave, true, false, unassigned != 0);
  }

  volum::custom::Screen mScreen = volum::custom::Screen::Presets;
  int mAmpIdx = 0;
  std::string mAmpName;
  std::vector<std::string> mPresets;
  int mPresetSel = -1;
  volum::custom::CustomAmp mBuilderAmp;
  std::vector<std::pair<IRECT, int>> mHotspots;
  PresetChosenCallback mPresetChosen;
  BuilderSavedCallback mBuilderSaved;
};
