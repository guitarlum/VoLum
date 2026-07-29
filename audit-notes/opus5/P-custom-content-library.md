# Custom Content Library — 1.2.1 Bug Audit

Scope read in full: `VoLumContentStore.h`, `VoLumCustomModel.h`, `VoLumCustomContentApi.h`, `VoLumCustomNamImport.h`, `VoLumPaths.h`, `VoLumIrFileGuard.h`, `VoLumPrePedalCaptures.h`, plus every custom-content path in `VoLumSceneRig.inc.cpp`, `VoLumCustomOverlay.h`, `VoLumLayoutBuild.inc.cpp`, `VoLumLoader.inc.cpp`, `VoLumSettingsScene/Locks.inc.cpp`, `NeuralAmpModeler.cpp`, and the five named test files.

---

## 1. BLOCKER — The IR trim migration reads WAVs through the ANSI code page, permanently mis-calibrating every IR on a non-ASCII content path

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:798-812`

**MECHANISM**

```cpp
const auto abs = volum::content::GlobalContentStore().ResolveStored(ir.file);
const std::string absUtf8 = volum::content::PathToUtf8(abs);      // UTF-8
...
if (!abs.empty()
    && dsp::wav::Load(absUtf8.c_str(), audio, fileSr) == dsp::wav::LoadReturnCode::SUCCESS && !audio.empty())
{ ...compute L2... }
ir.trimDb = trimDb;          // 0.0 when the load failed
ir.trimCalibrated = true;    // and we never retry
```

`dsp::wav::Load` is `AudioDSPTools/dsp/wav.cpp:362`, which does `std::ifstream wavFile(fileName, std::ios::binary)` — on Windows that interprets the bytes in the active ANSI code page. `_StageIR` gets this right (`NeuralAmpModeler.cpp:2057-2058` converts `u8path(...).string()` before handing the loader a narrow string); the migration does not. It is the mirror image of the pinned bug class: a UTF-8 `std::string` crossing into an ANSI-consuming API.

**TRIGGER** Any user whose `%LOCALAPPDATA%` contains a character outside the active code page (non-Latin username, redirected profile), or who imported an IR with a non-ASCII leaf name — `ImportFileCopy` deliberately preserves the Unicode leaf (`VoLumContentStore.h:790-792`, pinned by `test_volum_content_crud_edge.cpp:53`). First launch of 1.2.1 runs `_VolumMigrateIrTrims()` from the constructor (`NeuralAmpModeler.cpp:503`).

**IMPACT** `trimDb` stays 0.0 and `trimCalibrated = true` is persisted, so the headline 1.2.1 fix — custom IRs landing ~18 dB below the baked cabs — silently never applies, and never will, because the entry is now marked done. The log even claims success: `"IR 'x' auto-normalized to 0.000000 dB"`. The `.pre-1.2.1.bak` is written, so the user's only recovery is hand-editing JSON. IRs still load fine afterwards (`_StageIR` converts correctly), just 18 dB too quiet.

**CONFIDENCE** certain (mechanism); the trigger population is the same one the 1.2.1 Unicode work was done for.

**FIX SKETCH** One line, mirroring `_StageIR`: `dsp::wav::Load(abs.string().c_str(), ...)` (`abs` is already a `path`, so `.string()` is the native narrow the loader wants). Additionally, do not set `trimCalibrated = true` when the load failed — leave it uncalibrated so a later launch retries. No voicing change on working paths; on broken paths it *restores* the intended 1.2.1 level, which is the point of the release.

**TEST GAP** None of the five test files touch `_VolumMigrateIrTrims` — it is a plugin member with no harness. `test_volum_ir_shaping.cpp` covers only the pure `AutoNormalizeIrTrimDb` / ladder / clamp helpers, never the file-reading caller. `test_volum_upgrade_migration.cpp` stops at "a v2 IR loads uncalibrated" and never runs the calibration.

---

## 2. BLOCKER — Two processes clobber each other's library: whole-file last-writer-wins with no reload before write

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:749-756` and `917-928`; entry points `NeuralAmpModeler.cpp:499-500`, `VoLumSettingsScene.inc.cpp:381`, `NeuralAmpModeler.cpp:1031-1032`, `NeuralAmpModeler.cpp:552-553`

