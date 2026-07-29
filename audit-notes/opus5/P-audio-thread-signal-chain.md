# VoLum 1.2.1 — Audio Thread / Signal Chain / DSP Staging Audit

Read-only. No files modified.

**Headline:** the audio thread is not RT-safe. `ProcessBlock` → `_ApplyDSPStaging` → `_VolumDrainLoaderResults` performs file I/O (the diagnostic log the rule forbids), host-notification calls, UI-control mutation, NAM model construction/prewarm, and NAM model destruction. The 1.2.1 cab transaction itself is largely correct, but the per-IR trim/cuts do **not** land with their IR.

**Test coverage note that applies to nearly every finding below:** every test in `NeuralAmpModeler/tests/` is a doctest over *pure headers* or a source-string regex (`test_volum_ui_regressions.cpp`). No test constructs a `NeuralAmpModeler`, runs `ProcessBlock`, or exercises a second thread. `test_volum_dsp_staging.cpp` tests `StepDeferredIrSwap` / `IrConvolutionActive` in isolation and never touches `_ApplyDSPStaging`, the member atomics, or the ordering between them. So no existing test can catch an RT-safety, threading, or member-wiring defect, only pure-function math.

---

## 1. BLOCKER — The diagnostic log is written from the audio thread

**WHERE:** `NeuralAmpModeler/VoLumLoader.inc.cpp:175, 185, 203, 212, 232` (reached from `NeuralAmpModeler.cpp:577` → `:1669`)

**MECHANISM:** `_VolumDrainLoaderResults()` is called only from `_ApplyDSPStaging()`, which is called only from `ProcessBlock`:

```577:577:NeuralAmpModeler/NeuralAmpModeler.cpp
  _ApplyDSPStaging();
```

```1667:1669:NeuralAmpModeler/NeuralAmpModeler.cpp
void NeuralAmpModeler::_ApplyDSPStaging()
{
  _VolumDrainLoaderResults();
```

Inside the drain, on every successful or failed load:

```185:185:NeuralAmpModeler/VoLumLoader.inc.cpp
      VOLUM_LOG("model", "MAIN loaded " + result.path);
```

`VOLUM_LOG` → `volum::diag::Log::Write`, which takes a `std::mutex`, calls `std::filesystem::file_size`, possibly `remove` + `rename`, then opens an `std::ofstream` and writes:

```118:139:NeuralAmpModeler/VoLumDiagLog.h
  void Write(const char* category, const std::string& message)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    ...
    const std::string line = TimestampNow() + "  [" + (category ? category : "?") + "] " + message + "\n";
    ...
      std::ofstream out(mPath, std::ios::app | std::ios::binary);
```

The file's own header states the rule this violates: *"NEVER call it from the audio thread. Every entry opens a file."* The `"MAIN loaded " + result.path` concatenations also heap-allocate before the call.

**TRIGGER:** Any amp switch, cab switch, channel step, dual-amp support load, or PRE-pedal capture change — i.e. the single most common user action in the plugin. Guaranteed, not a race.

**IMPACT:** A file open (and occasionally a 512 KB rotate with `rename`) inside the audio callback. On a cold page-cache or a network/OneDrive-backed `%LOCALAPPDATA%`, this is tens of milliseconds → audible dropout/glitch on every rig change. Worst case (log rotation while another VoLum instance holds the mutex) the audio thread blocks on a lock held across file I/O.

**CONFIDENCE:** certain

**FIX SKETCH:** Latch the facts into preallocated fixed-size members (path index + an error enum + an atomic flag) and emit them from `OnIdle`, which already runs on the main thread and already handles `mNewModelLoadedInDSP`. No sound change.

---

## 2. BLOCKER — `SetLatency` (VST3 `restartComponent`) and UI label mutation from the audio thread

**WHERE:** `NeuralAmpModeler.cpp:1764-1779` → `:2218-2265` → `:2274-2286`

**MECHANISM:** `_ApplyDSPStaging` calls `_UpdateLatency()` whenever a model is applied or removed — on the audio thread:

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
```

`_UpdateLatency` then does two forbidden things:

```2256:2264:NeuralAmpModeler/NeuralAmpModeler.cpp
  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
  ...
  _VolumRefreshLatencyReport(/*force=*/true);
```

`SetLatency` in the VST3 wrapper calls the host back:

```244:257:iPlug2/IPlug/VST3/IPlugVST3.cpp
void IPlugVST3::SetLatency(int latency)
{
  IPlugProcessor::SetLatency(latency);
  if (componentHandler)
  {
    FUnknownPtr<IComponentHandler> handler(componentHandler);
    if (handler)
      handler->restartComponent(kLatencyChanged);
```

VST3 requires `restartComponent` from the controller/UI thread, never from `process()`. AU does the same via `InformListeners` (`IPlugAU.cpp:2007-2012`).

And `_VolumRefreshLatencyReport(true)` reaches straight into the control tree:

```2281:2285:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (auto* pGraphics = GetUI())
  {
    if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
      settings->As<NAMSettingsPageControl>()->SetCurrentLatency(report);
```

which builds `std::string`s and writes two label controls:

```741:751:NeuralAmpModeler/NeuralAmpModelerControls.h
  void SetCurrentLatency(const volum::LatencyReport& report)
  {
    ...
    const volum::LatencyLines lines = volum::FormatLatencyLines(report, kStandalone);
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.currentLatency))->SetStr(lines.headline.c_str());
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.latencyDetail))->SetStr(lines.detail.c_str());
  }
