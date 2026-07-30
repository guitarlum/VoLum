## Summary

This subsystem has good intent around asynchronous construction, staged ownership, NaN scrubbing, and bounded IR/cab coordination, but the final handoff still performs substantial non-realtime work inside `ProcessBlock`. The highest-severity defect is an unsynchronized cross-thread ownership/read scheme for `mModel` and the live path strings: the audio thread can replace and destroy them while the UI or state-serialization thread dereferences them, allowing use-after-free or corrupted saved state. I also found a fully broken user path: custom SUPPORT amps are loaded successfully but are then excluded from the processing plan because their factory support index is deliberately `-1`.

All findings below are proved from the code paths shown. Where the resulting crash depends on host-provided pointers or race timing, that nondeterminism is stated explicitly.

## Findings

### F-P1-1: BLOCKER — Model swaps race UI reads and state serialization

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1727-1759`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1011-1045`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2188-2213`

**Evidence:**

```cpp
// Audio thread
if (mStagedModel != nullptr)
{
  mModel = std::move(mStagedModel);
  mStagedModel = nullptr;
  volum::dsp_staging::CommitStagedPathOnApply(mNAMPaths);
  mNewModelLoadedInDSP = true;
}
```

```cpp
// Host/UI thread, no mStagingMutex and no owned snapshot
chunk.PutStr(mNAMPaths.live.Get());
chunk.PutStr(mIRPaths.live.Get());
```

```cpp
// UI thread, repeatedly dereferences the live unique_ptr with no lock
if (mModel == nullptr)
  return;
modelInfo.sampleRate.value = mModel->GetEncapsulatedSampleRate();
modelInfo.inputCalibrationLevel.known = mModel->HasInputLevel();
modelInfo.outputCalibrationLevel.known = mModel->HasOutputLevel();
```

**Mechanism:** `_ApplyDSPStaging()` replaces `mModel` and the live `WDL_String` paths on the audio thread. Assignment to `mModel` destroys the previous graph immediately. `OnIdle`/`OnUIOpen` call `_UpdateControlsFromModel()` and host state save calls `SerializeState()` without taking `mStagingMutex` or acquiring an owning snapshot. The atomics `mNewModelLoadedInDSP` and `mModelCleared` signal events but do not protect object lifetime or later swaps. A second model swap can therefore destroy an object while the UI is inside one of its getters; path mutation can race `PutStr()` and yield invalid string storage or mismatched model/path state.

**Trigger:** Keep the editor/settings page open and switch captures quickly enough for a second async result to land while `OnIdle` handles the first; or save the DAW project while an async model/IR result lands.

**Impact:** Host/plugin crash from use-after-free, undefined behavior from the `unique_ptr` data race, or a corrupt/internally inconsistent state chunk that recalls the wrong path.

**Fix sketch:** Publish an immutable model-metadata/path snapshot separately from the audio-owned DSP graph. UI and serialization should read an owned snapshot, not `mModel`. Use a coherent handoff protocol for live paths; do not solve this by making the audio thread wait for the UI.

**Proposed regression test:** `test_model_swap_ui_and_serialize_tsan` — repeatedly land model/IR swaps while invoking `OnIdle`, `OnUIOpen`, and `SerializeState`; assert ThreadSanitizer reports no race and every chunk's path identifies the same generation as its metadata.

### F-P1-2: MAJOR — Async handoff blocks, allocates, destroys graphs, and writes files on the audio thread

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:558-577`, `NeuralAmpModeler/NeuralAmpModeler.cpp:1667-1761`, `NeuralAmpModeler/VoLumLoader.inc.cpp:135-240`, `NeuralAmpModeler/VoLumDiagLog.h:118-139`

**Evidence:**

```cpp
// ProcessBlock
_PrepareBuffers(numChannelsInternal, numFrames);
_ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
mTunerDSP.Process(mInputPointers[0], nFrames);
_ApplyDSPStaging();
```

