## Summary

This subsystem is not release-safe yet. The highest-risk defects are process-global preset callbacks that point at the last-created plugin instance (and become dangling when it is destroyed), index-based destructive callbacks that can target a different shared-library item than the prompt named, and UTF-8 truncation in cabinet names that can throw during JSON persistence. I found four BLOCKERs, four MAJORs, and one MINOR; the confirmation dialog's basic single-click ordering, list clipping/scroll clamping, IR numeric parser, and settings click shield were otherwise sound.

## Findings

### F-P5-1: BLOCKER — Preset save/recall is routed through the last-created plugin instance and can call a destroyed instance

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:1257-1259`; `NeuralAmpModeler/VoLumCustomContentApi.h:462-470, 504-505, 531-532`; `NeuralAmpModeler/VoLumSettingsPresets.inc.cpp:4-13`

**Mechanism:** The overlay correctly calls the instance captured as `pPlugin`, but the backend operation then invokes process-global static capture/apply hooks. Every plugin instance overwrites those hooks with lambdas capturing its own raw `this`; there is no production teardown that clears or rebinds them. Consequently, the most recently initialized instance owns preset capture/recall for every other instance. If that instance is destroyed while an older instance remains, the static callback retains a dangling `this`.

```cpp
// VoLumCustomOverlay.h:1257-1259
overlay->SetPresetCallbacks([pPlugin](const std::string& name) { return pPlugin->_VolumSavePresetAs(name); },
                            [pPlugin](int index) { pPlugin->_VolumOverwritePreset(index); });
```

```cpp
// VoLumCustomContentApi.h:462-470
inline PresetSettingsCapture& PresetCaptureHook()
{
  static PresetSettingsCapture h;
  return h;
}
inline PresetSettingsApply& PresetApplyHook()
{
  static PresetSettingsApply h;
  return h;
}
```

```cpp
// VoLumSettingsPresets.inc.cpp:4-13
void NeuralAmpModeler::_VolumInstallPresetHooks()
{
  volum::custom::PresetCaptureHook() = [this]() -> volum::VoLumAmpSettings {
    _VolumSaveCurrentToSettings();
    return _VolumActiveScene();
  };
  volum::custom::PresetApplyHook() = [this](const volum::VoLumAmpSettings& s) { _VolumApplyRecalledPreset(s); };
}
```

**Trigger:** In one host process, create plugin instance A, then instance B. Save or overwrite a preset from A: B's scene is captured into A's preset bank. Recall from A: B is changed. Close/destroy B, then save or recall from A: the global hook dereferences B's destroyed object.

**Impact:** Wrong-instance state capture/application, preset corruption, use-after-free, and likely host crash.

**Fix sketch:** Remove process-global instance callbacks. Pass the owning `NeuralAmpModeler` operation explicitly, or make hooks instance-scoped and lifetime-bound. Add deterministic teardown only as defense in depth; it does not solve cross-instance routing by itself.

**Proposed regression test:** `two_instances_route_preset_hooks_to_caller_and_survive_peer_destruction` — assert save/recall in A never reads or mutates B, then destroy B and assert A can still save/recall without a stale callback.

### F-P5-2: BLOCKER — Confirmed delete/overwrite uses a stale shared-library index rather than the item named in the prompt

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:764-798`; `NeuralAmpModeler/VoLumCustomContentApi.h:292-299, 427-434, 558-570`; `NeuralAmpModeler/VoLumContentStore.h:917-927`

**Mechanism:** The prompt snapshots the display name but the confirm lambda snapshots only the row index. The custom-content store is explicitly process-wide, so a second plugin editor can insert/delete from the same list while the first editor's modal is open. The callback later applies that old index to the current vector. If a preceding row was removed, it deletes or overwrites the item that shifted into the index, not the item named by `nm`.

```cpp
// VoLumCustomOverlay.h:787-798
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
```

