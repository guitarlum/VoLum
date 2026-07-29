I've finished the audit. Here are the findings, ranked.

---

## 1. BLOCKER — Newly imported IRs are never auto-normalized, and the write locks them out of the migration forever

**WHERE** `NeuralAmpModeler/VoLumCustomOverlay.h:601-608`, `NeuralAmpModeler/VoLumCustomContentApi.h:270-280`, `NeuralAmpModeler/VoLumSceneRig.inc.cpp:789-826`

**MECHANISM** The 1.2.1 changelog states "On import each IR is auto-normalized once — trimDb = 18 - 20*log10(L2(h))". No import path does this. The Manage-Custom-IR import is:

```601:608:NeuralAmpModeler/VoLumCustomOverlay.h
      if (mManageKind == ManageKind::IR)
      {
        const std::string idp = volum::content::MintId(store.reg(), "ir");
        std::string rel = store.ImportFileCopy(volum::content::PathFromUtf8(fn.Get()), "ir", idp);
        if (rel.empty())
          rel = leaf;
        volum::custom::AddIR(base, rel);
      }
```

`AddIR` creates the entry at struct defaults (`trimDb = 0.0`, `trimCalibrated = false`) and immediately persists it:

```272:279:NeuralAmpModeler/VoLumCustomContentApi.h
  auto& reg = Store().reg();
  content::IRItem it;
  it.id = content::MintId(reg, "ir");
  it.name = name.empty() ? "Imported IR" : name;
  it.file = file;
  reg.irs.push_back(std::move(it));
  Store().Save();
```

`AutoNormalizeIrTrimDb` has exactly one caller in the whole product — `_VolumMigrateIrTrims`, which only runs in the constructor and only for entries where `trimCalibrated == false` (`VoLumSceneRig.inc.cpp:795`). The kill shot is that `RegistryToJson` writes `trimDb` unconditionally for every IR (`VoLumContentStore.h:512-517`), and `RegistryFromJson` treats the mere *presence* of that key as proof of calibration:

```584:588:NeuralAmpModeler/VoLumContentStore.h
      if (ir.contains("trimDb") && ir["trimDb"].is_number())
      {
        item.trimDb = ClampIrTrimDb(ir["trimDb"].get<double>());
        item.trimCalibrated = true;
      }
```

So `AddIR`'s `Save()` writes `"trimDb": 0.0`, and on the next launch the entry loads as calibrated-at-unity. The migration will never touch it again.

**TRIGGER** Import any .wav via Manage Custom IR in 1.2.1, then use it.

**IMPACT** The exact complaint 1.2.1 is built to fix. IRs imported *by 1.2.1* still land ~18 dB below the stock cabs, permanently, and the auto-normalize path can never reach them. Only IRs inherited from 1.2.0 or earlier get the fix. The user's only recourse is to open the gear popover and dial ~18 dB by hand on every import.

**CONFIDENCE** certain

**FIX SKETCH** In the IR branch of the import loop, after `AddIR` returns an index, load the copied .wav, compute the L2 norm, and call `SetIRShaping(idx, AutoNormalizeIrTrimDb(l2), 0, 0)`. Cleanest is to factor the load-and-measure loop body out of `_VolumMigrateIrTrims` into a free helper both call. No schema or chunk change — `trimDb` is already written and read.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_ir_shaping.cpp:34-49` tests `AutoNormalizeIrTrimDb` as a pure function and lines 178-224 test the `trimCalibrated` JSON round-trip. Nothing asserts that the *import* path produces a non-zero trim, so the feature's headline behavior is entirely uncovered.

---

## 2. BLOCKER — A DAW project focused on a custom amp loads the wrong capture (or none) when the editor is closed

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:448-454` and `306-330`, consumed at `NeuralAmpModeler/NeuralAmpModeler.cpp:816-834`

**MECHANISM** The main-lane .nam is resolved in `OnIdle` purely from two runtime caches:

```820:821:NeuralAmpModeler/NeuralAmpModeler.cpp
      const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
      const std::string rel = volum::content::CaptureFileFor(amp, mVolumCustomMainSlot, mVolumCustomMainChannel);
```

Those two members are written in only three places, and the restore path's one is gated on the editor existing:

```448:454:NeuralAmpModeler/VoLumSceneRig.inc.cpp
void NeuralAmpModeler::_VolumApplyCustomMainCabs(int customIdx, bool supportLane)
{
  if (GetUI() == nullptr)
    return;
  const auto amp = volum::custom::CustomAmpAt(customIdx);
  _VolumApplyUiSyncPlan(volum::MakeUiSyncPlan(_VolumMakeUiSyncInput(supportLane, amp)), supportLane);
}
```

