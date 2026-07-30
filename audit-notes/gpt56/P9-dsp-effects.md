## Summary

This subsystem has no obvious memory-indexing crash in its steady-state DSP loops, but it is not release-clean for realtime use. The highest-risk defects are synchronous heap allocation and multi-megabyte buffer clearing on the audio thread (including a guaranteed first-use allocation in the default tone stack and a large first-use allocation in Reverse Delay), plus the tuner's roughly 4.2-million-iteration YIN burst inside one callback. I found fourteen concrete issues: eleven MAJOR and three MINOR. The pitch shifter itself is VoLum's delay-line implementation in this branch; there is no signalsmith-stretch call in the assigned pitch path.

## Findings

### F-P9-1: MAJOR — Default tone stack and PRE effects allocate on the audio thread at first use

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:329`, `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:44-70,123-125`, `NeuralAmpModeler/ToneStack.cpp:3-20`, `NeuralAmpModeler/VoLumPreEffects.h:35-39,116-119`; allocation reached in `AudioDSPTools/dsp/dsp.cpp:58-80` and `AudioDSPTools/dsp/RecursiveLinearFilter.cpp:73-91`.

**Mechanism:** `OnReset()` calls `BasicNamToneStack::Reset()` and `VoLumPreEq::Reset()`, but those methods only refresh coefficients; they do not prepare output/history storage. `VoLumCompressor` has no prepare call either. The default-enabled tone stack therefore reaches its first filter `Process()` with zero channels, and the inherited preparation path resizes vectors and executes `new DSP_SAMPLE*[numChannels]`. Enabling Compressor or a PRE NAM slot reaches the same path for those objects.

```314:329:NeuralAmpModeler/NeuralAmpModeler.cpp
NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // ...
  _InitToneStack();
  // ...
  GetParam(kEQActive)->InitBool("ToneStack", true);
```

```3:19:NeuralAmpModeler/ToneStack.cpp
DSP_SAMPLE** dsp::tone_stack::BasicNamToneStack::Process(DSP_SAMPLE** inputs, const int numChannels,
                                                         const int numFrames)
{
  DSP_SAMPLE** bassPointers = mToneBass.Process(inputs, numChannels, numFrames);
  DSP_SAMPLE** midPointers = mToneMid.Process(bassPointers, numChannels, numFrames);
  DSP_SAMPLE** treblePointers = mToneTreble.Process(midPointers, numChannels, numFrames);
  return treblePointers;
}

void dsp::tone_stack::BasicNamToneStack::Reset(const double sampleRate, const int maxBlockSize)
{
  dsp::tone_stack::AbstractToneStack::Reset(sampleRate, maxBlockSize);
  SetParam("bass", mBassVal);
  SetParam("middle", mMiddleVal);
  SetParam("treble", mTrebleVal);
}
```

```63:80:AudioDSPTools/dsp/dsp.cpp
  const bool resizeChannels = oldChannels != numChannels;
  const bool resizeFrames = resizeChannels || (oldFrames != numFrames);
  if (resizeChannels)
  {
    this->mOutputs.resize(numChannels);
    this->_ResizePointers(numChannels);
  }
  // ...
  this->_DeallocateOutputPointers();
  this->_AllocateOutputPointers(numChannels);