```

`_VolumRefreshLatencyReport` also writes the non-atomic `mVolumLastLatencyReport`, which `OnIdle` reads and writes (`:793`) — an unsynchronised read/write race on a multi-field struct.

**TRIGGER:** Any model load/unload while the plugin is processing. In a DAW: pick a different amp or cab and keep playing.

**IMPACT:** `restartComponent(kLatencyChanged)` from `process()` causes host-dependent behaviour ranging from a suspend/resume glitch to a re-entrant call into the plugin to a hard deadlock (Cubase and Live have both been reported to deadlock on this pattern). Independently, `SetStr` mutates a `WDL_String` while the UI thread may be drawing it → torn text or a use-after-free crash if the Settings page is open.

**CONFIDENCE:** certain (wrong-thread call), likely (host-visible symptom depends on host)

**FIX SKETCH:** Set an atomic `mLatencyDirty` in `_ApplyDSPStaging` and do the `_UpdateLatency()` call in `OnIdle` — `OnIdle` already has a `mNewModelLoadedInDSP` handler at `:990` that is the natural home. No sound change (PDC value is unchanged, only *when* it is reported).

---

## 3. BLOCKER — A NAM model is constructed/prewarmed on the audio thread

**WHERE:** `NeuralAmpModeler/VoLumLoader.inc.cpp:147-152`

**MECHANISM:**

```145:152:NeuralAmpModeler/VoLumLoader.inc.cpp
  for (auto& result : results)
  {
    if (result.model != nullptr && (result.sampleRate != GetSampleRate() || result.blockSize != GetBlockSize()))
    {
      result.model->Reset(GetSampleRate(), GetBlockSize());
      result.sampleRate = GetSampleRate();
      result.blockSize = GetBlockSize();
    }
```

`ResamplingNAM::Reset` resizes the resampler and then calls `ResetAndPrewarm` on the encapsulated model:

```215:226:NeuralAmpModeler/NeuralAmpModeler.h
  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    ...
    mResampler.Reset(sampleRate, maxBlockSize);
    ...
    mEncapsulated->ResetAndPrewarm(sampleRate, maxEncapsulatedBlockSize);
  };