`_VolumApplyUiSyncPlan` is itself double-gated (`if (!pGfx) return;`). During `UnserializeState`, `_VolumSelectCustomAmp` takes the headless branch and calls into that no-op:

```325:330:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  auto* pGfx = GetUI();
  if (!pGfx)
  {
    _VolumApplyCustomMainCabs(customIdx);
    return;
  }
```

`_VolumApplyAmpSettings` (line 323) restores `mVolumSpeakerIdx` / `mVolumChannelIdx` from the persisted scene, but nothing translates those into `mVolumCustomMainSlot` / `mVolumCustomMainChannel`, which stay at their constructor defaults (`kDirectSlot`, `1` — `NeuralAmpModeler.h:548`). `IPlugVST3` calls `CreateTimer()` in its constructor, so `OnIdle` pumps headless and the load actually fires with the stale pair.

**TRIGGER** Save a VST3/AU project focused on a custom MAIN amp on any cab other than DIRECT channel 1 (e.g. CB2, gain stage 5). Reopen the project, or render it, without opening the plugin editor.

**IMPACT** Either the wrong capture plays — a raw DI/no-cab capture at gain stage 1 instead of the saved cab and stage, which is a drastic tone change at full level — or, if the amp has no DIRECT capture on channel 1, `rel` is empty, no model loads at all, and the track is silent with a "LOAD FAILED - custom capture path is missing" footer nobody sees. An offline mixdown of a closed project renders wrong. Opening the editor silently repairs it, which makes this maddening to report and easy to dismiss.

**CONFIDENCE** likely (certain in code; I could not run a host to confirm the audible result)

