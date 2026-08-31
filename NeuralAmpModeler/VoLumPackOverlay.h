#pragma once

// VoLumPackOverlayControl: the Export Pack / Import Pack modal behind the two
// Settings rows.
//
// Two screens in one control because they are two halves of one conversation:
// Export asks "what travels", Import shows "what this will do to your library and
// which of it is playing right now". Both have to be answered before anything is
// written, and neither is a question a file dialog can ask.
//
// The rules are all in VoLumPack.h. This file owns the chrome and nothing else:
// it reads the library to fill its lists, and hands a selection or a verb back to
// the plugin, which does the file IO. That split is why the permutations are
// testable without an editor.

#include "VoLumColorHelpers.h"
#include "VoLumPack.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class VoLumPackOverlayControl : public IControl
{
public:
  // Returns an error string, or "" on success. The plugin owns the file dialog
  // and the write, so a cancelled dialog is a silent "".
  using ExportCallback = std::function<std::string(const volum::pack::ExportSelection&)>;
  // Picks a Pack file and opens it; empty `ok` carries the reason in `.error`.
  using PickPackCallback = std::function<volum::pack::PackContents()>;
  using ImportCallback = std::function<std::string(volum::pack::ImportVerb verb, bool alsoSettings)>;

  explicit VoLumPackOverlayControl(const IRECT& fullBounds)
  : IControl(fullBounds)
  {
    mIgnoreMouse = false;
  }

  void SetCallbacks(ExportCallback exportCb, PickPackCallback pickCb, ImportCallback importCb)
  {
    mExport = std::move(exportCb);
    mPick = std::move(pickCb);
    mImport = std::move(importCb);
  }

  // Standalone gets the machine-settings checkbox; a plugin never writes
  // volum-settings.json, so it does not get the offer either.
  void SetStandalone(bool standalone) { mStandalone = standalone; }

  // The Pack the user opened, for the import callback to apply.
  const volum::pack::PackContents& OpenedPack() const { return mPack; }

  // The ids this instance's rig is playing, so the import preview can name what
  // would have to reload.
  void SetSoundingIds(std::vector<std::string> ids) { mSoundingIds = std::move(ids); }

  void ShowExport()
  {
    mScreen = Screen::Export;
    mEverything = true;
    mScroll = 0;
    mStatus.clear();
    _RebuildRows();
    Hide(false);
    SetDirty(false);
  }

  // Opens the picker straight away: "Import Pack..." means the user has already
  // decided to look at a file, and a screen that only holds a Browse button is a
  // click in the way.
  void ShowImport()
  {
    mScreen = Screen::Import;
    mScroll = 0;
    mStatus.clear();
    mVerb = volum::pack::ImportVerb::Overwrite;
    mAlsoSettings = false;
    mPack = volum::pack::PackContents{};
    Hide(false);
    if (mPick)
      mPack = mPick();
    if (!mPack.ok)
    {
      if (mPack.error.empty())
      {
        Hide(true); // cancelled picker: nothing to report
        return;
      }
      mStatus = mPack.error;
    }
    _RebuildPreview();
    SetDirty(false);
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(IColor(190, 8, 10, 14), mRECT); // scrim
    const IRECT box = _BoxRect();
    g.FillRoundRect(VoLumColors::SEL_GLOW, box.GetPadded(3.5f), 9.f);
    DrawPanelDepth(g, box, 6.f);
    g.DrawRoundRect(VoLumColors::AMBER, box, 6.f, nullptr, 1.6f);

    const IRECT inner = box.GetPadded(-18.f);
    g.DrawText(IText(15.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Top),
               mScreen == Screen::Export ? "EXPORT PACK" : "IMPORT PACK", inner.GetFromTop(20.f));

    if (mScreen == Screen::Export)
      _DrawExport(g, inner);
    else
      _DrawImport(g, inner);

    _DrawBtn(g, _CancelRect(), "Cancel", false);
    _DrawBtn(g, _GoRect(), mScreen == Screen::Export ? "Export..." : "Import", true);

    if (!mStatus.empty())
      g.DrawText(IText(10.5f, VoLumColors::DANGER, "Josefin-Sans", EAlign::Center, EVAlign::Middle), mStatus.c_str(),
                 IRECT(box.L + 18.f, _GoRect().T - 20.f, box.R - 18.f, _GoRect().T - 4.f));
  }

  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (IsHidden())
      return false;
    if (key.VK == kVK_ESCAPE)
    {
      Hide(true);
      return true;
    }
    return false;
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    if (!_ListRect().Contains(x, y))
      return;
    const int visible = _VisibleRows();
    const int total = mScreen == Screen::Export ? (int)mRows.size() : (int)mPreviewLines.size();
    mScroll = std::max(0, std::min(mScroll - (int)(d > 0 ? 1 : -1) * 2, std::max(0, total - visible)));
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (_GoRect().Contains(x, y))
    {
      _Go();
      return;
    }
    if (_CancelRect().Contains(x, y) || !_BoxRect().Contains(x, y))
    {
      Hide(true);
      return;
    }

    if (mScreen == Screen::Export)
    {
      if (_ScopeRect(0).Contains(x, y) || _ScopeRect(1).Contains(x, y))
      {
        mEverything = _ScopeRect(0).Contains(x, y);
        mScroll = 0;
        SetDirty(false);
        return;
      }
      if (!mEverything && _ListRect().Contains(x, y))
      {
        const int row = mScroll + (int)((y - _ListRect().T) / kRowH);
        if (row >= 0 && row < (int)mRows.size())
        {
          mRows[row].picked = !mRows[row].picked;
          _RecomputeClosure();
          SetDirty(false);
        }
      }
      return;
    }

    // Import screen.
    if (_VerbsVisible())
      for (int i = 0; i < 3; ++i)
        if (_VerbRect(i).Contains(x, y))
        {
          mVerb = (volum::pack::ImportVerb)i;
          _RebuildPreview();
          SetDirty(false);
          return;
        }
    if (_SettingsBoxVisible() && _SettingsCheckRect().Contains(x, y))
    {
      mAlsoSettings = !mAlsoSettings;
      _RebuildPreview();
      SetDirty(false);
    }
  }