```

That allocates the model's activation/state buffers and then *runs the network over its prewarm samples* — for a WaveNet this is thousands of samples of inference. On the audio thread.

**TRIGGER:** The block size or sample rate changes between the moment the load was queued (`request.sampleRate = GetSampleRate()` at queue time, `VoLumLoader.inc.cpp:47-48`) and the moment the audio thread drains it. Concretely: change the host/ASIO buffer size while a rig is loading, or switch amps during host `setupProcessing` on session load, or (standalone) change the device in Preferences mid-load.

**IMPACT:** A single audio callback that runs a full model prewarm — hundreds of milliseconds. Guaranteed dropout, likely an audible burst of silence or a repeated buffer.

**CONFIDENCE:** certain

**FIX SKETCH:** Don't re-`Reset` in the drain. If `result.sampleRate/blockSize` no longer match, drop the result and re-queue the load on the loader thread (set `mVolumNeedsLoad`). No sound change.

---

## 4. BLOCKER — NAM models are destroyed on the audio thread

**WHERE:** `NeuralAmpModeler/VoLumLoader.inc.cpp:137, 164-165, 170-177, 197-205, 226-234, 241`; plus `:182`, `:210`, `:239`

**MECHANISM:** `results` is a local deque of `VoLumLoadResult`, each owning a `std::unique_ptr<ResamplingNAM>`:

```136:143:NeuralAmpModeler/VoLumLoader.inc.cpp
{
  std::deque<VoLumLoadResult> results;
  {
    std::unique_lock<std::mutex> lock(mVolumLoaderMutex, std::try_to_lock);
    if (!lock.owns_lock())
      return;
    results.swap(mVolumLoadResults);
  }
```

Every `continue` path leaves `result.model` alive, so the model is freed when `results` is destroyed at the end of the function — on the audio thread:

```164:168:NeuralAmpModeler/VoLumLoader.inc.cpp
      if (superseded)
        continue;
      mVolumIsLoading.store(false);
      if (mVolumNeedsLoad.load())
        continue;
```

Same at `:176` (load error), `:198`/`:204` (support), `:227`/`:233` (PRE). Additionally, `mStagedModel = std::move(result.model)` at `:182` **destroys any previously staged model still sitting there**, also on the audio thread. `results` itself is a heap-allocating deque, and `volum::dsp_staging::StagePathOnSuccess(mNAMPaths, result.path.c_str())` at `:183` does a `WDL_String::Set` (heap allocation).

**TRIGGER:** Any superseded load. Click through three amps in the sidebar faster than they load, or toggle Dual Amp off while the support capture is in flight.

**IMPACT:** `free()` of a multi-megabyte model graph (Eigen matrices, per-layer buffers) inside the audio callback — an unbounded number of `operator delete` calls under the allocator lock. Dropout, and on Windows a potential multi-millisecond stall if the heap decommits pages.

**CONFIDENCE:** certain

**FIX SKETCH:** Move each unwanted `result.model` into a "graveyard" deque owned by the plugin and drain/destroy it in `OnIdle`. Same for the previous `mStagedModel` when overwriting. No sound change.

---

## 5. BLOCKER — `OnParamChange` runs on the audio thread in VST3 and does UI rebuilds, filesystem scans, and host callbacks

**WHERE:** `NeuralAmpModeler.cpp:1244-1294, 1342-1366`

**MECHANISM:** iPlug2 dispatches host parameter changes from inside `process()`:

```308:316:iPlug2/IPlug/VST3/IPlugVST3_ProcessorBase.cpp
              if (idx >= 0 && idx < mPlug.NParams())
              {
                mPlug.GetParam(idx)->SetNormalized(value);
                // In VST3 non distributed the same parameter value is also set via ...
                mPlug.OnParamChange(idx, kHost, offsetSamples);
```

VoLum's handler does main-thread-only work in several cases:

- `kDualAmpActive` (`:1244-1280`): `SendParameterValueFromDelegate` ×3 and `_UpdateVoLumLayout()` — a full show/hide pass over the control tree.
- `kSupportAmpIdx` / `kSupportSpeakerIdx` (`:1286-1294`): `_VolumRefreshSupportChannels()`, which does a **filesystem directory scan**:

```374:376:NeuralAmpModeler/VoLumAmpMenus.inc.cpp
    auto channels = volum::DiscoverChannels(volum::content::PathFromUtf8(mVolumRigsRoot),
                                            volum::kAmps[supportAmpIdx].folderName,
                                            volum::kSpeakerPrefixes[speakerIdx]);
```

  …then `_UpdateVoLumLayout()`.
- `kSupportChannelIdx` (`:1295-1304`): `GetUI()` + `stepper->SetChannels(...)` — writes a `std::vector<std::string>` into a live control.
- `kPreNam1Capture/Active`, `kPreNam2Capture/Active`, `kPrePitchActive/Mode/TransChar` (`:1342-1366`): `_UpdateLatency()` → finding #2's `restartComponent` + UI mutation.

**TRIGGER:** A DAW automation lane or a MIDI-learn/remote-control write on any of these params. Automating Dual Amp on/off across a section is the obvious one.

**IMPACT:** Directory enumeration and control-tree mutation inside the audio callback, racing the UI thread's draw pass. Dropout at best, crash (or the `restartComponent` deadlock) at worst.

**CONFIDENCE:** certain that the thread is wrong; likely that a user hits it (requires automating those params)

**FIX SKETCH:** In `OnParamChange`, only set the existing atomic request flags (`mVolumSupportNeedsLoad`, `mVolumPreNeedsLoad[]`) plus new `mVolumLayoutDirty` / `mLatencyDirty` atomics; move `_UpdateVoLumLayout`, `_VolumRefreshSupportChannels`, `SendParameterValueFromDelegate`, and `_UpdateLatency` into `OnIdle` / `OnParamChangeUI`. No sound change.

---

## 6. BLOCKER — A block larger than `GetBlockSize()` allocates, then throws out of `ProcessBlock`

**WHERE:** `NeuralAmpModeler.cpp:571` + `:2114-2139`; throw at `NeuralAmpModeler.h:190-194`

**MECHANISM:** `ProcessBlock` sizes its buffers from the incoming `nFrames`, not from a reserve:

```571:571:NeuralAmpModeler/NeuralAmpModeler.cpp
  _PrepareBuffers(numChannelsInternal, numFrames);
```

```2127:2138:NeuralAmpModeler/NeuralAmpModeler.cpp
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
```

`resize` past capacity allocates on the audio thread. Immediately after, the model rejects the block with an exception:

```190:195:NeuralAmpModeler/NeuralAmpModeler.h
  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");
```

`mMaxExternalBlockSize` is set to `GetBlockSize()` by `_StageModel` (`:1973`) and by the loader (`VoLumLoader.inc.cpp:310`). Nothing in `ProcessBlock` catches this, so it propagates out of the audio callback. It also leaks the FP environment: `std::feholdexcept` is entered at `:568` and `std::feupdateenv` at `:618` is never reached.

**TRIGGER:** A host that delivers a block larger than the last negotiated block size without an intervening `OnReset` — Reaper's "allow larger buffers" / anticipative FX, an offline render that raises the block size, or a VST3 host that calls `setupProcessing` with a smaller `maxSamplesPerBlock` than it later passes. Also reachable on the *first* block if a host processes before `OnReset`.

**IMPACT:** Exception through the audio callback → host-dependent hard crash or plugin unload. This is upstream NAM behaviour VoLum inherited, but it is still a shipping crash path.

**CONFIDENCE:** certain (the code path), likely (host behaviour required)

**FIX SKETCH:** Clamp: if `numFrames > GetBlockSize()`, pass the input through (or process in `GetBlockSize()`-sized sub-chunks) and skip the model. Do not change the reserve strategy — VoLum's own `VoLumPitch` already has the correct pattern (`kRealtimeBlockReserve = 8192`, pass dry beyond it; `VoLumPitchShifter.h:541, 603-609, 718-721`). No sound change in normal operation.

---

## 7. MAJOR — Delay, Reverb, Tremolo and the master safety stage run with denormals re-enabled

**WHERE:** `NeuralAmpModeler.cpp:566-569` vs `:617-618`

**MECHANISM:** The denormal-disabled window closes *before* the entire POST chain:

```566:569:NeuralAmpModeler/NeuralAmpModeler.cpp
  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();
```

```617:618:NeuralAmpModeler/NeuralAmpModeler.cpp
  // restore previous floating point state
  std::feupdateenv(&fe_state);
```

Everything after line 618 — the dual-amp merge, `_VolumProcessPostChain` (delay ring + feedback, reverb FDN tank, tremolo LFO), the metronome, the dual-amp peak scan, and the `SoftSafetyClip` pass — runs with the host's original MXCSR, i.e. denormals enabled. The delay feedback path and the reverb FDN are precisely the stages whose decaying tails produce denormals.

Upstream NAM restores the environment immediately before `_ProcessOutput` because upstream has nothing after it. VoLum added ~150 lines of stateful IIR/feedback DSP after the restore.

**TRIGGER:** Let a reverb with a long decay or a delay with high feedback ring out into silence (stop playing). Denormals accumulate in the tank/ring and never flush.

**IMPACT:** A 10–100× slowdown in the reverb/delay inner loops while tails decay — the classic "CPU spikes when I stop playing" symptom, and on a small buffer, dropouts.

**CONFIDENCE:** certain (the window is misplaced), likely (magnitude depends on how AudioDSPTools' Reverb/Delay handle sub-normals internally)

**FIX SKETCH:** Move `std::feupdateenv(&fe_state)` to just before `_UpdateMeters` at the end of `ProcessBlock`. **Sound impact:** flush-to-zero changes values below ~1e-38, which is ~-760 dBFS — mathematically not bit-identical, audibly identical. Flag for the release owner if bit-exact regression baselines exist, otherwise safe.

---

## 8. MAJOR — Per-IR trim and cuts do NOT travel with their IR (stated 1.2.1 invariant is violated)

**WHERE:** `NeuralAmpModeler.cpp:1785-1798` (flush) vs `:1750-1761` (promotion); `VoLumSceneRig.inc.cpp:563-572`

**MECHANISM:** The convolver swap is atomic on the audio thread — the staged IR is promoted in the same `mStagingMutex` critical section that clears the deferral:

```1750:1755:NeuralAmpModeler/NeuralAmpModeler.cpp
    if (mStagedIR != nullptr && !holdMainIr)
    {
      mIR = std::move(mStagedIR);
      mStagedIR = nullptr;
      volum::dsp_staging::CommitStagedPathOnApply(mIRPaths);
    }
```

But the IR's trim and cuts are held back on the *main* thread and only published on a later `OnIdle` tick:

```1785:1798:NeuralAmpModeler/NeuralAmpModeler.cpp
void NeuralAmpModeler::_VolumFlushDeferredIrShaping()
{
  const bool pending[2] = {
    mVolumDeferredRemoveIR.load(std::memory_order_relaxed) || mVolumDeferredApplyIR.load(std::memory_order_relaxed),
    ...};
  for (int lane = 0; lane < 2; ++lane)
  {
    if (!mVolumIrShapingPushPending[lane] || pending[lane])
      continue;
    mVolumIrShapingPushPending[lane] = false;
    _VolumPushIrShaping(lane == 1);
  }
}
```

`_VolumFlushDeferredIrShaping()` is called from `OnIdle` (`:794`), which runs on iPlug2's ~20 ms idle timer. So the sequence is: block N promotes the new IR and clears `mVolumDeferredApplyIR`; the *next* idle tick (0–20+ ms later) pushes that IR's trim/cuts. For that interval the new IR convolves with the **previous** lane shaping.

The comment at `VoLumSceneRig.inc.cpp:559-565` states the intent explicitly — *"its trim and cuts wait with it"* — and they do wait, but they do not arrive with it.

**TRIGGER:** Pick a custom IR on a lane that is currently on a baked cab (the normal way to enable a custom IR). The lane's old shaping is unity/no-cuts; a migrated IR's auto-normalized trim is up to +24 dB (`AutoNormalizeIrTrimDb`, `kIrTrimDbMax`). Also fires in reverse: pick a baked cab while a shaped IR is live.

**IMPACT:** Up to ~20 ms of the new IR at the wrong level (potentially 18–24 dB too quiet), followed by an audible level step — plus the wrong low/high cut for the same interval. Exactly the seam the 1.2.1 "one atomic transaction" work was supposed to eliminate, just moved from the convolver to the makeup gain. If `OnIdle` is throttled (some hosts idle the plugin only when the editor is open), the mismatch persists much longer.

**CONFIDENCE:** certain

**FIX SKETCH:** Stage the shaping alongside the IR instead of deferring it: add `mStagedIrTrimLin` / `mStagedIrLowCutHz` / `mStagedIrHighCutHz` (or a single `std::atomic<ShapingTriple>`-style struct published under `mStagingMutex`) and copy them into the live atomics in the same `if (mStagedIR != nullptr && !holdMainIr)` block. Delete `_VolumFlushDeferredIrShaping` and `mVolumIrShapingPushPending`. **Does not change the sound** — it changes only *when* the already-specified gain/cuts take effect, making it land on the intended block.

---

## 9. MAJOR — The new IR cut filters allocate on the audio thread, on their first engaged block

**WHERE:** `VoLumSceneRig.inc.cpp:758-783`; `AudioDSPTools/dsp/RecursiveLinearFilter.cpp:25-28, 73-93`; `AudioDSPTools/dsp/dsp.cpp:58-81`

**MECHANISM:** The comment above `_VolumApplyIrShaping` claims RT safety on the basis that `SetParams` doesn't allocate — which is true, but `Process` does:

```769:781:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  if (lowHz > 0.0)
  {
    auto& f = support ? mSupportIrLowCut : mIrLowCut;
    f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, lowHz));
    p = f.Process(p, numChannels, nFrames);
  }