**FIX SKETCH** Move the (slot, channel) derivation out of the UI gate. `MakeUiSyncPlan` is already pure and already computes `plan.customSlot` / `plan.customChannel` from `_VolumMakeUiSyncInput`, which reads no UI state for the main lane — so drop the `GetUI() == nullptr` early-out in `_VolumApplyCustomMainCabs`, build the plan unconditionally, apply the routing-cache half of `_VolumApplyUiSyncPlan` always, and keep only the control writes behind the `pGfx` check. No chunk or schema change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_ui_regressions.cpp:1018` pins the sibling 1.2.1 fix (`_VolumForceDirectCapture` must derive the stage from the persisted position, not the cache) by grepping the source text. It pins the one call site that was fixed, not the invariant "the routing caches are valid after a headless restore," so the identical bug one level up survived.

---

## 3. BLOCKER — Every instance constructor wipes the shared content registry, discarding other instances' unflushed scene edits

**WHERE** `NeuralAmpModeler/NeuralAmpModeler.cpp:497-503`, `NeuralAmpModeler/VoLumContentStore.h:711-747`

**MECHANISM** `GlobalContentStore()` is a process-wide singleton (`VoLumContentStore.h:924`), and `Load()` clears it before doing anything else:

```711:717:NeuralAmpModeler/VoLumContentStore.h
  bool Load()
  {
    mReg = Registry{};
    const auto path = RegistryPath();
    std::error_code ec;
    if (mBase.empty() || !std::filesystem::exists(path, ec))
      return true;
```

Yet every instance calls it unconditionally at construction:

```499:503:NeuralAmpModeler/NeuralAmpModeler.cpp
        volum::content::GlobalContentStore().SetBaseDir(contentDir);
        volum::content::GlobalContentStore().Load();
        // One-time: auto-normalize the trim of any pre-1.2.1 IR (no stored trim)
        // so previously-imported custom IRs stop landing ~18 dB below stock cabs.
        _VolumMigrateIrTrims();
```

A focused custom amp's live scene lives in that registry (`_VolumActiveScene()` returns `reg().customScenes[id]`, `VoLumSceneRig.inc.cpp:476-485`) and in plugin builds is flushed to disk only from `SerializeState` and the destructor (`NeuralAmpModeler.cpp:1031`, `552`). So knob edits sit in memory across the whole session.

There is a second, worse aliasing problem: `mVolumCustomMainIdx` is a *position* in `reg().amps`. When instance B deletes a custom amp, every other instance's index silently shifts to a different amp, and `_VolumActiveScene()` starts returning — and `_VolumSaveCurrentToSettings` starts overwriting — the wrong amp's scene.

**TRIGGER** Two VoLum instances in one project. Focus a custom amp in instance A, tweak knobs, then insert instance B (or duplicate the track). A's edits are gone. For the aliasing variant: delete a custom amp in B while A is focused on one above it in the list.

**IMPACT** Silent loss of custom-amp knob edits, and in the delete case, one custom amp's scene silently overwritten with another's settings. No error, no visible change until reload.

**CONFIDENCE** likely

**FIX SKETCH** Make `Load()` idempotent: keep a `bool mLoaded` and return early if already loaded from the same base dir, so instances 2..N reuse the shared in-memory registry instead of re-reading. Separately, key the focused custom amp on its id (`mVolumCustomMainId`) and resolve the index on demand rather than caching it. The `Load()` guard is small and safe for this release; the id-vs-index change is larger and could wait. No file-format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_content_store.cpp` drives a single `ContentStore` against a temp dir. Nothing constructs two plugin instances against one process-global store, and unit tests leave the base dir empty so `Save()` is a no-op.

---

## 4. MAJOR — Any plugin instance can rewrite the entire machine-global settings file via the Lite toggle

**WHERE** `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:507-523`, `NeuralAmpModeler/NeuralAmpModelerControls.h:940-951` and `1169`

**MECHANISM** The periodic settings write is correctly fenced to standalone (`NeuralAmpModeler.cpp:934-940`, `#ifdef APP_API`), and so is the destructor's (line 544-554). `_VolumSetLiteMode` is not:

```507:513:NeuralAmpModeler/VoLumSettingsScene.inc.cpp
void NeuralAmpModeler::_VolumSetLiteMode(bool lite)
{
  if (mVolumLiteMode.load() == lite)
    return;
  mVolumLiteMode.store(lite);
  // Persist the machine-global choice immediately (JSON, not the plugin chunk).
  _VolumSaveSettingsToFile();
```

`VoLumLiteModeSwitchControl::OnMouseDown` calls it directly, and the Performance card that hosts that control is added outside the only `#if defined(APP_API)` block in that layout region (which sits at line 1186, below). So the toggle is live in VST3 and AU. `_VolumSaveSettingsToFile` writes the *whole* file: all 16 factory scenes, `lastAmpIdx`, both lock flags and live lock snapshots, `volumCustomMainId`, `volumActivePresetId`, and — new in 1.2.1 — `CalibrateInput` / `InputCalibrationLevel` taken from this instance's live params (lines 340-345, 350-351).

**TRIGGER** In a DAW, open VoLum's settings page and click FULL/LITE.

**IMPACT** The DAW project's per-amp scenes, amp selection, active preset, and input calibration are promoted to machine-global startup defaults, clobbering whatever the standalone had saved. With two instances open, whichever one you toggle wins. The next standalone launch comes up with a stranger's rig and calibration.

**CONFIDENCE** certain (that the write happens); likely (on the full user-visible consequence)

**FIX SKETCH** Give lite mode the same treatment 1.2.1 gave calibration: a narrow read/modify/write that touches only `j["liteMode"]`, mirroring `_VolumSaveCalibrationDefaults`. Guarding the call with `#ifdef APP_API` would be smaller but silently drops the plugin user's choice on restart. No format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_ui_regressions.cpp:756` pins that the string `GlobalContentStore().Save();` appears in the source — it asserts the write *exists*, not that it is fenced. No test asserts which functions may reach `_VolumSaveSettingsToFile` from a plugin build.

---

## 5. MAJOR — The content registry ignores `schemaVersion` on read and always writes v3, so a downgrade strips per-IR shaping and the return trip overwrites user edits

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:546-667` (no `schemaVersion` read), `502` (unconditional write), `763-776` (`BackupBeforeMigration`)

**MECHANISM** `RegistryFromJson` never looks at `schemaVersion`; it reads `nextPedalIndex`, `customAmps`, `irLibrary`, `customPedals`, `presetBanks`, `customScenes` and drops everything else. `RegistryToJson` then stamps `j["schemaVersion"] = kContentSchemaVersion` and re-emits only the fields it knows. Unknown keys from a newer writer are destroyed on the first save. The header comment claims forward tolerance — "v3 files load in older builds" — which is true for *loading* and false for the save that inevitably follows, since 1.2.0 saves on every content CRUD.

The 1.2.1-specific damage comes from the interaction with the one-shot flag. Round trip 1.2.1 → 1.2.0 → 1.2.1:

1. 1.2.1 migrates, writes `volum-content.json.pre-1.2.1.bak`, and the user hand-dials trims and cuts in the gear popover.
2. The user runs 1.2.0 (a bad-release rollback, or just an older install still on the machine). Any import/rename/delete calls `Save()`, whose v2 `RegistryToJson` has no `trimDb`/`lowCutHz`/`highCutHz`. All shaping is gone from disk.
3. Back on 1.2.1, no entry has `trimDb`, so `trimCalibrated` is false for all of them and `_VolumMigrateIrTrims` re-runs — overwriting the user's hand-dialled values with auto-normalized ones. And `BackupBeforeMigration` refuses to help, by design:

```771:775:NeuralAmpModeler/VoLumContentStore.h
    const auto dst = MigrationBackupPath(tag);
    if (std::filesystem::exists(dst, ec))
      return true; // already snapshotted on an earlier attempt; never overwrite
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
```

The `.pre-1.2.1.bak` from step 1 is stale, so the user's edits exist in no file anywhere.

**TRIGGER** Run 1.2.0 (or any future older build) against a library that 1.2.1 has already shaped, then return to 1.2.1.

**IMPACT** Per-IR Level and cut settings are silently reset to machine-computed values, with no recoverable copy. Both cut filters revert to off, so a shaped IR changes audibly.

**CONFIDENCE** likely

**FIX SKETCH** Two independent small changes, both additive: (a) preserve unknown top-level keys — stash the parsed `json` in the store and merge the known fields into it on save instead of building a fresh object; (b) persist calibration explicitly as `"trimCalibrated": true` rather than inferring it from `trimDb`'s presence, so a v2 round-trip that drops the value cannot re-arm the migration. (b) is a schema addition, but a purely additive optional key, and it is the change that actually protects user edits. Also consider tagging the migration backup with the source schema version rather than a fixed `"1.2.1"`.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_ir_shaping.cpp:207` and `:224` cover exactly the two forward cases (v3 entry loads calibrated, v2 entry loads uncalibrated). There is no test for a v3 file surviving a v2 writer, because nothing in the repo simulates an older writer.

---

## 6. MAJOR — Loading a pre-1.2.0 chunk leaves the 22 params 1.2.0 added at their previous values

**WHERE** `NeuralAmpModeler/Unserialization.cpp:307-310`, `312-390`, `35-85`

**MECHANISM** For any chunk from 0.9.0 through 1.1.x, `_GetConfigFrom_0_9_0` populates `config` from a frozen 71-name list ending at `SupportAmpPan`. The params 1.2.0 appended — `SupportIRToggle`, the ten PRE Pitch params, the nine Tremolo params, `DelaySync`, `DelayDivision` — are absent from that list, and the migration hook that should backfill them is empty:

```307:310:NeuralAmpModeler/Unserialization.cpp
void _UpdateConfigFrom_0_9_0(nlohmann::json& /*config*/)
{
  // No-op: 0.9.0+ share the current param byte layout, so chunks saved here need no field changes.
}
```

`_UnserializeApplyConfig` only iterates over what is *in* `config` (`Unserialization.cpp:52-70`), so a param with no entry is never assigned. Note the contrast with the older chain, which does this correctly — `_UpdateConfigFrom_0_7_10` explicitly seeds `config[kCalibrateInputParamName]` and `config[kInputCalibrationLevelParamName]` (lines 475-476) precisely so an old chunk cannot inherit stale values.

This is harmless on a fresh instance, where the params still hold their `InitDouble` defaults. It bites whenever a host calls `SetState` on an instance that already holds state: switching projects, loading a `.vstpreset` or FX preset, or a host-level state undo.

**TRIGGER** With PRE Pitch engaged at, say, -12 semitones, load a project or preset saved by VoLum 1.1.x into the same instance.

**IMPACT** The pitch shifter, tremolo, and delay sync stay engaged with the previous project's settings even though the loaded state predates those features. Audibly wrong, and the UI agrees with the wrong values so nothing looks broken.

**CONFIDENCE** likely (certain that the params are unset; host instance reuse is host-dependent)

**FIX SKETCH** Fill in `_UpdateConfigFrom_0_9_0` with `config[name] = GetParam(idx)->GetDefault()` for the 22 params, or — more robust and drift-proof — have `_UnserializeApplyConfig` reset every param absent from `config` to its default. No chunk-format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_upgrade_migration.cpp` exercises the config-dict migrations, and `test_volum_state_roundtrip.cpp` only builds and parses current-format chunks. Nothing loads an old chunk into an instance carrying non-default state, which is the only way the omission is observable.

---

## 7. MAJOR — The chunk reader has no error checking, so truncation silently yields zeros and a poisoned `pos`

**WHERE** `NeuralAmpModeler/Unserialization.cpp:88-105`

**MECHANISM**

```98:104:NeuralAmpModeler/Unserialization.cpp
  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    config[*it] = v;
  }
  return pos;