```cpp
// Called by _ApplyDSPStaging on the audio thread
if (result.model != nullptr && (result.sampleRate != GetSampleRate() || result.blockSize != GetBlockSize()))
  result.model->Reset(GetSampleRate(), GetBlockSize());
...
std::lock_guard<std::mutex> lock(mStagingMutex);
...
VOLUM_LOG("model", "MAIN loaded " + result.path);
```

```cpp
// Every block, and old graphs are destroyed by these assignments on the same thread
std::lock_guard<std::mutex> lock(mStagingMutex);
...
mModel = std::move(mStagedModel);
mIR = std::move(mStagedIR);
```

```cpp
// VOLUM_LOG implementation
std::lock_guard<std::mutex> lock(mMutex);
const std::string line = TimestampNow() + ...;
const auto size = std::filesystem::file_size(mPath, ec);
std::ofstream out(mPath, std::ios::app | std::ios::binary);
```

**Mechanism:** The callback calls `_ApplyDSPStaging()` every block. Result draining can run `ResamplingNAM::Reset()`/prewarm after a sample-rate or block-size change, build strings, lock two mutexes, and synchronously rotate/open/write the diagnostic file. Committing a result destroys the old NAM/IR graph on that same callback. The local `results` deque and discarded/superseded result models are also deallocated when the function returns.

**Trigger:** Change amp, channel, PRE capture, custom IR, or Lite/Full mode while audio is running; the worst path occurs if the device rate/block size changed while the loader was working.

**Impact:** Audible dropout/click, deadline miss, or priority inversion. Slow filesystem/antivirus activity can make a nominally asynchronous model change block the realtime callback for an unbounded interval.

**Fix sketch:** Keep loading, reset/prewarm, logging, and reclamation off the callback. Use a nonblocking single-producer/single-consumer publication slot; the audio thread should only exchange a prepared pointer and enqueue the retired graph for non-audio destruction. Latch log events for `OnIdle`.

**Proposed regression test:** `test_model_handoff_is_rt_clean` — intercept allocation, file-open/write, and contended-lock operations during a callback that consumes a completed main/support/PRE result; assert all counters remain zero and the retired graph is destroyed on a non-audio thread. This fails today.

### F-P1-3: MAJOR — Model handoff calls host latency and UI APIs from `ProcessBlock`

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:1764-1778`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2218-2285`

**Evidence:**

```cpp
// Still inside _ApplyDSPStaging -> ProcessBlock
if (removedMainModel || appliedMainModel)
{
  _UpdateLatency();
  _SetInputGain();
  _SetOutputGain();
}
```

```cpp
if (GetLatency() != latency)
  SetLatency(latency);
_VolumRefreshLatencyReport(/*force=*/true);
...
if (auto* pGraphics = GetUI())
  if (auto* settings = pGraphics->GetControlWithTag(kCtrlTagSettingsBox))
    settings->As<NAMSettingsPageControl>()->SetCurrentLatency(report);
```

**Mechanism:** A staged model is applied by the audio callback, which immediately calls `_UpdateLatency()`. That calls the plugin/host latency setter and then traverses and mutates the graphics tree. Neither operation is an audio-thread API; the UI mutation also races normal editor work and can allocate while formatting the report.

**Trigger:** Finish loading or remove a main, support, or PRE NAM while transport/audio is running; open Settings to make the UI branch execute.

**Impact:** Host-specific instability, GUI data races/crashes, callback stalls, and PDC changes delivered from an unsafe thread.

**Fix sketch:** Calculate the new latency with the prepared generation, atomically latch it, and apply `SetLatency()` plus UI refresh from an allowed non-audio callback. Keep the DSP-generation transition and latency notification ordered by a generation counter.

**Proposed regression test:** `test_model_handoff_defers_latency_and_ui` — record thread IDs for `SetLatency` and `SetCurrentLatency` while a loader result lands; assert neither is called from the process thread. Both can be called there today.

### F-P1-4: MAJOR — Custom SUPPORT amps load but are never processed

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:579-598`, `NeuralAmpModeler/VoLumLoader.inc.cpp:414-452`

**Evidence:**

```cpp
const bool supportAmpSelected = GetParam(kSupportAmpIdx)->Int() >= 0;
const bool haveSupportModel = supportAmpSelected && (mSupportModel != nullptr);
...
const auto processingPlan = volum::MakeProcessingPlan(
  ... dualAmpActive, haveSupportModel, ...);
