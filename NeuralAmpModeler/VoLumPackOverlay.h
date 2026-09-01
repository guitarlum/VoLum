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
#include "VoLumPackLayout.h"
#include "VoLumScroll.h"

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

  // What the import callback should apply: the Pack the user opened, narrowed to
  // the rows they left ticked. `_Go` stages the subset before calling back, so the
  // plugin's import path never has to know ticks exist.
  const volum::pack::PackContents& OpenedPack() const { return mApply.ok ? mApply : mPack; }

  // The ids this instance's rig is playing, so the import preview can name what
  // would have to reload.
  void SetSoundingIds(std::vector<std::string> ids) { mSoundingIds = std::move(ids); }

  void ShowExport()
  {
    mScreen = Screen::Export;
    mScope = Scope::Everything;
    mScroll = 0.f;
    mStatus.clear();
    mApply = volum::pack::PackContents{};
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
    mScroll = 0.f;
    mStatus.clear();
    mVerb = volum::pack::ImportVerb::Overwrite;
    mAlsoSettings = false;
    mPack = volum::pack::PackContents{};
    mApply = volum::pack::PackContents{};
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
    // Everything the Pack carries starts ticked: the common answer to "import this
    // Pack" is "all of it", and a screen that opens with nothing ticked makes the
    // user do the work before they can even read what is in the file.
    mUserTicks = volum::pack::AllTicks(mPack);
    _RecomputeTicks();
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
    {
      const auto status = _StatusRect();
      g.PathClipRegion(status);
      g.DrawText(
        IText(10.5f, VoLumColors::DANGER, "Josefin-Sans", EAlign::Center, EVAlign::Middle), mStatus.c_str(), status);
      g.PathClipRegion();
    }
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
    const IRECT list = _ListRect();
    mScroll = volum::scroll::ClampScroll(
      mScroll + volum::scroll::WheelDelta(d, kRowH), std::max(0.f, _ListContentH() - list.H()));
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const auto scroll = _ListScrollMetrics();
    const IRECT track = _ListTrackRect();
    if (mBar.OnDown(x, y, track.L, track.R, scroll))
    {
      mScroll =
        volum::scroll::ThumbYToScroll(y - mBar.grabDY, scroll.trackTop, scroll.trackH, scroll.thumbH, scroll.maxScroll);
      SetDirty(false);
      return;
    }
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
            mScroll = 0.f;
            _RebuildRows();
          }
          SetDirty(false);
          return;
        }
      if (mScope != Scope::Everything && _ListRect().Contains(x, y))
      {
        const int row = _RowAt(y);
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
          // Reset is not offered on a subset: see _ResetAllowed.
          if (i == (int)volum::pack::ImportVerb::Reset && !_ResetAllowed())
            return;
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
      return;
    }
    if (_ListRect().Contains(x, y))
    {
      const int row = _RowAt(y);
      if (row >= 0 && row < (int)mImportRows.size() && mImportRows[row].tickable && !mImportRows[row].locked)
      {
        _ToggleTick(mImportRows[row].kind, mImportRows[row].id);
        SetDirty(false);
      }
    }
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (!mBar.dragging)
      return;
    const auto m = _ListScrollMetrics();
    const float next = mBar.OnDrag(y, m);
    if (next >= 0.f)
      mScroll = next;
    SetDirty(false);
    (void)x;
  }

  void OnMouseUp(float, float, const IMouseMod&) override { mBar.OnUp(); }

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
    const auto c = _Chrome();
    return IRECT(box.MW() - 150.f, c.goT, box.MW() - 8.f, c.goB);
  }
  IRECT _GoRect() const
  {
    const IRECT box = _BoxRect();
    const auto c = _Chrome();
    return IRECT(box.MW() + 8.f, c.goT, box.MW() + 150.f, c.goB);
  }
  volum::packui::PackChrome _Chrome() const
  {
    const IRECT box = _BoxRect();
    return volum::packui::LayoutPackChrome(box.T, box.B, mScreen == Screen::Export);
  }
  IRECT _StatusRect() const
  {
    const IRECT box = _BoxRect();
    const auto c = _Chrome();
    return IRECT(box.L + 18.f, c.statusT, box.R - 18.f, c.statusB);
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
    const auto c = _Chrome();
    const float top = mScreen == Screen::Export ? _ScopeRect(kScopeCount - 1).B + 8.f : box.T + 66.f;
    float bottom = c.listB;
    if (mScreen == Screen::Import)
    {
      if (_VerbsVisible())
        bottom = std::min(bottom, _VerbRect(0).T - 8.f);
      else if (_SettingsBoxVisible())
        bottom = std::min(bottom, _SettingsCheckRect().T - 8.f);
    }
    return IRECT(box.L + 18.f, top, box.R - 18.f, std::max(top + 24.f, bottom));
  }
  IRECT _AlsoRect() const
  {
    const IRECT box = _BoxRect();
    const auto c = _Chrome();
    return IRECT(box.L + 18.f, c.alsoT, box.R - 18.f, c.alsoB);
  }
  float _ListContentH() const
  {
    const size_t n = mScreen == Screen::Export ? mRows.size() : mImportRows.size();
    return static_cast<float>(n) * kRowH;
  }
  int _VisibleRows() const { return std::max(1, (int)(_ListRect().H() / kRowH)); }
  int _RowAt(float y) const
  {
    const IRECT list = _ListRect();
    return static_cast<int>((y - list.T + mScroll) / kRowH);
  }
  IRECT _ListTrackRect() const
  {
    const IRECT list = _ListRect();
    return IRECT(list.R - 6.f, list.T + 1.f, list.R - 1.f, list.B - 1.f);
  }
  volum::scroll::ScrollMetrics _ListScrollMetrics() const
  {
    const IRECT list = _ListRect();
    return volum::scroll::ComputeScroll(list.T, list.B, list.H(), _ListContentH(), mScroll);
  }
  void _DrawListScrollbar(IGraphics& g)
  {
    const auto scroll = _ListScrollMetrics();
    if (scroll.maxScroll <= 0.5f)
      return;
    const IRECT track = _ListTrackRect();
    DrawVoLumScrollbar(g, track, IRECT(track.L, scroll.thumbY, track.R, scroll.thumbY + scroll.thumbH), mBar.dragging);
  }
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
            mRows.push_back({false, pr.id, pr.name, volum::pack::OwnerDisplayName(reg, bank.first), pc, false, false});
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
    _DrawScope(g, 0, "Everything",
               mStandalone ? "your whole library, machine settings and MIDI slots"
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
      for (int i = 0; i < (int)mRows.size(); ++i)
      {
        const float top = list.T + i * kRowH - mScroll;
        if (top + kRowH < list.T || top > list.B)
          continue;
        const Row& r = mRows[i];
        const IRECT rowR(list.L + 4.f, top, list.R - 8.f, top + kRowH);
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
      _DrawListScrollbar(g);
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

    g.PathClipRegion(band);
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
      g.PathClipRegion();
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
      g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top), why, bodyR);
      g.PathClipRegion();
      return;
    }

    g.DrawText(
      cap,
      (std::to_string(mAlsoIncluding.size()) + (mAlsoIncluding.size() == 1 ? " ITEM COMES ALONG" : " ITEMS COME ALONG"))
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
      lines[1] +=
        (lines[1].empty() ? "" : ", ") + std::string("+") + std::to_string(mAlsoIncluding.size() - shown) + " more";
    for (int i = 0; i < 2; ++i)
      if (!lines[i].empty())
        g.DrawText(body, lines[i].c_str(), IRECT(bodyR.L, bodyR.T + i * 13.f, bodyR.R, bodyR.T + (i + 1) * 13.f));
    g.PathClipRegion();
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
    const IRECT detailR(titleR.R, r.T, r.R, r.B);
    g.PathClipRegion(detailR);
    g.DrawText(
      IText(11.f, on ? VoLumColors::CREAM : VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
      detail, detailR);
    g.PathClipRegion();
  }

  // -- Import ------------------------------------------------------------------

  static bool _Holds(const volum::pack::ImportTicks& ticks, volum::pack::ItemKind kind, const std::string& id)
  {
    switch (kind)
    {
      case volum::pack::ItemKind::Amp: return ticks.HasAmp(id);
      case volum::pack::ItemKind::Ir: return ticks.HasIr(id);
      case volum::pack::ItemKind::Pedal: return ticks.HasPedal(id);
      default: return ticks.HasPreset(id);
    }
  }

  std::vector<std::string>& _UserIds(volum::pack::ItemKind kind)
  {
    switch (kind)
    {
      case volum::pack::ItemKind::Amp: return mUserTicks.ampIds;
      case volum::pack::ItemKind::Ir: return mUserTicks.irIds;
      case volum::pack::ItemKind::Pedal: return mUserTicks.pedalIds;
      default: return mUserTicks.presetIds;
    }
  }

  void _ToggleTick(volum::pack::ItemKind kind, const std::string& id)
  {
    auto& ids = _UserIds(kind);
    const auto at = std::find(ids.begin(), ids.end(), id);
    if (at == ids.end())
      ids.push_back(id);
    else
      ids.erase(at);
    _RecomputeTicks();
  }

  // The user's raw ticks are never what gets imported: the closure adds what a
  // ticked preset needs and drops presets whose amp is not coming, and the locks
  // are what the rows draw as un-clickable.
  void _RecomputeTicks()
  {
    mTicks = volum::pack::ApplyCompanionLock(mPack.library, mUserTicks);
    mLocks = volum::pack::CompanionLocks(mPack.library, mTicks);
    if (mVerb == volum::pack::ImportVerb::Reset && !_ResetAllowed())
      mVerb = volum::pack::ImportVerb::Overwrite;
    _RebuildPreview();
  }

  // Companion closure can turn a row back on because a selected preset needs it;
  // that is import validity, not consent to Reset everything else in the library.
  bool _ResetAllowed() const { return volum::pack::ResetAllowed(mPack, mUserTicks); }

  void _RebuildPreview()
  {
    mImportRows.clear();
    if (!mPack.ok)
      return;
    const auto& reg = volum::content::GlobalContentStore().reg();

    for (const auto& item : volum::pack::ImportItems(reg, mPack, mSoundingIds))
    {
      ImportRow row;
      row.tickable = true;
      row.kind = item.kind;
      row.id = item.id;
      row.ticked = _Holds(mTicks, item.kind, item.id);
      row.locked = row.ticked && _Holds(mLocks, item.kind, item.id);
      row.what = item.label;
      if (!row.ticked)
      {
        row.verb = "Skip";
        row.color = VoLumColors::TEXT_DIM;
      }
      else if (!item.present)
      {
        row.verb = "Add";
        row.color = VoLumColors::CREAM;
        if (item.nameCollision)
          row.what += " - same name here, both are kept";
      }
      else if (mVerb == volum::pack::ImportVerb::Add)
      {
        row.verb = "Keep mine";
        row.color = VoLumColors::TEXT_DIM;
      }
      else if (item.sounding)
      {
        row.verb = "Reloads";
        row.what += " - playing now";
        row.color = VoLumColors::GOLD;
      }
      else
      {
        row.verb = "Replace";
        row.color = VoLumColors::AMBER;
      }
      mImportRows.push_back(std::move(row));
    }

    // What a Reset deletes and what the settings box writes are consequences, not
    // choices, so they are listed under the ticks without a box of their own.
    // Removals only ever appear under Reset, which _ResetAllowed already limits to
    // a whole-Pack import, so naming them off the full Pack is naming them exactly.
    const auto preview = volum::pack::BuildImportPreview(reg, mPack, mVerb, mAlsoSettings, mStandalone, mSoundingIds);
    for (const auto& s : preview.removals)
      mImportRows.push_back({false, volum::pack::ItemKind::Amp, "", false, false, "Delete", s, VoLumColors::DANGER});
    if (preview.writesSettings)
      mImportRows.push_back({false, volum::pack::ItemKind::Amp, "", false, false, "Restore",
                             "machine settings and MIDI slots", VoLumColors::AMBER});
    if (mImportRows.empty())
      mImportRows.push_back({false, volum::pack::ItemKind::Amp, "", false, false, "",
                             "This Pack carries nothing this build understands.", VoLumColors::TEXT_DIM});
    // Clamped, not reset: a tick 40 rows down must not throw the list back to the
    // top under the cursor that just clicked it.
    mScroll = volum::scroll::ClampScroll(mScroll, std::max(0.f, _ListContentH() - _ListRect().H()));
  }

  void _DrawImport(IGraphics& g, const IRECT& inner)
  {
    const IRECT headR(inner.L, inner.T + 22.f, inner.R, inner.T + 40.f);
    std::string head;
    if (mPack.ok)
    {
      head = volum::pack::PackSummaryLine(mPack);
      // Only when it is a subset: "importing 7 of 7" is noise on the default.
      const size_t all = volum::pack::AllTicks(mPack).Count();
      if (mTicks.Count() != all)
        head += "  -  importing " + std::to_string(mTicks.Count()) + " of " + std::to_string(all);
    }
    else
      head = "No Pack opened.";
    g.DrawText(IText(11.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle), head.c_str(), headR);

    const IRECT list = _ListRect();
    g.FillRect(IColor(60, 10, 12, 16), list);
    g.DrawRect(VoLumColors::FRAME, list);
    g.PathClipRegion(list);
    for (int i = 0; i < (int)mImportRows.size(); ++i)
    {
      const float top = list.T + i * kRowH - mScroll;
      if (top + kRowH < list.T || top > list.B)
        continue;
      const auto& row = mImportRows[i];
      const IRECT rowR(list.L + 6.f, top, list.R - 8.f, top + kRowH);
      float textL = rowR.L;
      if (row.tickable)
      {
        // Same tick dialect as the export list: a locked box is dim, not amber, so
        // "this came along" reads differently from "I chose this".
        const IRECT boxR(rowR.L, rowR.MH() - 5.f, rowR.L + 10.f, rowR.MH() + 5.f);
        g.DrawRect(row.locked ? VoLumColors::TEXT_DIM : VoLumColors::FRAME, boxR);
        if (row.ticked)
          g.FillRect(row.locked ? VoLumColors::TEXT_DIM : VoLumColors::AMBER, boxR.GetPadded(-2.5f));
        textL = boxR.R + 8.f;
      }
      g.DrawText(IText(10.5f, row.color, "Josefin-Bold", EAlign::Near, EVAlign::Middle), row.verb.c_str(),
                 IRECT(textL, rowR.T, textL + 62.f, rowR.B));
      const IRECT whatR(textL + 66.f, rowR.T, rowR.R, rowR.B);
      g.PathClipRegion(whatR);
      g.DrawText(IText(11.f, row.color, "Josefin-Sans", EAlign::Near, EVAlign::Middle), row.what.c_str(), whatR);
      g.PathClipRegion(list);
    }
    g.PathClipRegion();
    _DrawListScrollbar(g);

    if (_VerbsVisible())
    {
      static const char* kVerbNames[3] = {"Overwrite", "Add", "Reset"};
      static const char* kVerbHelp[3] = {"Pack wins, mine stay", "keep mine", "Pack wins, mine deleted"};
      for (int i = 0; i < 3; ++i)
      {
        const IRECT r = _VerbRect(i);
        const bool off = i == (int)volum::pack::ImportVerb::Reset && !_ResetAllowed();
        const bool on = (int)mVerb == i && !off;
        g.FillRoundRect(on ? IColor(70, 232, 168, 92) : VoLumColors::BTN_OFF_BG, r, 3.f);
        g.DrawRoundRect(on ? VoLumColors::AMBER : VoLumColors::FRAME, r, 3.f, nullptr, on ? 1.4f : 1.f);
        const IColor ink = off ? VoLumColors::TEXT_DIM : (on ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM);
        g.DrawText(IText(11.f, ink, "Josefin-Bold", EAlign::Center, EVAlign::Middle), kVerbNames[i],
                   r.GetFromTop(14.f).GetVShifted(2.f));
        g.DrawText(IText(9.f, VoLumColors::TEXT_DIM, "Josefin-Sans", EAlign::Center, EVAlign::Middle),
                   off ? "needs every item ticked" : kVerbHelp[i], IRECT(r.L, r.B, r.R, r.B + 12.f));
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
    const bool settingsOnly = mAlsoSettings && _SettingsBoxVisible();
    if (mTicks.Empty() && !settingsOnly)
    {
      mStatus = "Nothing ticked to import.";
      SetDirty(false);
      return;
    }
    if (mVerb == volum::pack::ImportVerb::Reset && !_ResetAllowed())
    {
      mStatus = "Reset needs every item in this Pack ticked.";
      SetDirty(false);
      return;
    }
    // The ticks are applied here and nowhere else: everything downstream sees an
    // ordinary Pack that happens to carry only the ticked items.
    mApply = volum::pack::SubsetPack(mPack, mTicks);
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

  // An import row is either a tickable incoming item or a consequence of the verb.
  struct ImportRow
  {
    bool tickable = false;
    volum::pack::ItemKind kind = volum::pack::ItemKind::Amp;
    std::string id;
    bool ticked = false;
    bool locked = false; // required by something else that is ticked
    std::string verb;
    std::string what;
    IColor color = VoLumColors::CREAM;
  };

  Screen mScreen = Screen::Export;
  Scope mScope = Scope::Everything;
  bool mStandalone = false;
  bool mAlsoSettings = false;
  volum::pack::ImportVerb mVerb = volum::pack::ImportVerb::Overwrite;
  float mScroll = 0.f;
  volum::scroll::Interaction mBar;
  std::string mStatus;
  std::vector<Row> mRows;
  std::vector<std::string> mAlsoIncluding;
  std::vector<ImportRow> mImportRows;
  std::vector<std::string> mSoundingIds;
  volum::pack::PackContents mPack; // what the file carries
  volum::pack::PackContents mApply; // the ticked subset, staged by _Go
  volum::pack::ImportTicks mUserTicks; // rows the user has on
  volum::pack::ImportTicks mTicks; // + companions, - presets of unticked amps
  volum::pack::ImportTicks mLocks; // the companions, which cannot be clicked off

  ExportCallback mExport;
  PickPackCallback mPick;
  ImportCallback mImport;
};
