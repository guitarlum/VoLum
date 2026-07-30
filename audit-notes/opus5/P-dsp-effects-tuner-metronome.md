I've read every line of the listed files plus the call sites that drive them (`VoLumProcessBlock.inc.cpp`, `VoLumSceneRig.inc.cpp` IR-shaping block, `_UpdateLatency`/`_VolumLatencyReport`, the app host's vector accumulator) and the `recursive_linear_filter` / `ImpulseResponse` code they depend on.

Two things I checked and *cleared*, so they don't appear below: `_UpdatePeriod`'s parabolic interpolation is provably bounded to ±0.5 lag (rm, rp ≤ best by construction), so the period estimate cannot leave the searched range; and denormals are globally handled by `disable_denormals()` at the top of `ProcessBlock` (FTZ+DAZ, 0x8040), so the unflushed envelope states in the compressor and tremolo are harmless.

---

# FINDINGS

## 1. MAJOR — Returning SEMI to 0 freezes the read pointer wherever the last splice left it; actual delay stops matching reported PDC

**WHERE** `NeuralAmpModeler/VoLumPitchShifter.h:200-278` (and `:145` for the contrast)

**MECHANISM** The read pointer only moves when the ratio is off unity, and it is only re-centred on a *character* change:

```201:  const double grow = 1.0 - f; // delay change per sample
222:      mDelay += grow;
239:        if (f < 1.0 && mDelay > mDHi)
253:        else if (f > 1.0 && mDelay < mDLo)
```

At `semitones == 0`, `mRatio == 1.0` exactly, so `grow == 0.0` and neither splice branch can fire. `mDelay` is frozen at whatever value the last splice + drift left it at. `SetRatio` (`:190`) only clamps; `_ApplyCharacters`/`SetCharacter` (`:126`) early-return when the character is unchanged, so the re-centring at `:145` — whose own comment says *"This matters most at ratio 1.0, where the delay never drifts back into the new band on its own"* — never runs on a ratio change. Meanwhile the dry path is delayed by exactly `mLatency` (`:622`) and the host compensates `mLatency` via PDC (`NeuralAmpModeler.cpp:2241`).

**TRIGGER** Enable PRE Pitch, Transpose, any non-zero SEMI, play for a moment, then set SEMI back to 0. At 48 kHz, POLY: `mLatency = 674`, but `mDelay` can be anywhere in `[194, 1826]` (`dLo` up to `dHi + search` minus in-fade drift) → **−10 ms to +24 ms** of uncompensated misalignment. DROP/INSTANT: ±6 ms. Persists indefinitely until SEMI is moved off 0 again.

**IMPACT** With Mix=1 (the default) the whole signal is up to 24 ms later than the host was told, so monitored/recorded guitar is out of time with everything else and the pedal feels laggy in a way that only appears *after* you've used a shift. With Mix<1 dry and wet sit up to 26 ms apart → a permanent slapback/comb on a "0 semitones" setting. Because `mDelay` also lands on a fractional value, the unity-shift path additionally picks up a fixed interpolation low-pass it does not have from Reset.