```

```cpp
// Loader documents and implements the custom-support representation
// supportAmpIdx is -1 while a custom partner is active.
if (mVolumCustomSupportIdx >= 0)
{
  ...
  _VolumQueueSupportModelLoad(fileToLoad, -1);
  return;
}
```

The selection path also explicitly sets `kSupportAmpIdx` to `-1` for a custom support partner (`VoLumAmpMenus.inc.cpp:220-233`).

**Mechanism:** The loader successfully builds and stages `mSupportModel` for a custom partner, but `ProcessBlock` defines “support selected” solely as factory index `>= 0`. A custom partner deliberately keeps that index at `-1`, so `haveSupportModel` is false, `MakeProcessingPlan()` sets `runSupportModel`/`runDualAmp` false, and the loaded custom model is ignored.

**Trigger:** Enable Dual Amp and choose any imported/custom amp as SUPPORT.

**Impact:** The SUPPORT lane contributes no audio even though the UI shows it and the footer can show its loaded filename. The feature is functionally broken.

**Fix sketch:** Define support presence as `(valid factory support index || valid custom support selection) && mSupportModel != nullptr`, ideally through one helper shared by loading, processing, UI, and latency calculation.

**Proposed regression test:** `test_custom_support_model_contributes_audio` — select a custom SUPPORT while `kSupportAmpIdx == -1`, feed an impulse with MAIN muted, and assert nonzero support output and `processingPlan.runDualAmp == true`. It is false today.

### F-P1-5: MAJOR — Reported plugin tail truncates long Delay and Reverb decays

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:722-731`, `NeuralAmpModeler/NeuralAmpModeler.cpp:341-354`, `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:231-250`

**Evidence:**

```cpp
// Fixed at ten 5 Hz HPF cycles: 2 seconds at every sample rate
const int tailCycles = 10;
SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
```

```cpp
GetParam(kDelayTime)->InitDouble("DelayTime", 320.0, 10.0, 2000.0, 1.0, "ms");
GetParam(kDelayFeedback)->InitDouble("DelayFeedback", 0.35, 0.0, 0.99, 0.01);
...
GetParam(kReverbDecay)->InitDouble("ReverbDecay", 2.5, 0.1, 10.0, 0.1, "s");
```

**Mechanism:** `OnReset()` always reports a two-second tail based only on the DC blocker. The active POST chain can have a ten-second reverb decay, and a two-second delay with 0.99 feedback persists far beyond two seconds. Hosts are allowed to stop calling a silent plugin after its declared tail.

**Trigger:** Use a long Reverb decay or high-feedback Delay, stop transport or end an offline-render region, and let the host honor the declared tail.

**Impact:** Reverb/delay is cut off abruptly in playback, bounce, freeze, or render.

**Fix sketch:** Report a conservative tail derived from enabled POST effects, or report an infinite tail while high-feedback effects are active if the API supports it. Notify the host when the effective tail changes, off the audio thread.

**Proposed regression test:** `test_reported_tail_covers_post_effects` — set Reverb decay to 10 s, excite it, and assert the declared tail is at least the last sample above the chosen silence threshold. The current declaration ends at 2 s.

### F-P1-6: MAJOR — A zero-input callback replays the previous input block

**Where:** `NeuralAmpModeler/VoLumProcessIO.h:11-29`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2114-2138`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2153-2169`

**Evidence:**

```cpp
for (std::size_t c = 0; c < nChansIn; ++c)
{
  for (std::size_t s = 0; s < nFrames; ++s)
  {
    ...
    if (c == 0)
      monoOut[s] = static_cast<Sample>(contrib);
  }
}
```

```cpp
// Buffers are cleared only when the frame count changes.
if (updateFrames)
{
  mInputArray[c].resize(numFrames);
  std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
}
```

**Mechanism:** With `NInChansConnected() == 0`, `MixExternalInputsToMono()` executes no loop and writes nothing. If the block length is unchanged, `_PrepareBuffers()` also does not clear `mInputArray`. The NAM therefore receives the complete previous callback's input again.

