# VoLum 1.2.1 — Class-Level Audit of `NeuralAmpModeler`

Read in full: `NeuralAmpModeler.h` (913 lines), `NeuralAmpModeler.cpp` (2331 lines), `VoLumParams.h`, `config.h`, `Unserialization.cpp`, `VoLumDspStagingWdl.h`, `VoLumChunkVersion.h`. Skimmed all tail-included siblings plus the relevant iPlug2 internals (`IPlugVST3_ProcessorBase.cpp`, `IPlugAU.cpp`, `IPlugEditorDelegate.h`, `IGraphicsEditorDelegate.cpp`, `IPlugStructs.h`) to establish thread contexts and lifetimes.

**Headline:** the single largest problem is not any one line — it is that `OnParamChange` is treated throughout as a main-thread/UI callback when in VST3 and AU it is an **audio-thread** callback for host automation. iPlug2 states this explicitly in its own header. Four of the findings below are consequences of that.

---

## 1. BLOCKER — `OnParamChange` performs UI mutation, filesystem I/O, container mutation and host latency renegotiation on the audio thread

**WHERE:** `NeuralAmpModeler.cpp:1213-1373` (handlers at 1274-1304, 1342-1366); framework contract at `iPlug2/IPlug/IPlugEditorDelegate.h:120`, call site `iPlug2/IPlug/VST3/IPlugVST3_ProcessorBase.cpp:316`, `iPlug2/IPlug/AUv2/IPlugAU.cpp:1576`

**MECHANISM:** iPlug2 documents the contract:

> `iPlug2/IPlug/IPlugEditorDelegate.h:120` — "whereas **OnParamChange may be called on the audio thread** and should be used to update DSP state, OnParamChangeUI is always called on the low-priority thread, should be used to update UI (e.g. for hiding or showing controls)."

And it means it. `IPlugVST3ProcessorBase::Process` (the realtime callback) drains automation and calls `mPlug.OnParamChange(idx, kHost, offsetSamples)` at line 316, inside `mParams_mutex`. AU's `SetParamProc` does the same at `IPlugAU.cpp:1576`. VoLum overrides only the 1-arg `OnParamChange(int)`, so it cannot tell `kHost` (audio thread) from `kUI`/`kDelegate` (main thread). What the handlers then do:

```1286:1304:NeuralAmpModeler/NeuralAmpModeler.cpp
    case kSupportAmpIdx:
    case kSupportSpeakerIdx:
      if (mVolumInitComplete)
      {
        mVolumSupportNeedsLoad.store(true);
        _VolumRefreshSupportChannels();
        _UpdateVoLumLayout();
      }
      break;
    case kSupportChannelIdx:
      if (mVolumInitComplete)
      {
        mVolumSupportNeedsLoad.store(true);
        if (auto* pGfx = GetUI())
          if (auto* stepper = pGfx->GetControlWithTag(kCtrlTagVoLumSupportChannelStep))
            stepper->As<VoLumChannelStepControl>()->SetChannels(
              mVolumSupportChannelLabels, GetParam(kSupportChannelIdx)->Int());
      }
      break;
```

- `_VolumRefreshSupportChannels()` (`VoLumAmpMenus.inc.cpp:349-397`) runs `volum::DiscoverChannels(...)` — a `std::filesystem` directory scan — then `clear()`/`push_back()` on `mVolumSupportChannelFiles` and `mVolumSupportChannelLabels`, calls `GetParam(...)->Set()`, `SendParameterValueFromDelegate(...)`, and finally mutates a control through `GetUI()`.
- `_UpdateVoLumLayout()` (`VoLumLayoutRuntime.inc.cpp:18`) walks the entire control tree ~35 times via `ForAllControlsFunc` calling `Hide()`, and touches cached control pointers.
- `kDualAmpActive` (line 1244-1280) additionally writes `mVolumAmpSettings[mVolumAmpIdx]`, calls `SendParameterValueFromDelegate` four times, and writes the non-atomic `mVolumDualAmpFocusedSupport`, which the UI thread reads in `_VolumSupportFocused()`.
- `kPreNam1/2Active|Capture` and `kPrePitch*` call `_UpdateLatency()` → `SetLatency()` → VST3 `restartComponent(kLatencyChanged)`, which the VST3 SDK forbids from the process call.

`mVolumSupportChannelLabels` is a `std::vector<std::string>` concurrently read by the UI thread (`_VolumMakeUiSyncInput`, `VoLumSceneRig.inc.cpp:351+`) and by `_VolumRefreshSupportChannels`'s own tail. `GetUI()` is `mGraphics.get()` on a plain `unique_ptr` that the main thread sets to `nullptr` in `IGEditorDelegate::CloseWindow` (`IGraphicsEditorDelegate.cpp:55`), which destroys every control.

**TRIGGER:** Automate any of `kSupportAmpIdx`, `kSupportSpeakerIdx`, `kSupportChannelIdx`, `kDualAmpActive`, `kPreNam*Active/Capture`, `kPrePitchActive/Mode/TransChar` from a DAW automation lane (or move them via a control surface / host generic UI, which also routes through the processor in VST3). Playing back a single automation envelope on the DUAL toggle is enough.

**IMPACT:** Audible dropouts and glitches from a filesystem scan and dozens of allocations inside the render callback; torn or corrupt channel labels; concurrent-modification crashes in the control tree; host stalls or hangs from `restartComponent` re-entering the processor. In debug/sanitizer builds this is a straight data-race report. The standalone (APP) is unaffected, so this never shows up in local testing — a genuine plugin-format divergence.

**CONFIDENCE:** certain (mechanism); likely (crash) / certain (glitch)

**FIX SKETCH:** Override the 3-arg form and split by source:

```cpp
void NeuralAmpModeler::OnParamChange(int paramIdx, EParamSource source, int offset) override
{
  _ApplyDspOnlyParamChange(paramIdx);              // gains, tone coeffs, atomics, *needsLoad flags
  if (source == EParamSource::kHost)               // audio thread: defer the rest
    { mVolumDeferredHostParamWork.store(true); return; }
  _ApplyMainThreadParamChange(paramIdx);           // layout, channel rescan, latency, GetUI() work
}
```
then drain `mVolumDeferredHostParamWork` in `OnIdle`. Zero sound change: the DSP-relevant half (`_SetInputGain`, `_SetOutputGain`, `mToneStack->SetParam`, `mVolumSupportNeedsLoad`) stays exactly where it is.