**CONFIDENCE** certain (arithmetic, no measurement needed)

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_pitch.cpp:501` ("0 semitones Mix=1 is a clean delayed passthrough") sets 0 semitones *from Reset*, where `mDelay == mLatency` by construction and then never moves. No test sets a non-zero shift and then returns to 0. That exact test would fail if it did.

**FIX SKETCH** On the edge where `f` becomes exactly 1.0, do what `SetCharacter` already does — target `mLatency` — but via the existing crossfade machinery (`mDelayNew = mLatency; mFading = true; mFadePos = 0`) so there is no discontinuity. **Does not change the sound**: at unity ratio the output is a delayed copy of the dry either way; this only fixes *when* it arrives. (A bare `mDelay = mLatency` assignment would click; use the splice path.)

---

## 2. MAJOR (presents as BLOCKER-grade dropouts at small buffers) — POLY's WSOLA search does ~0.6–1.2 M interpolated ring reads inside a single sample iteration

**WHERE** `NeuralAmpModeler/VoLumPitchShifter.h:396-432`, sized at `:334-336`

**MECHANISM** POLY's search and correlation window are both 16 ms:

```334:      t.search = std::max(1, static_cast<int>(std::lround(sr * 0.016)));
335:      t.corrWin = std::max(8, static_cast<int>(std::lround(sr * 0.016)));
```

`_WsolaRefine` is an exhaustive nested loop with no early-out:

```411:    for (int lag = -mSearch; lag <= mSearch; ++lag)
418:      for (int j = 0; j < win; ++j)
420:        const double v = _ReadAtDelay(dc + j);
```

At 48 kHz that is `(2·768+1) × 768 = 1,180,416` calls to `_ReadAtDelay` for an upshift splice (~591 k for a downshift, where the `dc < xfade+1` guard prunes the negative half). Each call does a `floor`, **two runtime-divisor integer modulos**, and a lerp:

```386:    const size_t i0 = static_cast<size_t>(fl) % mBuf.size();
387:    const size_t i1 = (i0 + 1) % mBuf.size();
```

All of that runs inside one iteration of the per-sample loop, i.e. inside one audio callback. Splice cadence is one grain per `band`: ~25/s upshifting at +7 st (the ceiling the fix pins), ~60/s downshifting.

**TRIGGER** PRE Pitch, Transpose, POLY, any non-zero SEMI. Worst at small host/device buffers.

**IMPACT** At ~20–60 cycles per `_ReadAtDelay`, one splice costs roughly 3–10 ms of a single callback. Against a 128-frame ASIO buffer (2.7 ms) or a 64-frame block that is an overrun ~25–60 times a second: crackle/dropout that will read to the user as exactly the POLY crackle that was supposedly fixed. Average load is also ~25–40 % of a core for one pedal.

**CONFIDENCE** likely — the operation count is certain; the wall-clock per splice depends on vectorisation and the CPU's `div` throughput. Easy to confirm: time `polySpliceCount(pow(2,7.0/12.0), 48000)` in `test_volum_pitch.cpp:364` and divide by the splice count it returns.

**WOULD A TEST HAVE CAUGHT IT** No. Every POLY test asserts output values only; none has a time budget. The suite pays this cost silently (~200 splices across the POLY cases).

**FIX SKETCH** Two strictly bit-exact changes, no voicing impact:
- Drop both modulos in `_ReadAtDelay`. `rp` is already wrapped into `[0, sz)` by the two `while` loops, so `i0 = (size_t)fl` is already in range and `i1` only needs `i0+1 == sz ? 0 : i0+1`. Hoist `mBuf.size()` into a local. **Bit-identical output**, roughly 2–3× faster.
- See finding 3 for a second free win on the same code path.

Anything that shortens `search`/`corrWin` **would change the sound** (the splice alignment, hence POLY's texture) and must be deferred past 1.2.1.

---

## 3. MAJOR — POLY runs the autocorrelation pitch tracker every 10 ms and then never uses the result

**WHERE** `NeuralAmpModeler/VoLumPitchShifter.h:207-211` and `:436-481`

**MECHANISM** `_UpdatePeriod` is called unconditionally from the sample loop:

```207:      if (--mPeriodCountdown <= 0)
209:        mPeriodCountdown = mPeriodUpdate;
210:        _UpdatePeriod();
```

but POLY (fixed-grain) never reads `mPeriod`:

```238:        const double P = mFixedGrain ? mBand : mPeriod;
```

`_UpdatePeriod` is `(tmax − tmin) × L` multiply-adds — at 48 kHz, `1120 × 1200 ≈ 1.34 M` — plus a 2400-sample `_ReadAtDelay` gather (`:444-445`), all in one sample iteration, `mPeriodUpdate = 480` samples apart (every 10 ms).

**TRIGGER** PRE Pitch, Transpose, POLY, any settings including SEMI=0.

**IMPACT** A wasted ~0.3–1 ms burst inside one callback every 10 ms, stacking with finding 2. Contributes to the same dropouts.

**CONFIDENCE** certain that the work is dead; likely for the ms figure.

**WOULD A TEST HAVE CAUGHT IT** No — no CPU assertions anywhere.

**FIX SKETCH** `if (!mFixedGrain && --mPeriodCountdown <= 0)`. **Bit-identical output for POLY** (the value is provably unread) and untouched for DROP/INSTANT.

---

## 4. MAJOR — The new per-IR cut filters allocate on the audio thread on first use, and carry stale history across every enable

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:758-783`; `AudioDSPTools/dsp/dsp.cpp:58-81`, `RecursiveLinearFilter.cpp:73-93`