**Trigger:** A host disconnects/deactivates the input bus, or temporarily reports zero connected inputs during a routing/device transition, while continuing to process same-sized blocks.

**Impact:** A burst or repeated stale guitar audio appears after input removal; stateful downstream stages are excited again instead of receiving silence.

**Fix sketch:** Initialize `monoOut[0..nFrames)` to zero before accumulating channels, or explicitly clear it on the zero-input path.

**Proposed regression test:** `test_zero_connected_inputs_produce_silence_not_stale_block` — process a nonzero block, then process the same frame count with zero input channels and POST effects disabled; assert the internal mono input is all zero. It retains the prior block today.

### F-P1-7: MAJOR — Zero connected outputs still drives the meter as one channel

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:558-562`, `NeuralAmpModeler/NeuralAmpModeler.cpp:2308-2323`

**Evidence:**

```cpp
const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
...
_UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
```

```cpp
const int nChansHack = 1;
mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
```

**Mechanism:** All output loops correctly execute zero iterations when no output channel is connected, but `_UpdateMeters()` ignores `nChansOut` and tells the sender that `outputPointer` contains one channel. Relative to the function's own connected-channel count this is an out-of-bounds read. Whether it immediately crashes is host-wrapper dependent: some wrappers retain a spare pointer, while others can pass null/no channel pointer for an inactive bus.

**Trigger:** Deactivate/disconnect the output bus while the host still invokes processing (valid in VST3 bus negotiation and during routing transitions).

**Impact:** Nondeterministic host/plugin crash or a meter reading uninitialized memory.

**Fix sketch:** Skip output-meter processing when `nChansOut == 0`; otherwise pass the real bounded count. Do not advertise one channel solely because the sender template was configured for one.

**Proposed regression test:** `test_zero_connected_outputs_do_not_touch_output_pointer` — call the callback with zero connected outputs and a guarded/null output pointer; assert no read occurs. The sender is unconditionally invoked for one channel today.

### F-P1-8: MAJOR — Shutdown can block indefinitely inside an active model load

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:540-555`, `NeuralAmpModeler/VoLumLoader.inc.cpp:20-35`, `NeuralAmpModeler/VoLumLoader.inc.cpp:264-310`

**Evidence:**

```cpp
NeuralAmpModeler::~NeuralAmpModeler()
{
  _VolumStopLoader();
  ...
}
```

```cpp
mVolumLoaderStop.store(true);
mVolumLoadRequests.clear();
...
if (mVolumLoaderThread.joinable())
  mVolumLoaderThread.join();
```

```cpp
auto model = makeModel(request.fileToLoad);
result.model = std::make_unique<ResamplingNAM>(std::move(model), request.sampleRate);
result.model->SetSlimmableSize(...);
result.model->Reset(request.sampleRate, request.blockSize);
```

**Mechanism:** Stop cancels queued requests but cannot cancel the request already inside `nam::get_dsp`, model construction, `SetSlimmableSize`, `Reset`, or prewarm. The worker checks `mVolumLoaderStop` only later during optional sibling-directory scanning. The destructor then performs an unbounded `join()` on the host/UI teardown thread.

**Trigger:** Close the plugin/DAW or switch audio device and destroy the instance while a large, slow, malformed, or storage-delayed custom NAM is parsing/prewarming.

**Impact:** Host shutdown/editor teardown freezes until the load returns; if the underlying load stalls, teardown never completes.

**Fix sketch:** Make model construction cancellable at safe checkpoints, or move work behind a process-level loader whose lifetime is not joined synchronously by each plugin destructor. At minimum, avoid starting nonessential prefetch during teardown and surface bounded cancellation behavior.

**Proposed regression test:** `test_destroy_during_blocked_model_load_is_bounded` — inject a loader that blocks after dequeuing, destroy the plugin, request cancellation, and assert destruction completes within a fixed deadline. It waits on `join()` today.

### F-P1-9: MINOR — IR shaping filters replay stale history after bypass or replacement

