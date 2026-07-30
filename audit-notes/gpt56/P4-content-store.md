## Summary

The content model uses stable IDs and the custom-NAM happy-path rollback is thoughtfully structured, but the subsystem is not durably transactional as a whole. The highest-severity issue is deletion: payload files are removed before the updated registry is known to be durable, so an ordinary registry write failure leaves the on-disk catalog pointing at files VoLum has already destroyed. Registry-open failures, unchecked registry paths, and whole-registry last-writer-wins behavior add further data-loss paths. The under-exercised IR path also accepts unusable WAVs and can synchronously decode every uncalibrated IR during plugin construction.

## Findings

### F-P4-1: BLOCKER — A temporarily unreadable registry is treated as a clean empty library

**Where:** `NeuralAmpModeler/VoLumContentStore.h:711-721`, production load at `NeuralAmpModeler/NeuralAmpModeler.cpp:497-503`

**Evidence:**

```cpp
// VoLumContentStore.h:711-721
bool Load()
{
  mReg = Registry{};
  const auto path = RegistryPath();
  std::error_code ec;
  if (mBase.empty() || !std::filesystem::exists(path, ec))
    return true;

  std::ifstream in(path, std::ios::binary);
  if (!in.good())
    return true;
```

```cpp
// NeuralAmpModeler.cpp:497-503
if (!contentDir.empty())
{
  volum::content::GlobalContentStore().SetBaseDir(contentDir);
  volum::content::GlobalContentStore().Load();
  _VolumMigrateIrTrims();
}
```

**Mechanism:** `Load()` clears `mReg` before opening the existing file. If the open fails, it returns `true`, which is also the clean-load result. The caller ignores the result in either case. Any later library or custom-scene save serializes the empty/incomplete in-memory registry over the still-valid file.

**Trigger:** Antivirus, backup software, cloud sync, another VoLum process, or permissions temporarily deny opening `volum-content.json`; the user then imports/renames content, saves a preset, serializes a focused custom-amp scene, or closes the instance.

**Impact:** The entire custom amp/IR/pedal/preset catalog can be overwritten with an empty or partial registry. Copied payload files remain, but their IDs, names, scenes, and preset references are lost.

**Fix sketch:** Do not clear the live registry until a complete load succeeds. Return a distinct failure state for “exists but unreadable,” surface it, and disable all writes until the registry can be reloaded or the user explicitly chooses recovery.

**Proposed regression test:** `ContentStore_unreadable_existing_registry_preserves_live_and_disk_state` — force open failure on an existing registry and assert `Load()` is false, the prior in-memory registry remains intact, and a normal mutation cannot overwrite the file.

### F-P4-2: BLOCKER — Delete removes the only managed payload before registry commit

**Where:** `NeuralAmpModeler/VoLumContentStore.h:813-823, 845-853, 873-885`; ignored commit result at `NeuralAmpModeler/VoLumCustomContentApi.h:213-220, 292-299, 427-434`

**Evidence:**

```cpp
// VoLumContentStore.h:845-853
for (auto it = mReg.irs.begin(); it != mReg.irs.end(); ++it)
{
  if (it->id == id)
  {
    RemoveStoredFile(it->file);
    mReg.irs.erase(it);
    break;
  }
}
```

```cpp
// VoLumCustomContentApi.h:292-299
inline void DeleteIR(int idx)
{
  auto& irs = Store().reg().irs;
  if (idx >= 0 && idx < (int)irs.size())
  {
    Store().RemoveIR(irs[(size_t)idx].id);
    Store().Save();
  }
}
```

The pedal and custom-amp delete paths have the same order and also discard `Save()`'s result.

**Mechanism:** The copied `.wav`/`.nam` is deleted first, then the in-memory registry is mutated, then `Save()` is attempted. If the atomic write fails, the old on-disk registry survives and still references the now-deleted payload. There is no snapshot/rollback and no way to restore the payload from VoLum's managed library.

**Trigger:** Delete any custom IR, pedal, or amp while the disk is full, the registry directory is read-only, or replacement of `volum-content.json` fails with an access/sharing violation.

**Impact:** On restart the item reappears but cannot load. Custom amps may lose several captures; scenes and presets continue to reference dead content. This violates the stated removal and transaction guarantees.