```

```25:28:AudioDSPTools/dsp/RecursiveLinearFilter.cpp
DSP_SAMPLE** recursive_linear_filter::Base::Process(DSP_SAMPLE** inputs, const size_t numChannels,
                                                    const size_t numFrames)
{
  this->_PrepareBuffers(numChannels, numFrames);
```

`_PrepareBuffers` resizes `mOutputs`, resizes `mInputHistory`/`mOutputHistory`, and calls `_ResizePointers` → `_AllocateOutputPointers` → `new DSP_SAMPLE*[numChannels]` (`dsp.cpp:29-37`), which also **throws** `std::runtime_error` on failure. Unlike `mHighPass` / `mPreInputGain`, which get their first `Process` during the first audio block after `OnReset` (already a bad-but-quiet moment), `mIrLowCut` / `mIrHighCut` / `mSupportIrLowCut` / `mSupportIrHighCut` are only ever `Process`ed when a cut is non-zero. Nothing in `OnReset` or `_ResetModelAndIR` prepares them.

**TRIGGER:** Open the IR panel mid-performance and step the low-cut off OFF for the first time in the session (or recall the first preset whose IR carries a cut). First engaged block allocates four vectors + a `new[]` per filter, per lane.

**IMPACT:** An allocator call inside the audio callback at the exact moment the user is tweaking a control — a click or short dropout on the first engaged block, and an unhandled exception path if allocation fails.

**CONFIDENCE:** certain

**FIX SKETCH:** In `OnReset`, prime all four filters once off the audio thread — e.g. `SetParams(...)` then a single `Process` over a scratch buffer of `maxBlockSize`, then reset their history. No sound change (the filters' first *engaged* block is unaffected because history is zeroed either way).

---

## 10. MAJOR — A latched removal flag silently eats a freshly staged IR

**WHERE:** `NeuralAmpModeler.cpp:1703-1709` (consumed) vs `:1750-1755` (promotion); `VoLumSceneRig.inc.cpp:539` vs `:555`

**MECHANISM:** Inside one `mStagingMutex` critical section, removal is processed *before* promotion:

```1703:1709:NeuralAmpModeler/NeuralAmpModeler.cpp
    if (mShouldRemoveIR)
    {
      mIR = nullptr;
      mStagedIR = nullptr;
      volum::dsp_staging::ClearLiveAndStagedPath(mIRPaths);
      mShouldRemoveIR = false;
    }
```

So any `mShouldRemoveIR` still latched when a new IR is staged destroys that IR before it is ever promoted. Three writers set it from the main thread without any check for a pending stage:

- `NeuralAmpModeler.cpp:1624` — `case kMsgTagClearIR: mShouldRemoveIR = true;`
- `VoLumSceneRig.inc.cpp:690` — `_VolumClearIR(support, /*deferToCabSwap=*/false)`
- `VoLumSceneRig.inc.cpp:720` — the orphaned-id branch of `_VolumApplyActiveIr`
- `VoLumSceneRig.inc.cpp:889` — `_VolumFallbackToAvailableCab`

And `_VolumSelectIR` cancels the *deferred* removal only **after** staging, leaving a window:

```539:555:NeuralAmpModeler/VoLumSceneRig.inc.cpp
  const dsp::wav::LoadReturnCode loadRc = _StageIR(p, support);
  ...
  const int lane = support ? 1 : 0;
  (support ? mVolumDeferredRemoveSupportIR : mVolumDeferredRemoveIR).store(false);
```

If `_ApplyDSPStaging` runs between those two statements and the deferral fires (`_VolumStepDeferredIrSwaps` → `mShouldRemoveIR.store(true)` at `:1816`), the just-staged IR is discarded on the same block.

The comment at `:551-553` shows the author knew this hazard existed but fixed only the ordering *within* `_VolumSelectIR`, not the underlying "remove wins over stage" precedence.

**TRIGGER:** Most reliable: any path where the plugin is not processing when the flag is set (standalone before the audio stream opens, a suspended plugin instance, a host that loads state before activating). `_VolumApplyAmpSettings` calls `_VolumApplyActiveIr` for both lanes back-to-back (`VoLumSettingsScene.inc.cpp:303-304`) during preset/amp restore, then a subsequent restore or re-pick stages an IR — first block after activation consumes the removal and drops the staged IR. The `_VolumSelectIR` race window is also real, just narrow.

**IMPACT:** The IR chip shows the IR as selected, `kIRToggle` is on, the scene records its id — and no IR convolves. The user hears the raw/DIRECT amp with no cab, with no visible reason. Recovers only by re-picking the IR.

**CONFIDENCE:** likely

**FIX SKETCH:** In `_StageIR`, clear the target lane's `mShouldRemove*IR` and `mVolumDeferredRemove*IR` inside the same `mStagingMutex` critical section that assigns `stagedSlot` — a stage is by definition the newest intent. Reorder `_VolumSelectIR` so the deferral cancel precedes `_StageIR`. No sound change.

---

## 11. MAJOR — POST tails survive a host bypass

**WHERE:** `VoLumProcessBlock.inc.cpp:205-218`; `NeuralAmpModeler.cpp:766-768`

**MECHANISM:** The active→inactive edge detector only advances while `ProcessBlock` runs:

```210:218:NeuralAmpModeler/VoLumProcessBlock.inc.cpp
  if (mPostDelayWasActive && !processingPlan.runDelay)
    mDelay.Reset();
  if (mPostReverbWasActive && !processingPlan.runReverb)
    mReverb.Reset();
  if (mPostTremoloWasActive && !processingPlan.runTremolo)
    mTremolo.Reset();
  mPostDelayWasActive = processingPlan.runDelay;
```

When the host bypasses the plugin, iPlug2 passes buffers through and never calls `ProcessBlock`, so no edge is ever observed and no `Reset()` happens. `OnReset` — which does clear the flags and the effects (`:763-768`) — is not called on bypass either.

**TRIGGER:** Play into a long reverb / high-feedback delay, engage host bypass, wait, disengage bypass.

**IMPACT:** The exact artifact the `mPostDelayWasActive` mechanism exists to prevent: re-engaging replays a ghost of whatever was in the tank/ring before the bypass. The documented invariant ("re-engaging never replays a stale tail") holds for the in-plugin toggles and preset switches but not for host bypass.

**CONFIDENCE:** likely (depends on the iPlug2 bypass path not calling `ProcessBlock`; verified for the VST3 processor base)

**FIX SKETCH:** Override `OnActivate(bool)` / hook the bypass transition to set a `mPostNeedsClear` atomic, and clear `mDelay`/`mReverb`/`mTremolo` on the first `ProcessBlock` after un-bypass. **Changes what the user hears** — by removing the ghost, which is the intended behaviour, not a voicing change. Worth confirming with the release owner that a *deliberate* "bypass to freeze the tail" workflow isn't being relied on.

---

## 12. MAJOR — `mStagingMutex` is held across model reset and IR resampling while the audio thread blocks on it

**WHERE:** `NeuralAmpModeler.cpp:1851-1902` vs `:1679`

**MECHANISM:** `_ApplyDSPStaging` takes the mutex with a *blocking* `lock_guard` on the audio thread:

```1678:1679:NeuralAmpModeler/NeuralAmpModeler.cpp
  {
    std::lock_guard<std::mutex> lock(mStagingMutex);
```

`_ResetModelAndIR` (main thread, from `OnReset`) holds that same mutex across the most expensive operations in the plugin:

```1855:1864:NeuralAmpModeler/NeuralAmpModeler.cpp
  std::lock_guard<std::mutex> lock(mStagingMutex);

  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, maxBlockSize);
  }