**Where:** `NeuralAmpModeler/NeuralAmpModeler.h:876-890`, `NeuralAmpModeler/NeuralAmpModeler.cpp:722-780`, `NeuralAmpModeler/VoLumSceneRig.inc.cpp:754-782`

**Evidence:**

```cpp
recursive_linear_filter::HighPass mIrLowCut;
recursive_linear_filter::LowPass mIrHighCut;
recursive_linear_filter::HighPass mSupportIrLowCut;
recursive_linear_filter::LowPass mSupportIrHighCut;
```

```cpp
// OnReset resets the lane DC blockers and POST effects, but none of the four shaping filters.
mHighPass.SetParams(highPassParams);
mSupportHighPass.SetParams(highPassParams);
...
mDelay.Reset();
mReverb.Reset();
mTremolo.Reset();
```

```cpp
if (lowHz > 0.0)
{
  auto& f = support ? mSupportIrLowCut : mIrLowCut;
  f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, lowHz));
  p = f.Process(p, numChannels, nFrames);
}
```

**Mechanism:** A cut at `0` bypasses the recursive filter without resetting it. IR removal/replacement and `OnReset()` also leave its delay/history state intact. Re-enabling that cut later resumes from samples belonging to the previous IR and previous time, so the first output is not the response to the new input alone.

**Trigger:** Process signal through an IR with low/high cut enabled, switch to an IR with that cut disabled (or remove the IR), wait, then select an IR with the cut enabled again. A sample-rate reset also carries old-rate history into new coefficients.

**Impact:** A click/pop or short burst of stale filtered audio on IR selection/cut re-enable.

**Fix sketch:** Reset the corresponding shaping filters when their stage transitions active-to-bypassed, when the lane IR generation changes, and in `OnReset()`.

**Proposed regression test:** `test_ir_shaping_reenable_has_no_stale_history` — excite a cut filter, bypass it for several blocks, re-enable it with a zero input block, and assert the output is exactly zero. It emits the saved recursive state today.

### F-P1-10: MINOR — IR trim and cuts can come from different settings generations

**Where:** `NeuralAmpModeler/NeuralAmpModeler.h:876-890`, `NeuralAmpModeler/VoLumSceneRig.inc.cpp:735-780`

**Evidence:**

```cpp
mIrTrimLin.store(trimLin, std::memory_order_relaxed);
mIrLowCutHz.store(s.lowCutHz, std::memory_order_relaxed);
mIrHighCutHz.store(s.highCutHz, std::memory_order_relaxed);
```

```cpp
const double trim = mIrTrimLin.load(std::memory_order_relaxed);
...
const double lowHz = mIrLowCutHz.load(std::memory_order_relaxed);
...
const double highHz = mIrHighCutHz.load(std::memory_order_relaxed);
```

**Mechanism:** The three properties form one logical IR-shaping snapshot but are published and consumed as unrelated atomics. The audio callback can load `trim`, be preempted while the UI publishes a new IR's values, then load the new `lowHz`/`highHz`. This interleaving is possible even on x86; relaxed ordering additionally provides no cross-property publication relation on Apple ARM.

**Trigger:** Switch IRs or edit trim/cuts while audio is processing, especially between IRs with materially different trim and cut settings.

**Impact:** One callback can apply a hybrid configuration that never existed in the library, causing a momentary wrong gain/filter response or click.

**Fix sketch:** Publish a coherent versioned POD snapshot with a realtime-safe seqlock/read-retry scheme, or double-buffer the settings and atomically publish the active index.

**Proposed regression test:** `test_ir_shaping_snapshot_is_generation_coherent` — force a writer between the audio reader's trim and cut reads and assert the reader observes either the complete old tuple or complete new tuple, never a hybrid. The current code permits a hybrid.

### F-P1-11: MAJOR — Denormal protection ends before the feedback-based POST chain

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:566-569`, `NeuralAmpModeler/NeuralAmpModeler.cpp:613-664`, `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:231-266`

**Evidence:**

```cpp
std::fenv_t fe_state;
std::feholdexcept(&fe_state);
disable_denormals();
...
sample* supportLane = _VolumProcessDualAmpSupportLane(...);