**Fix sketch:** Stage deletion by first committing a registry without the item, then remove payloads; if physical removal fails, retain a retryable garbage-collection record. Return and display failure. For a stronger transaction, rename payloads into a trash/staging directory and restore them if registry commit fails.

**Proposed regression test:** `DeleteIR_registry_save_failure_keeps_payload_and_registry_consistent` — inject a failed atomic registry write and assert either both entry and file remain or both are removed; today's code leaves entry present on disk and file absent.

### F-P4-3: BLOCKER — Registry paths can escape the content directory and delete arbitrary files

**Where:** `NeuralAmpModeler/VoLumContentStore.h:579-580, 612-613, 700-705, 799-805`

**Evidence:**

```cpp
// VoLumContentStore.h:700-705
std::filesystem::path ResolveStored(const std::string& relPath) const
{
  if (relPath.empty())
    return {};
  return mBase / PathFromUtf8(relPath);
}
```

```cpp
// VoLumContentStore.h:799-805
void RemoveStoredFile(const std::string& relPath)
{
  if (relPath.empty())
    return;
  std::error_code ec;
  std::filesystem::remove(ResolveStored(relPath), ec);
}
```

Registry `path`/`storedPath` strings are accepted without checking that they are relative, normalized, or contained under `mBase`.

**Mechanism:** A registry entry containing `../../some-file` resolves outside the VoLum library; an absolute path can replace the base altogether depending on platform path rules. Delete then passes that resolved path directly to `filesystem::remove`.

**Trigger:** The user restores, syncs, or hand-repairs a malformed `volum-content.json` containing an absolute or parent-traversing IR, pedal, or amp capture path, then deletes that entry in VoLum.

**Impact:** VoLum can delete an unrelated file outside its content directory. This is direct user-data loss.

**Fix sketch:** Validate registry paths on load and before every resolve/delete: reject absolute paths, reject `..`, canonicalize the candidate and require it to be a descendant of the canonical base, and optionally require the expected `amps/`, `ir/`, or `pedals/` prefix.

**Proposed regression test:** `RemoveStoredFile_rejects_paths_outside_content_base` — register `../../sentinel.wav`, delete the entry, and assert the outside sentinel still exists and the malformed entry is healed/rejected.

### F-P4-4: MAJOR — IR and pedal import reports success after copy or registry failure

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:596-621`; `NeuralAmpModeler/VoLumCustomContentApi.h:270-279, 400-414`

**Evidence:**

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
  if (volum::custom::AddPedal(base, rel) < 0)
```

```cpp
// VoLumCustomContentApi.h:270-279
reg.irs.push_back(std::move(it));
Store().Save();
return (int)reg.irs.size() - 1;
```

**Mechanism:** Any production copy failure is converted to a bare filename intended as a unit-test fallback. That filename later resolves as `<content-base>/<leaf>`, not the user-selected source, so it is normally nonexistent. If the copy succeeds but registry save fails, `AddIR`/`AddPedal` still returns success and keeps the session-only entry. If the pedal index pool is full, its file has already been copied and is never removed.

**Trigger:** Import an IR/pedal while its destination is unwritable/full, registry replacement is denied, or the 64 custom-pedal slots are exhausted.

**Impact:** The UI says the item was added, but it cannot load or disappears after restart; successful copies can be left orphaned. Batch import increments `added` and clears errors despite these failures.

**Fix sketch:** Allow the bare-leaf fallback only in an explicitly injected in-memory test store. Return structured errors from copy and `Add*`; on any registry failure remove the just-copied file and restore the in-memory registry. Check pedal capacity before copying.

**Proposed regression test:** `IR_import_copy_failure_does_not_add_phantom_entry` — force `ImportFileCopy` to fail with a configured base and assert no registry row is added, `added` remains zero, and an error is shown.

### F-P4-5: MAJOR — Deleting the currently playing custom amp leaves its old DSP running

**Where:** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:163-200`; correct factory-switch behavior at `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:80-86`

**Evidence:**

```cpp
// VoLumLayoutBuild.inc.cpp:186-200
if (mVolumCustomMainIdx == customIdx)
  mVolumCustomMainIdx = -1;