```

**Trigger:** Start audio in a fresh plugin instance with the default Tone Stack enabled; alternatively enable Compressor or a PRE NAM slot for the first time.

**Impact:** Heap allocator entry on the realtime thread can block on allocator locks and miss the first callback deadline, producing an xrun/click. This is guaranteed for the default tone stack rather than merely a host edge case.

**Fix sketch:** Add explicit off-thread `Prepare(numChannels, maxBlockSize)` methods for the tone stack, compressor, and both PRE EQs, and call them from `OnReset()`. Keep capacities stable and reject or safely handle callbacks beyond the prepared size.

**Proposed regression test:** `ProcessBlock_default_chain_performs_zero_allocations_after_OnReset`; install an allocation counter around the first and subsequent process callbacks and assert `audioThreadAllocations == 0`.

### F-P9-2: MAJOR — Reverse Delay allocates its capture rings on its first audio block

**Where:** `AudioDSPTools/dsp/Delay.cpp:50-54,139-160,196-205,374-380`; VoLum call site `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:231-243`.

**Mechanism:** `Delay::Prepare()` prepares ordinary output/delay buffers only. Reverse capture storage is deferred until `_ProcessReverse()`, which calls `_PrepareReverseBuffers(numChannels)` and `assign()`s a two-second vector per channel.

```50:54:AudioDSPTools/dsp/Delay.cpp
void Delay::Prepare(const size_t numChannels, const size_t numFrames, double sampleRate)
{
  mSampleRate = sampleRate;
  _PrepareBuffers(numChannels, numFrames);
}
```

```139:160:AudioDSPTools/dsp/Delay.cpp
void Delay::_PrepareReverseBuffers(const size_t numChannels)
{
  const size_t ringSize = _GetMaxFrames();
  // ...
  if (mReverseRing[c].size() != ringSize)
  {
    mReverseRing[c].assign(ringSize, 0.0);
    resized = true;
  }
  // ...
}
```

```374:380:AudioDSPTools/dsp/Delay.cpp
DSP_SAMPLE** Delay::_ProcessReverse(DSP_SAMPLE** inputs, const size_t numChannels, const size_t numFrames)
{
  _PrepareReverseBuffers(numChannels);
  // ...
}
```

**Trigger:** Select Reverse mode and process its first active block after plugin creation/reset.

**Impact:** At 48 kHz stereo this allocates and zeroes about 1.5 MiB on the audio thread (about 3 MiB at 96 kHz), with allocator-lock and callback-overrun risk exactly when the user switches the effect on.

**Fix sketch:** Call `_PrepareReverseBuffers(numChannels)` from `Delay::Prepare()` and prohibit growth in `Process()`.

**Proposed regression test:** `Delay_Prepare_preallocates_Reverse_storage`; after `Prepare()`, count allocations while processing the first Reverse block and assert zero.

### F-P9-3: MAJOR — Delay and Reverb mode changes clear megabytes synchronously in ProcessBlock

**Where:** `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:231-250`, `AudioDSPTools/dsp/Delay.cpp:63-100,103-113`, `AudioDSPTools/dsp/Reverb.cpp:197-260,364-412`.

**Mechanism:** VoLum invokes both `SetParams()` methods from the audio callback. A Delay mode or ping-pong change calls `Reset()`, which fills normal and Reverse rings. A Reverb mode or Oktaverb sub-mode change calls `Reset()`, which fills all Hall, Plate, pre-delay, pitched-grain, and pitched-pre-delay buffers, including buffers for inactive algorithms.

```231:250:NeuralAmpModeler/VoLumProcessBlock.inc.cpp
  if (processingPlan.runDelay)
  {
    // ...
    mDelay.SetParams(delayTimeMs, GetParam(kDelayFeedback)->Value(), GetParam(kDelayMix)->Value(),
                     GetParam(kDelayMode)->Int(), sampleRate, GetParam(kDelayTone)->Value(),
                     GetParam(kDelayAge)->Value(), GetParam(kDelayPingPong)->Bool());
    postPointers = mDelay.Process(postPointers, numChannelsExternalOut, nFrames);
  }
  if (processingPlan.runReverb)
  {
    mReverb.SetParams(/* ... */);
    postPointers = mReverb.Process(postPointers, numChannelsExternalOut, nFrames);
  }
```

```99:112:AudioDSPTools/dsp/Delay.cpp
  if (prevMode != mMode || prevPingPong != mPingPong)
    Reset();
}

void Delay::Reset()
{
  for (auto& buf : mBuffer)
    std::fill(buf.begin(), buf.end(), 0.0);
  // ...
  _ResetReverseState();
}
```

```253:260:AudioDSPTools/dsp/Reverb.cpp
  // ...
  if (prevMode != mMode || (mMode == kModeOktaverb && prevSubMode != mSubMode))
    Reset();