**MECHANISM** `GlobalContentStore()` is a per-process singleton loaded once at construction. `Save()` serializes the *entire* in-memory registry and replaces the file:

```cpp
bool Save()
{
  if (mBase.empty()) return true;
  std::filesystem::create_directories(mBase, ec);
  return WriteJsonAtomically(RegistryPath(), RegistryToJson(mReg), ec);
}
```

`WriteJsonAtomically` is atomic per write (temp + `MoveFileExW`), but there is no mtime check, no re-read-merge, and no cross-process lock. `Load()` is only ever called from the constructor.

**TRIGGER** Standalone VoLum open (loaded the registry at 09:00) while the user works in a DAW. In the DAW they import two IRs and a custom amp — the DAW process `Save()`s them. Then they close the standalone app: `OnUIClose` → `_VolumSaveSettingsToFile()` → `GlobalContentStore().Save()` writes its 09:00 snapshot over the file.

**IMPACT** Silent loss of the entire delta: the imported amps/IRs/pedals vanish from the library, their preset banks and scenes go with them, and the copied `.nam`/`.wav` files are orphaned on disk with no registry entry. Any DAW project referencing those ids now restores with dead references (see #6). Nothing is reported.

**CONFIDENCE** certain on mechanism; likelihood is "whenever the standalone and a plugin instance overlap", which is the normal tone-building workflow.

**FIX SKETCH** Smallest safe change for 1.2.1: in `Save()`, stat the registry's `last_write_time` at `Load()` and again before writing; if it moved, `Load()` into a scratch `Registry`, merge by id (union the `amps`/`irs`/`pedals` vectors and the `presetBanks`/`customScenes` maps, local wins on key conflict, take `max(nextPedalIndex)`), then write. Cheap and no format change. A proper single-writer lock file is 1.3 work. No audio impact.

**TEST GAP** No test opens two `ContentStore`s over one directory. `test_volum_content_store.cpp:167` ("Save/Load round-trips on disk") and `:675` (combined BYO round-trip) both use a single store, sequentially.

---

## 3. BLOCKER — Deleting the focused custom amp leaves its model playing and then overwrites the factory amp's scene

**WHERE** `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:163-201`

**MECHANISM** The delete handler fixes up the indices and the chrome but never re-stages the rig:

```cpp
auto doDelete = [this, customIdx]() {
  volum::custom::RemoveCustomAmp(customIdx);      // manifest + copied .nam files deleted
  ...list->SetCustomSelected(-1);  hero->SetName(factory name);
  if (mVolumCustomMainIdx == customIdx) mVolumCustomMainIdx = -1;
  else if (mVolumCustomMainIdx > customIdx) --mVolumCustomMainIdx;
  ... _VolumSyncPresetOwner(); _VolumRefreshPresetBar();
};
```

Compare the sidebar's factory-select callback 80 lines above (`:80-87`), which is the correct sequence: `_VolumSaveCurrentToSettings(); mVolumAmpIdx = …; mVolumCustomMainIdx = -1; _VolumRestoreFromSettings(); _VolumRefreshChannels(); mVolumNeedsLoad.store(true);`. The delete path does none of the last three.

**TRIGGER** Focus a custom amp, tweak knobs, click its bin icon in the sidebar, confirm.

**IMPACT** Three distinct symptoms:
- The deleted amp's `.nam` stays live in `mModel` — the user keeps hearing the amp they just deleted, while the sidebar/hero show a factory amp. Its files are gone from disk, so it cannot even be reloaded.
- `mVolumSpeakerIdx` / `mVolumChannelIdx` still hold the custom amp's slot/channel, so the cabinet row and channel stepper keep showing custom cab names against a factory amp.
- `_VolumActiveScene()` now resolves to `mVolumAmpSettings[mVolumAmpIdx]`, so the next `_VolumSaveCurrentToSettings()` (idle flush, amp switch, `OnUIClose`) writes the deleted amp's knob values into the factory amp's persisted scene. Silent corruption of unrelated saved state.

**CONFIDENCE** certain.

**FIX SKETCH** Inside `doDelete`, after the index fixups, when the deleted amp was the focused main: `_VolumRestoreFromSettings(mVolumAmpIdx); _VolumRefreshChannels(); mVolumNeedsLoad.store(true);` — i.e. reuse the factory-select sequence. Do **not** add a `_VolumSaveCurrentToSettings()` first: the live params belong to the amp being destroyed.

**TEST GAP** `test_volum_custom_content.cpp:835` ("RemoveCustomAmp keeps names and art ids aligned") only exercises the bridge's list/art alignment. The plugin-level focus fallback has no coverage — the same class of gap the changelog already flags as "needs plugin-level test infrastructure that does not exist yet".

---

## 4. MAJOR — Pedal-pool-exhausted import is a silent no-op: the error message is cleared before it can be shown, and the copied file is orphaned

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:609-638`

**MECHANISM**

```cpp
const std::string idp = volum::content::MintId(store.reg(), "pedal");
std::string rel = store.ImportFileCopy(volum::content::PathFromUtf8(fn.Get()), "pedals", idp);
if (rel.empty()) rel = leaf;
if (volum::custom::AddPedal(base, rel) < 0)
{
  mError = "Custom pedal slots are full - delete a pedal first.";
  continue;
}
...
if (!tooLarge.empty())        mError = ...;
else if (!skipped.empty())    mError = ...;
else                          mError.clear();     // <-- wipes the pool-full message
```

The pool-full branch is the only one that sets `mError` *inside* the loop, and the epilogue unconditionally overwrites or clears it. With no oversized and no duplicate-named files in the batch — the normal case — `mError.clear()` runs and the message never reaches the user.

**TRIGGER** Import a pedal once the 64-slot pool (`kCustomPedalIndexBase`..`kCustomPedalIndexMax`) is used up. Reachable directly via #5.

**IMPACT** The Manage-pedals panel does nothing at all: no new row, no error, `added == 0` so no selection change. The user re-picks the file and gets the same nothing. Meanwhile `ImportFileCopy` already succeeded, so a full copy of the `.nam` sits in `content/pedals/` forever with no registry entry — repeatable disk leak, several MB per attempt. This is exactly the failure the 1.2.0 phase-5 changelog claims was fixed ("the Manage-pedals importer surfaces 'Custom pedal slots are full'"); the surfacing is defeated by the epilogue.

**CONFIDENCE** certain.

**FIX SKETCH** Collect pool-full names into a third `std::vector<std::string> poolFull` and add a branch ahead of the `else mError.clear()`; and move the `ImportFileCopy` call *after* the `AddPedal` capacity check (or `store.RemoveStoredFile(rel)` on the `< 0` branch) so a refused import copies nothing.

**TEST GAP** `test_volum_custom_content.cpp:632-640` pins the *bridge* boundary (`AddPedal(...) == -1`, nothing appended) but not the overlay's error surfacing or the copied-file cleanup. `StartImport` has no test — it needs an `IGraphics` file picker.

---

## 5. MAJOR — The PRE-capture index pool is never reclaimed: 64 lifetime imports and custom pedals are permanently unavailable

**WHERE** `NeuralAmpModeler/VoLumCustomContentApi.h:400-415`, `VoLumContentStore.h:388` and `:614-618`, `RemovePedal` at `VoLumContentStore.h:813-839`

**MECHANISM** `nextPedalIndex` is strictly monotonic and only ever ratchets up:

```cpp
r.nextPedalIndex = std::max(r.nextPedalIndex, item.legacyIndex + 1);   // on load
...
if (reg.nextPedalIndex > content::kCustomPedalIndexMax) return -1;      // AddPedal
it.legacyIndex = reg.nextPedalIndex;
reg.nextPedalIndex = it.legacyIndex + 1;
```

`RemovePedal` deletes the entry and its file and clears referencing PRE slots, but never lowers `nextPedalIndex` and never records the freed index. The counter is persisted, so a reload re-reads 128 and `std::max(kCustomPedalIndexBase, 128)` keeps it there.

**TRIGGER** Import 64 pedals cumulatively over the product's life — importing and deleting counts. Import #65 is refused even with an empty pedal library.

**IMPACT** A permanent dead end: no custom PRE pedal can ever be imported again on that machine, and per #4 the refusal is invisible. The advice the (suppressed) message gives — "delete a pedal first" — cannot help, because deletion does not free a slot.

**CONFIDENCE** certain on mechanism; low-to-moderate likelihood (needs 64 cumulative imports, plausible for a user auditioning Tone3000 captures).

**FIX SKETCH** In `AddPedal`, when the monotonic counter is exhausted, fall back to the lowest unused index in `[kCustomPedalIndexBase, kCustomPedalIndexMax]` by scanning `reg.pedals`; only return `-1` when all 64 are genuinely occupied. Index reuse is safe once no live entry holds the index, because `RemovePedal` already scrubbed every scene/preset reference — with the caveat in #10 below, which should be fixed alongside it. Do not lower `nextPedalIndex` itself; keep the monotonic fast path so existing chunks are unaffected.

**TEST GAP** `test_volum_custom_content.cpp:603-640` covers the boundary in the "never deleted" direction only (last slot succeeds, overflow rejected). No test deletes a pedal and re-imports.

---

## 6. MAJOR — An IR listed in the library but missing from disk restores as a cab-less raw amp, silently

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:526-550`, reached from `:727` (`_VolumApplyActiveIr` → `_VolumSelectIR(idx, support, /*interactive=*/false)`)

**MECHANISM** The id resolves (the registry entry exists), so the orphan-clearing planner path never fires — `MakeUiSyncPlan` only sets `clearOrphanedIr` when `irIdPresent && !irResolved` (`VoLumUiSyncPlan.h:133`, `:167`). Only the *file* is gone, and `_StageIR` bails out:

```cpp
const dsp::wav::LoadReturnCode loadRc = _StageIR(p, support);
if (loadRc != dsp::wav::LoadReturnCode::SUCCESS)
{
  if (auto* pGfx = GetUI())    // interactive path only
  { ...message box... }
  return;                      // scene keeps activeIrId; no cab recovery
}
```

The restore path (`interactive == false`) reaches the same `return` with `pGfx` non-null but no message shown, because the message box is gated on the interactive caller having opened it — and more importantly nothing recovers the cab. The scene was saved with `speakerIdx == 0` (the IR had forced DIRECT via `_VolumForceDirectCapture`), and `_VolumApplyAmpSettings:199` restores that verbatim.

**TRIGGER** Delete or move the `.wav` under `content/ir/` (an antivirus quarantine, a half-restored backup, a machine transfer that copied `volum-content.json` but not the `ir/` folder, or the orphaning in #2/#4), then reopen the DAW project or relaunch the standalone.

**IMPACT** The lane plays the raw DIRECT capture with no cabinet and no IR — very bright and thin, the loudest possible "something is broken" symptom — with no dialog, no footer error, and no recovery. Only `volum.log` records `[ir] load FAILED`. It recurs on every load.

**CONFIDENCE** certain.

**FIX SKETCH** In `_VolumSelectIR`, on `loadRc != SUCCESS`, call `_VolumFallbackToAvailableCab()` (support lane: the `_VolumReconcileActiveIr` support branch) so a real baked cab takes over, and clear the lane's `activeIrId`. Voicing note: this only changes the *broken* configuration — from "raw cab-less amp" to "a real cab". Working IR loads are untouched, so nothing frozen moves.

**TEST GAP** `test_volum_content_store.cpp:789` ("Missing copied capture is detectable via ResolveStored") checks only that the store *can* detect a missing file; nothing exercises the IR restore path with a present entry and absent file. `test_volum_ui_sync_plan.cpp:138-168` covers the dead-*id* case, which is a different branch.

---

## 7. MAJOR — Two instances in one host share a single live custom-amp scene

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:386` (`std::map<std::string, VoLumAmpSettings> customScenes`), `VoLumSceneRig.inc.cpp:476-485`, `VoLumSettingsLocks.inc.cpp:88-95`

**MECHANISM** `_VolumActiveScene()` returns a reference *into the process-global registry* whenever a custom amp is focused:

```cpp
volum::VoLumAmpSettings& NeuralAmpModeler::_VolumActiveScene()
{
  if (mVolumCustomMainIdx >= 0)
  {
    const std::string id = volum::custom::CustomAmpIdAt(mVolumCustomMainIdx);
    if (!id.empty())
      return volum::content::GlobalContentStore().reg().customScenes[id];
  }
  return mVolumAmpSettings[mVolumAmpIdx];
}
```

`_VolumSaveCurrentToSettings` writes the live params into that same shared struct (`VoLumSettingsLocks.inc.cpp:93`). This directly contradicts the store's own header contract (`VoLumContentStore.h:917-920`: *"Per-instance/project state (active amp, live scene) lives in the plugin instance + DAW chunk, not here"*) — the DAW chunk carries only `customMainId` in the id tail (`NeuralAmpModeler.cpp:1057`), never the scene.

**TRIGGER** Two DAW tracks both focused on custom amp X, tuned differently. Save the project, or just let both instances hit `SerializeState`/`OnUIClose`.

**IMPACT** Whichever instance serializes last writes its knob values into `customScenes[X]`; on project reload both instances read that one scene, so one track comes back with the other's tone. Also: constructing a *third* instance calls `Load()` (`NeuralAmpModeler.cpp:500`), which does `mReg = Registry{}` and discards any in-memory scene edits the existing instances have not yet flushed.

**CONFIDENCE** certain on mechanism. Already acknowledged as known-unfixed in the 1.2.0 phase-5 note ("the `_VolumActiveScene()` read/write split needs plugin-level test infrastructure that does not exist yet") — it is still shipping.

**FIX SKETCH** Too large for 1.2.1: the real fix is a per-instance scene cache keyed by amp id, seeded from the registry on focus and serialized into the DAW chunk. For this release, the honest options are (a) document it as a known limitation, or (b) the minimal mitigation — have `Load()` not clobber a registry that a live instance has already touched (skip the reload when `mBase` is unchanged and the file mtime has not moved), which at least removes the third-instance data loss.

---

## 8. MAJOR — Registry write failures are swallowed by most mutators, so the UI reports success for content that will not exist next launch

**WHERE** `NeuralAmpModeler/VoLumCustomContentApi.h:278` (`AddIR`), `:288` (`RenameIR`), `:298` (`DeleteIR`), `:334` (`SetIRShaping`), `:413` (`AddPedal`), `:219` (`RemoveCustomAmp`), `:507`/`:521`/`:554`/`:570` (preset ops)

**MECHANISM** `AddCustomAmp` (`:163-167`) and `UpdateCustomAmp` (`:196-200`) correctly roll back and return `-1` when `Save()` fails. No other mutator checks:

```cpp
inline int AddIR(const std::string& name, const std::string& file = "")
{
  ...
  reg.irs.push_back(std::move(it));
  Store().Save();                       // return value discarded
  return (int)reg.irs.size() - 1;
}
```

**TRIGGER** Read-only or full `%LOCALAPPDATA%\VoLum\content`, a locked `volum-content.json` (antivirus, backup agent, OneDrive sync), or the invalid content dir from #17.

**IMPACT** Import an IR: the row appears, the file is copied, the entry is selectable and audible for the session — and it is gone on relaunch, leaving the copied `.wav` orphaned. Same for a renamed IR, a deleted pedal (which reappears), and every IR shaping edit made in the 1.2.1 popover. Nothing is reported at any point.

**CONFIDENCE** certain.

**FIX SKETCH** Make the mutators mirror `AddCustomAmp`: on `!Save()`, undo the in-memory change and return a failure the caller surfaces in `mError`. `SetIRShaping` and `RenameIR` need a return value they currently do not have; the smallest 1.2.1-safe subset is `AddIR` / `AddPedal` / `SetIRShaping`, since those are the ones that lose user work rather than merely failing to forget something.

**TEST GAP** Every store test uses a writable temp dir. `test_volum_content_crud_edge.cpp` covers a missing source and an unset base dir, never a write-protected base.

---

## 9. MAJOR — A failed copy silently produces a library entry that can never load

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:596-620`

**MECHANISM**

```cpp
const std::string leaf = LeafName(fn.Get());
...
std::string rel = store.ImportFileCopy(volum::content::PathFromUtf8(fn.Get()), "ir", idp);
if (rel.empty())
  rel = leaf;              // bare filename, resolves to <base>/<leaf> — which does not exist
volum::custom::AddIR(base, rel);
```

The fallback exists for unit tests, where the base dir is empty (comment at `:597-599`). In production the base dir is always set, so `ImportFileCopy` returns `""` only on a genuine failure: disk full, permission denied, or a stored name over `MAX_PATH` (`idPrefix + "__" + leaf`, ~32 characters of prefix on top of the leaf — `VoLumContentStore.h:790-796`).

**TRIGGER** Import an IR or pedal with a long leaf name from a deep source path, or onto a full/read-only content dir.

**IMPACT** The Manage list shows a new IR and `added > 0` selects it, so the import looks successful. Selecting it later fails: `IrFileSizeAcceptable` passes (it returns `true` when the size cannot be determined — `VoLumIrFileGuard.h:51-52`), then `_StageIR` fails and the user gets a generic "VoLum could not load this impulse response". The real cause — the copy never happened — is never stated. Note this contrasts sharply with the amp path, which rolls the whole transaction back on exactly this failure (`VoLumCustomNamImport.h:59-60`).

**CONFIDENCE** certain.

**FIX SKETCH** Gate the fallback on the base dir actually being empty: `if (rel.empty()) { if (!store.BaseDir().empty()) { copyFailed.push_back(base); continue; } rel = leaf; }`, and report `copyFailed` in the epilogue alongside `tooLarge`/`skipped`.

---

## 10. MAJOR — Deleting a pedal that is loaded in a live PRE slot keeps it processing audio; factory-amp scenes keep the dead index forever

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:813-839`; delete entry point `VoLumCustomOverlay.h:509-518`, `:784-802`

**MECHANISM** `RemovePedal` clears the freed `legacyIndex` out of `customScenes` and `presetBanks`:

```cpp
for (auto& sc : mReg.customScenes)      clearSlots(sc.second);
for (auto& bank : mReg.presetBanks)
  for (auto& pr : bank.second)          clearSlots(pr.settings);
```

Three reference holders are missed: (a) the **live params** `kPreNam1Capture` / `kPreNam2Capture`; (b) the per-instance **factory** scenes `mVolumAmpSettings[]`, which live in the plugin, not the registry; (c) the PRE/POST lock snapshots `mVolumLiveLockedPre`. And the delete path's `NotifyChanged()` callback (`VoLumLayoutBuild.inc.cpp:1247-1255`) handles IRs only — it never sets `mVolumPreNeedsLoad[slot]`.

**TRIGGER** Load a custom pedal into PRE NAM 1, open Manage custom pedals, delete it.

**IMPACT** Immediately: the deleted pedal's NAM keeps processing the signal, while the pill label goes to "Click to change" (`_VolumGetPreCaptureLabel` → `PedalNameByLegacy` returns `""` — `VoLumSceneRig.inc.cpp:132-136`). The user has no way to understand what they are hearing. On relaunch the slot resolves to an empty path and `_VolumRequestPreNamLoad` drops the model (`VoLumLoader.inc.cpp:403-408`), but the slot stays *engaged* with a blank label — a PRE pedal that looks on and does nothing. Every factory amp whose scene referenced the pedal keeps index 64+ indefinitely.

**CONFIDENCE** certain.

**FIX SKETCH** In the pedals branch of the Manage `NotifyChanged` callback, clear any live `kPreNamNCapture` that no longer resolves via `PedalNameByLegacy`, scrub the same index out of `mVolumAmpSettings[]` and `mVolumLiveLockedPre`, and set `mVolumPreNeedsLoad[slot]` so the audio thread drops the model on the next block. This is also a prerequisite for the index reuse in #5.

**TEST GAP** `test_volum_content_store.cpp:350` ("deleting a pedal clears referencing PRE slots") covers exactly the two containers the store owns and cannot see the plugin-side holders — a good example of a test that reports coverage the product does not have.

---

## 11. MINOR — Editing a custom amp orphans the captures whose rows were removed

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:893-900`; commit at `VoLumCustomContentApi.h:173-202`

**MECHANISM** The builder's remove-file action erases the manifest row and nothing else:

```cpp
if (i >= 0 && i < (int)mBuilderAmp.files.size())
  mBuilderAmp.files.erase(mBuilderAmp.files.begin() + i);
```

On Save, `PrepareCustomNamImport` only copies rows with a non-empty `sourcePath`, and `UpdateCustomAmp` replaces the manifest wholesale. Nothing diffs the outgoing manifest against the incoming one, so the removed row's `storedPath` file is never deleted. `RemoveCustomAmp` deletes copied files correctly (pinned by `test_volum_content_store.cpp:415`), so this is specifically the *edit* path.

**IMPACT** Unbounded disk growth in `content/amps/` — a NAM capture is typically 1-10 MB, and re-mapping a multi-file rig easily drops several rows. Invisible to the user; no functional breakage.

**CONFIDENCE** certain.

**FIX SKETCH** In `UpdateCustomAmp`, after a successful `Save()`, remove every `storedPath` present in `previous.files` but absent from the committed manifest. `previous` is already captured at `:194` for the rollback.

---

## 12. MINOR — The migration logs and reports success even when the backup or the write failed

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:815-825`

**MECHANISM**

```cpp
const bool backedUp = volum::content::GlobalContentStore().BackupBeforeMigration("1.2.1");
volum::content::GlobalContentStore().Save();          // return discarded
VOLUM_LOG("migrate", std::string("IR trim migration saved; pre-migration backup ") + ...);
```

The log claims "saved" unconditionally, and the migration proceeds to rewrite the user's 1.2.0 library even when `BackupBeforeMigration` returned `false` (copy failed — the snapshot is the only copy the user can downgrade to). The interrupted and run-twice cases are handled correctly: the in-memory mutation happens before the backup, so the snapshot is genuinely pre-upgrade, and `BackupBeforeMigration` refuses to overwrite an existing snapshot (`VoLumContentStore.h:772-773`, pinned at `test_volum_upgrade_migration.cpp:186`).

**FIX SKETCH** Check `Save()`'s return and log the failure; skip the rewrite when `!backedUp` and the registry file exists, deferring to the next launch.

---

## 13. MINOR — `dsp` cache key mixes native-narrow and UTF-8 encodings

**WHERE** `NeuralAmpModeler/VoLumLoader.inc.cpp:331`

**MECHANISM** `const std::string prefetchPath = fs::weakly_canonical(entry.path(), pathEc).string();` produces an ANSI-narrow string on Windows, compared against `request.fileToLoad` (UTF-8) and later fed to `fs::u8path(...)` at `:348`. Factory rig prefetch only (`ampIdx >= 0`), so custom content never reaches it, and shipped rig names are ASCII — but a repo/install path with non-ASCII characters makes the comparison at `:332` never match and the `u8path` reconstruction at `:348` target a bad path.

**IMPACT** Prefetch misses; a channel switch that should hit the cache re-parses the model. Performance only.

**FIX SKETCH** `volum::content::PathToUtf8(fs::weakly_canonical(entry.path(), pathEc))`, matching every other call site in the file.

---

## 14. MINOR — Auto-normalize measures the whole file, not the samples the convolver actually uses

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:805-808`

**MECHANISM** `sumSq` accumulates over every sample in the file at the file's own sample rate, but `dsp::ImpulseResponse` truncates to the first ~8192 samples and resamples to the host rate (`VoLumIrFileGuard.h:5-9`). A 96 kHz capture therefore measures ~√2 higher than the same IR at 48 kHz (~1.5 dB of trim error), and a long file that squeaks under the 64 MB guard measures far too high — clamping trim to −24 dB and making the IR much quieter than intended. `l2Norm == 0` is correctly guarded (`AutoNormalizeIrTrimDb` returns 0.0 for a silent WAV — no `log10(0)` → `-inf`, pinned at `test_volum_ir_shaping.cpp:34`).

**FIX SKETCH** Bound the sum to `min(audio.size(), 8192 * hostRate/fileRate)`. **Voicing is frozen for 1.2.1 and this changes the level of every auto-normalized IR — defer to 1.3.**

---

## 15-19. NITs

- **`fs::path(getenv("LOCALAPPDATA"))`** — `VoLumPaths.h:205`, `:232`, `:251`, `:271`. `getenv` returns the ACP transcoding of the wide environment, so characters outside the active code page arrive as `?`; `create_directories` then fails on the illegal character and the whole content library silently no-ops (compounding #8). The narrow→path conversion is self-consistent, so the regression pins do not catch it. Fix: `_wgetenv(L"LOCALAPPDATA")` or `SHGetKnownFolderPath`.
- **`AddCustomAmp`/`UpdateCustomAmp` dedupe amp names case-sensitively** (`VoLumCustomContentApi.h:150-155`, `:184-189`, `e.name == n`) while IRs, pedals and presets use `NameMatchesCI`. "Plexi" and "plexi" can coexist as two sidebar rows.
- **`MintRawId` emits only 32 bits** of the 64 drawn (`VoLumContentStore.h:399-405`, 8 nibbles). `MintId` re-rolls against `IdInUse`, so registry ids are safe; the *file-prefix* ids minted at `VoLumCustomOverlay.h:603`/`:611` are never registered, so a collision plus an identical leaf name would let `copy_options::overwrite_existing` replace a working capture. ~2⁻³² per pair.
- **`IdInUse` ignores `customScenes` keys and preset-bank owner keys** (`VoLumContentStore.h:408-424`), so a newly minted amp id could adopt an orphaned scene imported from another machine.
- **IR import performs no format validation** (`VoLumCustomOverlay.h:585-607`) — only a size check — whereas the amp transaction validates every capture with the production NAM parser. A non-WAV lands in the library and only fails at select time.

---

## Not bugs (checked and clean)

`PathFromUtf8`/`PathToUtf8` are correct under both `char8_t` and the pre-C++20 fallback. `PrepareCustomNamImport`'s rollback deletes exactly the files it created and nothing else, and uses a fresh transaction prefix so `overwrite_existing` cannot clobber a working capture mid-validation. The `Remove*` methods take `id` by value, avoiding the use-after-free the ASan test at `test_volum_content_store.cpp:444` pins. `RegistryFromJson` is genuinely lenient. `BackupBeforeMigration` is idempotent and kept distinct from the corrupt-file `.bak`. `AddPedal`'s pool boundary is off-by-one-correct (127 is allocatable). `ClampPreCaptureIndex`'s factory-count clamp is correctly bypassed for custom indices at `VoLumSceneRig.inc.cpp:193-195` and `ClampPreCaptureSlots` defaults to the full 0-127 param range.