else if (mVolumCustomMainIdx > customIdx)
  --mVolumCustomMainIdx;
...
if (mVolumCustomSupportIdx == customIdx)
  mVolumCustomSupportIdx = -1;
else if (mVolumCustomSupportIdx > customIdx)
  --mVolumCustomSupportIdx;
_VolumSyncPresetOwner();
_VolumRefreshPresetBar();
```

The normal factory selection additionally performs:

```cpp
// VoLumLayoutBuild.inc.cpp:80-86
_VolumSaveCurrentToSettings();
mVolumAmpIdx = ampIdx;
mVolumCustomMainIdx = -1;
_VolumRestoreFromSettings(ampIdx);
_VolumRefreshChannels();
mVolumNeedsLoad.store(true);
```

**Mechanism:** Delete updates indices and redraws factory identity, but never restores the factory scene, refreshes channels, queues a main model load, or queues a support load/removal. The already-instantiated NAM DSP object is independent of the deleted catalog/file and keeps processing.

**Trigger:** Select a custom amp as MAIN (or as the active SUPPORT partner), play audio, and delete that same amp from the sidebar.

**Impact:** The UI shows a factory amp or no custom support amp while audio still uses the deleted custom model. It can remain wrong until another action happens to request a model load.

**Fix sketch:** Route deletion of a focused item through the same full transition used by factory/support selection, including scene restore, channel refresh, `mVolumNeedsLoad`/`mVolumSupportNeedsLoad`, IR reconciliation, and preset/UI synchronization.

**Proposed regression test:** `Deleting_focused_custom_amp_queues_factory_and_support_reload` — assert the relevant load flag is set and the next resolved load path is the visible factory/support choice; today's callback only changes indices.

### F-P4-6: BLOCKER — Concurrent VoLum processes overwrite each other’s entire library

**Where:** `NeuralAmpModeler/VoLumContentStore.h:749-755, 917-927`; one-time load at `NeuralAmpModeler/NeuralAmpModeler.cpp:497-500`

**Evidence:**

```cpp
// VoLumContentStore.h:749-755
bool Save()
{
  if (mBase.empty())
    return true;
  std::error_code ec;
  std::filesystem::create_directories(mBase, ec);
  return WriteJsonAtomically(RegistryPath(), RegistryToJson(mReg), ec);
}
```

```cpp
// VoLumContentStore.h:917-927
// Process-wide content store. The custom-content library ... is shared user data,
// so all plugin instances in one host share it
inline ContentStore& GlobalContentStore()
{
  static ContentStore store;
  return store;
}
```

**Mechanism:** Each process holds its own registry snapshot and every save rewrites that snapshot in full. There is no inter-process lock, generation check, reload/merge, or compare-and-swap. Inside one host, every new plugin constructor also reloads the process-global registry without a mutex, so it can discard another instance's unsaved in-memory changes.

**Trigger:** Keep standalone VoLum open while a DAW plugin instance is open. Import an IR in one process, then rename content/save a preset/flush a custom scene from the other process, whose in-memory snapshot predates the import.

**Impact:** The later writer silently drops the other process's new items or edits. Their copied payloads become orphans; IDs referenced by projects/presets can disappear. Concurrent same-process constructors/UI callbacks also access `mReg` without synchronization, which is a C++ data race if the host uses different threads.

**Fix sketch:** Add a cross-process lock around read-modify-write, persist and compare a registry generation, reload under the lock, merge mutations by stable ID, and serialize same-process access with a mutex or a single owner/dispatcher.

**Proposed regression test:** `Two_content_store_writers_merge_nonconflicting_imports` — load two stores from the same base, add a different IR to each, save A then B, and assert a reload contains both. Today's result contains only B's snapshot.

### F-P4-7: MAJOR — Multiple uncalibrated IRs are fully decoded synchronously during construction

**Where:** constructor call at `NeuralAmpModeler/NeuralAmpModeler.cpp:497-503`; migration at `NeuralAmpModeler/VoLumSceneRig.inc.cpp:785-821`; guard behavior at `NeuralAmpModeler/VoLumIrFileGuard.h:43-56`

**Evidence:**

```cpp
// NeuralAmpModeler.cpp:497-503
volum::content::GlobalContentStore().SetBaseDir(contentDir);
volum::content::GlobalContentStore().Load();
_VolumMigrateIrTrims();
```

```cpp
// VoLumSceneRig.inc.cpp:791-807
auto& irs = volum::content::GlobalContentStore().reg().irs;
for (auto& ir : irs)
{
  if (ir.trimCalibrated)
    continue;
  ...
  std::vector<float> audio;
  double fileSr = 0.0;
  if (!abs.empty()
      && dsp::wav::Load(absUtf8.c_str(), audio, fileSr) == dsp::wav::LoadReturnCode::SUCCESS && !audio.empty())
  {
    double sumSq = 0.0;
    for (float v : audio)
      sumSq += static_cast<double>(v) * static_cast<double>(v);
```

**Mechanism:** Plugin construction calls migration inline. It decodes every uncalibrated WAV in full and scans every sample. It does not invoke `IrFileSizeAcceptable`; pre-1.2.1 imports were not protected by the new 64 MB guard. A batch of new imports also invokes this migration synchronously from the manage-panel changed callback.

**Trigger:** Upgrade with several pre-1.2.1 custom IRs, especially long/large WAVs, or multi-select many near-64 MB IRs in 1.2.1.

**Impact:** DAW plugin scanning/project opening or the import UI can freeze for a long time and consume large transient memory. If the final migration save fails, all entries remain uncalibrated on disk and the work repeats on every instantiation.

**Fix sketch:** Validate and analyze IRs before catalog commit on a worker, stream only the sample window the convolver uses, enforce the size guard in migration too, persist progress incrementally/transactionally, and never perform bulk decoding in the plugin constructor/UI callback.

**Proposed regression test:** `IR_trim_migration_is_bounded_and_not_constructor_synchronous` — provide multiple oversized legacy IRs and assert startup does not call the full decoder for each and does not block on their total sample count.

### F-P4-8: MAJOR — Any small `.wav`, including zero-byte and non-RIFF files, is persisted as a valid IR

**Where:** `NeuralAmpModeler/VoLumCustomOverlay.h:583-607`; `NeuralAmpModeler/VoLumIrFileGuard.h:23-29`; later failure at `NeuralAmpModeler/VoLumSceneRig.inc.cpp:526-549`

**Evidence:**

```cpp
// VoLumIrFileGuard.h:23-29
inline constexpr std::uintmax_t kMaxIrFileBytes = 64ull * 1024ull * 1024ull;
inline bool IrFileBytesAcceptable(std::uintmax_t bytes)
{
  return bytes <= kMaxIrFileBytes;
}
```

```cpp
// VoLumCustomOverlay.h:583-607
if (mManageKind == ManageKind::IR)
{
  std::error_code ec;
  const std::uintmax_t bytes = std::filesystem::file_size(..., ec);
  if (!ec && !volum::IrFileBytesAcceptable(bytes))
    ...
}
...
std::string rel = store.ImportFileCopy(...);
...
volum::custom::AddIR(base, rel);
```

**Mechanism:** Import checks only the extension supplied to the picker and an upper byte limit. There is no lower bound or WAV parse before copying and committing. Migration treats decode failure as calibrated unity and saves that status without surfacing an import error. Only later selection calls `_StageIR` and reports loader failure.

**Trigger:** Import a renamed text file, zero-byte file, truncated RIFF, unsupported bit depth/channel layout, or WAV with invalid chunk lengths under 64 MB.

**Impact:** The global library permanently contains an unusable IR that appeared to import successfully. Batch import gives no per-file validation result; projects/presets can then persist its stable ID and fail whenever recalled.

**Fix sketch:** Before registry commit, parse the copied file with the production WAV loader (plus explicit RIFF/chunk/length sanity checks), require nonempty decoded audio and supported format, and remove the copy on any failure.

**Proposed regression test:** `IR_import_rejects_zero_byte_and_truncated_RIFF_before_catalog_commit` — assert no entry/file remains and a concrete validation error is returned. Today's size predicate accepts both.

### F-P4-9: MAJOR — Filename parsing accepts channels beyond the UI/runtime cap

**Where:** `NeuralAmpModeler/VoLumCustomModel.h:38, 72-75, 364-378, 287-299`; builder use at `NeuralAmpModeler/VoLumCustomOverlay.h:839-852`

**Evidence:**

```cpp
// VoLumCustomModel.h:38,72-75
inline constexpr int kMaxChannels = 8;
inline bool FileAssigned(const CustomNamFile& f)
{
  return f.channel >= 1 && SlotAssigned(f.slot);
}
```

```cpp
// VoLumCustomModel.h:364-378
const std::string& last = tok.back();
bool numeric = !last.empty();
...
if (numeric)
{
  int ch = 0;
  for (char c : last)
    ch = ch * 10 + (c - '0');
  r.channel = ch;
}
```

`SaveDisabledReason()` only checks unassigned and duplicate cells; it never rejects `channel > kMaxChannels`.

**Mechanism:** A conventionally named capture such as `AMP-MyAmp-9.nam` is auto-assigned channel 9. `FileAssigned` treats it as valid and Save is enabled, but `AssignedChannels()` explicitly excludes channels above 8, so the saved amp has no navigable gain stage for that capture. A sufficiently long numeric suffix also causes signed `int` overflow while parsing.

**Trigger:** Import a valid NAM whose filename ends in `-9.nam` or any larger numeric suffix and save the auto-filled builder row.

**Impact:** VoLum accepts and persists a custom amp/capture that the channel UI cannot select; an amp consisting only of such captures is effectively unusable.

**Fix sketch:** Parse with overflow detection, require `1 <= channel <= kMaxChannels` at parsing, save validation, and registry-load boundaries, and leave out-of-range filenames unassigned with a clear builder error.

**Proposed regression test:** `SaveDisabledReason_rejects_channel_above_kMaxChannels` — parse `AMP-Test-9.nam`, build an otherwise valid amp, and assert Save is disabled. Today the assertion fails because the reason is empty.

### F-P4-10: MINOR — Editing an amp leaks every removed or replaced capture

**Where:** `NeuralAmpModeler/VoLumCustomContentApi.h:173-201`; commit call at `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1221-1230`

**Evidence:**

```cpp
// VoLumCustomContentApi.h:194-200
CustomAmp previous = reg.amps[(size_t)idx];
reg.amps[(size_t)idx] = std::move(a);
if (!Store().Save())
{
  reg.amps[(size_t)idx] = std::move(previous);
  return -1;
}
```

```cpp
// VoLumLayoutBuild.inc.cpp:1223-1230
const int idx =
  (editIdx >= 0) ? volum::custom::UpdateCustomAmp(editIdx, amp) : volum::custom::AddCustomAmp(amp);
if (idx < 0)
{
  for (const auto& rel : prepared.copiedPaths)
    store.RemoveStoredFile(rel);
```

**Mechanism:** On successful edit, `previous` is destroyed without comparing its `storedPath` set to the new manifest. Only newly copied files are tracked, and they are cleaned only when save fails. A capture removed in the builder or replaced by another file therefore remains in `amps/` forever with no registry reference.

**Trigger:** Edit a custom amp, remove one of its captures (or replace it with a newly selected capture), and save.

**Impact:** Managed storage grows with unreachable NAM files; repeated edits can leave many large models behind. The UI provides no way to discover or remove them.

**Fix sketch:** After the new registry is durably committed, compute `old storedPaths - new storedPaths` and garbage-collect those files. Keep cleanup after commit so rollback never deletes the old working capture.

**Proposed regression test:** `UpdateCustomAmp_removes_obsolete_capture_after_successful_commit` — edit a two-capture amp down to one and assert the removed capture's managed file is gone while the retained one still exists. Today both remain.

### F-P4-11: MINOR — A crash during custom-NAM preparation leaves transaction files permanently orphaned

**Where:** `NeuralAmpModeler/VoLumCustomNamImport.h:39-63, 90-92`; caller commit at `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1206-1229`

**Evidence:**

```cpp
// VoLumCustomNamImport.h:57-63
const std::string rel = store.ImportFileCopy(
  PathFromUtf8(file.sourcePath), "amps", transactionPrefix + "_" + std::to_string(i));
if (rel.empty())
  return fail(file, "could not copy the capture into the VoLum content library");
file.storedPath = rel;
result.copiedPaths.push_back(rel);
```

Handled errors call `fail()` and remove `copiedPaths`, and registry-save failure is also cleaned by the caller. There is no durable transaction marker or startup scavenger.

**Mechanism:** Copied files are written directly into the final `amps/` directory before the registry references them. Cleanup exists only in live control flow. A host/plugin/process crash after a copy and before successful registry commit bypasses every cleanup path.

**Trigger:** The DAW or standalone process crashes/is killed while saving a multi-capture amp, after at least one copy completes but before `AddCustomAmp`/`UpdateCustomAmp` commits.

**Impact:** One or more transaction-prefixed NAM files remain permanently orphaned. The catalog stays consistent, but disk usage leaks.

**Fix sketch:** Copy into a transaction staging directory with a durable manifest, atomically promote files only after registry commit, and scavenge abandoned transaction directories at startup after an age threshold.

**Proposed regression test:** `Startup_scavenges_uncommitted_custom_NAM_transaction` — create transaction-prefixed staged files with no registry references, reopen the store, and assert they are removed/quarantined. No recovery exists today.

## Voicing observations (report only)

None. No filter curves, gains, envelope timing, mix laws, or effect tuning were evaluated as defects in this pass.

## Areas read and found clean

- `NeuralAmpModeler/VoLumContentStore.h` — read in full. Stable opaque IDs, UTF-8 filesystem conversion, preset/scene reference clearing, IR-shaping serialization, and handled parse-failure backup flow were checked. The pure ID lookup and reference-clearing loops are internally consistent on their normal paths.
- `NeuralAmpModeler/VoLumCustomContentApi.h` — read in full. ID/index conversion is bounds-checked; custom-amp add/update correctly rolls the in-memory manifest back when its checked `Save()` fails; preset banks are keyed by stable owner IDs.
- `NeuralAmpModeler/VoLumCustomModel.h` — read in full. Slot/channel collision detection, channel-first cab resolution, case-insensitive IR/pedal/preset name helpers, and UTF-8-safe name truncation were checked. The out-of-range channel hole is reported above.
- `NeuralAmpModeler/VoLumCustomNamImport.h` — read in full. For non-crash failures, every copied path is recorded before subsequent validation, copy/regular-file/parser failures remove all transaction copies, old captures are protected by transaction-specific destination names, and registry mutation is deferred to the caller.
- `NeuralAmpModeler/VoLumIrFileGuard.h` — read in full. The upper-size threshold arithmetic and error messaging are sound; format/lower-bound omissions and migration bypass are reported above.
- `NeuralAmpModeler/VoLumAmpeteCatalog.h` — read in full. The 15 catalog entries, folder/display separation, speaker-prefix order, and fixed array cardinalities are internally aligned.
- `NeuralAmpModeler/VoLumAmpList.h` — read in full. Normal delete callbacks explicitly rebase selected main/support indices; draw-time scroll clamping prevents an offset from remaining beyond a shrunk list. The missing DSP reload for the deleted active row is reported above.
- `NeuralAmpModeler/VoLumAmpListScroll.h` — read in full. Scrollability, thumb geometry, and clamping handle empty and shrinking content without division by zero.
- `NeuralAmpModeler/VoLumPrePedalCaptures.h` — read in full. Discovery is non-recursive, deterministic, and bounded by one directory; factory capture indices and the custom 64..127 range do not overlap.
- Production call sites were traced through `VoLumCustomOverlay.h`, `VoLumLayoutBuild.inc.cpp`, `VoLumSceneRig.inc.cpp`, `VoLumLoader.inc.cpp`, `VoLumPaths.h`, `VoLumAmpMenus.inc.cpp`, `VoLumSettingsScene.inc.cpp`, and `NeuralAmpModeler.cpp`. `VoLumRigs` and development `rigs` names are both searched on Windows and macOS. Missing/partial factory folders produce empty channel lists and no new load rather than an out-of-bounds access, although the currently loaded model may remain until another valid load.
- The requested dedicated Cursor transaction rule was searched for under `.cursor/rules/`; no rule names or contents reference `VoLumCustomNamImport.h`. The applicable `neural-amp-modeler-native.mdc` rule contains general DSP/staging and verification guidance but no additional content-import invariants.