```cpp
// VoLumContentStore.h:917-927
// Process-wide content store. The custom-content library (amps / IRs / pedals /
// preset banks) is shared user data, so all plugin instances in one host share
// it
inline ContentStore& GlobalContentStore()
{
  static ContentStore store;
  return store;
}
```

**Trigger:** Open two plugin editors. In editor A, request deletion of IR/pedal/preset row N. In editor B, delete any earlier row. Return to A and confirm the still-visible prompt.

**Impact:** Irreversible deletion or overwrite of a different user item than the confirmation named.

**Fix sketch:** Capture the item's stable ID and owning preset-bank key when opening the dialog. On confirm, resolve by ID and abort with a stale-state message if it no longer exists. Never persist a destructive operation by list position.

**Proposed regression test:** `confirm_delete_resolves_snapshot_id_after_concurrent_list_change` — open a confirmation for ID B, remove ID A, confirm, and assert B (not the row now at B's old index) is removed.

### F-P5-3: BLOCKER — Saving an open amp editor can overwrite a different custom amp after a shared-list change

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:106-110, 868-875`; `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1200-1205, 1223-1224`

**Mechanism:** Builder edit state stores `mBuilderEditIdx`, not the amp's immutable ID. At save time the host callback re-reads `CustomAmpIdAt(editIdx)` from the current process-global list and then calls `UpdateCustomAmp(editIdx, amp)`. A deletion before that index in another editor shifts a different amp into the saved index; its ID is adopted and its record is overwritten with the stale draft.

```cpp
// VoLumCustomOverlay.h:106-110
void ShowBuilder(bool editExisting, const char* ampName, int editIdx = -1)
{
  mScreen = volum::custom::Screen::Builder;
  mBuilderEditIdx = editExisting ? editIdx : -1;
  mBuilderAmp = (editExisting && editIdx >= 0) ? volum::custom::CustomAmpAt(editIdx) : NewBuilderAmp(ampName);
```

```cpp
// VoLumLayoutBuild.inc.cpp:1200-1205, 1223-1224
std::string ampId = (editIdx >= 0) ? volum::custom::CustomAmpIdAt(editIdx) : std::string();
if (ampId.empty())
{
  amp.id = volum::content::MintId(store.reg(), "amp");
  ampId = amp.id;
}
// ...
const int idx =
  (editIdx >= 0) ? volum::custom::UpdateCustomAmp(editIdx, amp) : volum::custom::AddCustomAmp(amp);
```

**Trigger:** Open amp B for editing in editor A. In editor B, delete an amp preceding B. Save the still-open builder in editor A.

**Impact:** A different custom amp is silently replaced, including its manifest and identity-linked content.

**Fix sketch:** Store the immutable amp ID in builder state. Resolve its current index by ID immediately before commit; if absent, stop and report that the source amp was removed. The import transaction must also use that original ID.

**Proposed regression test:** `builder_save_updates_original_amp_id_after_index_shift` — edit B, remove A, save, and assert B is updated while C remains byte-for-byte unchanged.

### F-P5-4: BLOCKER — A three-byte-truncated cabinet glyph creates invalid UTF-8 and can throw while saving the registry

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:344-349`; `NeuralAmpModeler/VoLumCustomModel.h:88-101`; `NeuralAmpModeler/VoLumSettingsFileIO.h:90-99`

**Mechanism:** Cabinet text entry is normalized byte-by-byte and stops after three bytes, not three Unicode code points. A four-byte glyph such as an emoji is truncated to an invalid three-byte UTF-8 prefix and stored in `mBuilderAmp.cabNames`. Saving serializes that string through `nlohmann::json::dump()` with the default strict UTF-8 error handler and no exception guard on this path.

```cpp
// VoLumCustomModel.h:90-101
inline std::string NormalizeCabName(const std::string& in)
{
  std::string s;
  for (char c : in)
  {
    if (std::isspace((unsigned char)c))
      continue;
    s.push_back((char)std::toupper((unsigned char)c));
    if (s.size() >= 3)
      break;
  }
  return s;
}
```

```cpp
// VoLumCustomOverlay.h:344-349
case TextTarget::CabName:
  if (mTextCabSlot >= 0 && mTextCabSlot < kNumCabSlots)
  {
    const std::string norm = NormalizeCabName(s);
    if (!norm.empty())
      mBuilderAmp.cabNames[(size_t)mTextCabSlot] = norm;
  }
```

```cpp
// VoLumSettingsFileIO.h:90-99
const auto tmp = MakeAtomicJsonTempPath(target);
{
  std::ofstream out(tmp, std::ios::out | std::ios::trunc);
  // ...
  out << json.dump(2);
```

**Trigger:** Rename a cabinet to a four-byte character such as `😀`, then save the custom amp.

**Impact:** Uncaught JSON UTF-8 exception on the UI thread, failed persistence, and likely plugin/host termination.

**Fix sketch:** Normalize by decoded Unicode code points (or explicitly restrict cabinet labels to validated ASCII). Validate UTF-8 before updating draft state and catch persistence exceptions at the UI transaction boundary.

**Proposed regression test:** `cab_name_unicode_never_persists_invalid_utf8` — submit a four-byte glyph and assert the normalized result is valid UTF-8 and registry save does not throw.

### F-P5-5: MAJOR — Exact entry converts invalid text to zero/minimum and applies it

**Where:** `NeuralAmpModeler/VoLumExactEntry.h:90-101`; call into `iPlug2/IGraphics/IGraphics.cpp:233-248` and `iPlug2/IPlug/IPlugParameter.cpp:358-377`

**Mechanism:** `VoLumExactEntryControl` delegates parameter text directly to iPlug2 and hides after `SetValueFromUserInput`; it performs no syntax or finiteness validation. iPlug2 calls `IParam::StringToValue`, whose numeric fallback is `atof(str)`. Empty text, whitespace, `abc`, and `-` therefore become `0`; comma-decimal input is partially parsed; overflow becomes a rail value after constraining. The result is then applied as a real user parameter change.

```cpp
// VoLumExactEntry.h:97-101
void SetValueFromUserInput(double value, int valIdx) override
{
  mEditing = false;
  Hide(true);
  IControl::SetValueFromUserInput(value, valIdx);
}
```

```cpp
// iPlug2/IGraphics/IGraphics.cpp:240-244
if (pParam)
{
  const double v = pParam->StringToValue(str);
  mInTextEntry->SetValueFromUserInput(pParam->ToNormalized(v), mTextEntryValIdx);
}
```

```cpp
// iPlug2/IPlug/IPlugParameter.cpp:366-374
if (!mapped && Type() != kTypeEnum && Type() != kTypeBool)
{
  v = atof(str);
  // ...
  v = Constrain(v);
  mapped = true;
}
```

**Trigger:** Open exact entry on any knob, replace the value with `abc` (or clear it), and press Enter.

**Impact:** The parameter unexpectedly jumps to zero or its minimum instead of rejecting the invalid input and preserving the current value.

**Fix sketch:** Parse locally with `strtod`, require at least one digit and full consumption apart from accepted units/whitespace, reject non-finite values, and leave the parameter unchanged with visible validation feedback.

**Proposed regression test:** `exact_entry_invalid_text_preserves_parameter` — assert `""`, whitespace, `abc`, `-`, `1,5`, and `1e999` do not change the parameter; assert valid boundary values are clamped deliberately.

### F-P5-6: MAJOR — IR/pedal import records failed copies as successful and leaks files when pedal capacity is full

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:596-621, 624-638`; `NeuralAmpModeler/VoLumContentStore.h:778-796`

**Mechanism:** `ImportFileCopy` documents and implements an empty return on any production copy failure. The overlay treats empty as the unit-test-only case and falls back to the bare source leaf, then creates a registry entry and increments `added`. For pedals, the file is copied before `AddPedal`; when the finite index pool is full, the copied file is not rolled back. The capacity error assigned at line 617 is subsequently erased by the unconditional `mError.clear()` at lines 637-638 when there were no duplicate/oversize errors.

```cpp
// VoLumCustomOverlay.h:600-615
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
```

```cpp
// VoLumCustomOverlay.h:615-621, 630-638
if (volum::custom::AddPedal(base, rel) < 0)
{
  mError = "Custom pedal slots are full - delete a pedal first.";
  continue;
}
++added;
// ...
else
  mError.clear();
```

**Trigger:** Import with a read-only/full/unavailable content directory, or import a pedal after the custom pedal index pool is exhausted.

**Impact:** The UI reports success for an entry whose owned file does not exist; full-capacity pedal attempts leave orphaned copied files and display no failure.

**Fix sketch:** Treat empty copy results as fatal whenever `BaseDir()` is configured. Add the registry item only after a successful copy, roll the copy back if registry insertion fails, and aggregate capacity/copy errors without clearing them.

**Proposed regression test:** `overlay_import_is_transactional_on_copy_and_capacity_failure` — inject copy failure and full pedal capacity; assert no registry entry, no orphan file, `added == 0`, and a non-empty visible error.

### F-P5-7: MAJOR — Double-clicking Confirm clicks through into the underlying Manage list

**Where:** `NeuralAmpModeler/VoLumConfirmDialog.h:95-103`; `NeuralAmpModeler/VoLumCustomOverlay.h:269-297`; iPlug dispatch at `iPlug2/IGraphics/IGraphics.cpp:1181-1203`

**Mechanism:** Confirm runs and hides on the first mouse-down. The subsequent double-click event is independently hit-tested by iPlug2; because the confirm control is now hidden, the full-window Manage overlay underneath receives `OnMouseDblClick`. At the 900x600 default size, `DeleteRect()` is x=456..654/y=328..354, which overlaps Manage row 5's body. With at least five rows, that second event runs row 5's primary action and hides the Manage overlay.

```cpp
// VoLumConfirmDialog.h:95-103
if (DeleteRect().Contains(x, y))
{
  auto cb = mOnConfirm;
  Hide(true);
  if (cb)
    cb();
  return;
}
```

```cpp
// VoLumCustomOverlay.h:283-293
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
```

**Trigger:** In a Manage list containing at least five items, open Delete/Overwrite confirmation and double-click the confirm button.

**Impact:** Besides the confirmed operation, VoLum immediately recalls an unrelated preset, selects an unrelated IR, or loads an unrelated pedal, then closes Manage.

**Fix sketch:** Make modal activation one-shot through mouse-up/click completion and retain a hit-blocking scrim through the end of the gesture; alternatively consume/suppress the matching double-click after confirmation.

**Proposed regression test:** `confirm_double_click_never_reaches_underlying_overlay` — dispatch down/up/double-click at `DeleteRect` and assert exactly one confirm callback and zero Manage primary-action callbacks.

### F-P5-8: MAJOR — Bulk imports and amp saves synchronously copy and parse files on the host UI thread

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:550-621, 817-859, 868-875`; `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1206-1218`

**Mechanism:** Button handlers synchronously open the blocking multi-file picker, query/copy every selected file, and call `Store().Save()` once per imported IR/pedal. Amp Save synchronously copies and invokes `nam::get_dsp` for every capture before returning to the click handler. There is no worker, progress state, cancellation, or reentrancy guard.

```cpp
// VoLumCustomOverlay.h:552-569
auto* ui = GetUI();
// ...
ui->PromptForFiles(path, files, ext); // multi-select; each entry is a full path
if (files.empty())
  return;
// ...
for (const auto& fn : files)
{
```

```cpp
// VoLumLayoutBuild.inc.cpp:1206-1217
auto prepared = volum::content::PrepareCustomNamImport(
  store, amp, ampId, [](const std::filesystem::path& path) -> std::string {
    try
    {
      nam::dspData config;
      auto model = nam::get_dsp(path, config);
      return model ? std::string() : std::string("NAM parser returned no model");
    }
    catch (const std::exception& e)
```

**Trigger:** Multi-select many captures, use files on slow/network storage, or save a custom amp with several large/complex NAM files.

**Impact:** The plugin editor and host main UI become unresponsive for the entire copy/parse sequence; hosts may display “not responding,” and users can force-close during a partially completed non-amp import.

**Fix sketch:** Move copy/validation/model parsing to a cancellable worker transaction, disable the initiating controls while active, show progress, and marshal only the final registry/UI commit back to the UI thread.

**Proposed regression test:** `bulk_import_click_handler_returns_before_slow_io_completes` — inject a blocking copier/parser and assert the UI dispatch returns promptly, actions are disabled while pending, and the final commit is atomic.

### F-P5-9: MINOR — Action-code ranges alias or stop working at 100 list items/files

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:377-387, 717-739, 891-909`

**Mechanism:** Row/file actions are encoded as `base + index`, but each decode range is only 100 wide and adjacent bases are 100 apart. Neither preset/IR lists nor builder file selection are capped at 99. At index 100, Manage overwrite code 600 is decoded as rename index 0; Manage rename code 700 is decoded as delete index 0; builder speaker code 300 changes channel for file 0; builder channel code 400 removes file 0; builder remove code 500 is ignored.

```cpp
// VoLumCustomOverlay.h:377-386
kCabNameBase = 70,
kArtBase = 80,
kRowBase = 100,
kFileSpeakerBase = 200,
kFileChannelBase = 300,
kFileRemoveBase = 400,
kRowOverwriteBase = 500,
kRowRenameBase = 600,
kRowDeleteBase = 700,
kRowIrCfgBase = 800,
```

```cpp
// VoLumCustomOverlay.h:717-733
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
```

**Trigger:** Create/import at least 101 presets/IRs, or select 101 NAM files in Builder, then use an inline action on row/file 101.

**Impact:** The wrong earlier row is edited/removed or the clicked action silently does nothing. Manage's confirmation text can expose the mismatch, but Builder's wrong-file mutation is immediate.

**Fix sketch:** Stop encoding indices into overlapping integer ranges. Store typed hotspot records `{kind, stableId/index}` or allocate ranges based on a proven maximum and enforce that maximum before adding.

**Proposed regression test:** `overlay_actions_do_not_alias_at_index_100` — create 101 rows/files and assert every action on index 100 targets only index 100.

## Voicing observations (report only)

None. No audio-voicing changes are suggested.

## Areas read and found clean

- Read every line of `VoLumCustomOverlay.h`, `VoLumCustomUi.h`, `VoLumConfirmDialog.h`, `VoLumExactEntry.h`, `VoLumListMenu.h`, and `VoLumSettingsOverlay.h`.
- Confirmation callback ordering for a normal single click/Enter is sound: the dialog copies the callback, hides first, then invokes it.
- Confirmation Escape/cancel paths do not invoke the stored action.
- `VoLumListMenuControl` clips rows, clamps wheel scrolling, and avoids divide-by-zero when content is not scrollable.
- Manage and Builder drawing clip long lists and clamp scroll offsets; zero-item coverage uses a nonzero placeholder column.
- The IR trim/low-cut/high-cut typed-entry path uses a full-consumption parser, rejects unknown suffixes/non-finite text, preserves the old value on invalid input, and clamps valid values.
- Settings backdrop consumes shell clicks across the full window, so empty settings pixels do not click through to the main UI.
- Settings close-control parent access is synchronous and parent-owned; no escaping raw child pointer was found.
- Custom overlay callbacks reacquire `GetUI()` before touching editor controls after backend work.