**MECHANISM** The cuts are conditionally engaged per block:

```769:  if (lowHz > 0.0)
771:    auto& f = support ? mSupportIrLowCut : mIrLowCut;
772:    f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, lowHz));
773:    p = f.Process(p, numChannels, nFrames);
```

`Base::Process` calls `_PrepareBuffers`, and on the *first* call `_GetNumChannels() == 0 != 1`, so it takes the resize path: `mOutputs.resize(1)`, `_ResizePointers(1)` → `delete[]` + **`new DSP_SAMPLE*[1]`** (`dsp.cpp:33`, which can also `throw`), `mInputHistory.resize`, `mOutputHistory.resize`, `mOutputs[0].resize(nFrames)`. Nothing in `OnReset` touches `mIrLowCut`/`mIrHighCut`/`mSupportIrLowCut`/`mSupportIrHighCut`, so that first call is whenever the user first enables a cut.

Separately, `recursive_linear_filter` has no history-clear API, and `_PrepareBuffers` only zeroes history on a *channel-count* change. So when a cut is turned off and later back on (or a different IR with a cut becomes active), the filter resumes from `mInputHistory`/`mOutputHistory` left over from the last time it ran. For the high-pass, `y[n] = α(x[n] − x[n−1]) + α·y[n−1]` with a stale `x[n−1]`/`y[n−1]` is a step of up to full scale decaying over ~2 ms at 80 Hz.

**TRIGGER** Import a custom IR, open its panel, step the low-cut off OFF (first ever heap allocation on the audio thread, mid-performance). Toggle it off and on again while playing (stale-history click each time).

