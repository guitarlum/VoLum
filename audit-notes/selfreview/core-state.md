# Self-review: `ebb0215..HEAD` on `release/1.2.1`

Scope reviewed: plugin core (`NeuralAmpModeler.cpp/.h`), DSP entry points
(`ProcessBlock`, `_ApplyDSPStaging`, `VoLumProcessBlock.inc.cpp`), chunk
serialization (`Unserialization.cpp`, `VoLumChunkSafeRead.h`,
`VoLumChunkVersion.h`, `VoLumChunkCodec.h`), the async loader
(`VoLumLoader.inc.cpp`), preset save/recall (`VoLumSettingsPresets.inc.cpp`,
`VoLumCustomContentApi.h`, `VoLumPresetBar.h`, `VoLumPresetStep.h`), the
metronome/tuner DSP and overlay, plus the reconciliation paths those changes
now reach (`VoLumAmpMenus.inc.cpp`, `VoLumSceneRig.inc.cpp`). iPlug2 read at
`IPlugStructs.h`, `IPlugVST3_Common.h`, `IPlugVST3_View.h`, `IPlugAU.cpp`,
`IPlugAPP_dialog.cpp`, `IPlugEditorDelegate.h`.

## Verdict

Ship-blocking: one. The rest is shippable, but not clean. The seven changes you
asked me to stress-test are individually well-reasoned and most of them are
correct and complete — `SafeGetStr`, `TryParseChunkVersion`, the loader-thread
log move, the `mVolumSupportSelected` flag, the preset hook-owner token, the
tuner deactivation and the metronome NaN guards all hold up under tracing. What
does not hold up is the *blast radius* of the two changes that touch the state
reader. The new `try/catch` in `UnserializeState` converts a crash into a
silently half-initialised instance: any throw between `Unserialization.cpp:763`
and `:768` leaves `mVolumInitComplete == false` forever, which switches off
SUPPORT/PRE model loading, settings persistence and preset-dirty tracking for
the rest of the session with nothing on screen to say so. That is a worse
outcome than the crash it replaces and it needs a two-line RAII fix before
release. Separately, the new `_VolumSyncUiFromState()` at the end of
`_UnserializeStateWithKnownVersion` is not the read-only display refresh the
comment describes: it can reach `_VolumClearIR`, which erases a just-restored
IR id from the scene, writes `kIRToggle`, and marks the preset `(unsaved)` — now
during host state load, undo and host preset switching. Finally, the bounded-read
`return -1` you added to `_UnserializePathsAndExpectedKeys` is not honoured by
its only caller, so the fix stops the zeros but not the partial apply it was
written to prevent. No change in this range alters tone, filter curves or gain
staging.

## Findings

---

### 1. The new `UnserializeState` try/catch can leave `mVolumInitComplete` permanently `false`

- **Severity:** High (ship-blocking)
- **Confidence:** CONFIRMED (structural — the leak is guaranteed if anything in
  the window throws; the probability of a throw is the only uncertain part)