// restore previous floating point state
std::feupdateenv(&fe_state);
...
_VolumProcessPostChain(outputs, processingPlan, ...);
```

```cpp
postPointers = mDelay.Process(postPointers, numChannelsExternalOut, nFrames);
...
postPointers = mReverb.Process(postPointers, numChannelsExternalOut, nFrames);
...
mTremolo.Process(postPointers, numChannelsExternalOut, nFrames);
```

**Mechanism:** The code restores the host floating-point environment before output gain/routing and before Delay, Reverb, and Tremolo. Delay and Reverb are exactly the long-lived feedback/tail processors most likely to decay into subnormal values. If the host entered with FTZ/DAZ disabled, those stages run without the protection established at the start of the callback.

**Trigger:** In a host/thread whose incoming FP mode does not flush denormals, excite Delay/Reverb and let the tail decay near silence.

**Impact:** Severe callback CPU spikes and resulting crackles/dropouts near the end of tails, especially on x86.

**Fix sketch:** Use an RAII floating-point-state guard whose lifetime covers the entire callback, restoring the host environment only immediately before return.

**Proposed regression test:** `test_post_chain_runs_with_denormals_disabled` — start with FTZ/DAZ cleared, process a decaying POST tail, and assert the POST entry sees flush-to-zero enabled while the caller's original FP state is restored after `ProcessBlock`. POST currently sees the restored host state.

## Voicing observations (report only)

None. I did not classify intentional filter curves, gains, effect order, mix laws, or tuning choices as defects.

## Areas read and found clean

- `NeuralAmpModeler/NeuralAmpModeler.cpp` — read in full, including construction/destruction, process flow, reset/idle, state I/O, staging, buffer preparation, gain caches, latency, and metering. Outside the findings above, the main/support latency-alignment arithmetic, final safety clip placement, and fallback-silence path were internally consistent.
- `NeuralAmpModeler/NeuralAmpModeler.h` — read in full. Loader ownership order is safe once the worker has actually joined; member initialization is generally explicit; the PRE pitch reset uses a deliberate try-lock/process strategy rather than blocking the callback.
- `NeuralAmpModeler/VoLumProcessBlock.inc.cpp` — read in full. Main/support ordering, support scratch ownership, post-copyback, and NAM-output nonfinite scrubbing were coherent.
- `NeuralAmpModeler/VoLumProcessIO.h` — read in full. Stereo-to-mono averaging and mono-to-output broadcast are bounds-correct for positive channel counts.
- `NeuralAmpModeler/VoLumProcessingPlan.h` — read in full. Boolean dependencies are consistent once callers supply the correct `haveSupportModel` value.
- `NeuralAmpModeler/VoLumDspStaging.h` — read in full. Deferred-swap timeout and pending-removal convolution logic are internally consistent.
- `NeuralAmpModeler/VoLumDspStagingWdl.h` — read in full. Live/staged commit semantics are correct when all readers participate in the synchronization protocol.
- `NeuralAmpModeler/VoLumDspCacheParams.h` — read in full. The declared cached-parameter set matches the gain/tone caches visible in the assigned core.
- `NeuralAmpModeler/VoLumNanGuard.h` — read in full. Both overloads are null-safe, bounded, allocation-free, and correctly report whether any sample was scrubbed.
- `NeuralAmpModeler/VoLumMasterSafety.h` — read in full. It maps nonfinite values to zero and remains finite and continuous for finite input.
- `NeuralAmpModeler/VoLumLevelMute.h` — read in full. Mute-floor detection and dB conversion are numerically safe for the parameter ranges used here.
- `NeuralAmpModeler/VoLumLatencyReport.h` — read in full. Formatting and standalone/plugin arithmetic correctly avoid double-counting driver buffers; the defect is the upstream PDC/tail lifecycle, not this formatter.
- `NeuralAmpModeler/VoLumLoader.inc.cpp` — read in full. Queue deduplication, superseded-main rejection, cache ownership, condition-variable predicate, and final join-before-member-destruction are otherwise coherent.
- `NeuralAmpModeler/config.h` — read in full. Version identifiers and declared mono/stereo layouts match the process architecture.