```

— four model resets/prewarms (main, support, two PRE) plus, for IRs, a full convolver reconstruction:

```1888:1892:NeuralAmpModeler/NeuralAmpModeler.cpp
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
```

**TRIGGER:** Any host that calls `OnReset` (sample rate / block size / channel IO change) without first suspending processing. Some VST3 hosts and most standalone driver-change paths do exactly this.

**IMPACT:** The audio thread blocks for as long as four model prewarms take — hundreds of milliseconds of priority inversion. Silence or a stuck buffer.

**CONFIDENCE:** likely (depends on host suspend behaviour; the code is unconditionally exposed)

**FIX SKETCH:** Inside `_ResetModelAndIR`, take the mutex only to *move* the pointers into locals, do the reset/reconstruction unlocked, then re-take it to publish. Alternatively, since the documented contract is that only the legacy `_StageModel`/`_StageIR` entry points need this mutex, replace the audio-thread `lock_guard` at `:1679` with a `try_to_lock` and skip staging for one block on contention. No sound change.

---

## 13. MAJOR — `mNAMPaths.live` is read from the main thread without the staging mutex

**WHERE:** `NeuralAmpModeler.cpp:869, 992-993` vs `:1731, 1754, 1760`

**MECHANISM:** `_ApplyDSPStaging` reassigns the `WDL_String` under `mStagingMutex`:

```1731:1731:NeuralAmpModeler/NeuralAmpModeler.cpp
      volum::dsp_staging::CommitStagedPathOnApply(mNAMPaths);