```

`IByteChunk::Get` returns `-1` and leaves the destination untouched when the read would overrun. Nothing checks. Once `pos` goes negative every subsequent `Get` also fails, so all remaining params are recorded as the raw double `0.0` and pushed through `pParam->Set(0.0)`, which clamps to each param's range rather than restoring its default. The poisoned `pos` then propagates out of `_UnserializeStateWithKnownVersion` as the return value that the VST3/AU wrapper uses to read its trailing bypass int.

This is the same failure *shape* as the two 1.2.0 CRITICALs: a reader/writer length disagreement produces a silent wrong answer instead of a detectable failure. 1.2.0 fixed the specific arithmetic; the silence is still there for every other cause.

**TRIGGER** Any short or corrupt chunk: a host that truncates state, a partially-written project file, or finding 8 below.

**IMPACT** State silently reads as a mixture of clamped zeros rather than defaults, with no diagnostic. The `VOLUM_LOG` line at 592-593 reports success.

**CONFIDENCE** certain

**FIX SKETCH** Break out of the loop on `pos < 0` and propagate a failure up so `UnserializeState` can fall back to defaults, and log it. Reading no further is strictly better than reading garbage. No format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_state_roundtrip.cpp:350` and `:377` deliberately mis-count params to prove misalignment derails the *following* blocks, always against a chunk long enough to satisfy every read. No test truncates the chunk.