- **Introduced by:** `a2fa080` ("Fix seven defects found auditing 1.2.1 before
  release"), the `try/catch` hunk in `NeuralAmpModeler.cpp`

**Evidence**

```1146:1173:NeuralAmpModeler/NeuralAmpModeler.cpp
  try
  {
    // Look for the expected header. If it's there, then we'll know what to do.
    WDL_String header;
    int pos = volum::SafeGetStr(chunk, startPos, header);
    ...
  }
  catch (const std::exception& e)
  {
    VOLUM_LOG("state", std::string("UnserializeState failed: ") + e.what());
    return -1;
  }
```

The catch is *outside* this window, which the try now swallows:

```763:768:NeuralAmpModeler/Unserialization.cpp
    mVolumInitComplete = false;
    _VolumRestoreFromSettings(mVolumAmpIdx);
    _VolumApplyLiveLockSnapshots();
    _VolumRefreshChannels();
    mVolumNeedsLoad.store(true);
    mVolumInitComplete = true;
```

Before this commit an exception from inside that window propagated out of
`setState` and took the host down. Now it is caught and the instance keeps
running with `mVolumInitComplete == false`. That flag gates essentially every
reactive behaviour in the plugin:

```1315:1339:NeuralAmpModeler/NeuralAmpModeler.cpp
        mVolumSupportNeedsLoad.store(true);
        ...
    case kSupportAmpIdx:
    case kSupportSpeakerIdx:
      if (mVolumInitComplete)
      {
        mVolumSupportNeedsLoad.store(true);
```

```1385:1396:NeuralAmpModeler/NeuralAmpModeler.cpp
      if (mVolumInitComplete)
      {
        mVolumPreNeedsLoad[0].store(true);
        _UpdateLatency();
      }
```

```916:918:NeuralAmpModeler/NeuralAmpModeler.cpp
  // Always keep in-memory settings current (OnIdle runs on main thread, params valid)
  if (mVolumInitComplete)
    _VolumSaveCurrentToSettings();
```

```183:184:NeuralAmpModeler/VoLumSettingsPresets.inc.cpp
  if (!mVolumInitComplete)
    return;
```

So a stuck-`false` instance: never loads a SUPPORT or PRE NAM again when the
user changes one, never re-reports latency, never writes `volum-settings.json`,
never marks a preset dirty, and `mVolumSettingsDirty` is never set at the end of
`OnParamChange`. All silent.

Realistic throw sources inside the window: `_VolumRestoreFromSettings` →
`_VolumApplyAmpSettings` → `GlobalContentStore().reg().customScenes[id]` (map
insert, `bad_alloc`), and `_VolumRefreshChannels`/`_VolumRefreshSupportChannels`
→ `volum::DiscoverChannels`, which calls the throwing `path::string()` overload:

```176:181:NeuralAmpModeler/VoLumPaths.h
  for (const auto& entry : fs::directory_iterator(ampDir, ec))
  {
    if (!entry.is_regular_file(ec))
      continue;
    std::string name = entry.path().filename().string();
```

On MSVC, `path::string()` performs a narrow conversion that throws
`std::system_error` when a filename cannot be represented in the active code
page. A rigs folder with a filename the ACP cannot encode is enough.

**Repro** (deterministic, no exotic input needed): temporarily insert
`throw std::runtime_error("x");` between `Unserialization.cpp:764` and `:765`,
load a VST3 project, then change the SUPPORT amp — the support lane never loads,
and `volum-settings.json` stops updating in standalone. Nothing is logged except
the one `UnserializeState failed` line.

**Suggested fix**

Make the flag exception-safe rather than assignment-safe:

```cpp
struct InitCompleteGuard
{
  bool& flag;
  explicit InitCompleteGuard(bool& f) : flag(f) { flag = false; }
  ~InitCompleteGuard() { flag = true; }
} initGuard(mVolumInitComplete);
```

replacing the two bare assignments. Belt-and-braces: also set
`mVolumInitComplete = true;` in both `catch` blocks in `UnserializeState`, since
an unrelated future writer of that flag would reintroduce the same hazard.

---

### 2. `_VolumSyncUiFromState()` in the state reader is not read-only — it can erase a restored IR reference and mark the preset dirty

- **Severity:** Medium
- **Confidence:** CONFIRMED for the call graph and the writes; LIKELY for the
  user-visible outcome (requires `plan.clearOrphanedIr`)
- **Introduced by:** `a2fa080` (the `_VolumSyncUiFromState();` hunk in
  `Unserialization.cpp`), amplified by `bb01a44` (the `laneFocused` rework of
  `_VolumApplyUiSyncPlan`)

**Evidence**

```821:825:NeuralAmpModeler/Unserialization.cpp
  // Hosts also load state into an editor that is already open - project load with the
  // window up, undo, host preset switching - and that path never reached the applier,
  // so the cab row kept describing the state the chunk just replaced. A no-op when no
  // editor exists; OnUIOpen runs the same call once the controls are built.
  _VolumSyncUiFromState();
```

The "no-op when no editor exists" half is true (`VoLumAmpMenus.inc.cpp:285-287`
returns on a null `GetUI()`), and there is no path from here back into
`SerializeState`. But when an editor *is* open the chain is
`_VolumSyncUiFromState` → `_VolumApplyFocusedLaneCabs` → `_VolumApplyUiSyncPlan`,
and that is a mutating function:

```427:432:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  // The resolved channel cannot host this lane's stored IR; drop it so a real cab
  // takes over. Unconditional, because it repairs state and not just the display -
  // and _VolumClearIR guards its own row writes on focus. That cab's capture is
  // staged below, so the removal rides along with it.
  if (plan.clearOrphanedIr)
    _VolumClearIR(support, /*deferToCabSwap=*/true);
```

```722:741:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  if (support)
    _VolumActiveScene().supportActiveIrId.clear();
  else
    _VolumActiveScene().activeIrId.clear();
  ...
  const int toggle = support ? kSupportIRToggle : kIRToggle;
  GetParam(toggle)->Set(0.0);
  SendParameterValueFromDelegate(toggle, GetParam(toggle)->GetNormalized(), true);
  ...
  mVolumSettingsDirty = true;
  _VolumMarkPresetDirty();
```

So, inside `UnserializeState`, with the editor open:

- the scene's `activeIrId` / `supportActiveIrId` that the id tail just restored
  (`Unserialization.cpp:748-749`) is cleared — and the cleared value is what the
  next `SerializeState` writes, so the user's IR reference is destroyed, not
  merely ignored for the session;
- `kIRToggle` is written to 0 immediately after the chunk restored it;
- `_VolumMarkPresetDirty()` → `_VolumRecomputePresetDirty()` runs, and it does
  *not* early-out here because `mVolumInitComplete` was restored to `true` at
  `Unserialization.cpp:768` — so the preset bar comes up `(unsaved)` right after
  a host preset recall or an undo.

Two mitigations that keep this at Medium rather than High: the param write is
`SendParameterValueFromDelegate` (delegate → editor), **not**
`InformHostOfParamChange`, so it does not fight host automation; and the same
reconciliation already ran from `OnUIOpen` (`NeuralAmpModeler.cpp:1189`), so for
the "open the editor after loading" order the destructive clear pre-dates
tonight. What is new is that it now also fires for **undo and host preset
switching with the window already open**, which previously left the state alone.

**Repro** Custom MAIN amp with a custom IR assigned on a gain stage that has a
DIRECT capture; save the project; on a machine (or after a library edit) where
the resolved channel no longer carries a DIRECT capture, load with the editor
open, or hit undo. The IR chip clears, `(unsaved)` appears, and re-saving loses
the IR id.

**Suggested fix** Split the applier: give `_VolumApplyUiSyncPlan` a
`bool repairState` argument (or add a `_VolumSyncUiFromStateDisplayOnly()`) and
pass `false` from the state-load call site, so `clearOrphanedIr` /
`_VolumClearIR` only runs from interactive and post-restore paths. At minimum,
suppress `_VolumMarkPresetDirty()` for the duration of the state load with an
existing-style flag (`mVolumPostRestoreInProgress` is the local precedent).

---

### 3. The new `return -1` in `_UnserializePathsAndExpectedKeys` is never checked by its caller — the fix stops the zeros but not the apply

- **Severity:** Medium
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (the `if (pos < 0) return -1;` hunks in
  `Unserialization.cpp`). Note the `-1` *propagation* is pre-existing; what is
  new is that the commit's stated goal is not reached.

**Evidence**

```103:114:NeuralAmpModeler/Unserialization.cpp
  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    // A truncated chunk used to leave every remaining name at 0.0 in `config`,
    // which the caller then applied to live params as if the project had asked
    // for it. Failing here keeps a short read from silently rewriting the rig.
    if (pos < 0)
      return -1;
    config[*it] = v;
  }
```

The only caller never tests the result:

```559:563:NeuralAmpModeler/Unserialization.cpp
    std::vector<std::string> paramNames;
    paramNames.reserve(kNumParams);
    for (int i = 0; i < kNumParams; ++i)
      paramNames.push_back(GetParam(i)->GetName());
    pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
```

and then, with `pos == -1`, execution continues straight through:

```626:639:NeuralAmpModeler/Unserialization.cpp
  _UnserializeApplyConfig(config);
  ...
    volum::VoLumChunkSelection selection;
    pos = volum::GetVoLumChunkSelection(chunk, pos, selection);
    ...
    const int remainingPerAmpBytes = chunk.Size() - pos;
```

Consequences I traced:

- `_UnserializeApplyConfig(config)` still applies the partial param set to live
  params, calls `OnParamReset(kPresetRecall)`, and then `_StageModel` /
  `_StageIR` on the paths it did read (`Unserialization.cpp:75-85`). "Keeps a
  short read from silently rewriting the rig" is therefore only half true — the
  rig is still partly rewritten, just with real values instead of zeros.
- `remainingPerAmpBytes = chunk.Size() - (-1)` = `Size() + 1`, one byte more
  than exists, so `ChunkHasExtendedPerAmpSettings` /
  `ChunkHasDualAmpPerAmpSettings` / `ChunkHasPostPerAmpSettings` can all report
  sections that are not there.
- Every subsequent read is a no-op (`IByteGetter::GetBytes` returns `-1` and
  skips the `memcpy`, `IPlugStructs.h:73-82`), so `mVolumAmpSettings[i]` keeps
  its *previous* contents rather than being reset, and
  `GetVoLumChunkSelection`'s `ClampChunkSelection` clamps the untouched struct
  defaults — no out-of-range `mVolumAmpIdx`, so no OOB into `volum::kAmps`. That
  part is safe.
- `_VolumRestoreFromSettings` then runs over that mixture, and finding 2's
  `_VolumSyncUiFromState()` writes it to the editor.

**Repro** Truncate a saved VST3 state to just past the IR path string and reload.
The instance comes up with a partial rig applied and a `-1` returned to the host.

**Suggested fix** Test the result before using it:

```cpp
pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
if (pos < 0)
  return -1;   // before _UnserializeApplyConfig
```

and the same guard after each `_GetConfigFrom_*` branch (they all forward the
same `-1`). Bail *before* `_UnserializeApplyConfig(config)`, which is the only
way to actually honour "leaves the instance exactly as it was" — the phrase your
own comment at `Unserialization.cpp:527-530` uses for the version-parse failure.

---

### 4. Preset CRUD from the Manage overlay never claims the bridge, so it can edit another instance's bank

- **Severity:** Medium
- **Confidence:** CONFIRMED for the missing claim; LIKELY for the cross-instance
  outcome (needs interleaved interaction in two open editors)
- **Introduced by:** `a2fa080` / `5d6beb1` — the claim refactor covered the three
  hook-using operations and left the two bank-mutating ones out

**Evidence** — the three operations that use a hook do claim:

```112:146:NeuralAmpModeler/VoLumSettingsPresets.inc.cpp
int NeuralAmpModeler::_VolumSavePresetAs(const std::string& name)
{
  _VolumClaimPresetOps();
  ...
void NeuralAmpModeler::_VolumOverwritePreset(int index)
{
  _VolumClaimPresetOps();
  ...
void NeuralAmpModeler::_VolumRecallPreset(int index)
{
  _VolumClaimPresetOps();
```

but rename and delete bypass the plugin entirely and are called from the overlay:

```553:564:NeuralAmpModeler/VoLumCustomOverlay.h
      default: RenamePreset(mAmpIdx, idx, name); break;
      ...
      default: DeletePreset(mAmpIdx, idx); break;
```

and both resolve the bank from the process-global ambient key, then persist:

```599:627:NeuralAmpModeler/VoLumCustomContentApi.h
inline void RenamePreset(int /*ampIdx*/, int idx, const std::string& name)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  ...
    bank[(size_t)idx].name = name;
    Store().Save();
  ...
inline void DeletePreset(int /*ampIdx*/, int idx)
{
  auto& banks = Store().reg().presetBanks;
  auto it = banks.find(ActivePresetOwnerKey());
  ...
    bank.erase(bank.begin() + idx);
```

`ActivePresetOwnerKey()` is last written by whichever instance most recently ran
`_VolumRefreshPresetBar()` (`VoLumSettingsPresets.inc.cpp:88`) or
`_VolumSyncPresetOwner()` (`:59`) — neither is instance-scoped:

```467:475:NeuralAmpModeler/VoLumCustomContentApi.h
inline std::string& ActivePresetOwnerKey()
{
  static std::string key = content::FactoryOwnerKey(0);
  return key;
}
```

So: instance A opens Manage on amp X; instance B is then interacted with (any
amp switch or preset-bar refresh publishes B's key); A clicks Delete → the
preset is erased from **B's** bank and written to disk. Index-based, so it will
delete whatever happens to sit at that row.

There is also a latent second hole on the same surface:

```337:337:NeuralAmpModeler/VoLumCustomOverlay.h
            const int i = mSavePreset ? mSavePreset(s) : AddPreset(mAmpIdx, s);
```

The fallback branch invokes `AddPreset` (which calls `PresetCaptureHook()`) with
no claim at all. It is unreachable in production because
`VoLumLayoutBuild.inc.cpp:1263` always installs `mSavePreset`, but it is exactly
the "future call site that forgets to claim" your destructor comment warns about
(`NeuralAmpModeler.cpp:546-551`).

**Test gap** — `test_volum_ui_regressions.cpp:1152-1184` is a source-string lock
that counts `_VolumClaimPresetOps();` occurrences and asserts exactly three
`SetActivePresetOwner(_VolumActiveOwnerKey())` sites. It passes unchanged with
this bug present, because rename/delete are not part of what it counts.

**Suggested fix** Route rename and delete through plugin methods
(`_VolumRenamePreset` / `_VolumDeletePreset`) that call `_VolumClaimPresetOps()`
first, exactly like the other three. Then extend the count in the test from 3 to
5. Delete the `: AddPreset(mAmpIdx, s)` fallback so the unclaimed path cannot be
reached at all.

The ownership *token* itself is correct and I could not break it — see
"Reviewed and found correct".

---

### 5. The moved loader log lines now report loads that never went live

- **Severity:** Low (diagnostics only)
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (the log move in `VoLumLoader.inc.cpp`)

**Evidence** The drain is genuinely clean now — I read
`_VolumDrainLoaderResults` end to end (`VoLumLoader.inc.cpp:138-240`) and there
is no `VOLUM_LOG`, no allocation-visible file I/O, and no other blocking call;
`_ApplyDSPStaging` (`NeuralAmpModeler.cpp:1707-1820`) is likewise clean. The
justification for the move is accurate: every entry stats, may rename, and opens
an `ofstream` under a mutex (`VoLumDiagLog.h:118-145`).

But the messages moved to a point where they cannot know the outcome:

```377:396:NeuralAmpModeler/VoLumLoader.inc.cpp
    // This also reports loads the drain later discards as superseded, which is
    // the more honest record of what was actually read from disk.
    {
      ...
      if (!result.error.empty())
        VOLUM_LOG("model", kindLabel + " load FAILED " + result.path + " : " + result.error);
      else
        VOLUM_LOG("model", kindLabel + " loaded " + result.path);
    }
```

Meanwhile the drain still decides whether the result is used:

```167:171:NeuralAmpModeler/VoLumLoader.inc.cpp
      if (superseded)
        continue;
      mVolumIsLoading.store(false);
      if (mVolumNeedsLoad.load())
        continue;
```

So `MAIN loaded <path>` is now printed for models that were superseded or
dropped, and `SUPPORT load FAILED` no longer coincides with
`mShouldRemoveSupportModel` actually firing. Rapid amp/channel switching will
produce a log that reads as if several amps loaded when only the last one did —
the wording "loaded" claims a swap that did not happen. The comment
acknowledges this; the message text does not.

**Suggested fix** Rename to what the line now means: `MAIN read <path>` /
`MAIN read FAILED <path>`. If you want swap confirmation back, latch a small
enum in the drain and emit it from `OnIdle` (the pattern `VoLumDiagLog.h:161-163`
prescribes and that `mVolumMainLoadFailed` already uses).

---

### 6. `MetronomeDSP::SetBPM` throws away the user's tempo on non-finite input instead of keeping it

- **Severity:** Low
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (the `isfinite` hunk in `VoLumMetronomeDSP.h`)

**Evidence**

```150:153:NeuralAmpModeler/VoLumMetronomeDSP.h
  void SetBPM(float bpm)
  {
    mBPM.store(std::isfinite(bpm) ? std::clamp(bpm, kMinBPM, kMaxBPM) : kDefaultBPM, std::memory_order_relaxed);
  }
```

The overlay's own reject path gets this right — it keeps the previous tempo:

```449:454:NeuralAmpModeler/VoLumTunerMetronomeOverlay.h
      if (end != str && std::isfinite(bpm))
      {
        mBPM = std::clamp(bpm, volum::MetronomeDSP::kMinBPM, volum::MetronomeDSP::kMaxBPM);
        if (mOnBPMChanged)
          mOnBPMChanged(mBPM);
      }
```

so the two layers disagree about what "reject" means. If `SetBPM` ever fires its
NaN branch, a user at 76 BPM silently jumps to 120 — and because the control's
`mBPM` is not updated, the overlay keeps *displaying* 76. That is the
"stale/zero previous value" problem you asked about, in mirror image.

**Coverage note (answering the "every entry point" question):** I enumerated
every writer. `mMetronomeDSP.SetBPM` has exactly one production caller
(`VoLumLayoutBuild.inc.cpp:1315`, wired to `mOnBPMChanged`). The overlay writes
`mBPM` from: the `-`/`+` buttons (`VoLumTunerMetronomeOverlay.h:364,372`, both
clamped against a finite start value), the text entry (`:449`, guarded), and
`Show(...)` (`:470`), whose argument is `mMetronomeDSP.GetBPM()`
(`VoLumSceneRig.inc.cpp:31`) — already sanitised. `_RecalcSamplesPerBeat` floors
a NaN correctly via `if (!(bpm >= kMinBPM))` (`:181`) and the division cannot
produce `mSamplesPerBeat < 1` (`:187`). **There is no chunk-restore or
settings-file entry point at all** — metronome BPM is not a param and is not in
`VoLumAmpSettings` / `VolumUserSettingsToJson`, so those two entry points you
were worried about do not exist. The guards are complete.

**Suggested fix** `if (!std::isfinite(bpm)) return;` — leave the last good
tempo in place, matching the overlay.

---

### 7. `OnUIClose` fires on more paths than "the user closed the plugin window"

- **Severity:** Low (behavioural, not a defect)
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (the `mTunerDSP.SetActive(false)` hunk)

**Answers to the specific questions you asked:**

*Is the tuner's active state also driven by a parameter or a control that will
re-enable it on reopen?* No. `mTunerDSP` has exactly three writers, all
editor-scoped: `_ToggleVoLumTuner` (`VoLumSceneRig.inc.cpp:5-21`), the tuner
control's dismiss action (`VoLumLayoutBuild.inc.cpp:1081`), and now `OnUIClose`.
There is no `kTuner*` param and no persisted field, and the editor is rebuilt
with the tuner control hidden, so there is **no new UI-shows-on/DSP-off
inconsistency** — the states agree after a reopen. This part is correct and
complete.

*Is it safe to touch DSP state from `OnUIClose` relative to the audio thread?*
Yes. `SetActive` is a single relaxed atomic store plus message-thread-only
scratch resets that are skipped on the `false` branch:

```84:94:NeuralAmpModeler/VoLumTunerDSP.h
  void SetActive(bool active)
  {
    mActive.store(active, std::memory_order_relaxed);
    if (active)
    {
      mSmoothedFreq = 0.f;
      ...
```

The audio thread only reads it via `mTunerDSP.IsActive()`
(`NeuralAmpModeler.cpp:611`), so the worst case is a one-block-late unmute.

*Is `OnUIClose` called on all paths?* It is called from
`IPlugEditorDelegate::CloseWindow()` (`iPlug2/IPlug/IPlugEditorDelegate.h:92`),
and the callers include two that a user would not describe as closing the
plugin:

- standalone preferences: `IPlugAPP_dialog.cpp:609` → `IPlugAPPHost::CloseWindow()`
  → `IPlugAPP_host.cpp:103` → `mIPlug->CloseWindow()`. Applying an audio-device
  change now switches the tuner off.
- VST3 `IPlugVST3View::removed()` (`IPlugVST3_View.h:131`), which some hosts
  invoke when re-parenting or hiding the editor rather than destroying it.

Also worth knowing: a host that tears an instance down without closing the
editor first reaches the destructor and not `OnUIClose`, but the process is going
away, so no silence survives.

No fix required. Flagging it so the changelog line does not promise "closing the
window" when the standalone Settings dialog does it too.

---

### 8. Clearing the SUPPORT amp is no longer instantly silent

- **Severity:** Low (audible timing change, arguably an improvement)
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (the `mVolumSupportSelected` hunk in
  `NeuralAmpModeler.cpp` / `VoLumLoader.inc.cpp`)

The old predicate was evaluated on the audio thread from a param, so setting the
support amp to "(none)" muted the lane on the very next block:

```593:599:NeuralAmpModeler/NeuralAmpModeler.cpp
  // Not GetParam(kSupportAmpIdx) >= 0: a custom (library) SUPPORT partner has no
  ...
  const bool supportAmpSelected = mVolumSupportSelected.load(std::memory_order_relaxed);
  const bool haveSupportModel = supportAmpSelected && (mSupportModel != nullptr);
```

The new flag is only written from the message thread, one `OnIdle` tick later:

```901:905:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (mVolumSupportNeedsLoad.load() && !mVolumSupportIsLoading.load())
  {
    mVolumSupportNeedsLoad.store(false);
    _VolumRequestSupportModelLoad();
  }
```

so between the param change and `mVolumSupportSelected.store(false)`
(`VoLumLoader.inc.cpp:478`) plus the `mShouldRemoveSupportModel` swap, the lane
keeps playing the previous capture for an idle tick. Note the extra hazard in
that gate: if a support load is in flight, `!mVolumSupportIsLoading` blocks the
request until it completes, stretching the window to the length of a model load.

No fix requested — a fade-out beats a hard cut — but it is a behaviour change
caused by tonight's work and belongs in the manual test notes.

---

### 9. `_VolumHasSupportAmp()` trusts a possibly stale `mVolumCustomSupportIdx`

- **Severity:** Low
- **Confidence:** SPECULATIVE (I did not find a concrete path that leaves the
  index out of range, but nothing validates it here)
- **Introduced by:** `bb01a44`

```391:400:NeuralAmpModeler/VoLumAmpMenus.inc.cpp
bool NeuralAmpModeler::_VolumHasSupportAmp()
{
  const int factory = GetParam(kSupportAmpIdx)->Int();
  return (factory >= 0 && factory < volum::kAmpCount) || mVolumCustomSupportIdx >= 0;
}

void NeuralAmpModeler::_VolumClampSupportFocus()
{
  if (mVolumDualAmpFocusedSupport && !_VolumHasSupportAmp())
    mVolumDualAmpFocusedSupport = false;
}
```

The factory index is range-checked; the custom index is not. Every other reader
of a custom index in this range bounds it against the library — e.g.
`_VolumShowSupportAmpMenu` uses
`mVolumCustomSupportIdx < static_cast<int>(customAmps.size())`
(`VoLumAmpMenus.inc.cpp:132`), and `_VolumSetSupportCustom` validates on entry
(`:214`). If the custom amp backing the SUPPORT lane is deleted from Manage and
the index is not reset, `_VolumClampSupportFocus` will keep focus on a lane that
no longer has an amp — the exact condition the clamp was added to prevent.

**Suggested fix**
`mVolumCustomSupportIdx >= 0 && mVolumCustomSupportIdx < (int)volum::custom::MockCustomAmps().size()`.

---

### 10. The `SafeGetStr` test's chunk model writes a NUL that iPlug2 does not

- **Severity:** Low (test hygiene)
- **Confidence:** CONFIRMED
- **Introduced by:** `a2fa080` (new file `tests/test_volum_chunk_safe_read.cpp`)

```93:98:NeuralAmpModeler/tests/test_volum_chunk_safe_read.cpp
  void PutStr(const std::string& s)
  {
    PutInt(static_cast<int>(s.size()) + 1); // iPlug2 writes the NUL too
    bytes.insert(bytes.end(), s.begin(), s.end());
    bytes.push_back('\0');
  }
```

iPlug2 does not:

```189:194:iPlug2/IPlug/IPlugStructs.h
  inline int PutStr(const char* str)
  {
    int slen = (int) strlen(str);
    Put(&slen);
    return PutBytes(str, slen);
  }
```

The length prefix excludes the terminator. Every assertion in the file still
holds (the fake is internally consistent, and its `GetStr` stops at the first
NUL, so the round-trip strings match), but the file claims byte-for-byte
fidelity — "Kept verbatim from IPlugStructs.h:73-107" at line 20 — and the
writer half is not. Someone reading this later will conclude there is an
off-by-one in the real length prefix and go looking for it.

**Suggested fix** `PutInt((int)s.size());` and drop the `push_back('\0')`, then
have the fake's `GetStr` `assign(first, safe)` without the NUL scan, matching
`WDL_String::Set(ptr, len)`.

---

## Reviewed and found correct

These I traced and am satisfied with — no action needed.

**`volum::ChunkStrLenInBounds` / `volum::SafeGetStr` (`VoLumChunkSafeRead.h`).**
The premise is accurate: `IByteGetter::GetStr` computes
`int strEndPos = strStartPos + len;` with `len` straight from the chunk
(`IPlugStructs.h:96`), so a length near `INT_MAX` wraps negative, satisfies
`strEndPos <= srcSize`, and reaches `str.Set((char*)(pSrc + strStartPos), len)`
with the huge length — a real out-of-bounds copy. A negative `len` returns a
position behind the input, as documented. The fix is correct on every axis I
checked: the subtraction form `declaredLen <= chunkSize - strStartPos` cannot
overflow or underflow given the preceding non-negativity checks;
`strStartPos == chunkSize` with `declaredLen == 0` is accepted (empty string at
the very end); `startPos < 0` is rejected up front; the length prefix width
matches `Get(&declaredLen, ...)` with `int declaredLen`, i.e. the same 4 bytes
`PutStr` writes; and re-reading the prefix inside the delegated `GetStr` is
harmless. No off-by-one. The accompanying test suite is unusually good — it
pins the *premise* too ("The unchecked reader really does accept an INT_MAX
length"), so it will tell you if iPlug2 ever hardens `GetStr`.

**`volum::TryParseChunkVersion` (`VoLumChunkVersion.h:62-119`).** No regression
in accepted inputs. The old `ChunkVersion(const std::string&)` also required
exactly three dot-separated segments (`:31-32`), so requiring three is not a
tightening. The strictness the comment claims is real and only removes inputs
`std::stoi` accepted by accident — leading whitespace, a `+`/`-` sign, trailing
junk, `"1.2.3."` — none of which any VoLum or NAM build has ever serialized
(`PLUG_VERSION_STR`, `"0.7.15"`). The `kMaxComponent` cap of 1e6 keeps
accumulation three orders of magnitude from `int` overflow. Rejecting instead of
throwing does leave the instance untouched, as the comment claims — the early
`return -1` at `Unserialization.cpp:533/541` precedes every mutation.

**Removing `VOLUM_LOG` from `_VolumDrainLoaderResults`.** Verified there is
nothing else blocking or allocating left on the audio thread in the drain path.
`_VolumDrainLoaderResults` (`VoLumLoader.inc.cpp:138-240`) uses `try_to_lock` on
the loader mutex and bails if contended, takes `mStagingMutex` only to move
`unique_ptr`s, and does no I/O. `_ApplyDSPStaging`
(`NeuralAmpModeler.cpp:1707-1820`) is log-free. The regression test
(`test_volum_ui_regressions.cpp:1328-1352`) bounds the drain body by the next
function and asserts `RequireDoesNotContain(drainBody, "VOLUM_LOG")` — it would
genuinely fail if a log were reintroduced. The one-per-load `result.model->Reset`
at `:150-155` is a pre-existing audio-thread cost, unchanged tonight.

**`mVolumSupportSelected` (`NeuralAmpModeler.h:849`, `VoLumLoader.inc.cpp`).**
I traced every writer and every consumer and could not find a stale read that
matters.

- All seven exits of `_VolumRequestSupportModelLoad` set the flag: `false` at
  `:444, 462, 478, 491, 511`, `true` at `:468, 518`. It is the sole writer
  (`NeuralAmpModeler.h:845-849`).
- Everything that can change the support lane routes through
  `mVolumSupportNeedsLoad` → `OnIdle:901-905`: `OnParamChange` for
  `kDualAmpActive` / `kSupportAmpIdx` / `kSupportSpeakerIdx` /
  `kSupportChannelIdx` (`:1315, 1331, 1339`), `_VolumSetSupportAmp` (`:203`),
  `_VolumSetSupportCustom` (`:240`), `_VolumApplyAmpSettings` (`:288`) — which is
  what chunk restore and preset recall both go through —
  `_VolumApplyUiSyncPlan` (`VoLumSceneRig.inc.cpp:454`), and the layout/keyboard
  paths. Construction is covered because the ctor's restore reaches
  `_VolumApplyAmpSettings`. I found no path that loads or unloads a support model
  without going through the flag.
- **Flag `true`, `mSupportModel` null mid-swap: safe.**
  `haveSupportModel` is `false`, so `MakeProcessingPlan` yields
  `runSupportModel = false` and `runDualAmp = false`
  (`VoLumProcessingPlan.h:41-42`), which is the sole guard on
  `mSupportModel->process` — `_VolumProcessDualAmpSupportLane` returns `nullptr`
  immediately on `!runDualAmp` (`VoLumProcessBlock.inc.cpp:145-146`) and the
  merge is additionally gated on `supportLane != nullptr`
  (`NeuralAmpModeler.cpp:637`). No null dereference is reachable. Both reads
  happen on the audio thread after `_ApplyDSPStaging`, and only the audio thread
  mutates `mSupportModel`, so it cannot go null between the check and the call.
- **Flag `false`, model non-null:** reachable when a support load lands after
  dual-amp was disabled (the drain stages it at `VoLumLoader.inc.cpp:207-211`
  and `_ApplyDSPStaging` processes removal at `:1737` *before* staging at
  `:1775`, so the model is resurrected). Result is a stale model held in memory
  and correctly not run. Re-enabling dual amp re-queues, because the drain
  cleared `mVolumLoadingSupportPath` (`:194-195`). No audio consequence.

The failure it fixes is real: with a custom SUPPORT partner, `kSupportAmpIdx` is
deliberately parked at `-1` (`_VolumSetSupportCustom`,
`VoLumAmpMenus.inc.cpp:220-224`), so the old predicate made the whole custom
dual-amp lane silent. Good fix. Caveat: the pinning test
(`test_volum_ui_regressions.cpp:1292-1326`) is source-string counting — it would
pass if a `store` moved into an unreachable branch. Adequate as a tripwire, not
as verification.

**Preset hook-owner token (`VoLumCustomContentApi.h:493-512`,
`NeuralAmpModeler.cpp:546-551`).** The token logic is correct and I could not
construct a use-after-free from it. A destroyed instance clears the hooks only
when it still owns them; if another instance has claimed since, the clear is a
no-op and the live instance's hooks survive. Since every hook-using operation
claims immediately before the synchronous call
(`VoLumSettingsPresets.inc.cpp:114, 130, 142`), and the whole
claim→`AddPreset`→`PresetCaptureHook()()` sequence runs to completion on the
message thread with no re-entry point (`Store().Save()` does not pump a message
loop on either platform), there is no window in which one instance's operation
can be interleaved with another's destruction. The residual risk is the two
non-claiming call sites in finding 4, not the token.

**`_VolumSyncUiFromState()` null-graphics safety and non-recursion.** Safe on
both counts. `GetUI()` is checked first (`VoLumAmpMenus.inc.cpp:285-287`), as it
is again in `_VolumApplyFocusedLaneCabs` (`:252-257`),
`_VolumApplyUiSyncPlan` (`VoLumSceneRig.inc.cpp:398-403`) and
`_VolumApplyCustomMainCabs` (`:468-469`), so a headless project load is a true
no-op. No path from it reaches `SerializeState` or `UnserializeState`, so it
cannot recurse into serialization. `volum::kAmps[mVolumAmpIdx]` at `:308` is
safe because `mVolumAmpIdx` comes from `ClampChunkSelection`
(`VoLumChunkCodec.h:488`). Its param write-back — `kSupportChannelIdx` clamping
in `_VolumRefreshSupportChannels` (`VoLumAmpMenus.inc.cpp:376-380`) — uses
`SendParameterValueFromDelegate`, which targets the editor, **not**
`InformHostOfParamChange`, so it does not fight host automation; and that
particular call already ran during restore via `_VolumApplyAmpSettings:287`.
The genuine problem with this call is the *state repair* it drags in, which is
finding 2, not the null-safety or the recursion you asked about.

**The `laneFocused` gate in `_VolumApplyUiSyncPlan`
(`VoLumSceneRig.inc.cpp:415-438`).** I went looking for a lane switch that now
leaves the cab row describing the wrong lane and did not find one. The
ordering holds: `_VolumApplyAmpSettings` calls `_UpdateVoLumLayout` (`:299-300`),
which reaches `_VolumApplyDualAmpFocus` → `_VolumClampSupportFocus`
(`VoLumAmpMenus.inc.cpp:409`) *before* `_VolumSelectCustomAmp`'s tail
`_VolumApplyCustomMainCabs(customIdx)` (`VoLumSceneRig.inc.cpp:344`), so focus is
already settled when the gate is evaluated. The hero's focus callback clamps then
calls `_VolumApplyFocusedLaneCabs` in the right order
(`VoLumLayoutBuild.inc.cpp:374-381`), the factory-amp sidebar click goes through
`_VolumApplyFocusedLaneCabs` (`:114`), and `_VolumApplyRecalledPreset` ends in it
(`VoLumSettingsPresets.inc.cpp:169`). The unconditional parts — the per-lane
channel stepper (separate control per lane), the routing caches, the scene
writes, `mVolumSupportNeedsLoad` — are correctly left outside the gate.

**`volum::StepPresetIndex` (`VoLumPresetStep.h`).** Correct, and it fixes a real
asymmetry. The old expression `((mIdx < 0 ? 0 : mIdx) + dir % n + n) % n` both
mapped "nothing selected" to 0 (so `>` skipped the first preset) and applied
`%` to `dir` before the addition through operator precedence. `-1` now maps to
`0` for forward and `count - 1` for backward, and the `count <= 0` guard is
redundant-but-harmless behind the caller's `mList.empty()` check
(`VoLumPresetBar.h:197-201`).

**`mTunerDSP.SetActive(false)` thread safety** — see finding 7; the state
machine has no UI/DSP disagreement and the store is audio-thread-safe.

**Audio voicing.** Nothing in this range alters tone, filter curves or gain
staging. The only `VoLumProcessBlock.inc.cpp` change in the range is
clang-format reflow of the `tremoloRateHz` ternary (`dba9ed0`), semantically
identical. The metronome guards do not change click synthesis
(`VoLumMetronomeDSP.h:111-131` is reflow only). `MakeProcessingPlan` is
untouched. `mVolumSupportSelected` changes *whether* the support lane runs, not
how it sounds, and only in the direction of running a lane that should have been
running (finding 8 is the one timing side effect).

## Answers to questions not covered above

**"On the catch path, what state is the plugin left in?"** Half-restored, with no
resync and — critically — possibly with `mVolumInitComplete == false` (finding 1).
The `_VolumSyncUiFromState()` at `Unserialization.cpp:825` is skipped on the
throw path, so params can be partly the new project's while the editor still
shows the old one, and `_VolumApplyDspCaches()` never runs, so cached gains and
tone-stack coefficients stay on the pre-load values. Fixing finding 1 with an
RAII guard also gives you the natural place to call `_VolumSyncUiFromState()` and
`_VolumApplyDspCaches()` on the way out.

**"Does returning the wrong byte position corrupt the host's expectation of how
many bytes were consumed?"** Yes, in two different ways, and both are worth
knowing before release.

- **VST3** hands the return value straight to `IBStream::seek`:

```69:76:iPlug2/IPlug/VST3/IPlugVST3_Common.h
    int pos = pPlug->UnserializeState(chunk,0);

    Steinberg::int32 savedBypass = 0;

    pState->seek(pos,Steinberg::IBStream::IStreamSeekMode::kIBSeekSet);
    if (pState->read (&savedBypass, sizeof (Steinberg::int32)) != Steinberg::kResultOk) {
      return false;
    }
```

  `seek(-1, kIBSeekSet)` is out of contract and host stream implementations are
  not required to reject it — Steinberg's own `MemoryStream` stores the cursor
  verbatim, so the following `read` is a 4-byte read at offset `-1`. In practice
  the `read` fails and `setState` returns `kResultFalse`, which is the outcome
  you want; but the negative seek is a host-dependent hazard, not a defined
  failure signal. LIKELY.

- **AUv2 treats `-1` as success**, because it tests for zero, not for negative:

```1502:1505:iPlug2/IPlug/AUv2/IPlugAU.cpp
  if (!UnserializeState(chunk, 0))
  {
    return kAudioUnitErr_InvalidPropertyValue;
  }
```

  So on macOS AU, every chunk your new code rejects is reported to the host as
  loaded successfully. CONFIRMED. This is pre-existing iPlug2 behaviour, but the
  new early `return -1` paths (header `SafeGetStr` failure at
  `NeuralAmpModeler.cpp:1150-1152`, unparsable version at
  `Unserialization.cpp:538-542`, both `catch` blocks) make it reachable for
  inputs that previously either threw or fell through to the legacy reader. If
  you want the AU host to see the failure, `return 0` rather than `-1` — but
  check `IPlugPluginBase.cpp:427/522`, which test `> 0`, before changing it.

## Test coverage notes

- `tests/test_volum_chunk_safe_read.cpp` — strong. Models the real iPlug2
  failure and pins the premise. One writer-side inaccuracy (finding 10).
- `tests/test_volum_chunk_version.cpp` — covers the new parser.
- `tests/test_metronome_dsp.cpp:263-291` — covers NaN/inf into `SetBPM` and the
  clamp. It asserts the *sanitised* outcome, so it passes with the
  reset-to-default behaviour of finding 6 as well as with the keep-previous
  behaviour; it does not pin which one you want.
- `tests/test_volum_ui_regressions.cpp:1328-1352` (drain has no logging) — good,
  fails for the right reason.
- `tests/test_volum_ui_regressions.cpp:1292-1326` (support flag) — source-string
  counting; a tripwire, not verification.
- `tests/test_volum_ui_regressions.cpp:1152-1184` (preset claims) — **would pass
  unchanged with finding 4 present.** It counts `_VolumClaimPresetOps();`
  occurrences and asserts exactly three `SetActivePresetOwner` sites; rename and
  delete are outside what it looks at.
- **No test covers findings 1, 2 or 3.** All three are in
  `_UnserializeStateWithKnownVersion`, which needs a real `IByteChunk` and a live
  plugin instance. The cheapest coverage is to move the `pos < 0` bail into a
  pure predicate and unit-test that, plus a `test_volum_state_roundtrip.cpp` case
  that truncates a real serialized chunk at a param boundary and asserts the
  instance is unchanged.