**IMPACT** A heap allocation (and a potential exception) on the real-time thread at an arbitrary user-chosen moment — a lock/page-fault stall long enough to drop a buffer; plus an audible click every time a cut is engaged.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_ir_shaping.cpp` is entirely pure model helpers (clamps, ladders, typed entry, JSON) — it never instantiates or runs `_VolumApplyIrShaping` or any filter. There is no DSP-level test of the IR bus at all.

**FIX SKETCH** In `OnReset`, run one block of zeros through all four filters at the reset block size to force the allocation and zero the history off the audio thread; and re-run that zero-priming on the 0→non-zero cut edge (track the previous `lowHz`/`highHz` per lane). **Does not change the sound** — it removes a click and an allocation, and the steady-state response is identical.

---

## 5. MAJOR — The metronome is silenced whenever the tuner overlay is open

**WHERE** `NeuralAmpModeler/NeuralAmpModeler.cpp:666-674`; `VoLumMetronomeDSP.h:81-136`; `VoLumProcessingPlan.h:51`

**MECHANISM** The click is summed into the output bus, then the tuner mute blanks the whole bus:

```667:  mMetronomeDSP.Process(outputs, nFrames, static_cast<int>(numChannelsExternalOut));
670:  if (processingPlan.silenceForTuner)
672:    for (size_t c = 0; c < numChannelsExternalOut; c++)
673:      std::memset(outputs[c], 0, numFrames * sizeof(iplug::sample));
```

`plan.silenceForTuner = tunerActive` unconditionally, and the two are independent toggles (`VoLumSceneRig.inc.cpp:12`, `VoLumLayoutBuild.inc.cpp:1306`) — nothing prevents both being on.

**TRIGGER** Turn the metronome on, then open the tuner. Clicks stop. Close the tuner, clicks return.

**IMPACT** The metronome silently stops working. The user's most likely reading is "the metronome is broken", not "the tuner mutes it". The DSP keeps counting, so the click reappears mid-bar.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No. `test_metronome_dsp.cpp` tests `MetronomeDSP` in isolation; nothing tests the ProcessBlock ordering of metronome vs. tuner mute.

**FIX SKETCH** Move the `mMetronomeDSP.Process` call after the `silenceForTuner` memset. **Does not change the sound** of any effect; the click is a synthesised sum, not part of the guitar chain.

---

## 6. MINOR — Tuner cannot detect the low strings at 96 kHz and above

**WHERE** `NeuralAmpModeler/VoLumTunerDSP.h:27, 147-152`

**MECHANISM** The YIN lag range is clamped to the analysis half-window, which is a fixed sample count, not a fixed time:

```27:  static constexpr int kBufferSize = 4096;
148:    int maxTau = static_cast<int>(mSampleRate / kMinFreq);
151:    if (maxTau >= halfBuf)
152:      maxTau = halfBuf - 1;
```

`halfBuf = 2048`, so the lowest detectable frequency is `sampleRate / 2047`, not `kMinFreq = 20 Hz`.

**TRIGGER** Run the standalone or plugin at 176.4/192 kHz: the floor becomes 93.8 Hz, above standard low E (82.41 Hz) — no low string tunes. At 88.2/96 kHz the floor is 46.9 Hz, which excludes 8-string F#1 (46.25 Hz) — a range this codebase explicitly designs for elsewhere (`VoLumPitchShifter.h:80-88` calls out drop C, 7-string B1 and 8-string F#1 by name).

**IMPACT** At 192 kHz the tuner reads nothing (or an octave-up harmonic) for every note below G. At 96 kHz the lowest 8-string note cannot be tuned.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No. Every case in `test_tuner_dsp.cpp` is hardcoded to 48 kHz; the low-E case (line 37) passes there because `maxTau` clamps to 2047 and E2's period is 582 samples.

**FIX SKETCH** Scale the analysis window with the sample rate (e.g. `max(4096, next_pow2(3 · sampleRate / kMinFreq))`) or decimate the input to a fixed internal analysis rate. **Does not change the sound** — the tuner is analysis-only and its output is silenced anyway. Note this makes finding 7's cost worse, so pair them.

---

## 7. MINOR — YIN's O(N²) analysis burst lands entirely inside one audio callback

**WHERE** `NeuralAmpModeler/VoLumTunerDSP.h:58-62, 132-144`; called from `NeuralAmpModeler.cpp:576`

**MECHANISM** `_RunYIN` fires from the audio thread on the sample where the buffer fills, and does `2047 × 2048 ≈ 4.2 M` difference-function iterations in that one call, plus two 8 KB stack arrays.

**TRIGGER** Tuner active at a 64- or 128-frame buffer.

**IMPACT** ~1–2.5 ms of work every 4096 samples (every 85 ms at 48 kHz, every 21 ms at 192 kHz). Mitigated in practice because `silenceForTuner` mutes the bus while the tuner is open — the xrun exists but has little to be heard on. Still an unbounded per-callback spike, and it steals headroom from whatever else the interface is driving.

**CONFIDENCE** likely

**WOULD A TEST HAVE CAUGHT IT** No — no timing assertions.

**FIX SKETCH** Spread the tau loop across callbacks (a fixed tau budget per block, publishing when the sweep completes), or hand the analysis to a worker. **Does not change the sound.**

---

## 8. MINOR — IR trim is pushed to the audio thread unsmoothed and one block ahead of the convolver swap

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:735-765`, `:571-572`, `:700-701`

**MECHANISM** `_VolumPushIrShaping` stores a raw linear gain that `_VolumApplyIrShaping` applies with no ramp:

```761:  const double trim = (support ? mSupportIrTrimLin : mIrTrimLin).load(std::memory_order_relaxed);
762:  if (trim != 1.0)
765:        in[c][i] = static_cast<iplug::sample>(static_cast<double>(in[c][i]) * trim);
```

Two consequences. (a) Dragging or stepping the trim (range ±24 dB, `VoLumContentStore.h:93-94`) changes the gain in one jump per block → zipper. (b) On IR select, the shaping is pushed on the main thread at `:572` while the convolver only swaps in the next block's `_ApplyDSPStaging`; on immediate clear at `:701` the trim resets to unity while `mShouldRemoveIR` is likewise honoured a block later. So for one block the *old* convolver output is scaled by the *new* trim (up to 24 dB wrong, or 18 dB low on a clear). The three atomics are also stored individually, so a block can see a new trim with old cut frequencies.