---

## 8. MAJOR — A latent repeat of the 1.2.0 catastrophe: the current-format reader uses the live `kNumParams` for every version ≥ 1.2.0

**WHERE** `NeuralAmpModeler/Unserialization.cpp:525-542`

**MECHANISM**

```537:541:NeuralAmpModeler/Unserialization.cpp
    std::vector<std::string> paramNames;
    paramNames.reserve(kNumParams);
    for (int i = 0; i < kNumParams; ++i)
      paramNames.push_back(GetParam(i)->GetName());
    pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
```

This guarantees the reader matches the *writer in the same build*, which is what the 1.2.0 fix needed. It does not handle a writer from a different build: the branch is `version >= ChunkVersion(1, 2, 0)`, so a future 1.3.0 with new params will use its own larger `kNumParams` to read a 1.2.x chunk, consume too many doubles, and misalign the per-amp block — precisely the 1.2.0 failure, in the other direction.

1.2.1 is safe: the changelog repeatedly records "No parameter or state-format change," so `kNumParams` is identical to 1.2.0's. This is a trap set for the next release that adds a param.

**TRIGGER** Ship any release that appends to `EParams`, then open a 1.2.x project in it.

**IMPACT** When it fires: DAW project state resets to defaults on load — the same CRITICAL, third time.

**CONFIDENCE** certain (as a latent defect); not reachable in 1.2.1

**FIX SKETCH** Not for this release. When a param is next added, pin the count: record `kNumParams` in the chunk right before the param block, or freeze an explicit name list per released version and branch on `version >= ChunkVersion(1,3,0)`. Worth a comment at line 525 warning the next author, since the current comment reads as if the problem were solved permanently.

**WOULD A TEST HAVE CAUGHT IT?** No, and it cannot as written. `test_volum_state_roundtrip.cpp` derives both the writer and the reader from the same `kNumParams` (lines 108, 135), so writer/reader skew across *versions* is structurally invisible to it. Catching this needs a fixture chunk with a hardcoded historical param count.

---

## 9. MAJOR — Content-library mutations ignore write failures, so the UI shows content that no longer exists after restart

**WHERE** `NeuralAmpModeler/VoLumCustomContentApi.h:278`, `288`, `298`, `334`, `413`, `433`, `507`, `520`, `554`, `570`

**MECHANISM** `AddCustomAmp` and `UpdateCustomAmp` check `Save()` and roll the registry back on failure (lines 163-167, 196-200). Every other mutator discards the result — `AddIR`, `RenameIR`, `DeleteIR`, `SetIRShaping`, `AddPedal`, `DeletePedal`, `AddPreset`, `OverwritePreset`, `RenamePreset`, `DeletePreset`. `Save()` returns `false` for a read-only directory, a full disk, or a failed `MoveFileExW` after 20 retries (`VoLumSettingsFileIO.h:49-65`).

