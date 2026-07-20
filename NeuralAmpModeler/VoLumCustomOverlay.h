#pragma once

// VoLumCustomOverlayControl: the full-window custom-content overlay (Manage CRUD
// list + file-first amp Builder). Extracted from VoLumCustomUi.h for file-size
// hygiene; include VoLumCustomUi.h (the umbrella) rather than this directly.

#include "VoLumColorHelpers.h"
#include "VoLumCustomContentApi.h"
#include "VoLumFractalArt.h"
#include "VoLumIrFileGuard.h"
#include "VoLumPresetBar.h"
#include "VoLumListMenu.h"
#include "VoLumConfirmDialog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

  // editIdx is the custom-amp index being edited, or -1 for a brand-new amp.
  // The host updates that entry in place (no duplicate) when editIdx >= 0.
  // Empty return value means success. A non-empty error keeps the builder open
  // so a failed copy/NAM validation can never look like a successful save.
  using BuilderSavedCallback = std::function<std::string(const volum::custom::CustomAmp& amp, int editIdx)>;
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
  using ConfirmCallback =
    std::function<void(const std::string& message, std::function<void()> onConfirm, const std::string& confirmLabel)>;

  // Primary action for a double-clicked Manage row (recall preset / select IR /
  // load pedal). pedalSlot is the originating PRE NAM slot for Pedals (else -1).
  using PrimaryActionCallback = std::function<void(ManageKind kind, int ampIdx, int pedalSlot, int index)>;

  void SetCallbacks(BuilderSavedCallback builderCb, ChangedCallback changedCb = nullptr)
  {
    mBuilderSaved = std::move(builderCb);
    mChanged = std::move(changedCb);
  }

  void SetConfirmCallback(ConfirmCallback cb) { mConfirm = std::move(cb); }
  void SetPrimaryActionCallback(PrimaryActionCallback cb) { mPrimaryAction = std::move(cb); }

  // F5 preset capture hooks (real backend). SaveCb captures the live settings
  // into a new named preset and returns its bank index; OverwriteCb replaces the
  // snapshot of preset `index`. When unset the bridge stores default settings.
  using SavePresetCallback = std::function<int(const std::string& name)>;
  using OverwritePresetCallback = std::function<void(int index)>;
  void SetPresetCallbacks(SavePresetCallback saveCb, OverwritePresetCallback overwriteCb)
  {
    mSavePreset = std::move(saveCb);
    mOverwritePreset = std::move(overwriteCb);
  }

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

  // editIdx >= 0 edits an existing custom amp (loads its real manifest so the
  // coverage grid shows that amp's actual files/cabs - not a fixed demo); -1
  // starts a fresh draft. ampName seeds the name field for a new amp.
  void ShowBuilder(bool editExisting, const char* ampName, int editIdx = -1)
  {
    mScreen = volum::custom::Screen::Builder;
    mBuilderEditIdx = editExisting ? editIdx : -1;
    mBuilderAmp = (editExisting && editIdx >= 0) ? volum::custom::CustomAmpAt(editIdx) : NewBuilderAmp(ampName);
    mBuilderFileScroll = 0.f;
    ResetTransient();
    Hide(false);
    SetDirty(false);
  }

  // Keyboard nav for the builder art picker (3-column grid). Called by the
  // global key handler while this overlay is the focused surface so arrows move
  // the art selection here instead of the amp list behind the overlay. Returns
  // true when it consumed the key.
  bool OnArrowKey(int vk)
  {
    if (mScreen != volum::custom::Screen::Builder)
      return false;
    const int cols = 3;
    const int n = volum::custom::kNumCustomArts;
    int a = std::clamp(mBuilderAmp.art, 0, n - 1);
    if (vk == kVK_LEFT)
      a = (a - 1 + n) % n;
    else if (vk == kVK_RIGHT)
      a = (a + 1) % n;
    else if (vk == kVK_UP)
      a = (a - cols + n) % n;
    else if (vk == kVK_DOWN)
      a = (a + cols) % n;
    else
      return false;
    mBuilderAmp.art = a;
    SetDirty(false);
    return true;
  }

  void Draw(IGraphics& g) override
  {
    mHotspots.clear();
    mHotspotTips.clear();
    mPopupHotspots.clear();
    g.FillRect(IColor(185, 8, 10, 14), mRECT);

    const IRECT panel = PanelRect();
    DrawPanelDepth(g, panel);
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
    AddHotspot(closeR, kClose, "Close");

    const IRECT body = panel.GetPadded(-22.f, -56.f, -22.f, -18.f);
    if (mScreen == volum::custom::Screen::Presets)
      DrawManage(g, body);
    else if (mScreen == volum::custom::Screen::Builder)
      DrawBuilder(g, body);

    // Hover affordance: subtle highlight on whichever interactive hotspot the
    // cursor is over (Manage overwrite/rename/trash icons, rows, and every
    // builder component). Drawn last so it reads as a button-hover glow.
    if (!mPopupOpen && mHoverAction >= 0)
      for (const auto& hs : mHotspots)
        if (hs.second == mHoverAction)
        {
          const IRECT hr = hs.first.GetPadded(-1.f);
          g.FillRoundRect(IColor(30, 230, 202, 120), hr, 3.f);
          g.DrawRoundRect(IColor(150, 230, 202, 120), hr, 3.f);
          break;
        }

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
    // Popups consume their own wheel; otherwise scroll the active list (the
    // Builder's file manifest or the Manage list). Upper bound is clamped in Draw.
    if (mPopupOpen || !PanelRect().Contains(x, y))
      return;
    if (mScreen == volum::custom::Screen::Builder)
      mBuilderFileScroll = std::max(0.f, mBuilderFileScroll - d * 38.f);
    else
      mManageScroll = std::max(0.f, mManageScroll - d * 38.f);
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    // Per-hotspot tooltips + hover highlight: find the hovered hotspot once and
    // surface both its hint and its action code (for the Draw hover glow).
    const char* tip = "";
    int hoverAction = -1;
    if (!mPopupOpen && PanelRect().Contains(x, y))
      for (size_t i = 0; i < mHotspots.size(); ++i)
        if (mHotspots[i].first.Contains(x, y))
        {
          hoverAction = mHotspots[i].second;
          if (i < mHotspotTips.size())
            tip = mHotspotTips[i].c_str();
          break;
        }
    if (mCurTip != tip || hoverAction != mHoverAction)
    {
      mCurTip = tip;
      mHoverAction = hoverAction;
      SetTooltip(tip);
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (!mCurTip.empty() || mHoverAction >= 0)
    {
      mCurTip.clear();
      mHoverAction = -1;
      SetTooltip("");
      SetDirty(false);
    }
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
    const std::string s = ClampName(str ? str : "", (std::size_t)NameEntryCap(mTextTarget));
    switch (mTextTarget)
    {
      case TextTarget::NewItem: // presets only (IR/pedals add via file dialog)
        if (!s.empty() && mManageKind == ManageKind::Presets)
        {
          if (NameTaken(s, -1))
            SetNameError(s);
          else
          {
            mError.clear();
            const int i = mSavePreset ? mSavePreset(s) : AddPreset(mAmpIdx, s);
            ReloadList();
            mSel = i;
            NotifyChanged();
          }
        }
        break;
      case TextTarget::RenameItem:
        if (mSel >= 0)
        {
          if (!s.empty() && NameTaken(s, mSel))
            SetNameError(s);
          else
          {
            mError.clear();
            ApplyRename(mSel, s);
            ReloadList();
            NotifyChanged();
          }
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
      default: break;
    }
    mTextTarget = TextTarget::None;
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
    kRowIrCfgBase = 800, // Manage inline [gear] icon (IR only): open shaping editor
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
    Channel,
    IrSettings // per-IR trim + low/high cut editor (VoLum 1.2.1)
  };

  // Popup-local action codes for the IR-settings editor (offsets from kPopupBase).
  enum IrCfgPopupAction
  {
    kIrCfgTrimDown = 0,
    kIrCfgTrimUp,
    kIrCfgLowDown,
    kIrCfgLowUp,
    kIrCfgHighDown,
    kIrCfgHighUp
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

  void AddHotspot(const IRECT& r, int action, const char* tip = "")
  {
    mHotspots.emplace_back(r, action);
    mHotspotTips.emplace_back(tip ? tip : "");
  }

  void ResetTransient()
  {
    mPopupOpen = false;
    mTextTarget = TextTarget::None;
    mError.clear();
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

  // Leaf filename WITH extension (e.g. "Mesa_4x12_sm57.wav").
  static std::string LeafName(const char* full)
  {
    std::string s = full ? full : "";
    const size_t slash = s.find_last_of("/\\");
    if (slash != std::string::npos)
      s = s.substr(slash + 1);
    return s;
  }

  // IR/pedal "add" = OS file picker (.wav / .nam). The chosen filename seeds the
  // mock entry name; real loading lands with the backend.
  void StartImport()
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    const char* ext = (mManageKind == ManageKind::IR) ? "wav" : "nam";
    WDL_String path;
    std::vector<WDL_String> files;
    ui->PromptForFiles(path, files, ext); // multi-select; each entry is a full path
    if (files.empty())
      return;

    // Each file's display name defaults to its filename (minus extension); the
    // exact filename is stored alongside. Duplicate names (case-insensitive) are
    // skipped - including duplicates within the same batch, since the library
    // grows as we add - and reported together.
    std::vector<std::string> skipped;
    std::vector<std::string> tooLarge;
    int added = 0;
    for (const auto& fn : files)
    {
      if (!fn.GetLength())
        continue;
      // Cap the imported display name (the filename base) so a very long file
      // name does not overflow the IR/pedal list and chip labels.
      const std::string base = volum::custom::ClampName(BaseName(fn.Get()), volum::custom::kMaxCustomNameLen);
      if (base.empty())
        continue;
      if (NameTaken(base, -1))
      {
        skipped.push_back(base);
        continue;
      }
      // Reject oversized IR captures at import (only the first ~8192 samples are
      // ever convolved, so a huge WAV is almost always a wrong-file pick).
      if (mManageKind == ManageKind::IR)
      {
        std::error_code ec;
        const std::uintmax_t bytes =
          std::filesystem::file_size(volum::content::PathFromUtf8(fn.Get()), ec);
        if (!ec && !volum::IrFileBytesAcceptable(bytes))
        {
          tooLarge.push_back(base);
          continue;
        }
      }
      const std::string leaf = LeafName(fn.Get());
      // Copy the picked file into the VoLum-owned content library and store the
      // resolvable registry-relative path. When no base dir is set (unit tests),
      // ImportFileCopy returns "" and we fall back to the bare filename.
      auto& store = volum::content::GlobalContentStore();
      if (mManageKind == ManageKind::IR)
      {
        const std::string idp = volum::content::MintId(store.reg(), "ir");
        std::string rel = store.ImportFileCopy(volum::content::PathFromUtf8(fn.Get()), "ir", idp);
        if (rel.empty())
          rel = leaf;
        volum::custom::AddIR(base, rel);
      }
      else
      {
        const std::string idp = volum::content::MintId(store.reg(), "pedal");
        std::string rel = store.ImportFileCopy(volum::content::PathFromUtf8(fn.Get()), "pedals", idp);
        if (rel.empty())
          rel = leaf;
        if (volum::custom::AddPedal(base, rel) < 0)
        {
          mError = "Custom pedal slots are full - delete a pedal first.";
          continue;
        }
      }
      ++added;
    }

    ReloadList();
    if (added > 0)
    {
      mSel = (int)mItems.size() - 1;
      NotifyChanged();
    }
    if (!tooLarge.empty())
      mError = tooLarge.size() == 1
                 ? ("\"" + tooLarge.front() + "\" is too large for an IR - skipped.")
                 : (std::to_string(tooLarge.size()) + " files were too large for IRs - skipped.");
    else if (!skipped.empty())
      mError = skipped.size() == 1 ? ("\"" + skipped.front() + "\" already exists - skipped.")
                                   : (std::to_string(skipped.size()) + " names already existed - skipped.");
    else
      mError.clear();
    SetDirty(false);
  }

  volum::custom::CustomAmp NewBuilderAmp(const char* name)
  {
    volum::custom::CustomAmp a;
    a.name = (name && *name) ? name : "New custom amp";
    a.files = {}; // start empty; Add .nam seeds DIRECT/Ch1
    return a;
  }

  // Max characters for a custom name, by what is being named. Caps keep long
  // names from overflowing the hero/sub-row/list labels (cab names are already
  // capped to 3 by NormalizeCabName).
  int NameEntryCap(TextTarget target) const
  {
    switch (target)
    {
      case TextTarget::CabName: return 3;
      case TextTarget::ProfileName: return (int)volum::custom::kMaxCustomNameLen; // amp name
      case TextTarget::NewItem: return (int)volum::custom::kMaxPresetNameLen; // new preset
      case TextTarget::RenameItem:
        return (int)(mManageKind == ManageKind::Presets ? volum::custom::kMaxPresetNameLen
                                                         : volum::custom::kMaxCustomNameLen);
      default: return (int)volum::custom::kMaxCustomNameLen;
    }
  }

  void StartTextEntry(TextTarget target, const IRECT& bounds, const std::string& current)
  {
    auto* ui = GetUI();
    if (!ui)
      return;
    mTextTarget = target;
    SetTextEntryLength(NameEntryCap(target));
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
    if (action >= kRowIrCfgBase && action < kRowIrCfgBase + 100)
    {
      OpenIrSettingsPopup(action - kRowIrCfgBase, rect);
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
        // presets: overwrite snapshot with live state (no-op in shell), gated by
        // an "are you sure" confirm so a stray click can't clobber a saved preset.
        if (mSel >= 0 && mSel < (int)mItems.size())
        {
          const int idx = mSel;
          const std::string nm = mItems[(size_t)idx];
          auto doOverwrite = [this, idx]() {
            if (mOverwritePreset)
              mOverwritePreset(idx);
            NotifyChanged();
            SetDirty(false);
          };
          if (mConfirm)
            mConfirm("Overwrite preset \"" + nm + "\" with the current settings?", doOverwrite, "Overwrite");
          else
            doOverwrite();
        }
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
          if (mConfirm)
            mConfirm(
              "Delete " + std::string(ItemNoun()) + " \"" + nm + "\"? This cannot be undone.", doDelete, "Delete");
          else
            doDelete();
        }
        break;
      default: break;
    }
  }

  void HandleBuilderAction(int action, const IRECT& rect)
  {
    using namespace volum::custom;
    if (action != kBuilderSave)
      mError.clear();
    if (action == kEditName)
    {
      StartTextEntry(TextTarget::ProfileName, rect.GetPadded(-2.f), mBuilderAmp.name);
      return;
    }
    if (action == kAddFile)
    {
      auto* ui = GetUI();
      if (!ui)
        return;
      WDL_String path;
      std::vector<WDL_String> files;
      ui->PromptForFiles(path, files, "nam"); // multi-select .nam captures
      int added = 0;
      for (const auto& fn : files)
      {
        if (!fn.GetLength())
          continue;
        std::string base = fn.Get();
        const size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos)
          base = base.substr(slash + 1);
        // Re-importing a file already in this amp's manifest is a no-op (matched
        // by filename, case-insensitive) so the same capture never stacks up as
        // duplicate rows - whether re-picked alone or within a multi-select batch.
        if (volum::custom::ManifestHasFile(mBuilderAmp, base))
          continue;
        // Factory naming convention (PREFIX-CODE-CHANNEL.nam) auto-fills the cab
        // slot, channel, and cab name; other names are added unassigned for
        // manual mapping.
        const auto parsed = volum::custom::ParseNamFileName(base);
        const int slot = parsed.matched ? parsed.slot : volum::custom::kUnassignedSlot;
        const int channel = parsed.matched ? parsed.channel : 0;
        // Keep the absolute source path so Save can copy the capture into the
        // VoLum-owned content library (F6 import). storedPath is filled on save.
        volum::custom::CustomNamFile nf;
        nf.file = base;
        nf.slot = slot;
        nf.channel = channel;
        nf.sourcePath = fn.Get();
        mBuilderAmp.files.push_back(nf);
        if (parsed.matched && slot >= 0 && slot < volum::custom::kNumCabSlots && !parsed.cabName.empty())
          mBuilderAmp.cabNames[(size_t)slot] = volum::custom::NormalizeCabName(parsed.cabName);
        ++added;
      }
      if (added > 0)
        mBuilderFileScroll = 1e9f; // jump to the newly appended rows (clamped in Draw)
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
        {
          mError = mBuilderSaved(mBuilderAmp, mBuilderEditIdx);
          if (!mError.empty())
          {
            SetDirty(false);
            return;
          }
        }
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

  // IR shaping editor: a compact fixed-size popover (trim + low/high cut), anchored
  // to the row's gear icon. Distinct from the list popups (Speaker/Channel).
  void OpenIrSettingsPopup(int irIdx, const IRECT& anchor)
  {
    if (mManageKind != ManageKind::IR || irIdx < 0 || irIdx >= (int)mItems.size())
      return;
    mPopupKind = PopupKind::IrSettings;
    mPopupIrIdx = irIdx;
    const float w = 248.f, h = 150.f;
    const IRECT panel = PanelRect();
    float left = anchor.R - w; // hang left from the gear so it stays on-panel
    if (left < panel.L + 6.f)
      left = panel.L + 6.f;
    float top = anchor.B + 2.f;
    if (top + h > panel.B - 6.f)
      top = std::max(panel.T + 6.f, anchor.T - 2.f - h);
    mPopupRect = IRECT(left, top, left + w, top + h);
    mPopupOpen = true;
    SetDirty(false);
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
    if (mPopupKind == PopupKind::IrSettings)
    {
      HandleIrSettingsPopup(code - kPopupBase);
      return;
    }
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

  // One +/- click on the IR editor: step the value, persist, and re-push live so the
  // audible level/tone change is immediate. The popover stays open for further tweaks.
  void HandleIrSettingsPopup(int act)
  {
    using namespace volum::custom;
    if (mPopupIrIdx < 0 || mPopupIrIdx >= (int)mItems.size())
    {
      mPopupOpen = false;
      SetDirty(false);
      return;
    }
    IRShaping s = IRShapingAt(mPopupIrIdx);
    switch (act)
    {
      case kIrCfgTrimDown: s.trimDb = volum::content::StepIrTrimDb(s.trimDb, -1); break;
      case kIrCfgTrimUp: s.trimDb = volum::content::StepIrTrimDb(s.trimDb, +1); break;
      case kIrCfgLowDown: s.lowCutHz = volum::content::StepIrLowCutHz(s.lowCutHz, -1); break;
      case kIrCfgLowUp: s.lowCutHz = volum::content::StepIrLowCutHz(s.lowCutHz, +1); break;
      case kIrCfgHighDown: s.highCutHz = volum::content::StepIrHighCutHz(s.highCutHz, -1); break;
      case kIrCfgHighUp: s.highCutHz = volum::content::StepIrHighCutHz(s.highCutHz, +1); break;
      default: return; // click on the panel chrome: keep it open
    }
    SetIRShaping(mPopupIrIdx, s.trimDb, s.lowCutHz, s.highCutHz);
    NotifyChanged(); // plugin migrates/re-pushes shaping to the live IR lanes
    SetDirty(false);
  }

  void DrawPopup(IGraphics& g)
  {
    if (mPopupKind == PopupKind::IrSettings)
    {
      DrawIrSettingsPopup(g);
      return;
    }
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

  static std::string FmtIrTrim(double db)
  {
    char b[24];
    std::snprintf(b, sizeof(b), "%+.1f dB", db);
    return b;
  }
  static std::string FmtIrCut(double hz)
  {
    if (!(hz > 0.0))
      return "Off";
    char b[24];
    if (hz >= 1000.0)
      std::snprintf(b, sizeof(b), "%.1f kHz", hz / 1000.0);
    else
      std::snprintf(b, sizeof(b), "%.0f Hz", hz);
    return b;
  }

  // One label / value / [-] [+] line inside the IR editor. `downAct`/`upAct` are
  // popup action offsets registered as hotspots for the steppers.
  void DrawIrStepperRow(IGraphics& g, const IRECT& row, const char* label, const std::string& value, int downAct,
                        int upAct)
  {
    const float btnW = 22.f;
    g.DrawText(IText(11.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), label,
               row.GetPadded(0.f, 0.f, 0.f, 0.f));
    const IRECT plus(row.R - btnW, row.T, row.R, row.B);
    const IRECT minus(plus.L - 4.f - btnW, row.T, plus.L - 4.f, row.B);
    const IRECT valueR(row.L + 58.f, row.T, minus.L - 6.f, row.B);
    g.DrawText(IText(12.f, VoLumColors::CREAM, "Josefin-Bold", EAlign::Far, EVAlign::Middle), value.c_str(), valueR);
    for (const auto& b : {std::make_pair(minus, false), std::make_pair(plus, true)})
    {
      g.FillRoundRect(VoLumColors::BTN_OFF_BG, b.first, 3.f);
      g.DrawRoundRect(VoLumColors::TEAL_DIM, b.first, 3.f, nullptr, 1.f);
      const float cx = b.first.MW(), cy = b.first.MH();
      g.DrawLine(VoLumColors::CREAM, cx - 4.f, cy, cx + 4.f, cy, nullptr, 1.4f);
      if (b.second)
        g.DrawLine(VoLumColors::CREAM, cx, cy - 4.f, cx, cy + 4.f, nullptr, 1.4f);
    }
    mPopupHotspots.emplace_back(minus, kPopupBase + downAct);
    mPopupHotspots.emplace_back(plus, kPopupBase + upAct);
  }

  void DrawIrSettingsPopup(IGraphics& g)
  {
    using namespace volum::custom;
    g.FillRoundRect(VoLumColors::HERO_BG, mPopupRect, 4.f);
    g.DrawRoundRect(VoLumColors::TEAL_DIM, mPopupRect, 4.f, nullptr, 1.5f);
    const IRShaping s = IRShapingAt(mPopupIrIdx);
    const IRECT inner = mPopupRect.GetPadded(-10.f);
    const IRECT title(inner.L, inner.T, inner.R, inner.T + 20.f);
    g.DrawText(IText(12.f, VoLumColors::GOLD, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "IR SHAPING", title);
    const float rowH = 30.f;
    float t = title.B + 4.f;
    DrawIrStepperRow(g, IRECT(inner.L, t, inner.R, t + rowH), "Level", FmtIrTrim(s.trimDb), kIrCfgTrimDown,
                     kIrCfgTrimUp);
    t += rowH;
    DrawIrStepperRow(g, IRECT(inner.L, t, inner.R, t + rowH), "Low cut", FmtIrCut(s.lowCutHz), kIrCfgLowDown,
                     kIrCfgLowUp);
    t += rowH;
    DrawIrStepperRow(g, IRECT(inner.L, t, inner.R, t + rowH), "High cut", FmtIrCut(s.highCutHz), kIrCfgHighDown,
                     kIrCfgHighUp);
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
                  bool disabled = false, const char* tip = "")
  {
    const IColor bg =
      disabled ? IColor(8, 200, 162, 78)
               : (primary ? IColor(70, 232, 168, 92) : (on ? VoLumColors::ITEM_SEL_BG : VoLumColors::BTN_OFF_BG));
    const IColor border = disabled
                            ? VoLumColors::FRAME
                            : (primary ? VoLumColors::AMBER : (on ? VoLumColors::ITEM_SEL_BORDER : VoLumColors::FRAME));
    g.FillRect(bg, r);
    g.DrawRect(border, r);
    const IColor txt =
      disabled ? VoLumColors::CREAM_DIM : ((on || primary) ? VoLumColors::TEXT_BRIGHT : VoLumColors::CREAM);
    g.DrawText(IText(11.f, txt, "Josefin-Bold", EAlign::Center, EVAlign::Middle), label, r);
    if (action >= 0 && !disabled)
      AddHotspot(r, action, tip);
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

  // Within-list name uniqueness (case-insensitive). exceptIdx skips the row being
  // renamed so re-confirming its own name is allowed.
  bool NameTaken(const std::string& name, int exceptIdx) const
  {
    using namespace volum::custom;
    switch (mManageKind)
    {
      case ManageKind::IR: return IRNameExists(name, exceptIdx);
      case ManageKind::Pedals: return PedalNameExists(name, exceptIdx);
      default: return PresetNameExists(mAmpIdx, name, exceptIdx);
    }
  }

  void SetNameError(const std::string& name)
  {
    mError = "\"" + name + "\" already exists - names must be unique.";
    SetDirty(false);
  }

  void DrawManage(IGraphics& g, const IRECT& body)
  {
    const bool presets = (mManageKind == ManageKind::Presets);
    const std::string deleteTip = std::string("Delete ") + ItemNoun();
    const std::string renameTip = std::string("Rename ") + ItemNoun();
    const char* rowTip = presets ? "Double-click to recall this preset"
                                 : (mManageKind == ManageKind::IR ? "Double-click to use on the focused cab"
                                                                  : "Double-click to load this pedal");

    char sub[160];
    if (presets)
      std::snprintf(sub, sizeof(sub), "for  %s   -   double-click a row to recall", mAmpName.c_str());
    else if (mManageKind == ManageKind::IR)
      std::snprintf(sub, sizeof(sub), "shared across amps   -   double-click to use on the focused cab");
    else
      std::snprintf(sub, sizeof(sub), "shown under CUSTOM in the PRE pedal menu");
    g.DrawText(IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Near, EVAlign::Top), sub, body.GetFromTop(14.f));

    // Single top action button: save-as-new (presets) or import (IR / pedals).
    const char* addLabel = presets ? "+  Save current as new"
                                   : (mManageKind == ManageKind::IR ? "+  Import IR (.wav)" : "+  Import pedal (.nam)");
    const IRECT addBtn(body.L, body.T + 20.f, body.R, body.T + 48.f);
    const char* addTip =
      presets ? "Save the current settings as a new preset"
              : (mManageKind == ManageKind::IR ? "Import a .wav impulse response" : "Import a .nam pedal capture");
    DrawButton(g, addBtn, addLabel, kAdd, true, false, false, addTip);

    // Scrollable list filling the rest of the panel; per-row inline icons.
    const IRECT listArea(body.L, addBtn.B + 10.f, body.R, body.B - 26.f);
    DrawInsetWell(g, listArea, 2.f);
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
        AddHotspot(trash, kRowDeleteBase + i, deleteTip.c_str());
        ix -= iconW;
        const IRECT pen(ix, row.T, ix + iconW, row.B);
        DrawPenGlyph(g, pen, VoLumColors::CREAM_DIM);
        AddHotspot(pen, kRowRenameBase + i, renameTip.c_str());
        if (mManageKind == ManageKind::IR)
        {
          ix -= iconW;
          const IRECT gear(ix, row.T, ix + iconW, row.B);
          // A tinted gear when this IR carries a non-default trim/cut, so a shaped
          // IR reads as edited at a glance.
          const volum::custom::IRShaping s = volum::custom::IRShapingAt(i);
          const bool shaped = (s.trimDb != 0.0) || (s.lowCutHz > 0.0) || (s.highCutHz > 0.0);
          DrawGearGlyph(g, gear, shaped ? VoLumColors::GOLD : VoLumColors::CREAM_DIM);
          AddHotspot(gear, kRowIrCfgBase + i, "Level, low-cut & high-cut for this IR");
        }
        ix -= iconW;
        if (presets)
        {
          const IRECT ovr(ix, row.T, ix + iconW, row.B);
          DrawOverwriteGlyph(g, ovr, VoLumColors::CREAM_DIM);
          AddHotspot(ovr, kRowOverwriteBase + i, "Overwrite this preset with the current settings");
          ix -= iconW;
        }

        const IRECT nameR(row.L + (sel ? 20.f : 12.f), row.T, ix - 6.f, row.B);
        g.PathClipRegion(nameR);
        // IR / pedal rows show the exact source filename as a dim subtitle so
        // people can see where the entry was imported from (rename never touches
        // it). Presets have no source file, so the name stays vertically centred.
        std::string fileSub;
        if (mManageKind == ManageKind::IR)
          fileSub = volum::custom::IRFileAt(i);
        else if (mManageKind == ManageKind::Pedals)
          fileSub = volum::custom::PedalFileAt(i);
        const IColor nameCol = sel ? VoLumColors::TEXT_BRIGHT : VoLumColors::TEXT_MED;
        if (!fileSub.empty())
        {
          g.DrawText(IText(12.5f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Bottom), mItems[(size_t)i].c_str(),
                     nameR.GetFromTop(nameR.H() * 0.56f));
          g.DrawText(IText(9.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top), fileSub.c_str(),
                     nameR.GetFromBottom(nameR.H() * 0.44f));
        }
        else
        {
          g.DrawText(
            IText(13.f, nameCol, "Josefin-Bold", EAlign::Near, EVAlign::Middle), mItems[(size_t)i].c_str(), nameR);
        }
        g.PathClipRegion(listArea);

        AddHotspot(row, kRowBase + i, rowTip);
      }
      g.PathClipRegion();

      if (scrollable)
      {
        const float trackX = listArea.R - sbW - 1.f;
        IRECT track(trackX, listArea.T + 2.f, listArea.R - 1.f, listArea.B - 2.f);
        const float maxScroll = contentH - listArea.H();
        const float thumbH = std::max(18.f, track.H() * (listArea.H() / contentH));
        const float t = (maxScroll > 0.f) ? (mManageScroll / maxScroll) : 0.f;
        const IRECT thumb(track.L, track.T + (track.H() - thumbH) * t, track.R,
                          track.T + (track.H() - thumbH) * t + thumbH);
        DrawVoLumScrollbar(g, track, thumb);
      }
    }

    const IRECT hintR(body.L, body.B - 22.f, body.R, body.B);
    g.PathClipRegion(hintR); // safety net: long copy can never bleed past the panel
    if (!mError.empty())
    {
      g.DrawText(IText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), mError.c_str(), hintR);
    }
    else
    {
      const char* hint = presets
                           ? "Double-click a row to recall. Icons: overwrite / rename / delete."
                           : (mManageKind == ManageKind::IR
                                ? "Import a .wav, then double-click to convolve it with the focused DIRECT capture."
                                : "Import a .nam capture of your pre-amp gear. Appears under CUSTOM in the PRE menu.");
      g.DrawText(IText(9.5f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Middle), hint, hintR);
    }
    g.PathClipRegion();
  }

  void ClampManageScroll(float contentH, float viewH)
  {
    const float maxScroll = std::max(0.f, contentH - viewH);
    mManageScroll = std::clamp(mManageScroll, 0.f, maxScroll);
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
    g.DrawText(IText(13.f, VoLumColors::TEXT_BRIGHT, "Josefin-Sans", EAlign::Near, EVAlign::Middle),
               mBuilderAmp.name.c_str(), nameTextR);
    g.PathClipRegion();
    AddHotspot(nameBox, kEditName, "Edit profile name");

    const IRECT drop(left.L, nameBox.B + 12.f, left.R, nameBox.B + 40.f);
    g.DrawDottedRect(VoLumColors::TEAL_DIM, drop, nullptr, 1.f, 4.f);
    g.DrawText(
      IText(11.f, VoLumColors::TEAL, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "+ Add .nam files", drop);
    AddHotspot(drop, kAddFile, "Add .nam captures - PREFIX-CODE-CHANNEL names auto-fill cab + channel");

    // file rows: filename | speaker | channel | remove (scrollable - a long
    // import must not spill past the panel onto the footer/help text).
    const float rowH = 26.f;
    const float rowStride = rowH + 4.f;
    const IRECT listArea(left.L, drop.B + 10.f, left.R, left.B - 36.f);
    const float contentH = (float)mBuilderAmp.files.size() * rowStride;
    const bool scrollable = contentH > listArea.H() + 0.5f;
    const float sbW = scrollable ? 6.f : 0.f;
    const float maxScroll = std::max(0.f, contentH - listArea.H());
    mBuilderFileScroll = std::clamp(mBuilderFileScroll, 0.f, maxScroll);

    g.PathClipRegion(listArea);
    float y = listArea.T - mBuilderFileScroll;
    for (int i = 0; i < (int)mBuilderAmp.files.size(); i++)
    {
      const IRECT row(left.L, y, left.R - sbW, y + rowH);
      y += rowStride;
      // Cull rows fully outside the viewport. Only fully-visible rows register
      // hotspots, so a partially clipped row can't be clicked through the footer.
      if (row.B < listArea.T || row.T > listArea.B)
        continue;
      const bool rowVisible = (row.T >= listArea.T - 0.5f && row.B <= listArea.B + 0.5f);

      auto& f = mBuilderAmp.files[(size_t)i];
      const bool unassigned = !FileAssigned(f);
      const bool dup = FileIsDuplicate(mBuilderAmp, (size_t)i);
      // The manifest rows are not selectable, so a normal row stays flat (no
      // button-like fill/border). Only the validation states paint: duplicate
      // (slot,channel) -> red, unassigned -> amber.
      const bool flagged = dup || unassigned;
      if (flagged)
      {
        g.FillRect(dup ? IColor(34, 235, 70, 70) : IColor(28, 232, 168, 92), row);
        g.DrawRect(dup ? IColor(220, 235, 90, 90) : VoLumColors::AMBER, row);
      }

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
      g.DrawText(
        IText(10.f, slotOk ? VoLumColors::GOLD : VoLumColors::AMBER, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
        spkLabel.c_str(), spk);
      if (rowVisible)
        AddHotspot(spk, kFileSpeakerBase + i, "Assign this capture to a cabinet (or DIRECT)");

      // channel chip
      char chLabel[12];
      if (f.channel >= 1)
        std::snprintf(chLabel, sizeof(chLabel), "Ch %d", f.channel);
      else
        std::snprintf(chLabel, sizeof(chLabel), "Ch -");
      g.FillRect(VoLumColors::HERO_BG, ch);
      g.DrawRect(VoLumColors::TEAL_DIM, ch);
      g.DrawText(IText(10.f, VoLumColors::TEXT_MED, "Josefin-Bold", EAlign::Center, EVAlign::Middle), chLabel, ch);
      if (rowVisible)
        AddHotspot(ch, kFileChannelBase + i, "Assign this capture to a channel");

      // remove
      g.DrawText(IText(13.f, VoLumColors::GOLD_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle), "x", rem);
      if (rowVisible)
        AddHotspot(rem, kFileRemoveBase + i, "Remove this file");
    }
    g.PathClipRegion();

    if (mBuilderAmp.files.empty())
    {
      g.DrawText(IText(11.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top),
                 "No files yet. Add a .nam, then pick its speaker + channel.",
                 IRECT(left.L, listArea.T, left.R, listArea.T + 18.f));
      // Explain the auto-fill convention so users can name files to skip manual
      // mapping. PREFIX = G12 / G65 / V30 / AMP (DI / DIRECT), last number = channel.
      // Split across two lines (break after "and") so it never overflows the
      // ~53%-width left pane into the art gallery on the right.
      const IText tipText(10.f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Near, EVAlign::Top);
      g.DrawText(tipText, "Tip: name files CAB-NAME-CHANNEL (e.g. G65-Plexi-3.nam) and",
                 IRECT(left.L, listArea.T + 20.f, left.R, listArea.T + 34.f));
      g.DrawText(tipText, "the cab + channel auto-fill. Use AMP- or DI- for a DIRECT capture.",
                 IRECT(left.L, listArea.T + 34.f, left.R, listArea.T + 50.f));
    }

    // Scrollbar for the file manifest (matches the Manage list styling).
    if (scrollable)
    {
      IRECT track(listArea.R - sbW + 1.f, listArea.T + 2.f, listArea.R - 1.f, listArea.B - 2.f);
      g.FillRect(IColor(40, 200, 162, 78), track);
      const float thumbH = std::max(18.f, track.H() * (listArea.H() / contentH));
      const float t = (maxScroll > 0.f) ? (mBuilderFileScroll / maxScroll) : 0.f;
      const float thumbY = track.T + (track.H() - thumbH) * t;
      g.FillRect(VoLumColors::GOLD_DIM, IRECT(track.L, thumbY, track.R, thumbY + thumbH));
    }

    const IRECT builderFooter(left.L, left.B - 30.f, left.R, left.B);
    if (!mError.empty())
    {
      g.PathClipRegion(builderFooter);
      g.DrawText(IText(10.f, VoLumColors::AMBER, "Josefin-Bold", EAlign::Near, EVAlign::Middle), mError.c_str(),
                 builderFooter);
      g.PathClipRegion();
    }
    else
    {
      DrawFooter(g, builderFooter,
                 "Pick a speaker (DIRECT = amp-only, pair with a custom IR) and channel per file.",
                 "Sparse coverage is fine. Stored as a per-amp manifest - your files are never renamed.");
    }

    // ---- RIGHT (top): art picker as a 3x2 gallery -------------------------
    {
      const IRECT artHdr(right.L, right.T, right.R, right.T + 13.f);
      g.DrawText(IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "ART", artHdr);
      g.DrawLine(IColor(70, 198, 162, 90), right.L + 30.f, artHdr.MH(), right.R, artHdr.MH(), nullptr, 1.f);
    }
    const float gTop = right.T + 20.f;
    const float gGap = 8.f;
    const int artCols = 3;
    const float tileW = (right.W() - gGap * (artCols - 1)) / (float)artCols;
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
      // Same cyan identity as the hero/sidebar; selection is just a brighter
      // (more opaque) version of the SAME hue - the gold border + badge below
      // mark the active swatch. No hue shift on select.
      const IColor bright = selArt ? VoLumColors::CUSTOM_ART_BRIGHT
                                   : IColor(150, VoLumColors::CUSTOM_ART_BRIGHT.R, VoLumColors::CUSTOM_ART_BRIGHT.G,
                                            VoLumColors::CUSTOM_ART_BRIGHT.B);
      const IColor dim = selArt ? VoLumColors::CUSTOM_ART_DIM
                                : IColor(120, VoLumColors::CUSTOM_ART_DIM.R, VoLumColors::CUSTOM_ART_DIM.G,
                                         VoLumColors::CUSTOM_ART_DIM.B);
      DrawCustomAmpArt(g, t.GetPadded(-7.f), a, bright, dim);
      g.DrawRoundRect(selArt ? VoLumColors::GOLD : VoLumColors::TEAL_DIM, t, 6.f, nullptr, selArt ? 1.8f : 1.f);
      if (selArt)
      {
        const float bx = t.R - 9.f, by = t.T + 9.f;
        g.FillCircle(VoLumColors::GOLD, bx, by, 6.f);
        g.DrawLine(IColor(255, 22, 22, 30), bx - 2.6f, by + 0.2f, bx - 0.7f, by + 2.4f, nullptr, 1.6f);
        g.DrawLine(IColor(255, 22, 22, 30), bx - 0.7f, by + 2.4f, bx + 3.f, by - 2.4f, nullptr, 1.6f);
      }
      AddHotspot(t, kArtBase + a, "Use this art for the amp");
    }

    // ---- RIGHT (bottom): fixed 4-row coverage grid (DIRECT + 3 cab slots) ---
    const float covTop = gTop + artGridH + 16.f;
    {
      const IRECT covHdr(right.L, covTop, right.R, covTop + 13.f);
      g.DrawText(
        IText(10.f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Near, EVAlign::Middle), "COVERAGE", covHdr);
      g.DrawLine(IColor(70, 198, 162, 90), right.L + 64.f, covHdr.MH(), right.R, covHdr.MH(), nullptr, 1.f);
      g.DrawText(IText(8.5f, VoLumColors::CREAM_DIM, "Josefin-Sans", EAlign::Far, EVAlign::Middle),
                 "tap a cab name to rename", covHdr);
    }

    // Columns: only the channels (gain stages) actually present, labeled with
    // their real numbers. A brand-new amp with nothing assigned shows a single
    // "1" placeholder so the grid never collapses to zero width.
    std::vector<int> channels = volum::custom::AssignedChannels(mBuilderAmp);
    if (channels.empty())
      channels.push_back(1);
    const int nCols = (int)channels.size();
    const float labelW = 64.f;
    const float gridL = right.L + labelW;
    const float cw = std::min(26.f, (right.R - gridL) / (float)nCols);

    // channel header numbers
    float gy = covTop + 20.f;
    {
      const IRECT hrow(right.L, gy, right.R, gy + 14.f);
      for (int ci = 0; ci < nCols; ci++)
      {
        const IRECT cell(gridL + ci * cw, hrow.T, gridL + ci * cw + cw, hrow.B);
        g.DrawText(IText(8.5f, VoLumColors::CREAM_DIM, "Josefin-Bold", EAlign::Center, EVAlign::Middle),
                   std::to_string(channels[(size_t)ci]).c_str(), cell);
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
        AddHotspot(chip, kCabNameBase + slot, "Rename this cab (max 3 chars)");
      }
      for (int ci = 0; ci < nCols; ci++)
      {
        const int c = channels[(size_t)ci];
        const IRECT cell(gridL + ci * cw + 1.f, srow.T + 3.f, gridL + ci * cw + cw - 1.f, srow.B - 3.f);
        const int n = CellFileCount(mBuilderAmp, slot, c);
        if (n >= 2)
        {
          g.FillRect(IColor(40, 235, 70, 70), cell);
          g.DrawRect(IColor(230, 235, 90, 90), cell);
          char badge[6];
          std::snprintf(badge, sizeof(badge), "x%d", n);
          g.DrawText(
            IText(9.f, IColor(255, 255, 150, 150), "Josefin-Bold", EAlign::Center, EVAlign::Middle), badge, cell);
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
               !reason.empty(), "Save this custom amp");
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

  // Gear / cog: a ring of short teeth around a hollow hub. Opens the IR shaping editor.
  static void DrawGearGlyph(IGraphics& g, const IRECT& r, const IColor& col)
  {
    const float cx = r.MW(), cy = r.MH();
    const float t = 1.2f;
    const float rIn = 3.2f, rOut = 6.0f;
    for (int k = 0; k < 8; ++k)
    {
      const float a = (float)k * (3.14159265f / 4.f);
      const float ca = std::cos(a), sa = std::sin(a);
      g.DrawLine(col, cx + ca * rIn, cy + sa * rIn, cx + ca * rOut, cy + sa * rOut, nullptr, t);
    }
    g.DrawCircle(col, cx, cy, rIn, nullptr, t);
  }

  volum::custom::Screen mScreen = volum::custom::Screen::Presets;
  ManageKind mManageKind = ManageKind::Presets;
  int mAmpIdx = 0;
  std::string mAmpName;
  std::vector<std::string> mItems;
  int mSel = -1;
  std::string mError; // transient validation/name banner (Manage + Builder)
  volum::custom::CustomAmp mBuilderAmp;
  std::vector<std::pair<IRECT, int>> mHotspots;
  std::vector<std::string> mHotspotTips; // parallel to mHotspots; hover tooltip text
  int mHoverAction = -1; // hotspot action under the cursor (-1 = none)
  std::string mCurTip; // last tooltip pushed via SetTooltip (de-dupe)

  // in-overlay popup (builder speaker/channel pickers)
  bool mPopupOpen = false;
  PopupKind mPopupKind = PopupKind::Speaker;
  int mPopupFileIdx = -1;
  int mPopupIrIdx = -1; // IR library index for PopupKind::IrSettings
  IRECT mPopupRect;
  std::vector<std::string> mPopupItems;
  std::vector<std::pair<IRECT, int>> mPopupHotspots;

  // pending text entry target
  TextTarget mTextTarget = TextTarget::None;
  int mTextCabSlot = -1; // cab slot being renamed (TextTarget::CabName)
  IText mEntryText;

  int mPedalSlot = -1; // originating PRE NAM slot for ManageKind::Pedals
  int mBuilderEditIdx = -1; // custom-amp index being edited (-1 = new draft)
  float mManageScroll = 0.f; // Manage list scroll offset (px)
  float mBuilderFileScroll = 0.f; // builder file-manifest scroll offset (px)

  BuilderSavedCallback mBuilderSaved;
  ChangedCallback mChanged;
  ConfirmCallback mConfirm;
  PrimaryActionCallback mPrimaryAction;
  SavePresetCallback mSavePreset;
  OverwritePresetCallback mOverwritePreset;
};