**TRIGGER** Step the trim on a shaped IR while playing; or switch/clear an IR while playing a sustained note.

**IMPACT** Zipper while adjusting trim; a single-block level burst or dip (a click) on IR switch and on clear.

**CONFIDENCE** certain for (a); likely for the audibility of the one-block (b) burst.

**WOULD A TEST HAVE CAUGHT IT** No — `test_volum_ir_shaping.cpp` never exercises the DSP or the staging handoff.

**FIX SKETCH** Smooth `trim` toward its target with a one-pole (~10–20 ms) in `_VolumApplyIrShaping`, and gate the shaping push on the same "swap has landed" signal `_VolumFlushDeferredIrShaping` already implements for the deferred direction. **Does not change the steady-state sound** — the settled trim value is unchanged; only the transition is ramped.

---

## 9. MINOR — Switching the Pitch character or mode steps the dry delay line in one sample

**WHERE** `NeuralAmpModeler/VoLumPitchShifter.h:620-628`, `:672-685`, `:140-148`

**MECHANISM** The dry tap is read at a delay derived directly from the live latency:

```622:    const size_t lat = static_cast<size_t>(std::min(mLatency, static_cast<int>(ringLen) - 1));
626:      mDryScratch[i] = mDryRing[(mDryWrite + ringLen - lat) % ringLen];
```

`_ApplyCharacters` reassigns `mLatency` the instant the character pill or mode changes, and `SetCharacter` simultaneously slams `mDelay = mLatency` and aborts any in-flight crossfade. Nothing crossfades either path.

**TRIGGER** Click INSTANT → POLY → DROP, or Transpose ↔ Octaver, while a note is ringing. At 48 kHz `mLatency` moves between 413 (INSTANT), 674 (POLY) and 809 (DROP) — up to 396 samples (8 ms) of instantaneous jump in both dry and wet.

**IMPACT** An audible click on every character/mode switch.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_pitch.cpp:205` switches characters but only reads back `Latency()`; nothing streams audio across a switch and looks for a discontinuity.

**FIX SKETCH** Ramp `lat` toward the new value over a few ms, or crossfade two dry taps across the switch. **Would change the sound only at the moment of the switch** (a short blend replacing a hard step); steady-state voicing on every character is untouched. Safe, but it is a behaviour change at the switch instant, so it is your call whether it belongs in 1.2.1.

---

## 10. MINOR — The Octaver's up-octave voice resumes from a stale ring after a round trip through Transpose

**WHERE** `NeuralAmpModeler/VoLumPitchShifter.h:640-659`, `:672-685`, `:578-585`

**MECHANISM** In Transpose only `mVoices[0]` is processed (`:633`); `mVoices[1]` is dormant, so its `mWrite`, `mWriteCount`, `mDelay`, `mDelayNew`, `mFading`, `mFadePos` and ring contents freeze. On return to Octaver, `_ApplyCharacters` calls `SetCharacter(Character::Drop)` on it — but that early-returns at `:126` because the voice was already Drop, so nothing is re-centred and nothing is cleared. `VoLumPitch::Reset()` (`:578`) is only reached from `OnReset`.

**TRIGGER** Octaver with OCT UP > 0 → switch to Transpose → play → switch back to Octaver.

**IMPACT** The up-octave voice outputs ~23 ms (up to `dHi`) of audio from *before* the mode switch, with a hard discontinuity where the frozen write head sits; if `mFading` was mid-crossfade it resumes at a stale `mFadePos` with `mDelayNew` now pointing somewhere unrelated. Masked at the shipping default (`kPrePitchOctUp` = 0.0) but plainly audible once OCT UP is used.

**CONFIDENCE** likely (certain that the state is stale; audibility depends on OCT UP)

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_pitch.cpp:575` iterates modes but constructs a fresh `VoLumPitch` per mode, so it never exercises a live mode switch on a warm engine.

**FIX SKETCH** In `_ApplyCharacters`, `Reset()` any voice that is transitioning from dormant to active (track a per-voice "was processed last block" flag). **Does not change the sound** — it replaces a stale-audio blip with the silent onset the first-ever Octaver engage already has.

---

## 11. MINOR — Tremolo SHAPE reaches the gain path unsmoothed while DEPTH and MIX are smoothed