```

**Trigger:** Change Delay Digital/Analog/Reverse, toggle Ping-Pong, change Hall/Plate/Oktaverb, or change an Oktaverb sub-mode while audio is running.

**Impact:** At common high sample rates the callback performs several MiB of synchronous writes. Small host buffers can overrun, causing a click/dropout on an ordinary UI parameter action.

**Fix sketch:** Move large clears to off-thread replacement-state preparation and atomically/lock-freely swap at a block boundary, or clear incrementally while output is crossfaded. Do not execute whole-engine `Reset()` from audio-thread `SetParams()`.

**Proposed regression test:** `Post_mode_switch_has_bounded_audio_callback_work`; instrument bytes cleared/work units in the switch callback and assert it does not scale with allocated delay/reverb capacity.

### F-P9-4: MAJOR — Pitch bypass replays pre-bypass audio when re-enabled

**Where:** `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:23-42`, `NeuralAmpModeler/VoLumPitchShifter.h:175-187,578-585,620-628`; contrast POST edge handling at `VoLumProcessBlock.inc.cpp:205-218`.

**Mechanism:** The pitch object is processed only while `runPrePitch` is true. There is no active-edge tracking or `mPitch.Reset()` when it becomes inactive, so both voice rings and the latency-matched dry ring stop at their old positions with old audio intact. Re-enabling resumes those rings and reads the old contents. Pitch is reset only from `OnReset()`.

```23:42:NeuralAmpModeler/VoLumProcessBlock.inc.cpp
  if (processingPlan.runPrePitch)
  {
    // ...
    if (lock.owns_lock() && mPitch.Configured())
    {
      // ...
      preAmpPointers = mPitch.Process(preAmpPointers, numChannelsInternal, nFrames);
    }
  }
```

```578:585:NeuralAmpModeler/VoLumPitchShifter.h
  void Reset()
  {
    for (auto& voice : mVoices)
      voice.Reset();
    std::fill(mDryRing.begin(), mDryRing.end(), static_cast<DSP_SAMPLE>(0));
    mDryWrite = 0;
    mVintageLpState = {0.0, 0.0};
  }
