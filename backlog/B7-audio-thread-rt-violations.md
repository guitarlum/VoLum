# B7 — What the audio thread does that it must not

Deferred out of the 1.2.1 pre-release audit. Full evidence:
`audit-notes/opus5/P-audio-thread-signal-chain.md` (findings 2–6, 9, 12, 13, 15),
`audit-notes/opus5/P-neuralampmodeler-class.md` (findings 1, 2, 4, 12),
`audit-notes/opus5/P-process-lifetime-standalone.md` (findings 3, 4),
`audit-notes/gpt56/P1-core-lifecycle.md` (`F-P1-1`, `F-P1-2`, `F-P1-3`),
`audit-notes/gpt56/P9-dsp-effects.md` (`F-P9-1`, `F-P9-2`, `F-P9-3`, `F-P9-6`, `F-P9-7`),
`audit-notes/gpt56/P5-overlay-ui.md` (`F-P5-8`),
`audit-notes/gpt56/P8-triptych-art.md` (`F-P8-3`),
`audit-notes/gpt56/P4-content-store.md` (`F-P4-7`).

Ledger: `backlog/1.2.1-audit-deferred.md`. Supersedes the RT half of
`P2-volum-1.2.0-rt-and-perf-hardening.md`, which should be folded in or retired
when this is planned.

## What 1.2.1 already did

One violation was fixed, because it was audible on every switch: the diagnostic
log was being written from `ProcessBlock`, so each amp/channel/cab change stopped
to stat, possibly rename, and open a file. Logging now happens on the loader
thread.

Everything else on the list is still there. None of it is new in 1.2.1 — this is
long-standing structure, which is exactly why it did not get fixed on release
night.

## The violations

**Allocation and deallocation.** The staging drain frees multi-megabyte models
inside the callback and can run a full model `Reset()` there. The per-IR cut
filters allocate on their first engaged block. The default tone stack and the
PRE effects allocate at first use. Reverse Delay allocates its capture rings on
its first audio block. Delay and Reverb mode changes clear megabytes
synchronously in `ProcessBlock`.

**Constructing and destroying NAM models on the audio thread**, including
prewarming (opus5 `P-audio-thread` 3, 4).

**Host and UI calls from the callback.** `_UpdateLatency()` runs from
`_ApplyDSPStaging`, calling `SetLatency()` — a VST3 `restartComponent` — and
dereferencing `GetUI()` to mutate a label. In VST3, `OnParamChange` itself runs
on the audio thread and from there does UI rebuilds, filesystem scans and host
callbacks.

**Blocking synchronization.** `mStagingMutex` is held across a model reset and IR
resampling while the audio thread blocks on it; the loader mutex is acquired
blocking in the callback; `mNAMPaths.live` is written under the staging mutex on
the audio thread and read on the main thread without it.

**Unbounded work in one callback.** The tuner's YIN analysis is a
~4.2-million-iteration burst inside a single callback, and POLY's WSOLA search
does roughly 0.6–1.2 M interpolated ring reads per sample iteration — that one
presents as dropouts at small buffers, not as a theoretical concern.

**Exception safety.** A block larger than `GetBlockSize()` allocates and then
throws out of `ProcessBlock`; `ResamplingNAM::process` throws the same way.
`ENTER_PARAMS_MUTEX` is not RAII, so any exception escaping the callback
abandons a critical section for the life of the process.

**Non-atomic cross-thread state.** `mVolumLastLatencyReport` is a plain struct
written from both the audio thread and `OnIdle`; IR shaping is published as three
independent relaxed atomics, so the convolver can see a half-applied set.

## Why it is one ticket

These share a root cause: model/IR handoff is done *by* the audio thread rather
than *to* it. Fix the handoff — a lock-free single-producer queue with all
construction, resampling and destruction on the loader thread, and all host/UI
notification deferred to the main thread — and most of the list falls out. Fixing
them one at a time means touching `_ApplyDSPStaging` five times.

## Constraints

- **Audio must not change.** Every item here is a scheduling or lifetime defect.
  If a fix alters the rendered signal beyond removing a dropout or a denormal
  stall, it is the wrong fix.
- The 1.2.1 cab-swap transaction (`VoLumProcessingPlan.h`, the deferred-removal
  staging) is recent, tested, and audibly correct. Preserve its invariants;
  do not reopen the "one atomic move" guarantee.
- The pure planners (`VoLumProcessingPlan.h`, `VoLumUiSyncPlan.h`) are the model
  to follow: decision logic as a tested pure function, application as thin glue.

## Acceptance criteria

- A test that fails when the audio callback allocates, frees, locks or throws.
  A real-time-safety harness that instruments the allocator around a driven
  `ProcessBlock` is the cheapest version; macOS TSan/ASan runs
  (`run-tests-mac.sh --sanitize`) already exist and should be part of the gate.
- Model load, IR load, resample, prewarm and destruction all provably off the
  audio thread.
- `SetLatency` and every `GetUI()` touch provably on the main thread.
- Oversized blocks pass through without allocating and without throwing.
- The tuner and POLY do bounded work per callback at 64 frames / 44.1 kHz.
- Bit-identical output against a recorded reference for a fixed rig, before and
  after, at 44.1/48/96 kHz.

## Suggested phasing

1. Fix `mVolumLastLatencyReport`, the IR-shaping publication, and
   `ENTER_PARAMS_MUTEX`. Small, local, no design needed.
2. Move `SetLatency` / UI notification out of the callback onto a deferred flag
   the main thread drains.
3. Replace the staging drain with a lock-free handoff; move construction,
   resampling and destruction to the loader thread.
4. Pre-allocate every effect's worst case in `OnReset`, so nothing allocates on
   first use.
5. Bound the tuner and POLY, splitting their analysis across callbacks.