**WHERE** `NeuralAmpModeler/VoLumTremolo.h:137, 169-170, 240-260`

**MECHANISM** `mDepth` and `mMix` get a ~15 ms one-pole (`:169-170`, `:225`); `mShape` is assigned raw at `:137` and used directly in `_ModGain`:

```255:      const double drive = 1.0 + mShape * mShape * mShape * 12.0;
256:      shaped = std::tanh(bipolar * drive) / std::tanh(drive);
```

At mid-slope the shaping moves the unipolar gain substantially (e.g. `bipolar = 0.5`: 0.75 at shape 0 vs ~1.0 at shape 1), so a jump in `mShape` is a step in gain of up to ~2.5 dB at full depth.

**TRIGGER** Automate SHAPE, recall a preset with a different SHAPE, or drag the knob quickly.

**IMPACT** A click on a jump, zipper on a drag.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No — `test_volum_tremolo.cpp:227` compares steady-state shape values; no test changes SHAPE mid-stream.

**FIX SKETCH** Give `mShape` the same `mSmoothCoef` treatment as depth/mix. **Would not change the sound at any fixed SHAPE setting**; it only affects the transition.

---

## 12. MINOR — Engaging Tremolo starts the LFO at mid-gain, producing an instant level step

**WHERE** `NeuralAmpModeler/VoLumTremolo.h:150-157`; `VoLumProcessBlock.inc.cpp:214-215`

**MECHANISM** The bypass edge resets the engine, and `Reset` zeroes the phase and snaps depth and mix straight to their targets:

```152:    mPhase = 0.0;
153:    mDepth = mDepthTarget;
154:    mMix = mMixTarget;
```

At phase 0, `sin(0) = 0` → `unipolar = 0.5` → gain = `1 − depth/2`. Since the knob mapping floors internal depth at 0.40 (`:97-101`), the step is never smaller than −1.9 dB and reaches −6 dB at full depth.

**TRIGGER** Toggle Tremolo on while a note is ringing.

**IMPACT** An audible level drop the instant the pedal engages, before the first throb.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No — the mode-switch test (`:204`) only asserts finiteness and bounds.

**FIX SKETCH** Either start the phase at the unity peak (0.25) or ramp `mMix` in from 0 rather than snapping it. **Both would change the sound**: the first shifts where in the LFO cycle the throb begins, the second softens the engage. Flagging so it can be deferred rather than applied under the voicing freeze.

---

## 13. MINOR — The IR cut filters' corner frequency moves with sample rate

**WHERE** `AudioDSPTools/dsp/RecursiveLinearFilter.h:144-148, 179-183`, used at `VoLumSceneRig.inc.cpp:772, 779`

**MECHANISM** These one-poles use the forward-Euler coefficient `α = ω/(ω+1)` (low-pass) and `1/(ω+1)` (high-pass) with `ω = 2πf/sr`, which only approximates the intended corner for `ω ≪ 1`. A 5 kHz high-cut gives `ω = 0.712, α = 0.416` at 44.1 kHz versus the exact `1 − e^{−ω} = 0.509` — the filter behaves like roughly 3.5 kHz. At 192 kHz the same setting lands within a few percent of 5 kHz. (`VoLumTremolo.h:216-220` uses the correct exponential form, so the two are inconsistent.)

**TRIGGER** Set a high-cut on a custom IR, compare 44.1 kHz vs 96/192 kHz.

**IMPACT** The same stored IR sounds noticeably darker at 44.1 kHz than at 96 kHz, and the number in the popover does not mean what it says at low rates. Stability is not at risk — `α ∈ (0,1)` for all positive `ω`, so both filters stay stable across the whole range and the 20 kHz ceiling at 44.1 kHz is merely a gentle tilt rather than a cut.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No — no test runs these filters at any rate.

**FIX SKETCH** Switch to `α = 1 − e^{−ω}` (the form the tremolo already uses). **This WOULD change the sound at every sample rate, including 48 kHz** — every existing IR with a cut would get brighter. Defer past the voicing freeze.

---

## 14. MINOR — IR auto-normalize measures energy the convolver never uses

