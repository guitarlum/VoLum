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
    mScope = Scope::Everything;
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
      _DrawExport(g);
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
      for (int i = 0; i < kScopeCount; ++i)
        if (_ScopeRect(i).Contains(x, y))
        {
          if (mScope != (Scope)i)
          {
            // Ticks do not survive a scope change on purpose: "this Sound" and
            // "this whole amp" are different questions, so carrying an answer
            // across would export something the user never looked at.
            mScope = (Scope)i;
            mScroll = 0;
            _RebuildRows();
          }
          SetDirty(false);
          return;
        }
      if (mScope != Scope::Everything && _ListRect().Contains(x, y))
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

  // What travels. Three questions a player actually asks, mapped onto the two
  // fields ExportSelection already has: Everything is the whole library, Sounds
  // ticks presets, Amps ticks custom amps and lets BuildExportPlan pull each
  // amp's whole bank in behind it.
  enum class Scope
  {
    Everything,
    Sounds,
    Amps
  };
  static constexpr int kScopeCount = 3;

  struct Row
  {
    bool isAmp = true;
    std::string id;
    std::string label;
    std::string sub; // the amp a Sound belongs to, or an amp's preset count
    int pc = -1; // Program Change slot this Sound is on in PLAY, or -1
    bool picked = false;
    bool required = false; // dragged in by the closure: shown ticked and locked
  };

  static constexpr float kRowH = 20.f;
  // Items per line in the closure band, tuned to the 524 px band at 11 px Josefin.
  static constexpr size_t kAlsoPerLine = 2;

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
    const float t = box.T + 42.f + which * 24.f;
    return IRECT(box.L + 18.f, t, box.R - 18.f, t + 23.f);
  }
  IRECT _ListRect() const
  {
    const IRECT box = _BoxRect();
    const float top = mScreen == Screen::Export ? _ScopeRect(kScopeCount - 1).B + 8.f : box.T + 66.f;
    const float bottom = mScreen == Screen::Export ? _AlsoRect().T - 8.f : box.B - (_VerbsVisible() ? 126.f : 66.f);
    return IRECT(box.L + 18.f, top, box.R - 18.f, bottom);
  }
  // The closure band. Fixed height and always drawn: what a selection drags along
  // is the one thing about a Pack that surprises people, so it gets a permanent
  // block under the list rather than a caption that appears when it has news.
  IRECT _AlsoRect() const
  {
    const IRECT box = _BoxRect();
    return IRECT(box.L + 18.f, box.B - 104.f, box.R - 18.f, box.B - 54.f);
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
    mAlsoIncluding.clear();
    const auto& reg = volum::content::GlobalContentStore().reg();

    if (mScope == Scope::Amps)
    {
      for (const auto& a : reg.amps)
      {
        const auto bank = reg.presetBanks.find(a.id);
        const size_t presets = bank == reg.presetBanks.end() ? 0u : bank->second.size();
        mRows.push_back({true, a.id, a.name,
                         presets == 1 ? std::string("1 preset") : std::to_string(presets) + " presets", -1, false,
                         false});
      }
      return;
    }
    if (mScope == Scope::Sounds)
    {
      // PLAY's assignments first, tagged with their program number: "export the
      // Sounds I gig with" is the common case, and hunting for them inside an
      // undifferentiated bank list is what made the old tick list cryptic.
      for (int pass = 0; pass < 2; ++pass)
        for (const auto& bank : reg.presetBanks)
          for (const auto& pr : bank.second)
          {
            int pc = -1;
            for (const auto& slot : reg.midiSoundMap)
              if (slot.second.presetId == pr.id && slot.second.ampId == bank.first)
                pc = slot.first;
            if ((pass == 0) != (pc >= 0))
              continue;
            mRows.push_back(
              {false, pr.id, pr.name, volum::pack::OwnerDisplayName(reg, bank.first), pc, false, false});
          }
    }
  }

  volum::pack::ExportSelection _Selection() const
  {
    volum::pack::ExportSelection sel;
    sel.everything = mScope == Scope::Everything;
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

  void _DrawExport(IGraphics& g)
  {
    // Three scopes in the player's own words. "Everything" is a backup, "Sounds"
    // is what a set list is made of, "amp" is the thing a player hands to a
    // friend - and the second column says what each one costs.
    _DrawScope(g, 0, "Everything", mStandalone ? "your whole library, machine settings and MIDI slots"
                                               : "your whole library, every amp, IR, pedal and preset");
    _DrawScope(g, 1, "Sounds", "pick presets - your PLAY assignments are at the top");
    _DrawScope(g, 2, "A whole amp", "one custom amp and every preset saved on it");

    const IRECT list = _ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);

    if (mScope == Scope::Everything)
    {
      g.DrawText(IText(11.5f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                 "Nothing to pick: everything in your library travels, unused IRs and pedals included.", list);
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

        const IColor ink = r.required ? VoLumColors::TEXT_DIM : VoLumColors::CREAM;
        // A PC chip, so a row the player recognises from their footswitch reads as
        // the same object here.
        float textL = rowR.L + 18.f;
        if (r.pc >= 0)
        {
          const IRECT chip(textL, rowR.T + 3.f, textL + 30.f, rowR.B - 3.f);
          g.DrawRoundRect(VoLumColors::GOLD_DIM, chip, 2.f);
          g.DrawText(IText(9.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                     (r.pc < 10 ? "0" + std::to_string(r.pc) : std::to_string(r.pc)).c_str(), chip);
          textL = chip.R + 8.f;
        }
        const IRECT nameR(textL, rowR.T, rowR.R - 168.f, rowR.B);
        g.PathClipRegion(nameR);
        g.DrawText(IText(11.5f, ink, "Josefin-Sans", EAlign::Near, EVAlign::Middle), r.label.c_str(), nameR);
        g.PathClipRegion(list);
        g.DrawText(IText(10.f, r.required ? VoLumColors::TEXT_DIM : VoLumColors::TEAL_DIM, "Josefin-Sans", EAlign::Far,
                         EVAlign::Middle),
                   r.sub.c_str(), IRECT(rowR.R - 164.f, rowR.T, rowR.R - 4.f, rowR.B));
      }
      if (mRows.empty())
        g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                   mScope == Scope::Amps ? "No custom amps in your library yet."
                                         : "No saved presets yet - a Sound is an amp plus a named preset.",
                   list);
      g.PathClipRegion();
    }

    _DrawAlso(g);
  }

  // The closure, as the loudest thing on the screen after the scope. Two lines of
  // names, then a count if it still does not fit: "also including" is where a Pack
  // stops being what you ticked and starts being what will actually import.
  void _DrawAlso(IGraphics& g)
  {
    const IRECT band = _AlsoRect();
    g.FillRoundRect(IColor(58, 232, 168, 92), band, 3.f);
    g.DrawRoundRect(VoLumColors::AMBER.WithOpacity(0.65f), band, 3.f);

    const IRECT capR(band.L + 10.f, band.T + 4.f, band.R - 10.f, band.T + 18.f);
    const IRECT bodyR(band.L + 10.f, band.T + 18.f, band.R - 10.f, band.B - 4.f);
    const IText cap(10.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle);
    const IText body(11.f, VoLumColors::CREAM, "Josefin-Sans", EAlign::Near, EVAlign::Top);

    if (mScope == Scope::Everything)
    {
      g.DrawText(cap, "ALSO INCLUDING", capR);
      // Two DrawText calls, not one string with a newline: IGraphics lays out a
      // single line and the second half would be dropped.
      g.DrawText(body, "Every IR, pedal and capture file your library references.", bodyR);
      if (mStandalone)
        g.DrawText(body, "Plus your machine settings and the MIDI Program Change slots.",
                   IRECT(bodyR.L, bodyR.T + 13.f, bodyR.R, bodyR.T + 26.f));
      return;
    }
    if (mAlsoIncluding.empty())
    {
      // Three different silences, and telling a player to "tick something" while
      // their tick is on screen reads as the band not having noticed.
      const bool anyPicked = std::any_of(mRows.begin(), mRows.end(), [](const Row& r) { return r.picked; });
      const char* why = mRows.empty() ? "Nothing to export in this scope."
                        : anyPicked   ? "Nothing extra: what you ticked carries everything it needs."
                                      : "Tick something and its requirements appear here.";
      g.DrawText(cap, "ALSO INCLUDING", capR);
      g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top), why,
                 bodyR);
      return;
    }

    g.DrawText(cap,
               (std::to_string(mAlsoIncluding.size()) + (mAlsoIncluding.size() == 1 ? " ITEM COMES ALONG"
                                                                                   : " ITEMS COME ALONG"))
                 .c_str(),
               capR);
    // Two lines, chunked by item rather than by character: a name cut in half is
    // worse than a "+2 more".
    std::string lines[2];
    size_t shown = 0;
    for (const auto& item : mAlsoIncluding)
    {
      const size_t line = shown < kAlsoPerLine ? 0u : 1u;
      if (line > 1 || shown >= kAlsoPerLine * 2)
        break;
      lines[line] += (lines[line].empty() ? "" : ", ") + item;
      ++shown;
    }
    if (shown < mAlsoIncluding.size())
      lines[1] += (lines[1].empty() ? "" : ", ") + std::string("+")
                  + std::to_string(mAlsoIncluding.size() - shown) + " more";
    for (int i = 0; i < 2; ++i)
      if (!lines[i].empty())
        g.DrawText(body, lines[i].c_str(),
                   IRECT(bodyR.L, bodyR.T + i * 13.f, bodyR.R, bodyR.T + (i + 1) * 13.f));
  }

  void _DrawScope(IGraphics& g, int which, const char* title, const char* detail)
  {
    const IRECT r = _ScopeRect(which);
    const bool on = mScope == (Scope)which;
    const float cy = r.MH();
    g.DrawCircle(on ? VoLumColors::AMBER : VoLumColors::FRAME, r.L + 6.f, cy, 5.5f, nullptr, 1.2f);
    if (on)
      g.FillCircle(VoLumColors::AMBER, r.L + 6.f, cy, 3.f);
    const IRECT titleR(r.L + 18.f, r.T, r.L + 128.f, r.B);
    g.DrawText(IText(12.f, on ? VoLumColors::GOLD : VoLumColors::CREAM, "Josefin-Bold", EAlign::Near, EVAlign::Middle),
               title, titleR);
    g.DrawText(IText(11.f, on ? VoLumColors::CREAM : VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near,
                     EVAlign::Middle),
               detail, IRECT(titleR.R, r.T, r.R, r.B));
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
  Scope mScope = Scope::Everything;
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