```

**Trigger:** Play a note with Pitch active, bypass Pitch, wait/play different material, then re-enable it.

**Impact:** The first re-enabled block can contain a ghost of the note from before bypass. It can also make an enable click because the resumed delayed stream is unrelated to the current input.

**Fix sketch:** Track the active edge as POST effects do and clear pitch state on active-to-inactive (or before inactive-to-active), with an enable crossfade if preserving continuity is desired.

**Proposed regression test:** `VoLumPitch_bypass_reenable_does_not_replay_stale_audio`; prime with an impulse, simulate inactive blocks, re-enable on silence, and assert output energy is zero after the declared startup latency.

### F-P9-5: MAJOR — Oversized pitch callbacks return undelayed audio despite nonzero reported PDC

**Where:** `NeuralAmpModeler/VoLumPitchShifter.h:603-609,718-721`; latency reporting call at `NeuralAmpModeler/NeuralAmpModeler.cpp:2237-2241`.

**Mechanism:** `_PrepareIO()` rejects any callback beyond the fixed 8192-sample reserve. `Process()` then returns the raw input pointer. The plugin still reports the pitch latency and the pitch state does not advance, so this one block is neither latency-aligned nor state-continuous.

```603:609:NeuralAmpModeler/VoLumPitchShifter.h
  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, size_t numChannels, size_t numFrames)
  {
    // ...
    if (!_PrepareIO(numChannels, numFrames))
      return inputs;
```

```718:721:NeuralAmpModeler/VoLumPitchShifter.h
  bool _PrepareIO(size_t numChannels, size_t numFrames) const
  {
    return numChannels <= kMaxChannels && static_cast<int>(numFrames) <= mMaxBlock && mWet0.size() >= numFrames;
  }
```

**Trigger:** A host/offline renderer supplies a callback larger than 8192 samples while Pitch is active.

**Impact:** The block jumps early by the reported pitch latency, and the next accepted block resumes old pitch state. This causes a timing discontinuity/click and invalidates host PDC alignment.

**Fix sketch:** Prepare from the host's true maximum where available. For an unexpected oversize, process in internal chunks without allocation; at minimum produce latency-delayed dry and advance state rather than returning raw input.

**Proposed regression test:** `VoLumPitch_oversized_block_preserves_latency_and_stream_state`; compare chunked reference output with one 16384-frame call and assert sample alignment and continuity.

### F-P9-6: MAJOR — Tuner executes a 4.2-million-iteration YIN burst inside one audio callback

**Where:** `NeuralAmpModeler/NeuralAmpModeler.cpp:575-576`, `NeuralAmpModeler/VoLumTunerDSP.h:46-63,107-144`.

**Mechanism:** Every 4096 collected samples, `_RunYIN()` runs synchronously on the realtime thread. Its `tau=1..2047` loop contains a 2048-sample inner loop: 4,192,256 difference/square iterations, followed by additional scans and interpolation.

```46:62:NeuralAmpModeler/VoLumTunerDSP.h
  void Process(const float* input, int nFrames)
  {
    // ...
    if (mSamplesCollected >= kBufferSize)
    {
      mSamplesCollected = 0;
      _RunYIN();
    }
  }
```

```132:144:NeuralAmpModeler/VoLumTunerDSP.h
  for (int tau = 1; tau < halfBuf; ++tau)
  {
    float sum = 0.f;
    for (int j = 0; j < halfBuf; ++j)
    {
      const float diff = mAnalysisBuffer[j] - mAnalysisBuffer[j + tau];
      sum += diff * diff;
    }
    // ...
  }
```

**Trigger:** Open the tuner and collect each 4096-sample window, under any host block size.

**Impact:** One callback periodically absorbs millions of scalar operations. At 48 kHz this repeats about 11.7 times per second and can exceed small-buffer callback deadlines, causing xruns/host instability even though tuner mode later mutes the audio output.

**Fix sketch:** Copy/publish windows lock-free to a worker and run YIN off the audio thread, or use a bounded incremental/FFT implementation whose per-callback work is fixed and demonstrated deadline-safe.

**Proposed regression test:** `TunerDSP_audio_callback_work_is_bounded`; instrument operations (or a worker handoff) and assert no single `Process(1..8192)` invocation performs the full YIN nested loop.

### F-P9-7: MAJOR — Tuner activation races the audio thread on non-atomic detector state

**Where:** `NeuralAmpModeler/VoLumTunerDSP.h:84-96,223-252`; UI-thread call sites `NeuralAmpModeler/VoLumSceneRig.inc.cpp:5-19` and `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1089`.

**Mechanism:** `SetActive(true)` is invoked directly by UI callbacks. After the atomic active store it writes `mSmoothedFreq`, `mSmoothedValid`, and `mInvalidDetections`. `_PublishFrequency()` and `_PublishInvalid()` read/write the same fields on the audio thread. The relaxed atomic active flag neither protects nor orders those non-atomic fields.

```84:93:NeuralAmpModeler/VoLumTunerDSP.h
  void SetActive(bool active)
  {
    mActive.store(active, std::memory_order_relaxed);
    if (active)
    {
      mSmoothedFreq = 0.f;
      mSmoothedValid = false;
      mInvalidDetections = 0;
      mResult.store({});
    }
  }
```

```223:239:NeuralAmpModeler/VoLumTunerDSP.h
  void _PublishFrequency(float freq)
  {
    if (mSmoothedValid)
    {
      // ...
      mSmoothedFreq = /* ... */;
    }
    // ...
    mInvalidDetections = 0;
    mResult.store(_BuildResult(mSmoothedFreq));
  }
```

**Trigger:** Toggle/open the tuner while the audio callback is completing `_RunYIN()`—especially near a 4096-sample boundary.

**Impact:** This is a C++ data race and therefore undefined behavior. Observable outcomes include torn smoothing state, a bogus result, or optimizer-dependent malfunction.

**Fix sketch:** Make audio-thread detector state single-owner. UI should only publish an atomic desired-active/reset request; the audio thread consumes that request and clears all non-atomic fields itself.

**Proposed regression test:** `TunerDSP_toggle_is_thread_race_free`; repeatedly toggle from a UI thread while processing windows under ThreadSanitizer and assert zero race reports.

### F-P9-8: MINOR — Re-enabling the tuner can immediately publish a stale pre-disable window

**Where:** `NeuralAmpModeler/VoLumTunerDSP.h:46-63,84-93,255-262`.

**Mechanism:** `SetActive(true)` clears smoothing/result fields but does not reset `mWritePos`, `mSamplesCollected`, or `mBuffer`. If disabled with 4095 samples collected, the first new sample after re-enable completes an analysis window containing 4095 old samples and one current sample.

```84:93:NeuralAmpModeler/VoLumTunerDSP.h
  void SetActive(bool active)
  {
    mActive.store(active, std::memory_order_relaxed);
    if (active)
    {
      mSmoothedFreq = 0.f;
      mSmoothedValid = false;
      mInvalidDetections = 0;
      mResult.store({});
    }
  }
```

**Trigger:** Close the tuner late in a collection window, play a different note while it is closed, then reopen it.

**Impact:** The tuner can immediately display the old note/frequency and hold it through its invalid-detection hysteresis before enough fresh audio arrives.

**Fix sketch:** Consume activation on the audio thread and reset write position, sample count, and analysis buffer together with smoothing state.

**Proposed regression test:** `TunerDSP_reenable_requires_fresh_window`; collect 4095 A4 samples, disable/re-enable, submit one E2 sample, and assert the result remains invalid until 4096 fresh E2 samples have arrived.

### F-P9-9: MAJOR — BPM text entry accepts NaN and sends undefined state into the metronome

**Where:** `NeuralAmpModeler/VoLumTunerMetronomeOverlay.h:433-447`, `NeuralAmpModeler/VoLumMetronomeDSP.h:147,170-180`; callback connection `NeuralAmpModeler/VoLumLayoutBuild.inc.cpp:1310`.

**Mechanism:** C `strtof()` accepts strings such as `nan`, and `end != str` passes. `std::clamp(NaN, 30, 300)` remains NaN because both comparisons are false. `SetBPM()` repeats the same ineffective clamp. `_RecalcSamplesPerBeat()` then divides by NaN and converts NaN to `int`, which is undefined behavior.

```438:447:NeuralAmpModeler/VoLumTunerMetronomeOverlay.h
    if (str && str[0])
    {
      char* end = nullptr;
      const float bpm = std::strtof(str, &end);
      if (end != str)
      {
        mBPM = std::clamp(bpm, volum::MetronomeDSP::kMinBPM, volum::MetronomeDSP::kMaxBPM);
        if (mOnBPMChanged)
          mOnBPMChanged(mBPM);
      }
    }
```

```170:180:NeuralAmpModeler/VoLumMetronomeDSP.h
  void _RecalcSamplesPerBeat()
  {
    float bpm = mCachedBPM;
    if (bpm < kMinBPM)
      bpm = kMinBPM;
    // ...
    float effectiveBPM = compound ? (bpm * 3.f) : bpm;
    mSamplesPerBeat = static_cast<int>(mSampleRate * 60.f / effectiveBPM);
    if (mSamplesPerBeat < 1)
      mSamplesPerBeat = 1;
  }
```

**Trigger:** Click the BPM value, enter `nan` (or a platform-accepted NaN spelling), and enable the metronome.

**Impact:** Undefined float-to-int conversion and persistent NaN state. Common outcomes are a silent metronome or a pathological one-sample beat interval; the NaN also compares unequal every block, forcing recalculation continuously.

**Fix sketch:** Require a full finite numeric parse (`end` at allowed trailing whitespace/end and `std::isfinite(bpm)`) in the UI, and defensively sanitize non-finite values in `SetBPM()` and `_RecalcSamplesPerBeat()`.

**Proposed regression test:** `Metronome_BPM_entry_rejects_nonfinite`; submit `"nan"` and `"inf"` and assert BPM stays at its previous finite value and `samplesPerBeat >= 1` without undefined conversion.

### F-P9-10: MAJOR — Reverse Delay silently caps every time above 1000 ms to one second

**Where:** `AudioDSPTools/dsp/Delay.cpp:78-97,374-383,482-485`; VoLum exposes/clamps Delay to 2000 ms at `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:233-241`.

**Mechanism:** The general Delay range accepts 2000 ms and `_GetMaxFrames()` allocates two seconds. Reverse then clamps `mReverseSegmentFrames` to half that ring. Because half of a two-second ring is one second, every Reverse setting from 1000 through 2000 ms produces the same one-second segment.

```90:97:AudioDSPTools/dsp/Delay.cpp
  mReverseSegmentFrames = std::clamp<size_t>(
    static_cast<size_t>(std::round(mTargetDelayFrames)), 2,
    std::max<size_t>(2, _GetMaxFrames() / 2));
```

```482:485:AudioDSPTools/dsp/Delay.cpp
size_t Delay::_GetMaxFrames() const
{
  return std::max<size_t>(1, static_cast<size_t>(2.0 * mSampleRate));
}
```

**Trigger:** Select Reverse and set Time above 1000 ms, including synced low-tempo divisions that clamp to 2000 ms.

**Impact:** Half of the advertised control range does nothing in Reverse; synced repeats can be twice as fast as selected.

**Fix sketch:** Size the Reverse ring for the overlap design's actual worst case (at least twice the maximum segment), or expose/meter a mode-specific maximum rather than silently truncating.

**Proposed regression test:** `Delay_Reverse_1500ms_impulse_reverses_at_1500ms`; assert the measured segment launch/playback period for 1500 ms is approximately 1.5 seconds and differs from 1000 ms.

### F-P9-11: MINOR — Digital Delay restarts its Age noise sequence at every host block

**Where:** `AudioDSPTools/dsp/Delay.cpp:209-223,243-248`.

**Mechanism:** `noiseSeed` is a local initialized to `12345` on every `_ProcessDigital()` call. With Age above zero, each callback emits the identical noise sequence from sample zero instead of a continuous sequence.

```215:223:AudioDSPTools/dsp/Delay.cpp
  // Age (Digital) = bit-crush + low-level noise floor.
  // ...
  unsigned int noiseSeed = 12345;
  auto rnd = [&]() {
    noiseSeed = noiseSeed * 1103515245u + 12345u;
    return (static_cast<double>((noiseSeed >> 8) & 0xFFFF) / 65535.0 - 0.5) * 2.0;
  };
```

**Trigger:** Use Digital Delay with Age above zero in a host with a stable callback size.

**Impact:** The intended noise floor becomes periodic at the host block frequency and changes timbre/pitch when the host buffer size changes. The repeated boundary can present as a faint tonal buzz rather than noise.

**Fix sketch:** Store the PRNG state as a `Delay` member, seed it in `Reset()`, and advance it continuously across callbacks.

**Proposed regression test:** `Delay_DigitalAge_noise_is_block_partition_invariant`; render the same silence stream as one large block and as uneven blocks, and assert sample-identical output.

### F-P9-12: MINOR — Tone-stack and PRE-EQ Reset leave recursive filter histories alive

**Where:** `NeuralAmpModeler/ToneStack.cpp:12-20`, `NeuralAmpModeler/VoLumPreEffects.h:19-24`; host reset calls at `NeuralAmpModeler/NeuralAmpModeler.cpp:722-753`. History is only initialized on channel-count change in `AudioDSPTools/dsp/RecursiveLinearFilter.cpp:73-91`.

**Mechanism:** Both product `Reset()` methods only update sample rate/coefficient values. The underlying recursive filters retain input/output history whenever the channel count is unchanged, which is the normal case across transport/device/sample-rate resets.

```12:20:NeuralAmpModeler/ToneStack.cpp
void dsp::tone_stack::BasicNamToneStack::Reset(const double sampleRate, const int maxBlockSize)
{
  dsp::tone_stack::AbstractToneStack::Reset(sampleRate, maxBlockSize);
  SetParam("bass", mBassVal);
  SetParam("middle", mMiddleVal);
  SetParam("treble", mTrebleVal);
}
```

```19:24:NeuralAmpModeler/VoLumPreEffects.h
  void Reset(double sampleRate, int maxBlockSize)
  {
    (void) maxBlockSize;
    mSampleRate = sampleRate;
    _Refresh();
  }
```

**Trigger:** Feed nonzero audio, then invoke a host reset/sample-rate reconfiguration without changing channel count, and process silence.

**Impact:** Old filter state leaks into the new stream as a short transient. A sample-rate change also applies new coefficients to history generated under the old rate, increasing click/burst risk.

**Fix sketch:** Expose and invoke a true filter-state clear from both reset methods; combine it with the off-thread preparation required by F-P9-1.

**Proposed regression test:** `ToneStack_and_PreEq_Reset_clear_histories`; prime each EQ with DC/impulse, reset at the same channel count, process silence, and assert every sample is zero within numerical tolerance.

### F-P9-13: MAJOR — Tremolo bypass enable applies an instantaneous gain step

**Where:** `NeuralAmpModeler/VoLumProcessBlock.inc.cpp:205-218,253-267`, `NeuralAmpModeler/VoLumTremolo.h:150-157,159-173,239-259`.

**Mechanism:** Disabling Tremolo resets its phase to zero and snaps depth/mix to their targets. Re-enabling processes immediately with no bypass crossfade. At phase zero, `_ModGain()` produces `1 - depth/2`; with full depth and mix the output jumps from bypass gain 1.0 to gain 0.5 on the first sample.

```210:218:NeuralAmpModeler/VoLumProcessBlock.inc.cpp
  if (mPostDelayWasActive && !processingPlan.runDelay)
    mDelay.Reset();
  if (mPostReverbWasActive && !processingPlan.runReverb)
    mReverb.Reset();
  if (mPostTremoloWasActive && !processingPlan.runTremolo)
    mTremolo.Reset();
  // ...
  mPostTremoloWasActive = processingPlan.runTremolo;
```

```150:156:NeuralAmpModeler/VoLumTremolo.h
  void Reset()
  {
    mPhase = 0.0;
    mDepth = mDepthTarget;
    mMix = mMixTarget;
    // ...
  }
```

```258:259:NeuralAmpModeler/VoLumTremolo.h
    const double unipolar = 0.5 * (shaped + 1.0);
    return 1.0 - depth * (1.0 - unipolar);
```

**Trigger:** Toggle Tremolo off and back on while a sustained nonzero signal is playing, especially at high Depth/Mix.

**Impact:** Up to a 6 dB one-sample boundary step creates an audible click/thump. Warm crossover/envelope state does not address the bypass gain discontinuity.

**Fix sketch:** Add a short sample-domain active/bypass crossfade (without changing the established modulation curve), or begin at unity and ramp into the current modulation gain.

**Proposed regression test:** `Tremolo_enable_has_no_large_boundary_step`; process constant input across an inactive-to-active edge at full depth/mix and assert the first-difference peak remains below a click-safe threshold.

### F-P9-14: MAJOR — Reverb pre-delay zero bypass preserves and later replays stale audio

**Where:** `AudioDSPTools/dsp/Reverb.cpp:197-210,272-278,434-445`.

**Mechanism:** At pre-delay zero, `_ReadWritePreDelay()` returns input immediately without writing the ring or advancing its index. Changing pre-delay only updates the length. Therefore audio captured before switching to zero remains frozen in the ring; restoring a nonzero pre-delay reads that old audio before newly written samples replace it.

```272:278:AudioDSPTools/dsp/Reverb.cpp
void Reverb::_SetPreDelayLength(double preDelayMs)
{
  mPreDelayMs = preDelayMs;
  mPreDelayLen = static_cast<size_t>(mPreDelayMs * mSampleRate / 1000.0);
  if (!mPreDelayBuf.empty() && mPreDelayLen >= mPreDelayBuf.size())
    mPreDelayLen = mPreDelayBuf.size() - 1;
}
```

```434:445:AudioDSPTools/dsp/Reverb.cpp
double Reverb::_ReadWritePreDelay(double input)
{
  if (mPreDelayBuf.empty())
    return input;
  if (mPreDelayLen == 0)
    return input;

  const size_t readIdx = (mPreDelayIdx + mPreDelayBuf.size() - mPreDelayLen) % mPreDelayBuf.size();
  const double out = mPreDelayBuf[readIdx];
  mPreDelayBuf[mPreDelayIdx] = input;
  mPreDelayIdx = (mPreDelayIdx + 1) % mPreDelayBuf.size();
  return out;
}
```

**Trigger:** Run Reverb with nonzero pre-delay, automate Pre-Delay to 0 for a while, then restore the former nonzero value.

**Impact:** The reverb input receives a ghost burst from before the zero-pre-delay interval, unrelated to current audio.

**Fix sketch:** Always write/advance the pre-delay ring even when the selected read delay is zero, while returning the direct input for the zero-delay output; alternatively clear/rebase state on the zero/nonzero edge.

**Proposed regression test:** `Reverb_predelay_zero_interval_does_not_replay_old_audio`; prime nonzero pre-delay with an impulse, run zero pre-delay long enough on silence, restore nonzero pre-delay, and assert no old impulse energy appears.

## Voicing observations (report only)

None. I did not classify unconventional mix laws, modulation shapes, compressor timing, pitch character geometry, or effect tuning as defects unless a control failed to apply, processing became block-dependent, stale audio replayed, or a discontinuity/realtime violation had a concrete code path.

## Areas read and found clean

- Read every line of `NeuralAmpModeler/VoLumPitchShifter.h`. Apart from the bypass/oversize/edge behavior reported above, ring interpolation wraps both endpoints, scratch storage is preallocated for accepted blocks, dry latency matches wet latency in steady state, WSOLA reference storage is prepared off-thread, and final pitch output scrubs non-finite samples.
- Read every line of `NeuralAmpModeler/VoLumTremolo.h`. Steady-state processing is allocation-free, channel count is bounded to the two stored states, phase wraps under supported rate/sample-rate ranges, depth/mix automation is smoothed, and decaying crossover states are explicitly flushed.
- Read every line of `NeuralAmpModeler/VoLumPreEffects.h`. Compressor parameter domains prevent zero-time divisions, logarithms are floored, output is checked for finiteness, and no feedback index arithmetic is present.
- Read every line of `NeuralAmpModeler/VoLumTunerDSP.h`. Silence/DC paths avoid division by zero and result publication uses an atomic snapshot; the realtime cost, activation race, and stale collection state are reported above.
- Read every line of `NeuralAmpModeler/VoLumMetronomeDSP.h`. Finite values from the normal UI are clamped, beat counters wrap, zero/negative finite BPM cannot reach division, and output/UI phase handoff is atomic.
- Read every line of `NeuralAmpModeler/VoLumTunerMetronomeOverlay.h`. Formatting buffers are bounded and callbacks are installed outside the audio process path; the non-finite BPM parse is reported above.
- Read every line of `NeuralAmpModeler/VoLumKeyboard.inc.cpp`, `VoLumKeyboardModel.h`, and `VoLumKeyboardNav.h`. Keyboard target arrays match their mode-specific controls, navigation wraps safely for actual ±1 callers, plugin Space is left to DAW transport, and UI-owned strings/functions do not run in the audio process path.
- Read every line of `NeuralAmpModeler/ToneStack.h` and `ToneStack.cpp`. Parameter names/call sites agree and steady-state chaining is structurally correct; preparation and reset defects are reported above.
- Located the code exercised by `NeuralAmpModeler/tests/test_delay_reverb_dsp.cpp` and read every line of `AudioDSPTools/dsp/Delay.h`, `Delay.cpp`, `Reverb.h`, and `Reverb.cpp`, plus their `dsp.h`/`dsp.cpp` and recursive-filter preparation behavior needed to prove VoLum call-site effects. Steady-state circular indices and interpolation endpoints are wrapped; feedback values have finite checks/bounds in the owned call paths.
- Traced the assigned engines through `NeuralAmpModeler/VoLumProcessBlock.inc.cpp`, `NeuralAmpModeler.cpp` reset/idle/latency paths, `VoLumSceneRig.inc.cpp`, and `VoLumLayoutBuild.inc.cpp`.