```

`OnIdle` reads it with no lock at all, twice:

```869:869:NeuralAmpModeler/NeuralAmpModeler.cpp
        if (fileToLoad == mNAMPaths.live.Get() && !forceMainReload)
```

```992:993:NeuralAmpModeler/NeuralAmpModeler.cpp
    mVolumLastLoadedFile =
      volum::content::PathToUtf8(volum::content::PathFromUtf8(mNAMPaths.live.Get()).filename());
```

`WDL_String::operator=` frees the old buffer and allocates a new one. `Get()` hands out a raw `const char*` into that buffer.

**TRIGGER:** A model finishes loading (audio thread commits the path) during the same instant `OnIdle` is comparing or copying it. Rig switching while the UI is idling.

**IMPACT:** Read of freed memory — garbage in the footer filename, or a crash. Also causes the wrong same-path short-circuit decision at `:869`, which can skip a needed reload (rig appears not to change).

**CONFIDENCE:** likely

**FIX SKETCH:** Guard both reads with `std::lock_guard<std::mutex> lock(mStagingMutex)` and copy into a `std::string` before use. No sound change.

---

## 14. MINOR — The deferred wait keys on "any staged main model", not the capture it is waiting for

**WHERE:** `NeuralAmpModeler.cpp:1812-1823`

**MECHANISM:**

```1812:1818:NeuralAmpModeler/NeuralAmpModeler.cpp
  const bool mainStaged = mStagedModel != nullptr;
  const bool supportStaged = mStagedSupportModel != nullptr;

  if (step(mVolumDeferredRemoveIR, mVolumDeferredRemoveIrBlocks, mainStaged).fire)
    mShouldRemoveIR.store(true, std::memory_order_relaxed);