**Would a test have caught it?** No. `tests/` contains no instantiated-plugin threading test; `test_volum_ui_sync_plan.cpp` and friends exercise the pure planners, and the rest are source-string pins.

---

## 2. BLOCKER — `_UpdateLatency()` runs on the audio thread from `_ApplyDSPStaging`, calling `SetLatency()` and dereferencing `GetUI()`

**WHERE:** `NeuralAmpModeler.cpp:1764-1779` (call sites), `2218-2265` (`_UpdateLatency`), `2274-2286` (`_VolumRefreshLatencyReport`)

**MECHANISM:** `_ApplyDSPStaging()` is called from exactly one place — `ProcessBlock` line 577. Its tail:

```1764:1779:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (removedMainModel || appliedMainModel)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (removedSupportModel || appliedSupportModel)
  {
    _UpdateLatency();
    _SetSupportOutputGain();
  }
```

`_UpdateLatency` then:

```2255:2264:NeuralAmpModeler/NeuralAmpModeler.cpp
  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }

  // Force: our PDC just changed, and the editor may have been rebuilt with empty
  // labels, so an unchanged device-side report still has to be re-sent.
  _VolumRefreshLatencyReport(/*force=*/true);
```

and `_VolumRefreshLatencyReport` writes the non-atomic `mVolumLastLatencyReport` (also written from `OnIdle` line 793) and then does `GetUI()` → `GetControlWithTag(kCtrlTagSettingsBox)` → `SetCurrentLatency(report)`. Three separate violations in one call: `SetLatency` → `restartComponent` from the process call; a data race on `mVolumLastLatencyReport`; and a `GetUI()`/control dereference racing the main thread's `mGraphics = nullptr` + control destruction.

Note the comment at 2262-2263 ("the editor may have been rebuilt…") shows `force=true` exists *because* of editor rebuilds — i.e. the author was reasoning about editor lifetime here and still reached into the editor from the audio thread.

**TRIGGER:** Any model finishing its async load, or any cab/channel switch, i.e. normal use. The crash variant needs the editor to be closing in the same window as a model landing — click a different rig, then immediately close the plugin window. Also reachable on every project load, where the loader stages a model while the host may be opening/closing views.

**IMPACT:** Sporadic crash on plugin-window close shortly after a rig switch; host-side stalls or "plugin requested restart" churn during rig switching; garbled latency readout on the Settings page.

**CONFIDENCE:** certain (SetLatency/UI-touch from audio thread); likely (crash)

**FIX SKETCH:** In `_ApplyDSPStaging`, replace the three `_UpdateLatency()` calls with `mVolumLatencyDirty.store(true, std::memory_order_relaxed);` and call `_UpdateLatency()` from `OnIdle` when the flag is set. Keep `_SetInputGain`/`_SetOutputGain`/`_SetSupportOutputGain` where they are (they only write `double` members, and moving them would delay a gain change by one idle tick — a sound change). No voicing impact; PDC is reported one idle tick later, which hosts handle.

**Would a test have caught it?** No. `test_volum_latency_report.cpp` tests the pure `LatencyReport` computation, not the thread it runs on.

---

## 3. BLOCKER — `ChunkVersion` throws out of `UnserializeState` on a truncated or malformed chunk; nothing catches it

**WHERE:** `Unserialization.cpp:518-521`, `VoLumChunkVersion.h:22-37`

**MECHANISM:**

```518:521:NeuralAmpModeler/Unserialization.cpp
  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  volum::ChunkVersion version(versionStr);
```

```28:36:NeuralAmpModeler/VoLumChunkVersion.h
    while (std::getline(stream, token, '.'))
      parts.push_back(std::stoi(token));

    if (parts.size() != 3)
      throw std::invalid_argument("Version string must have exactly 3 dot-separated segments");
```

`IByteGetter::GetStr` (`iPlug2/IPlug/IPlugStructs.h:90-107`) leaves the `WDL_String` **untouched** when the string would run past the end of the buffer, yet still returns a positive position. So a chunk that carries the correct `###NeuralAmpModeler###` header but is truncated (or has a corrupt length prefix) yields `versionStr == ""` → `parts.size() == 0` → `std::invalid_argument`. `std::stoi` throws on its own for non-numeric segments.

I grepped `iPlug2/IPlug/VST3/IPlugVST3.cpp`, `IPlugVST3_Common.h`, `AUv2/IPlugAU.cpp` and `APP/IPlugAPP.cpp`: there is **no `catch` anywhere** on the state-restore path. `IPlugVST3::setState` (line 110-115) forwards straight to `IPlugVST3State::SetState`, and `IPlugAU.cpp:1502` calls `UnserializeState` bare. The exception crosses the VST3/AU C ABI.

**TRIGGER:** Any truncated, byte-swapped, or partially-written chunk — a project file damaged by a crash or a full disk, a `.vstpreset` copied incompletely, a host that hands back a zero-length blob, or pluginval's corrupt-state / fuzzed-state pass.

**IMPACT:** Host process terminates while loading a project. The user loses unsaved work in every other track, not just VoLum's state.

**CONFIDENCE:** certain (throw); certain (unhandled by iPlug2)

**FIX SKETCH:** Wrap the whole body of `_UnserializeStateWithKnownVersion` in `try { … } catch (const std::exception& e) { VOLUM_LOG("chunk", …); return startPos; }`. Returning `startPos` makes iPlug treat it as a failed restore and keep constructor defaults, which is the correct degradation. Optionally give `ChunkVersion` a `TryParse` that returns `std::optional`. No sound change.

**Would a test have caught it?** No. `test_volum_chunk_version.cpp` only feeds well-formed version strings; `test_volum_state_roundtrip.cpp` round-trips a valid chunk.

---