**TRIGGER** Import an IR, save a preset, or edit IR shaping while `%LOCALAPPDATA%\VoLum\content` is read-only, on a full disk, or with the file locked by a backup/AV scanner.

**IMPACT** Everything appears to work — the IR appears in the list, the preset appears in the bar, the shaping applies and sounds right. On next launch it is all gone. For an import, the copied .wav is orphaned on disk with no registry entry.

**CONFIDENCE** certain

**FIX SKETCH** Mirror the `AddCustomAmp` pattern: roll back and return a failure indicator, and surface it in the overlay's existing `mError` line. `SetIRShaping` and the preset ops matter most.

**WOULD A TEST HAVE CAUGHT IT?** No. Unit tests run with an empty base dir, where `Save()` returns `true` unconditionally (`VoLumContentStore.h:751-752`), so a failing save is never exercised.

---

## 10. MAJOR — A failed settings write silently discards custom-amp scene edits

**WHERE** `NeuralAmpModeler/VoLumSettingsScene.inc.cpp:364-381`

**MECHANISM** The content-store flush is last and both preceding writes `return` on failure:

```364:381:NeuralAmpModeler/VoLumSettingsScene.inc.cpp
  std::error_code ec;
  if (!volum::WriteJsonAtomically(settingsPath, j, ec))
  {
    std::cerr << "VoLum: write failed for settings file: " << settingsPath.string() << " (" << ec.message() << ")"
              << std::endl;
    return;
  }

  if (!volum::WriteJsonAtomically(dualAmpSettingsPath, dualAmpJson, ec))
  {
    std::cerr << "VoLum: write failed for dual-amp settings file: " << dualAmpSettingsPath.string() << " ("
              << ec.message() << ")" << std::endl;
    return;
  }

  // Persist the shared content library too (custom-amp scenes accumulate live
  // knob edits via _VolumSaveCurrentToSettings). No-op when no base dir is set.
  volum::content::GlobalContentStore().Save();
```

The three files are independent. A failure on `volum-settings.json` says nothing about whether `volum-content.json` — which holds every custom-amp scene — is writable.

**TRIGGER** Standalone shutdown (the destructor path, `NeuralAmpModeler.cpp:544-545`) with `volum-settings.json` locked or unwritable, while focused on a custom amp.

**IMPACT** The session's custom-amp knob edits are lost, with only a `std::cerr` line that a GUI app on Windows discards.

**CONFIDENCE** certain

**FIX SKETCH** Move `GlobalContentStore().Save()` above the two settings writes, or record failures and attempt all three regardless. No format change.

**WOULD A TEST HAVE CAUGHT IT?** No — same reason as finding 9: no test drives a failing write.

---

## 11. MINOR — The legacy per-amp detector false-positives on the VST3 trailing bypass int

**WHERE** `NeuralAmpModeler/VoLumChunkLayout.h:54-57`, used at `NeuralAmpModeler/Unserialization.cpp:617-618`

**MECHANISM** `remainingPerAmpBytes = chunk.Size() - pos` includes iPlug's 4-byte trailing bypass int. The extended-block probe is a strict `>`:

```54:57:NeuralAmpModeler/VoLumChunkLayout.h
inline bool ChunkHasExtendedPerAmpSettings(int remainingBytes, int ampCount)
{
  return remainingBytes > LegacyPerAmpSettingsPayloadBytes(ampCount);
}
```

A chunk that wrote only the legacy block leaves `16 × 64 = 1024` payload bytes plus 4 bypass bytes. `1028 > 1024` is true, so the reader attempts the much larger pre-pedal layout, overruns, and lands in finding 7. Every other detector in this header uses `>=` against a total that dwarfs 4 bytes, so this is the only one the bypass int can flip. Reachable only for chunks written before the pre-pedal block existed (VoLum 0.1.x-0.7.x).

**IMPACT** Per-amp scenes in a very old VST3 session load as clamped zeros.

**CONFIDENCE** likely