**WHERE** `NeuralAmpModeler/VoLumSceneRig.inc.cpp:789-826`; `VoLumContentStore.h:100-131`; `AudioDSPTools/dsp/ImpulseResponse.cpp:75-83`

**MECHANISM** The migration integrates the whole decoded file at the file's own sample rate:

```805:      double sumSq = 0.0;
806:      for (float v : audio)
807:        sumSq += static_cast<double>(v) * static_cast<double>(v);
808:      trimDb = volum::content::AutoNormalizeIrTrimDb(std::sqrt(sumSq));
```

The convolver only uses `min(resampled.size(), 8192)` taps and applies `gain = 10^{-18/20} · 48000/mSampleRate` (`ImpulseResponse.cpp:75-80`). So the measurement differs from the thing it is calibrating in two ways: it counts energy past tap 8192 (a 200 ms IR at 48 kHz is already 9600 samples, and reverb-tailed captures far more), and it ignores the `48000/sr` term, which combined with the longer resampled kernel leaves a residual ~3 dB level shift per octave of sample rate. The result is written to disk with `trimCalibrated = true`, so it is never recomputed.

**TRIGGER** Migrate a pre-1.2.1 library containing an IR longer than ~170 ms; or migrate at 48 kHz and then run the plugin at 96 kHz.

**IMPACT** The stated goal ("effective convolution gain toward unity") is missed: long IRs land quieter than short ones, which is the complaint the migration exists to fix, and the level shifts with sample rate. Bounded by the ±24 dB clamp, so no runaway.

**CONFIDENCE** certain for the mechanism; likely for the per-IR magnitude (depends on tail energy)

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_ir_shaping.cpp:34` pins `AutoNormalizeIrTrimDb` against its own closed form — it verifies the formula is implemented as written, not that the L2 fed to it is the one the convolver sees.

**FIX SKETCH** Sum only the first `min(size, 8192)` samples and fold in the `48000/sr` factor. **This WOULD change the level of every already-migrated IR**, including ones the user has since re-trimmed by hand. Defer; do not apply during the freeze.

---

## 15. MINOR — Standalone round trip omits the app's 64-frame vector accumulator

**WHERE** `NeuralAmpModeler/VoLumLatencyReport.h:70-73`; `NeuralAmpModeler.cpp:2292-2306`; `iPlug2/IPlug/APP/IPlugAPP_host.cpp:885-928`

**MECHANISM** The report is `pluginSamples + driverFrames`:

```70:inline int RoundTripFrames(const LatencyReport& r)
72:  return DriverLatencyKnown(r) ? r.pluginSamples + r.driverFrames : 0;
```

with `r.pluginSamples = GetLatency()` — our PDC only. But the standalone host pops output *before* pushing input each device frame and only processes on a full vector (`IPlugAPP_host.cpp:887-927`), so output frame *i* is emitted at device frame *i + APP_SIGNAL_VECTOR_SIZE*. `APP_SIGNAL_VECTOR_SIZE` is 64 (`config.h:57`), and that 64 is in neither `pluginSamples` nor the driver's figure.

**TRIGGER** Any standalone session with an ASIO driver that reports latency.

**IMPACT** The headline "Round trip" under-reports by 1.33 ms at 48 kHz / 1.45 ms at 44.1 kHz. Small against a typical 21 ms, but this readout exists specifically to stop under-reporting, and the file's own comment argues that a flattering number is worse than none.

**CONFIDENCE** certain

**WOULD A TEST HAVE CAUGHT IT** No. `test_volum_latency_report.cpp` tests the pure formatting/arithmetic of `LatencyReport` and never asserts what `_VolumLatencyReport()` populates it with; `test_iplug_app_vector_accumulator.cpp:73-77` checks the accumulator's output *sequence* (`out[i] == in[i] + 0.25`) but not its device-time alignment, so the one-vector delay is invisible to it.

**FIX SKETCH** In `_VolumLatencyReport`, add `APP_SIGNAL_VECTOR_SIZE` to `pluginSamples` under `#if defined(APP_API)` (or carry it as a separate field so the detail line can name it). **Does not change the sound**; it changes the displayed number, which is the point.

---

# NITs