## 4. BLOCKER (rare) — `mNAMPaths.live` / `mIRPaths.live` are written on the audio thread under `mStagingMutex` and read on the main thread without it

**WHERE:** writes `NeuralAmpModeler.cpp:1692, 1707, 1714, 1731, 1754, 1760`; unsynchronized reads `NeuralAmpModeler.cpp:869, 993, 1039, 1040`; stale contract at `NeuralAmpModeler.h:861-865`

**MECHANISM:** The audio thread mutates the live path strings inside `_ApplyDSPStaging`'s lock:

```1750:1755:NeuralAmpModeler/NeuralAmpModeler.cpp
    if (mStagedIR != nullptr && !holdMainIr)
    {
      mIR = std::move(mStagedIR);
      mStagedIR = nullptr;
      volum::dsp_staging::CommitStagedPathOnApply(mIRPaths);
    }
```

`CommitStagedPathOnApply` does `paths.live = paths.staged` — a `WDL_String` assignment, i.e. free + malloc + memcpy of the buffer. Meanwhile the main thread reads the same buffer with no lock at all:

- `OnIdle:869` — `if (fileToLoad == mNAMPaths.live.Get() && !forceMainReload)`
- `OnIdle:993` — `PathFromUtf8(mNAMPaths.live.Get()).filename()`
- `SerializeState:1039-1040` — `chunk.PutStr(mNAMPaths.live.Get()); chunk.PutStr(mIRPaths.live.Get());`

The header's documented contract covers only the staged pointers and is additionally wrong about who takes the lock:

```861:865:NeuralAmpModeler/NeuralAmpModeler.h
  // Serializes writes from non-audio threads (UnserializeState path -> _StageModel /
  // _StageIR) against the audio-thread read/move in _ApplyDSPStaging. The VoLum
  // worker-queue path drains on the audio thread already, so it does not need this
  // mutex; it is for the legacy NAM staging entry points only.
  mutable std::mutex mStagingMutex;
```

The worker-queue path *does* take `mStagingMutex` — `VoLumLoader.inc.cpp:181, 209, 238` — so this comment is stale in exactly the way the audit brief warns about, and it is what makes the `live`-path gap easy to miss.

**TRIGGER:** A model or IR going live in the same instant the main thread saves state or runs its idle tick. Switching rigs while the host auto-saves; or simply an OnIdle tick at 60 Hz colliding with a swap.

**IMPACT:** Usually a torn read (footer shows a garbled filename, or the same-path short-circuit misfires and reloads a model needlessly). Occasionally a use-after-free on the freed `WDL_String` buffer → crash, or a corrupt path written into the saved project.

**CONFIDENCE:** certain (unsynchronized); speculative (frequency)