```

Any staged main model releases the wait, including one queued by an unrelated action, and including a model that was *already* staged when the deferral was armed (the plugin was not processing). The counters themselves are safe — `waited` is stored back to `0` on every block where the deferral is not pending (`StepDeferredIrSwap` returns a default-constructed `out`), so an unrelated event cannot leave a large stale count and cause a premature fire.

**TRIGGER:** Arm a cab-source switch, then immediately switch amps from the sidebar; or arm one while the plugin is suspended with a model already staged.

**IMPACT:** The IR change lands on the block a *different* capture goes live. Usually still coherent (both are cab changes), but the "one atomic transaction" guarantee is nominal rather than actual.

**CONFIDENCE:** likely (mechanism certain, audibility low)

**FIX SKETCH:** Stamp a monotonically increasing "capture generation" counter when arming, and require `mStagedModelGeneration >= armedGeneration` rather than merely non-null. No sound change.

---

## 15. MINOR — Blocking `mVolumLoaderMutex` acquisitions on the audio thread

**WHERE:** `VoLumLoader.inc.cpp:158, 192, 222`

The initial `results.swap` correctly uses `std::try_to_lock` (`:139-141`), but the per-result bookkeeping then re-acquires the same mutex with a blocking `std::lock_guard`. The loader thread holds it only briefly, but `_VolumStopLoader` (`:22-30`) and `_VolumQueueMainModelLoad` (`:50-60`) also hold it, so the audio thread can block on a non-RT thread. **FIX:** hoist all the `mVolumLoadingXPath` bookkeeping into the single `try_to_lock` scope. No sound change.

---

## 16. MINOR — IR shaping is published as three independent relaxed atomics

**WHERE:** `VoLumSceneRig.inc.cpp:740-751`; read at `:761, 768, 775`

`_VolumPushIrShaping` stores trim, low-cut and high-cut separately with `memory_order_relaxed`, so the audio thread can observe a new trim with the old cuts (or vice versa) for one block. **FIX:** pack the three doubles into a POD and publish via a single `std::atomic<Shaping>` (it will be lock-free at 24 bytes on neither platform — use a seqlock or a double-buffer + atomic index instead). No sound change beyond removing a one-block inconsistency.

---

## NITs (one line each)

- `VoLumSceneRig.inc.cpp:758-783` — the four IR cut filters are never `Reset()`; stale history after a cut is disabled and later re-enabled produces a small decaying DC step. Fix is a `Reset` on the disabled→enabled edge; no voicing change.
- `NeuralAmpModeler.cpp:701` — the master-safety telemetry threshold `1.4` duplicates `constexpr double knee = 1.4` inside `SoftSafetyClip` (`VoLumMasterSafety.h:20`); the two can drift apart silently. Expose the knee as a named constant.
- `NeuralAmpModeler.cpp:2318-2323` — `mOutputSenderR` stops being fed when Dual Amp is switched off, so the right OUT meter freezes at its last value instead of falling to silence.
- `NeuralAmpModeler.cpp:676-683` — the dual-amp "output hot" detector is a second full O(channels × frames) pass over the output purely for a footer message; it can be folded into the master-safety loop directly below it.
- `NeuralAmpModeler.cpp:576` — `mTunerDSP.Process(...)` runs unconditionally every block even when the tuner is inactive; guard on `mTunerDSP.IsActive()`.
- `VoLumPitchShifter.h:608-609` — when a block exceeds `mMaxBlock` the engine returns dry but does not advance `mDryRing`, so the dry path is permanently `nFrames` out of alignment with the reported PDC after one such block.
- `NeuralAmpModeler.cpp:738-739` — `mMasterSafetyHoldSamples` (non-atomic) is written by both `OnReset` and `ProcessBlock`; benign in practice but it is an unsynchronised int.
- `VoLumProcessBlock.inc.cpp:61` — `mPreModel[slot]->process(...)` output is not passed through `volum::ScrubNonFiniteInPlace`, unlike the main and support models; the downstream `recursive_linear_filter` per-sample `isnan` guard incidentally contains it, so this is a documentation/consistency gap rather than a live NaN leak.
- `NeuralAmpModeler.cpp:1908-1911, 1932-1935` — `ComputeInputGainDb` / `ComputeOutputModeGainDb` results are unclamped before `DBToAmp`, so a `.nam` whose `input_level_dbu` / `loudness` / `output_level` metadata is absurd yields an infinite gain multiplier. Worth confirming that `VoLumCustomNamImport` range-checks those fields on import, since the POST delay/reverb state would then be permanently poisoned (the NaN guard only scrubs the *model* output, upstream of the gain).

---

## Invariants: verification results

| Documented invariant | Holds? |
|---|---|
| POST effects `Reset()` on every active→inactive edge, tracked by `mPostDelayWasActive` / `mPostReverbWasActive` | Holds for in-plugin toggles, preset switches, and missing-model (`VoLumProcessBlock.inc.cpp:210-218`, `:105-111`). **Fails for host bypass** — finding #11. |
| NAM output scrubbed via `ScrubNonFiniteInPlace` after every `process`, POST reset on non-finite | Holds for main (`VoLumProcessBlock.inc.cpp:94-100`) and support (`:169-175`), and correctly includes `mTremolo`. Not applied to PRE NAM output (NIT). Note the guard scrubs the *output* only — a NaN reaching the model's recurrent state is not recovered from. |
| `SoftSafetyClip` maps NaN/±Inf to 0, final-bus contract | Holds. `VoLumMasterSafety.h:15-18`, applied to every output channel at `NeuralAmpModeler.cpp:694-705`. |
| Legacy `_StageModel`/`_StageIR` serialized against the audio-thread move via `mStagingMutex`; VoLum worker-queue drain lock-free | Mutex serialization holds (`:1979`, `:2072`, `:1679`), but see #12 for what else that mutex covers. **The worker-queue drain is NOT lock-free** — it takes `mVolumLoaderMutex` blockingly (#15), writes a log file (#1), allocates, and frees models (#3, #4). |
| Dual-amp scratch pre-reserved in `OnReset`, no `ProcessBlock` allocation | Holds. `OnReset:772-776` `assign`s to `maxBlockSize`; asserts at `:607`, `:643-646`, `VoLumProcessBlock.inc.cpp:148`. Breaks only in the oversized-block case (#6), where the assert fires in debug and `resize` allocates in release. |
| PRE Pitch reserves practical block growth, never reconfigures from `Process`, oversized block passes dry | Holds. `VoLumPitchShifter.h:541` (`kRealtimeBlockReserve = 8192`), `:544-556` (Configure off-thread), `:603-609` + `:718-721` (pass dry). This is the correct pattern the rest of the chain should adopt. |