- `VoLumPitchShifter.h:239-277` — for `f > ~2.0` the crossfade drives `mDelay` negative (POLY at f=4: `2 + xfade·(2−f) = −382`), so the outgoing tap reads ahead of the write pointer, i.e. buffer-length-old garbage. `_ReadAtDelay`'s wrap keeps it in-bounds (no crash). Unreachable today — the EParam and `Unserialization.cpp:681` both cap at −12..+7 — but `SetParams` advertises ±24 st and `SetRatio` 0.25..4.0, and `test_volum_pitch.cpp:678` already exercises f=2.0, which lands at `mDelay ≈ 1`. Clamp the crossfade so `mDelay` cannot fall below ~1 if that range is ever widened.
- `ToneStack.cpp:22-61` — `BasicNamToneStack`'s constructor calls `SetParam` while `GetSampleRate() == 0.0`, so `GetOmega0()` is `inf` and every biquad coefficient is NaN until `Reset` runs. Masked by `RecursiveLinearFilter.cpp:60` (`isnan(out) → 0`, i.e. silence rather than poison) and by `OnReset` always preceding `ProcessBlock`. Guard `SetParam` on `sampleRate > 0` the way `VoLumPreEffects.h:45` already does.
- `NeuralAmpModeler.cpp:1229-1231` — `mToneStack->SetParam` runs on the main thread from `OnParamChange` while the audio thread reads the same coefficient vectors in `Process`; the five doubles are written unsynchronised, so a block can see a torn coefficient set. Pre-existing upstream pattern, mild given the shelving filters' low Q, but it is a genuine race.
- `VoLumTunerDSP.h:265` — `std::atomic<TunerResult>` over a 20-byte struct is not lock-free, so `mResult.store(...)` at `:239`/`:252` takes the runtime's internal spinlock **on the audio thread**, against a UI-thread reader. Publish the fields as separate lock-free atomics or a seqlock.
- `VoLumTunerDSP.h:199-205` — a NaN in the input survives every guard (`rms < 0.0005f`, `minVal > 0.5f`, `freq < kMinFreq || freq > kMaxFreq` are all false for NaN) and reaches `_BuildResult`, where `static_cast<int>(std::round(NaN))` is UB. `noteIndex` is masked back into range so there is no OOB read, but the display shows garbage. Add `!std::isfinite(freq)` to the range check.
- `VoLumPitchShifter.h:718-721` — the oversized-block fail-safe returns `inputs` *undelayed* while the host still compensates the reported PDC, and skips the `mDryRing` writes for that block, so a second glitch surfaces `mLatency` samples later when the gap is read out. Needs a >8192-frame block, so effectively unreachable; behaviour is asserted by `test_volum_pitch.cpp:163-170`.
- `VoLumPitchShifter.h:661-665` — only `mOut[0]` is written, but `_Pointers(numChannels)` hands back both channels, so at `numChannels == 2` channel 1 would be silence. Safe today (`kNumChannelsInternal == 1`).
- `VoLumPitchShifter.h:103-115` — `mRefWin` is sized from the character chosen as "worst" by `dHi + search + corrWin`, but what it actually needs is `max(corrWin)`. POLY happens to win both metrics at every supported rate (`0.056·sr` vs DROP's `0.029·sr`), so there is no overflow — but the two criteria are unrelated and a future retune could decouple them into an out-of-bounds write at `:405`. Size it from `max(corrWin)` explicitly.
- `VoLumMetronomeDSP.h:91-109` — a BPM change recomputes `mSamplesPerBeat` without touching `mSampleCounter`, so speeding up while `mSampleCounter` already exceeds the new period fires one click immediately. Arguably desirable; noting it as a deliberate-looking edge rather than a defect.
- `VoLumLatencyReport.h:45-50` — with `sampleRate == 0` (before the first `OnReset`) the headline reads "Plugin latency: 0.0 ms (N samples)" for a non-zero N. Pinned as intended by `test_volum_latency_report.cpp:140`; the poll in `OnIdle` corrects it as soon as the stream opens.

I've confirmed the memory allocation happens on the audio thread and can throw, which is good. Now I'm finalizing the confidence and severity levels for the report. Let me double-check the first finding's logic—`mDelay` stays frozen when `grow` equals zero, and the conditional branch behavior checks out.