**FIX SKETCH:** Take `std::lock_guard<std::mutex> lock(mStagingMutex)` around each of the four reads (the mutex is already `mutable`, so `SerializeState`'s `const` is fine), copying into a local `std::string` before use. Then correct the header comment to say the pair's `live` field is also mutex-protected. No sound change.

---

## 5. MAJOR — `_ResetModelAndIR` never resamples the SUPPORT-lane IR

**WHERE:** `NeuralAmpModeler.cpp:1851-1903`; contract at `NeuralAmpModeler.h:743-744` and `817-820`

**MECHANISM:** `dsp::ImpulseResponse` resamples its wav data **once, in its constructor**, for the sample rate it is handed; `GetSampleRate()` reports that rate. So the only way to re-rate a live convolver is to rebuild it. `_ResetModelAndIR` does that for the MAIN lane and stops:

```1884:1903:NeuralAmpModeler/NeuralAmpModeler.cpp
  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}
```

`mSupportIR` and `mStagedSupportIR` are never mentioned. Every other resource in the function is handled per-lane: `mModel`/`mSupportModel`/`mPreModel[2]` all get their `Reset(sampleRate, maxBlockSize)`. Two header comments assert the invariant this breaks:

```743:744:NeuralAmpModeler/NeuralAmpModeler.h
  // Resetting for models and IRs, called by OnReset
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);
```

```817:820:NeuralAmpModeler/NeuralAmpModeler.h
  // And the IR. The MAIN amp and the dual-amp SUPPORT lane each get their own
  // convolver so a custom IR is local to one lane (per-lane custom IR, spec 3.2).
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  std::unique_ptr<dsp::ImpulseResponse> mSupportIR;
```

**TRIGGER:** Dual Amp on, a custom IR loaded on the SUPPORT lane, then the sample rate changes: open the project at 44.1 kHz after saving at 48 kHz, change the project rate mid-session, or change the device rate in the standalone's Preferences. `mSupportIRPaths` is likewise left untouched, so nothing downstream notices either.

**IMPACT:** The SUPPORT lane convolves with IR weights resampled for the *old* rate. At 48 k → 44.1 k the cab impulse is stretched ~8.8%: resonances shift down roughly a semitone and a half and the tail lengthens. Clearly audible as a wrong, dull, slightly "pitched-down" cab on one side of the stereo image, while MAIN sounds correct — which is precisely the confusing presentation that makes this hard to report.

**CONFIDENCE:** certain

**FIX SKETCH:** Duplicate the MAIN block for the support pair immediately after it:

```cpp
  if (mStagedSupportIR != nullptr)
  {
    if (mStagedSupportIR->GetSampleRate() != sampleRate)
      mStagedSupportIR = std::make_unique<dsp::ImpulseResponse>(mStagedSupportIR->GetData(), sampleRate);
  }
  else if (mSupportIR != nullptr)
  {
    if (mSupportIR->GetSampleRate() != sampleRate)
      mStagedSupportIR = std::make_unique<dsp::ImpulseResponse>(mSupportIR->GetData(), sampleRate);
  }
```

`CommitStagedPathOnApply` already no-ops on an empty staged path (`VoLumDspStagingWdl.h:28`), so `mSupportIRPaths.live` is preserved without extra work. **This changes sound** — but only to correct it: the support lane starts convolving at the right rate. It is a fix to a wrong-rate bug, not a voicing change, and at matched rates it is a no-op.

**Would a test have caught it?** No. `test_volum_dsp_staging.cpp` covers the pure staging predicates; nothing instantiates the plugin and drives `OnReset` at two rates with a support IR loaded. A behavioral test is straightforward: build a `ResamplingNAM`-free harness that stages a support IR at 48 k, calls `_ResetModelAndIR(44100, …)`, and asserts `mStagedSupportIR->GetSampleRate() == 44100`.

---

## 6. MAJOR — With no editor open, a custom (BYO) MAIN amp loads the wrong capture: `mVolumCustomMainSlot`/`Channel` are only ever written inside an editor-gated function

**WHERE:** `NeuralAmpModeler.h:546-549`; `VoLumSceneRig.inc.cpp:325-330, 396-403, 440-441, 448-454`; consumer `NeuralAmpModeler.cpp:816-834`

**MECHANISM:** These two members decide which manifest `.nam` gets staged:

```546:549:NeuralAmpModeler/NeuralAmpModeler.h
  // Selected (slot, channel) within the focused custom MAIN amp, used to resolve
  // which manifest .nam to stage. Only meaningful when mVolumCustomMainIdx >= 0.
  int mVolumCustomMainSlot = volum::custom::kDirectSlot;
  int mVolumCustomMainChannel = 1;
```

`OnIdle` consumes them:

```816:826:NeuralAmpModeler/NeuralAmpModeler.cpp
    if (mVolumCustomMainIdx >= 0)
    {
      const auto amp = volum::custom::CustomAmpAt(mVolumCustomMainIdx);
      const std::string rel = volum::content::CaptureFileFor(amp, mVolumCustomMainSlot, mVolumCustomMainChannel);
      if (!rel.empty())
      {
        fileToLoad = volum::content::PathToUtf8(volum::content::GlobalContentStore().ResolveStored(rel));
        customMainLoad = true;
      }
```

The only restore-path writer is `_VolumApplyUiSyncPlan` — which bails before reaching them when there is no editor:

```396:403:NeuralAmpModeler/VoLumSceneRig.inc.cpp
void NeuralAmpModeler::_VolumApplyUiSyncPlan(const volum::UiSyncPlan& plan, bool support)
{
  auto* pGfx = GetUI();
  if (!pGfx)
    return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl)
    return;
```

`_VolumSelectCustomAmp` clearly *intends* to cover the headless case, and the branch is dead:

```325:330:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  auto* pGfx = GetUI();
  if (!pGfx)
  {
    _VolumApplyCustomMainCabs(customIdx);
    return;
  }
```

because `_VolumApplyCustomMainCabs` opens with `if (GetUI() == nullptr) return;` (line 450-451). So on the headless path both calls are no-ops.

The asymmetry is the tell: `_VolumApplyAmpSettings` **does** restore the SUPPORT equivalents without needing an editor (`VoLumSettingsScene.inc.cpp:271-272`, `mVolumCustomSupportSlot = sl; mVolumCustomSupportChannel = ch;`). MAIN has no such path. All the data needed is present — `mVolumSpeakerIdx` and `mVolumChannelIdx` are restored from the chunk, and `_VolumMakeUiSyncInput`/`MakeUiSyncPlan` are pure functions that map them to `(slot, channel)`.

**TRIGGER:** In a DAW, open a project whose MAIN amp is a custom/BYO amp on any capture other than DIRECT/channel 1, and play or bounce **without opening the plugin window**. Offline render farms, batch stem exports, and "hit play before opening the GUI" all hit it. Standalone is immune (the editor always exists).

**IMPACT:** Two flavours, both bad. If the custom amp has a DIRECT capture, the plugin loads the raw un-cabbed amp on gain stage 1 — grossly wrong tone, buzzy and thin, on a bounce the user may not audition. If it has no DIRECT capture, `rel` is empty and the main model never loads at all: the footer reads "LOAD FAILED - custom capture path is missing" and the track renders silence. Opening the window "fixes" it, which makes the bug look like a phantom.

**CONFIDENCE:** likely (certain that the members stay at defaults; the exact audible result depends on the amp's manifest)

**FIX SKETCH:** Split the plan application so derivation and cache write-back happen unconditionally, and only the control updates stay behind the graphics guard:

```cpp
void NeuralAmpModeler::_VolumApplyUiSyncPlan(const volum::UiSyncPlan& plan, bool support)
{
  _VolumRecordUiSyncCaches(plan, support);   // lines 426-445 body, no GetUI() needed
  auto* pGfx = GetUI();
  if (!pGfx) return;
  auto* spkCtrl = pGfx->GetControlWithTag(kCtrlTagVoLumSpeakerRow);
  if (!spkCtrl) return;
  /* ...existing control updates... */
}
```

and drop the `GetUI() == nullptr` early return from `_VolumApplyCustomMainCabs` so the intended headless call in `_VolumSelectCustomAmp` actually does something. Note `plan.clearOrphanedIr` → `_VolumClearIR` also currently only runs with an editor, so an orphaned IR is likewise not reconciled headlessly — worth folding into the same fix.

**Would a test have caught it?** No, and there are three source-string pins in this exact neighbourhood that give a false sense of coverage: `test_volum_ui_regressions.cpp:1018-1036` pins that `_VolumForceDirectCapture` derives the stage from `mVolumChannelIdx` rather than the runtime cache, and `:1055-1079` pins the recall path — both assert on substrings of the source, neither instantiates a plugin without an editor.

---

## 7. MAJOR — A focused custom amp's scene lives in the process-global content store instead of the DAW chunk: instances overwrite each other, and `SerializeState` writes to disk

**WHERE:** `NeuralAmpModeler.cpp:1011-1033` (`SerializeState`), `540-556` (destructor), `787-903` (`OnIdle` → `_VolumSaveCurrentToSettings`), `VoLumSceneRig.inc.cpp:321-323`

**MECHANISM:** For a factory amp the whole scene is in the chunk (`PutCurrentVoLumChunkState`, line 1044-1045, writes all `kAmpCount` slots). For a custom amp only the **id** goes in the chunk (`idTail.customMainId`, line 1057); the scene itself lives in `GlobalContentStore().reg().customScenes[id]`, a process-wide singleton shared by every instance in the host. `_VolumSaveCurrentToSettings` runs from `OnIdle` **every idle tick** (line 902-903) and redirects into that shared map; `_VolumSelectCustomAmp` reads it back (`VoLumSceneRig.inc.cpp:323`).

The code is explicit that it knows instances fight over shared files — and then does exactly that with a different one:

```544:554:NeuralAmpModeler/NeuralAmpModeler.cpp
#ifdef APP_API
  _VolumSaveSettingsToFile();
#else
  // Plugin formats deliberately don't rewrite the shared per-amp
  // volum-settings.json from every instance (instances would fight over it),
  // but a focused custom amp's scene lives in the content library, so flush it
  // on teardown too - covers a host quit that tears down instances without a
  // preceding SerializeState. No-op when no custom amp is focused / no base dir.
  if (mVolumCustomMainIdx >= 0)
    volum::content::GlobalContentStore().Save();
#endif
```

`SerializeState` does the same (line 1031-1032). So `customScenes[id]` is written unsynchronized from every instance's idle tick and flushed to one JSON file from every instance's state-save and teardown.

Two distinct failures:

1. **Cross-instance leakage and loss.** Two instances using the same custom amp with different cabs/IRs/knobs write their live values into the same map entry ~60×/s. Whichever wrote last wins. Save and reload the project and *both* instances restore that one scene: one instance's rig silently becomes the other's.
2. **Disk I/O inside `getState`.** `GlobalContentStore().Save()` serializes and rewrites the whole content library JSON. Hosts call `getState` often — Reaper on every undo point, most hosts on autosave. Two instances flushing near-simultaneously can interleave writes to the same path and truncate the library.

`SerializeState` is also declared `const` and `const_cast`s itself (line 1023) to mutate several dozen members. The comment at 1013-1020 justifies this for pluginval, and on the main thread it is serialized against `OnIdle` — but a host that calls `getState` off the main thread turns it into a race against `OnIdle`'s writes to the same `mVolumAmpSettings` array.

**TRIGGER:** Put two instances of VoLum on different tracks, focus the same custom amp in both, give them different cabs or custom IRs, save, reopen.

**IMPACT:** Silent state loss across instances — the user's second guitar track comes back with the first track's rig. The docs claim custom-amp selections survive a reload; with one instance they do, with two they do not.

**CONFIDENCE:** certain (the shared write and last-writer-wins semantics); likely (the truncation variant)

**FIX SKETCH:** Not a 1.2.1 change — the proper fix is to serialize the focused custom amp's scene into the chunk id tail alongside `customMainId`, which is a chunk-format bump. For 1.2.1, two cheap mitigations: (a) drop `GlobalContentStore().Save()` out of `SerializeState` and keep only the destructor flush, cutting the write frequency from every-autosave to once-per-teardown; (b) guard the store's save/load with a `static std::mutex` the way `_VolumSaveCalibrationDefaults` already does (`VoLumSettingsScene.inc.cpp:391-394` — that function gets this right, so the pattern exists in-tree). Also document the single-instance-per-custom-amp limitation in the release notes. No sound change.

**Would a test have caught it?** No. `test_volum_content_store.cpp` and `test_volum_custom_content.cpp` exercise one store from one "instance"; nothing models two concurrent owners.

---

## 8. MINOR — Denormals are re-enabled before the POST chain, the dual-amp merge, and the safety clipper

**WHERE:** `NeuralAmpModeler.cpp:566-569` and `617-618` vs. `622-719`

**MECHANISM:** `disable_denormals()` sets FTZ/DAZ in MXCSR; `std::feholdexcept`/`std::feupdateenv` save and restore that same control word. `feupdateenv` is called at line 618, before roughly 100 lines of DSP:

```617:618:NeuralAmpModeler/NeuralAmpModeler.cpp
  // restore previous floating point state
  std::feupdateenv(&fe_state);
```

Everything after it — `MergeDualAmpToStereo`, `_ProcessOutput`, `_VolumProcessPostChain` (delay, reverb, tremolo), the metronome, the dual-amp peak scan, and the `SoftSafetyClip` loop — runs with denormal flushing off. Delay and reverb feedback loops are exactly the DSP where denormals accumulate.

**TRIGGER:** Let a long reverb or delay tail decay to silence with a high feedback setting, then stop playing.

**IMPACT:** CPU spikes (denormal arithmetic is 10-100× slower on x86) that can push a tight buffer into dropouts, appearing seconds *after* the user stops playing — hard to attribute.

**CONFIDENCE:** certain (mechanism); speculative (whether `dsp::effect::Delay`/`Reverb` have their own DC-offset or flush guards that already mask it)

**FIX SKETCH:** Move line 618 to just before `_UpdateMeters` at line 719. **Flag: this can alter sound**, because flushing denormals to zero in a feedback loop truncates the last ~10⁻³⁰⁸ of a decaying tail. That is inaudible by any measure, but it is not bit-identical, so it belongs behind the voicing freeze unless you decide it does not count.

---

## 9. MINOR — `_VolumRefreshChannels` writes the factory per-amp slot while a custom amp is focused

**WHERE:** `VoLumSceneRig.inc.cpp:40-76`; callers `NeuralAmpModeler.cpp:519`, `Unserialization.cpp:744`

**MECHANISM:** The function has no `mVolumCustomMainIdx` guard, and its clamp paths write the *factory* array directly rather than going through `_VolumActiveScene()`:

```45:50:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  if (mVolumSpeakerIdx < 0 || mVolumSpeakerIdx >= 4)
  {
    mVolumSpeakerIdx = std::clamp(mVolumSpeakerIdx, 0, 3);
    mVolumAmpSettings[mVolumAmpIdx].speakerIdx = mVolumSpeakerIdx;
    mVolumSettingsDirty = true;
  }
```

It then rescans `kAmps[mVolumAmpIdx]`'s factory rig folder and, if `mVolumChannelIdx` doesn't fit that folder's channel count, resets it to 0 (lines 64-69). Called at `Unserialization.cpp:744` — where `mVolumCustomMainIdx` may already be ≥ 0 from the constructor's settings restore, before the chunk's own `_VolumSelectCustomAmp` at line 774.

**TRIGGER:** Load a DAW project while the machine-global settings had a custom amp focused, where the custom amp has more gain stages than the underlying factory rig folder.

**IMPACT:** The custom lane's channel selection resets to the first stage; a factory amp's stored `speakerIdx`/`channelIdx` gets stomped by a clamp that belonged to a different amp.

**CONFIDENCE:** likely

**FIX SKETCH:** Early-return `if (mVolumCustomMainIdx >= 0) return;` at the top of `_VolumRefreshChannels`, matching the branch `_VolumApplyRecalledPreset` already has (and which `test_volum_ui_regressions.cpp:1055-1079` pins). Route the two clamp write-backs through `_VolumActiveScene()` instead of `mVolumAmpSettings[mVolumAmpIdx]`.

---

## 10. MINOR — The dual-amp pan heuristic fires during `OnParamReset` and sticks on legacy chunk paths

**WHERE:** `NeuralAmpModeler.cpp:1244-1272`; `Unserialization.cpp:71`, `607-608`, `742`

**MECHANISM:** `_UnserializeApplyConfig` sets every param, then calls `OnParamReset(kPresetRecall)` (line 71), which iPlug2 implements as a loop over all params calling `OnParamChange(i)` (`IPlugEditorDelegate.h:130-133`). At `i == kDualAmpActive` the uncustomised-pan heuristic runs mid-restore and force-writes `kMainAmpPan = -1.0`, `kSupportAmpPan = 1.0`, `mSupportPolarityInvert = true`, plus `mVolumAmpSettings[mVolumAmpIdx].supportPolarityInvert = true` — into whatever amp slot happens to be current, which at that moment is the *machine-global* last amp, not the project's.

On the current chunk path this converges: `_VolumRestoreFromSettings(mVolumAmpIdx)` at `Unserialization.cpp:742` runs afterwards and `_VolumApplyAmpSettings` re-sets both pans (`VoLumSettingsScene.inc.cpp:219, 231`) and the polarity atomic (line 232) from the restored scene. So this is not the state-loss bug it first looks like.

Where it does stick: the per-amp tail block is gated on `ChunkUses0700/0600/0500/0715SerializedConfig(version)` (line 607-608), which is false for chunks written by 0.7.9-0.7.14, and `_UnserializeStateWithUnknownVersion` never calls `_VolumRestoreFromSettings` at all. On those paths nothing undoes the forced hard L/R.

**TRIGGER:** Load a project saved by NAM 0.7.9-0.7.14, or one whose header doesn't match, with dual-amp active and both pans centered in the machine settings.

**IMPACT:** Both lanes jump to hard L/R; a deliberately centered mono stack is lost.

**CONFIDENCE:** likely

**FIX SKETCH:** Add a `mVolumUnserializeInProgress` guard around `_UnserializeApplyConfig` and skip the heuristic when it is set — the restore path already has its own equivalent migration at `VoLumSettingsScene.inc.cpp:238-242`, so nothing is lost. **This can alter sound** for the degenerate centered+inverted case, which is the case the heuristic exists to rescue; keep the `_VolumApplyAmpSettings` copy so behaviour is unchanged on the paths that matter.

---

## 11. MINOR — `mNewModelLoadedInDSP` and `mModelCleared` are only cleared when an editor exists

**WHERE:** `NeuralAmpModeler.cpp:990-1008`

**MECHANISM:**

```990:1000:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (mNewModelLoadedInDSP)
  {
    mVolumLastLoadedFile =
      volum::content::PathToUtf8(volum::content::PathFromUtf8(mNAMPaths.live.Get()).filename());
    mVolumMainLoadError.clear();
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
```

The flag reset sits *inside* the `GetUI()` guard. With no editor it stays set forever, so every idle tick re-parses a path and allocates a `std::string`. Same shape for `mModelCleared` at 1001-1008. (Upstream NAM has this pattern too.)

**IMPACT:** A small permanent allocation per idle tick per editorless instance. No functional error — nothing else reads the flags.

**CONFIDENCE:** certain

**FIX SKETCH:** Move the two flag resets outside the `GetUI()` guard.

---

## 12. MINOR — `ResamplingNAM::process` throws from the audio thread on an oversized block

**WHERE:** `NeuralAmpModeler.h:190-194`

**MECHANISM:**

```190:194:NeuralAmpModeler/NeuralAmpModeler.h
  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");
```

`mMaxExternalBlockSize` comes from `Reset(sampleRate, GetBlockSize())`. A host that calls `ProcessBlock` with `nFrames > GetBlockSize()` without an intervening `OnReset` throws out of the render callback, unhandled. The same premise underpins the `assert`s at `NeuralAmpModeler.cpp:607, 643, 645` and the `mDualMainLaneBuffer.resize(numFrames)` at 609, which would reallocate on the audio thread in that case.

**TRIGGER:** A host that varies block size without re-preparing (some AU offline-render paths, some hosts' freeze/bounce).

**IMPACT:** Immediate host crash rather than a graceful degradation. Upstream NAM has the same hazard, so it is not a VoLum regression.

**CONFIDENCE:** speculative (depends on host behaviour); certain that nothing catches it

**FIX SKETCH:** Clamp in `ProcessBlock`: process in `GetBlockSize()`-sized sub-blocks, or bail to `_FallbackDSP` when `nFrames > GetBlockSize()`. No sound change for conforming hosts.

---

## NITs (one line each)

- `NeuralAmpModeler.cpp:2203-2213` and `1005` — `GetControlWithTag(...)` results are `static_cast` and dereferenced with no null check, unlike `_VolumRefreshLatencyReport` (2283) which checks; a layout variant that omits `kCtrlTagSettingsBox` becomes a crash.
- `NeuralAmpModeler.h:569` — "nullptr until UI is attached" is inaccurate: the six cached control pointers are never reset in `OnUIClose` (`NeuralAmpModeler.cpp:1204-1211`), so they dangle after a close; safe today only because `_UpdateVoLumLayout`'s `if (pGfx)` happens to gate every read.
- `NeuralAmpModeler.h:861-865` — stale `mStagingMutex` contract: the worker-queue drain *does* take it (`VoLumLoader.inc.cpp:181, 209, 238`), contradicting the comment (see finding 4).
- `config.h:28` — `PLUG_DOES_STATE_CHUNKS 0` is dead in this iPlug2 revision (zero references outside `config.h`) while the class overrides `SerializeState`/`UnserializeState`; misleading, and AU/VST3 both use chunks regardless.
- `NeuralAmpModeler.h:136-145` — `GetNAMSampleRate` is a non-`inline` free function defined in a header; links today only because exactly one TU includes it.
- `NeuralAmpModeler.cpp:1418-1439` — `IsPostBlockParam` omits all nine tremolo params and `kDelaySync`/`kDelayDivision`, so POST-lock "(unsaved)" chrome lags a tick until `OnIdle`'s poll catches up (cosmetic only).
- `NeuralAmpModeler.h:908` — `mNAMParams` is dead legacy state.
- `NeuralAmpModeler.cpp:1655-1664, 1833-1841` — `_AllocateIOPointers`/`_DeallocateIOPointers` throw after a `new`/`delete` that cannot fail (`new` throws, and the post-`delete` null checks are unreachable); upstream noise, but `_AllocateIOPointers` is reachable from `_PrepareBuffers` on the audio thread.
- `NeuralAmpModeler.cpp:1229-1231` — `mToneStack->SetParam(...)` recomputes filter coefficients from the param-change thread with no synchronization against the audio thread's `Process`; upstream NAM behaviour, torn coefficients at worst.
- `VoLumChunkCodec.h:23` — `ClampChunkSelection` bounds `channelIdx` below but not above; harmless because every consumer re-checks against `mVolumChannelFiles.size()`, but the asymmetry with `ampIdx`/`speakerIdx` invites a future OOB.

---

## Verified clean

- **Version consistency.** `config.h:3-4` (`0x00010201` / `"1.2.1"`), `resources/main.rc:224-225, 240-241` (`1,2,1,0` / `"1.2.1"`), `installer/VoLum.iss:8-9` (`1.2.1`), and all ten `resources/*.plist` files (`CFBundleShortVersionString`, `CFBundleVersion` = `1.2.1`; AU `AudioUnit Version` = `0x00010201`) all agree. The 1.2.1 stale-`main.rc` bug is fixed.
- **`ampIdx` bounds.** `GetVoLumChunkSelection` clamps internally (`VoLumChunkCodec.h:488`), and `VoLumChunkSelection` has member initializers (12-17), so a truncated read leaves defaults rather than indeterminate values. `mVolumAmpSettings[mVolumAmpIdx]` and `kAmps[mVolumAmpIdx]` are safe.
- **`CommitStagedPathOnApply` does not clobber a live path** when the staged one is empty (`VoLumDspStagingWdl.h:28`), so `_ResetModelAndIR`'s rebuild-without-staging-a-path is fine.
- **Expensive work is outside `mStagingMutex`.** `_StageModel` (1962-1998) and `_StageIR` (2050-2093) both construct fully before locking; hold times are a pointer move plus a short string copy, so the audio thread's blocking `lock_guard` is not a practical priority-inversion risk.
- **`mPrePitchMutex` honours its documented contract** (`NeuralAmpModeler.h:802-805`): `VoLumProcessBlock.inc.cpp:28` uses `std::try_to_lock`.
- **`_VolumStepDeferredIrSwaps` honours "Audio thread, mStagingMutex held"** (`NeuralAmpModeler.h:289`) — called at line 1686 inside the lock.
- **DSP caches are initialized before first read.** `_InitToneStack()` runs first in the constructor (line 319), and `_VolumRestoreFromSettings` → `_VolumApplyDspCaches` (line 516) sets `mInputGain`, `mOutputGain`, `mSupportOutputGain` before `mVolumInitComplete = true` (522) and before any `OnReset`/`ProcessBlock`.
- **Dual-amp scratch buffers** are pre-sized in `OnReset` (773-776) to `maxBlockSize`, matching the capacity invariant the `assert`s at 607/643/645 document.

---

# PAIRED STATE INVENTORY

Every set of members that must move together, and whether every write site honours it.

| # | Members that must move together | Honoured? | Detail |
|---|---|---|---|
| 1 | `mSupportIR` / `mStagedSupportIR` ↔ construction sample rate | **NO** | `_ResetModelAndIR` (1851-1903) rebuilds only `mIR`/`mStagedIR`. Finding 5. |
| 2 | `mVolumCustomMainIdx` ↔ `mVolumCustomMainSlot` ↔ `mVolumCustomMainChannel` | **NO** | `mVolumCustomMainIdx` is set headlessly (`_VolumSelectCustomAmp:317`); the other two are only written in editor-gated `_VolumApplyUiSyncPlan:440-441` and in the orphaned-IR recovery `_VolumFallbackToAvailableCab:914-915`. Finding 6. |
| 3 | `mNAMPaths.live` / `mIRPaths.live` / `mSupportIRPaths.live` ↔ `mModel` / `mIR` / `mSupportIR` | **Yes, but unsynchronized** | Always committed together in `_ApplyDSPStaging` under the mutex; the *reads* at 869, 993, 1039, 1040 take no lock. Finding 4. |
| 4 | `mVolumSpeakerIdx` ↔ `mVolumAmpSettings[…].speakerIdx`, `mVolumChannelIdx` ↔ `…channelIdx` | **Partly** | Paired in `_VolumRefreshChannels:45-69`, `_VolumApplyUiSyncPlan:442-443`, `_VolumApplyAmpSettings:199-200`, `_VolumFallbackToAvailableCab:916-917`. But `_VolumRefreshChannels` writes `mVolumAmpSettings[mVolumAmpIdx]` even when a custom amp owns the scene, so it can update the wrong owner. Finding 9. |
| 5 | `mVolumCustomSupportIdx` ↔ `mVolumCustomSupportSlot` ↔ `mVolumCustomSupportChannel` ↔ `scene.supportCustomSlot/Channel` | **Yes** | Six write sites all move the trio together: `VoLumAmpMenus.inc.cpp:242-247, 364-365`; `VoLumSceneRig.inc.cpp:432-433, 852-855`; `VoLumSettingsScene.inc.cpp:271-275`; `VoLumSettingsLocks.inc.cpp:132-133`. This is the pattern MAIN should copy. |
| 6 | `mSupportPolarityInvert` (atomic) ↔ `mVolumAmpSettings[…].supportPolarityInvert` | **Yes** | Written at `NeuralAmpModeler.cpp:1256-1257`, `VoLumLayoutBuild.inc.cpp:415`, restored at `VoLumSettingsScene.inc.cpp:232`, captured at `VoLumSettingsLocks.inc.cpp:125`. The stray write at 1257 during deserialize targets the wrong slot but is overwritten by the chunk read. |
| 7 | `mVolumChannelFiles` ↔ `mVolumChannelLabels` (equal size, index-compatible) | **Yes** | Cleared and filled in lockstep, `VoLumSceneRig.inc.cpp:56-62`. `mVolumChannelIdx` re-clamped against `.size()` at 64-69. |
| 8 | `mVolumSupportChannelFiles` ↔ `mVolumSupportChannelLabels` ↔ `kSupportChannelIdx` | **Yes (logically); NO (thread-safety)** | Lockstep at `VoLumAmpMenus.inc.cpp:351-352, 377-381`, param clamped at 383-389. But reachable from the audio thread via `OnParamChange`. Finding 1. |
| 9 | `mVolumPreCaptureFiles` ↔ `mVolumPreCaptureLabels` ↔ `mVolumPreCaptureShortLabels` ↔ `mVolumPreCaptureGroups` | **Yes** | All four cleared and rebuilt together in `_VolumRefreshPrePedalCaptures` (`VoLumSceneRig.inc.cpp:78+`). |
| 10 | `mVolumActivePresetId` ↔ `mVolumRecalledSnapshot` ↔ `mVolumHasRecalledSnapshot` ↔ the two per-owner maps | **Yes** | Moved together at `NeuralAmpModeler.cpp:1192-1197`, `Unserialization.cpp:788-791`, and via `_VolumRememberActivePreset`/`_VolumForgetActivePreset`. |
| 11 | `mVolumPreLocked` ↔ `mVolumLiveLockedPre`; `mVolumPostLocked` ↔ `mVolumLiveLockedPost` | **Yes** | Serialized as a unit (`PutPrePostLockFlags` + `PutPrePostLockSnapshots`, 1046-1050) with size detection keyed off the flags on read (`Unserialization.cpp:649-665`). |
| 12 | `mVolumPreLocked/PostLocked` ↔ `mVolumPreLockUiDirty/PostLockUiDirty` | **Yes (eventually)** | `_VolumRefreshPrePostLockChrome` misses tremolo and delay-sync params (`IsPostBlockParam`, 1418-1439), but `OnIdle:905-922` re-derives both flags each tick. Cosmetic lag only. |
| 13 | `mShouldRemoveX` flags ↔ `mX` / `mStagedX` pointers | **Yes** | Every flag is consumed and cleared in the same locked block that nulls its pointer (`_ApplyDSPStaging:1688-1725`). |
| 14 | `mVolumDeferredRemove/ApplyIR` ↔ their `…Blocks` counters ↔ `mVolumIrShapingPushPending[lane]` | **Yes** | `_VolumStepDeferredIrSwaps:1803-1823` always writes flag and counter together; `_VolumFlushDeferredIrShaping:1785-1798` only lands shaping once both deferral flags for that lane are clear. |
| 15 | `mMasterSafetyHoldSamples` (plain `int`) ↔ `mMasterSafetyEngaged` (atomic) | **Yes** | Both audio-thread-only, updated in the same block (`ProcessBlock:706-715`), reset together in `OnReset:738-739`. |
| 16 | `mVolumLastLatencyReport` ↔ the Settings control's displayed report | **Yes (logically); NO (thread-safety)** | `_VolumRefreshLatencyReport:2277-2284` always updates both, but is reachable from the audio thread. Finding 2. |
| 17 | `mVolumLastLoadedFile` ↔ `mNewModelLoadedInDSP`; `mVolumLastLoadedSupportFile` ↔ support load completion | **Partly** | `mNewModelLoadedInDSP` is cleared only under `GetUI()` (finding 11); `mVolumLastLoadedSupportFile` is set at *request* time in `_VolumRequestSupportModelLoad`, so the footer can name a support file that failed to load. |
| 18 | `mVolumDspCache` ↔ `mVolumDspCacheOrder` (LRU map/order) | **Yes** | Insert and evict move together in `VoLumLoader.inc.cpp`; bounded by `kVolumDspCacheMaxEntries`. |
| 19 | `mVolumCustomMainIdx` ↔ `_VolumActiveScene()` target ↔ `_VolumActiveOwnerKey()` | **Yes** | `_VolumSelectCustomAmp:317-323` sets the index, syncs the preset owner, and applies the matching scene in that order. |
| 20 | `mInputArray`/`mOutputArray` ↔ `mInputPointers`/`mOutputPointers` | **Yes** | `_PrepareBuffers:2114-2146` re-points after any resize; `_AllocateIOPointers` refuses to double-allocate. |
| 21 | `mVolumAmpIdx` ↔ `mVolumSpeakerIdx` ↔ `mVolumChannelIdx` as one selection triple | **Yes** | Serialized and read as a unit via `PutCurrentVoLumChunkState`/`GetVoLumChunkSelection`, with `ClampChunkSelection` applied on both sides. |
| 22 | `mVolumEffectSettings.delayMode/reverbMode` ↔ the per-mode snapshot arrays | **Yes** | Guarded against re-entrant restore cascades by `mVolumReverbRestoreInProgress`, `mVolumPostRestoreInProgress`, `mVolumPreRestoreInProgress`, `mVolumTremoloRestoreInProgress`; the `kReverbSubMode` no-op at 1334-1341 is deliberate and correctly documented. |
| 23 | `customScenes[id]` (process-global) ↔ per-instance live scene | **NO** | One shared map entry per custom amp id, written from every instance's idle tick. Finding 7. |