**FIX SKETCH** Subtract the known trailing bypass int from `remainingPerAmpBytes` before the detectors, or gate the extended block on the semantic version instead of byte counting. Given the age of affected sessions, this is defensible to defer. No format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_chunk_codec.cpp` and `test_volum_state_roundtrip.cpp` build current-format chunks; the roundtrip test appends the bypass int (`kBypassBytes`) but only ever with a full modern payload, where the detector's answer is right either way.

---

## 12. MINOR — One negative `channel` value in `volum-settings.json` resets all 16 amp scenes, and the reset is persisted

**WHERE** `NeuralAmpModeler/VoLumUserSettingsIO.h:819-841`

**MECHANISM**

```829:841:NeuralAmpModeler/VoLumUserSettingsIO.h
      if (a.contains("channel") && a["channel"].is_number_integer() && a["channel"].get<long long>() < 0)
      {
        resetAmpSettings = true;
        break;
      }
...
    if (resetAmpSettings)
    {
      for (int i = 0; i < ampCount; ++i)
        ampSettings[i] = VoLumAmpSettings{};
      healed = true;
    }
```

One bad value in one amp discards all sixteen. `healed = true` sets `mVolumSettingsDirty` (`VoLumSettingsScene.inc.cpp:494-495`), so the standalone writes the defaults back over the file — the original is not recoverable. Every writer in the codebase clamps `channel` to `0..127`, so this needs a hand-edit, a partial write, or a file from a build with a since-fixed bug.

**IMPACT** Total loss of all per-amp settings in the machine-global file, silently.

**CONFIDENCE** certain (given the trigger); the trigger itself is unlikely

**FIX SKETCH** Heal per-amp instead of globally: clamp the offending `channel` to its default and leave the other fifteen alone. `loadInt` at line 854 already does exactly that. No format change.

**WOULD A TEST HAVE CAUGHT IT?** Possibly, if `test_volum_user_settings_io.cpp` pins this behavior deliberately — worth checking before changing it, since it may be a codified response to a specific historical bug. The blast radius still looks wrong.

---

## 13. MINOR — A malformed id tail leaves `pos` on the sentinel, so the host reads `0x564C4944` as the bypass flag

**WHERE** `NeuralAmpModeler/VoLumChunkIdTail.h:429-463`, `NeuralAmpModeler/Unserialization.cpp:670-675`, `799`

**MECHANISM** `TryGetChunkIdTail` returns `false` from three points *after* the sentinel has matched — the length sanity check at 439, a non-object at 452, and the `catch` at 456-459. The caller only advances on success:

```671:675:NeuralAmpModeler/Unserialization.cpp
    int idTailPos = pos;
    const bool haveIdTail = volum::TryGetChunkIdTail(chunk, pos, chunk.Size(), idTail, &idTailPos);
    if (haveIdTail)
    {
      pos = idTailPos;
```

`pos` is then returned to the wrapper, which reads its trailing bypass int there and gets the sentinel `0x564C4944` (1447379268) — nonzero, i.e. bypassed.

**IMPACT** The plugin loads bypassed, silently, on a chunk whose tail JSON is damaged.

**CONFIDENCE** likely (certain in code; requires a corrupted chunk, since the tail carries only minted hex ids and cannot be malformed by normal use)

**FIX SKETCH** On a matched sentinel with unusable payload, still advance `*posOut` past `sentinel + len + len bytes` while returning `false`, so refs come up empty but byte alignment survives. No format change.

**WOULD A TEST HAVE CAUGHT IT?** No. `test_volum_state_roundtrip.cpp:391` asserts `pos == chunk.Size() - kBypassBytes` for a well-formed tail only.

---

## 14. MINOR — An out-of-range `legacyIndex` in the registry permanently bricks pedal imports

**WHERE** `NeuralAmpModeler/VoLumContentStore.h:614-617`

`item.legacyIndex` is read with no clamp to `kCustomPedalIndexBase..kCustomPedalIndexMax`, and line 617 raises `nextPedalIndex` to `legacyIndex + 1`. A value like 5000 makes `AddPedal` return -1 forever ("Custom pedal slots are full"), and the entry's index is outside the PRE-capture param range so it can never be selected. Fix: clamp on read and skip entries outside the pool. No format change.

---

## 15. MINOR — NaN or Inf in a chunk param double reaches the DSP

`_UnserializeApplyConfig` accepts anything where `it->is_number()` is true (`Unserialization.cpp:58-63`) and `IParam::Set` clamps via `std::max`/`std::min`, which propagate NaN rather than rejecting it. A NaN `kInputLevel` becomes a NaN input gain. Needs a corrupt chunk. Fix: reject non-finite values in that loop, one `std::isfinite` check.

---

## 16. MINOR — A corrupt dual-amp sidecar aborts the rest of settings loading, and the file is never quarantined

`dualIn >> dualAmpJson` at `VoLumSettingsScene.inc.cpp:487` sits inside the outer `try`, so a throw skips `mVolumSettingsDirty` and `_VolumRestoreEffectSettings()` (lines 494-499). Unlike `volum-content.json`, the sidecar is never renamed to `.bak`, so it fails identically on every launch. Fix: wrap the sidecar read in its own try/catch and quarantine it, matching `ContentStore::BackupCorrupt`.

---

## NITs

- `VoLumUserSettingsIO.h:786-797` — `*calibrateInput = false` / `*inputCalibrationLevel = 12.0` and the `-60..60` clamp duplicate `kDefaultCalibrateInput`, `kDefaultInputCalibrationLevel`, and the `InitDouble` range as literals; they agree today, so this is drift risk only.
- `VoLumSettingsScene.inc.cpp:386-389` — `_VolumSaveCalibrationDefaults` returns when `VolumUserSettingsFilePath()` is empty, while `_VolumSaveSettingsToFile` (356-362) and `_VolumLoadSettingsFromFile` (429-439) both fall back to the rigs root; calibration defaults silently never persist if `LOCALAPPDATA`/`HOME` is unset.
- `VoLumSettingsScene.inc.cpp:393` — the `static std::mutex` only serializes this one function within one process; it does not protect against `_VolumSaveSettingsToFile` in the same process or a second process, both of which rewrite the whole file.
- `VoLumSceneRig.inc.cpp:476-485` — `_VolumActiveScene()` uses `customScenes[id]`, so a read inserts a default entry; the const dirty-checks in `VoLumSettingsLocks.inc.cpp:206`/`220` therefore mutate the shared registry.
- `VoLumContentStore.h:546-667` — duplicate ids across `irs`/`amps`/`pedals` are accepted on load; `MintId` only guards freshly minted ones.
- `VoLumContentStore.h:651-662` — `customScenes` entries whose key matches no amp are never pruned and accumulate indefinitely.
- `VoLumCustomOverlay.h:603-607` — the `idp` minted for the stored filename is thrown away because `AddIR` mints its own, so stored paths carry an id that matches no registry entry.
- `VoLumSettingsFileIO.h:98` — `json.dump(2)` throws on invalid UTF-8 and is not wrapped, so that failure escapes into the host instead of returning `false`.
- `VoLumChunkLayout.h:13` — `kPrePedalPerAmpSettingsBytes` declares 9 ints but `GetExtendedPerAmpSettings` reads 5; the constant is only used for a `>` comparison so nothing breaks today, but the layout contract it documents is wrong.
- `VoLumAmpSettingsJson.h` — `AmpSettingsFromJson` derives `healed` from `ReadAmpCoreBlock` alone and discards the return values of `PreBlockFromJson`, `PostBlockFromJson`, and `ReadDualAmpBlock`, so healing in those blocks is not reported to callers who would rewrite the file.
- `NeuralAmpModeler.cpp:1041` — `SerializeState` captures `ok` from `SerializeParams` and then does substantial further work before returning it; a param-serialization failure still emits a full, plausible-looking tail.

---

Two things I want to flag about scope. Findings 2, 3, 6, 7, 8, 11 and 12 predate 1.2.1 — I included them because they sit squarely in the subsystem and findings 2 and 3 are the kind of silent state loss this release is trying to close out. And finding 1 is the one I would not ship without: it is a small, self-contained fix, and without it the release's headline feature does not work for anyone who imports an IR after installing it.

The `VolumUserSettingsFilePath()` function returns an empty object only when environment variables like `LOCALAPPDATA` or `HOME` aren't set, which is uncommon but does occur in certain DAWs, plugin hosts, and sandboxed environments where the environment gets sanitized. This creates a minor asymmetry where the save and load functions both fall back to the rigs root directory, but there's an inconsistency in how calibration data is handled.

Now I'm gathering exact line numbers and file references to cite in the report—pulling specific sections from the header files and implementation files that document the import paths, migration logic, calibration markers, serialization patterns, and chunk layout definitions.

Let me verify one more detail about the parameter handling: iPlug's state deserialization doesn't reset parameters to defaults beforehand, and the config application only sends a notification after setting values, so parameters not in the config file remain unchanged. That confirms the issue.

Time to write this up concisely, keeping the minor issues to single-line descriptions.