private:
  enum class Screen
  {
    Export,
    Import
  };

  struct Row
  {
    bool isAmp = true;
    std::string id;
    std::string label;
    bool picked = false;
    bool required = false; // dragged in by the closure: shown ticked and locked
  };

  static constexpr float kRowH = 20.f;

  IRECT _BoxRect() const
  {
    const float w = std::min(560.f, mRECT.W() - 40.f);
    const float h = std::min(408.f, mRECT.H() - 40.f);
    return IRECT(mRECT.MW() - w / 2.f, mRECT.MH() - h / 2.f, mRECT.MW() + w / 2.f, mRECT.MH() + h / 2.f);
  }
  IRECT _CancelRect() const
  {
    const IRECT box = _BoxRect();
    return IRECT(box.MW() - 150.f, box.B - 46.f, box.MW() - 8.f, box.B - 20.f);
  }
  IRECT _GoRect() const
  {
    const IRECT box = _BoxRect();
    return IRECT(box.MW() + 8.f, box.B - 46.f, box.MW() + 150.f, box.B - 20.f);
  }
  IRECT _ScopeRect(int which) const
  {
    const IRECT box = _BoxRect();
    const float t = box.T + 44.f + which * 22.f;
    return IRECT(box.L + 18.f, t, box.R - 18.f, t + 20.f);
  }
  IRECT _ListRect() const
  {
    const IRECT box = _BoxRect();
    const float top = mScreen == Screen::Export ? box.T + 94.f : box.T + 66.f;
    const float bottom = mScreen == Screen::Export ? box.B - 90.f : box.B - (_VerbsVisible() ? 126.f : 66.f);
    return IRECT(box.L + 18.f, top, box.R - 18.f, bottom);
  }
  int _VisibleRows() const { return std::max(1, (int)(_ListRect().H() / kRowH)); }
  bool _VerbsVisible() const
  {
    // The three verbs are a FULL-import question. A Share Pack is a merge by id -
    // Overwrite's rule - and offering Reset for it would invite someone to wipe
    // their library with a Pack a friend sent them.
    return mScreen == Screen::Import && mPack.ok && mPack.job == volum::pack::Job::Everything;
  }
  bool _SettingsBoxVisible() const
  {
    return mScreen == Screen::Import && mPack.ok && mStandalone && !mPack.settingsJson.empty();
  }
  IRECT _VerbRect(int i) const
  {
    const IRECT box = _BoxRect();
    const float w = (box.W() - 36.f) / 3.f;
    const float t = box.B - 118.f;
    return IRECT(box.L + 18.f + i * w, t, box.L + 18.f + (i + 1) * w - 6.f, t + 22.f);
  }
  IRECT _SettingsCheckRect() const
  {
    const IRECT box = _BoxRect();
    return IRECT(box.L + 18.f, box.B - 72.f, box.R - 18.f, box.B - 54.f);
  }

  // -- Export ------------------------------------------------------------------

  void _RebuildRows()
  {
    mRows.clear();
    const auto& reg = volum::content::GlobalContentStore().reg();
    for (const auto& a : reg.amps)
      mRows.push_back({true, a.id, "Custom amp   " + a.name, false, false});
    for (const auto& bank : reg.presetBanks)
      for (const auto& pr : bank.second)
        mRows.push_back({false, pr.id, "Preset          " + pr.name, false, false});
    mAlsoIncluding.clear();
  }

  volum::pack::ExportSelection _Selection() const
  {
    volum::pack::ExportSelection sel;
    sel.everything = mEverything;
    for (const auto& r : mRows)
      if (r.picked)
        (r.isAmp ? sel.ampIds : sel.presetIds).push_back(r.id);
    return sel;
  }

  // Re-run the closure so the preview can say what the ticks dragged along, and
  // lock those rows: a requirement is not a choice.
  void _RecomputeClosure()
  {
    const auto plan = volum::pack::BuildExportPlan(volum::content::GlobalContentStore().reg(), _Selection());
    mAlsoIncluding.clear();
    for (const auto& s : plan.alsoIncluding)
      if (std::find(mAlsoIncluding.begin(), mAlsoIncluding.end(), s) == mAlsoIncluding.end())
        mAlsoIncluding.push_back(s);
    for (auto& r : mRows)
    {
      const auto& ids = r.isAmp ? plan.ampIds : plan.presetIds;
      r.required = !r.picked && std::find(ids.begin(), ids.end(), r.id) != ids.end();
    }
  }

  void _DrawExport(IGraphics& g, const IRECT& inner)
  {
    _DrawRadio(g, _ScopeRect(0), "Everything - your whole library, MIDI slots and machine settings", mEverything);
    _DrawRadio(g, _ScopeRect(1), "Selected amps and presets", !mEverything);

    const IRECT list = _ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);

    if (mEverything)
    {
      g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "Everything in your library travels, including unused IRs and pedals.", list);
    }
    else
    {
      g.PathClipRegion(list);
      const int visible = _VisibleRows();
      for (int i = 0; i < visible && mScroll + i < (int)mRows.size(); ++i)
      {
        const Row& r = mRows[mScroll + i];
        const IRECT rowR(list.L + 4.f, list.T + i * kRowH, list.R - 4.f, list.T + (i + 1) * kRowH);
        const IRECT boxR(rowR.L + 2.f, rowR.MH() - 5.f, rowR.L + 12.f, rowR.MH() + 5.f);
        const bool on = r.picked || r.required;
        g.DrawRect(r.required ? VoLumColors::TEXT_DIM : VoLumColors::FRAME, boxR);
        if (on)
          g.FillRect(r.required ? VoLumColors::TEXT_DIM : VoLumColors::AMBER, boxR.GetPadded(-2.5f));
        g.DrawText(IText(11.f, r.required ? VoLumColors::TEXT_DIM : VoLumColors::CREAM, "Josefin-Sans", EAlign::Near,
                         EVAlign::Middle),
                   r.label.c_str(), IRECT(rowR.L + 18.f, rowR.T, rowR.R, rowR.B));
      }
      if (mRows.empty())
        g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                   "No custom amps or presets in your library yet.", list);
      g.PathClipRegion();
    }

    const IRECT alsoR(inner.L, list.B + 4.f, inner.R, list.B + 34.f);
    std::string also;
    for (const auto& s : mAlsoIncluding)
      also += (also.empty() ? "" : ", ") + s;
    if (!mEverything)
      g.DrawText(IText(10.5f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top),
                 also.empty() ? "Requirements are added automatically." : ("Also including: " + also).c_str(), alsoR);
  }

  // -- Import ------------------------------------------------------------------

  void _RebuildPreview()
  {
    mPreviewLines.clear();
    if (!mPack.ok)
      return;
    const auto preview = volum::pack::BuildImportPreview(
      volum::content::GlobalContentStore().reg(), mPack, mVerb, mAlsoSettings, mStandalone, mSoundingIds);
    for (const auto& s : preview.adds)
      mPreviewLines.push_back({"Add", s, VoLumColors::CREAM});
    for (const auto& s : preview.replaces)
      mPreviewLines.push_back({"Replace", s, VoLumColors::AMBER});
    for (const auto& s : preview.inUseReloads)
      mPreviewLines.push_back({"Reloads", s + " - playing now", VoLumColors::GOLD});
    for (const auto& s : preview.nameCollisions)
      mPreviewLines.push_back({"Same name", s + " - both are kept", VoLumColors::TEXT_DIM});
    for (const auto& s : preview.removals)
      mPreviewLines.push_back({"Delete", s, VoLumColors::DANGER});
    if (preview.writesSettings)
      mPreviewLines.push_back({"Restore", "machine settings and MIDI slots", VoLumColors::AMBER});
    if (preview.Empty() && mPreviewLines.empty())
      mPreviewLines.push_back({"", "Nothing to do - your library already matches this Pack.", VoLumColors::TEXT_DIM});
    mScroll = 0;
  }

  void _DrawImport(IGraphics& g, const IRECT& inner)
  {
    const IRECT headR(inner.L, inner.T + 22.f, inner.R, inner.T + 40.f);
    auto count = [](size_t n, const char* one, const char* many) {
      return std::to_string(n) + " " + (n == 1 ? one : many);
    };
    std::string head;
    if (mPack.ok)
      head = std::string(mPack.job == volum::pack::Job::Everything ? "Everything Pack" : "Share Pack") + "  -  "
             + count(mPack.library.amps.size(), "amp", "amps") + ", " + count(mPack.library.irs.size(), "IR", "IRs")
             + ", " + count(mPack.library.pedals.size(), "pedal", "pedals");
    else
      head = "No Pack opened.";
    g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle), head.c_str(), headR);

    const IRECT list = _ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);
    g.PathClipRegion(list);
    const int visible = _VisibleRows();
    for (int i = 0; i < visible && mScroll + i < (int)mPreviewLines.size(); ++i)
    {
      const auto& line = mPreviewLines[mScroll + i];
      const IRECT rowR(list.L + 6.f, list.T + i * kRowH, list.R - 6.f, list.T + (i + 1) * kRowH);
      g.DrawText(IText(10.5f, line.color, "Josefin-Bold", EAlign::Near, EVAlign::Middle), line.verb.c_str(),
                 IRECT(rowR.L, rowR.T, rowR.L + 64.f, rowR.B));
      g.DrawText(IText(11.f, line.color, "Josefin-Sans", EAlign::Near, EVAlign::Middle), line.what.c_str(),
                 IRECT(rowR.L + 68.f, rowR.T, rowR.R, rowR.B));
    }
    g.PathClipRegion();

    if (_VerbsVisible())
    {
      static const char* kVerbNames[3] = {"Overwrite", "Add", "Reset"};
      static const char* kVerbHelp[3] = {"Pack wins, mine stay", "keep mine", "Pack wins, mine deleted"};
      for (int i = 0; i < 3; ++i)
      {
        const IRECT r = _VerbRect(i);
        const bool on = (int)mVerb == i;
        g.FillRoundRect(on ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, r, 3.f);
        g.DrawRoundRect(on ? VoLumColors::AMBER : VoLumColors::FRAME, r, 3.f, nullptr, on ? 1.4f : 1.f);
        g.DrawText(IText(11.f, on ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                         EVAlign::Middle),
                   kVerbNames[i], r.GetFromTop(14.f).GetVShifted(2.f));
        g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle), kVerbHelp[i],
                   IRECT(r.L, r.B, r.R, r.B + 12.f));
      }
    }

    if (_SettingsBoxVisible())
    {
      const IRECT r = _SettingsCheckRect();
      const IRECT boxR(r.L, r.MH() - 6.f, r.L + 12.f, r.MH() + 6.f);
      g.DrawRect(VoLumColors::FRAME, boxR);
      if (mAlsoSettings)
        g.FillRect(VoLumColors::AMBER, boxR.GetPadded(-2.5f));
      g.DrawText(IText(11.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
                 "Also restore machine settings (last amp, scenes, Lite, calibration, MIDI slots)",
                 IRECT(r.L + 18.f, r.T, r.R, r.B));
    }
  }

  // -- Shared ------------------------------------------------------------------

  void _Go()
  {
    mStatus.clear();
    if (mScreen == Screen::Export)
    {
      if (!mExport)
        return;
      const std::string err = mExport(_Selection());
      if (err.empty())
        Hide(true);
      else
        mStatus = err;
      SetDirty(false);
      return;
    }
    if (!mPack.ok || !mImport)
    {
      mStatus = mPack.error.empty() ? "No Pack opened." : mPack.error;
      SetDirty(false);
      return;
    }
    const std::string err = mImport(mVerb, mAlsoSettings);
    if (err.empty())
      Hide(true);
    else
      mStatus = err;
    SetDirty(false);
  }

  void _DrawRadio(IGraphics& g, const IRECT& r, const char* label, bool on)
  {
    const float cy = r.MH();
    g.DrawCircle(on ? VoLumColors::AMBER : VoLumColors::FRAME, r.L + 6.f, cy, 5.5f, nullptr, 1.2f);
    if (on)
      g.FillCircle(VoLumColors::AMBER, r.L + 6.f, cy, 3.f);
    g.DrawText(
      IText(11.5f, on ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
      label, IRECT(r.L + 18.f, r.T, r.R, r.B));
  }

  void _DrawBtn(IGraphics& g, const IRECT& r, const char* label, bool primary)
  {
    g.FillRoundRect(primary ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, r, 3.f);
    g.DrawRoundRect(primary ? VoLumColors::AMBER : VoLumColors::FRAME, r, 3.f, nullptr, primary ? 1.3f : 1.f);
    g.DrawText(IText(12.f, primary ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM, "Josefin-Bold", EAlign::Center,
                     EVAlign::Middle),
               label, r);
  }

  struct PreviewLine
  {
    std::string verb;
    std::string what;
    IColor color;
  };

  Screen mScreen = Screen::Export;
  bool mEverything = true;
  bool mStandalone = false;
  bool mAlsoSettings = false;
  volum::pack::ImportVerb mVerb = volum::pack::ImportVerb::Overwrite;
  int mScroll = 0;
  std::string mStatus;
  std::vector<Row> mRows;
  std::vector<std::string> mAlsoIncluding;
  std::vector<PreviewLine> mPreviewLines;
  std::vector<std::string> mSoundingIds;
  volum::pack::PackContents mPack;

  ExportCallback mExport;
  PickPackCallback mPick;
  ImportCallback mImport;
};
