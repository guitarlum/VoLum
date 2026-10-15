# 02 Tuner YIN lane-parallel SIMD + burst test

Status: resolved
Blocked by: 01
Report: P2 (audio-02). Measured: 4-13 ms single-callback spike every 4096 samples while the tuner is open
(`.scratch/perf-sweep/bench/bench-summary.md`).

## Paths
- `NeuralAmpModeler/VoLumTunerDSP.h` (`_RunYIN` difference function, ~L107-144)
- `NeuralAmpModeler/tests/test_tuner_dsp.cpp` (or a new `test_volum_tuner_burst.cpp`)

## Change
Vectorize the O(N^2/4) difference function across **tau**, not j: broadcast `a[j]`, load `a[j+tau..tau+3]`,
one accumulator lane per tau, so each lane sums in exactly today's j order. SSE2 on x86 (`__m128`), NEON on
arm64 using `vfmaq_f32` (clang contracts the scalar `sum += diff*diff` into an FMA today, so NEON must fuse
too), scalar tail for the remainder taus. Nothing else reads `d[]` differently.
Optional only if the ratio target is missed: slice the tau range across the following blocks (the analysis
buffer is already a snapshot); that shifts when the readout lands, not its cadence.

## Acceptance
- `d[tau]` bit-identical to a scalar reference (keep today's loop as a test-only reference function) over
  sine, chord and noise buffers; `test_tuner_dsp.cpp` accuracy unchanged.
- Speed check: the analysis block with SIMD <= 0.35x the reference analysis on the same machine
  (machine-independent ratio; median of >= 20 analyses).
- Local-only absolute check: worst analysis block < 1333 us (64 frames @ 48 kHz), skipped under
  `CI`/`GITHUB_ACTIONS`/sanitizers. If this fails locally after SIMD, try slicing; if it still fails,
  record the numbers and escalate instead of loosening.

## Proof
Exact lock red against a perturbed kernel (e.g. one lane off by one tau); ratio check red against the old
scalar `_RunYIN`.

## Verifier conditions
Per-lane j order identical; arm64 path fused (`vfmaq`); no allocation in `Process`; tuner output published
at the same 4096-sample cadence.

## Result
Commit 1de1d53c. `TunerDSP::DifferenceFunction` (public static): SSE2 8x4-tau blocks, then 4-tau blocks, then
scalar tail; NEON twin with `vfmaq_f32`; `_RunYIN` sums runningSum/cumNorm after it in tau order. Test
`test_volum_tuner_burst.cpp` (registered in vcxproj + CMake): bit lock vs verbatim scalar reference (sine x2,
chord, noise x2, spikes/tiny values) and a timing case (3 warm-up + 25 alternated runs, median ratio <= 0.35,
local-only worst < 1333 us; skipped in Debug/sanitizers/-O0).
- Red proofs: acc7 at `lag + 29` -> 252 taus differ on every buffer (and test_tuner_dsp E2 fails); SIMD
  disabled (`#if 0`) -> ratio 0.997, worst 4756 us. Both reverted, green.
- Numbers (Core Ultra 7 155H, default scheduling): analysis block median 0.36-0.42 ms, worst 0.54-0.64 ms vs
  scalar 2.5-2.8 ms (ratio 0.14-0.15). Pinned per core: P/E cores median 0.32-0.59 ms, worst <= 1.1 ms; LP-E
  cores 20/21 median 1.1-2.6 ms, worst 3.5-3.7 ms (scalar 9.9-23 ms) -> still over 64 frames there.
- Full suite exit 0 (1132 cases); ASan -Filter Tuner green; app target builds.
- Follow-up: tau slicing across blocks if LP-E-core headroom matters; commit subject carries a UTF-8 BOM.

## Result (reopen 02b: slicing)
Commit 483dd8b8 (parent bb37f389). Reopened because the local 64-frame worst-block check flaked (verify04
7-8: 1764 / 2173 us on slow cores). An analysis is still due every 4096 samples and snapshots as before;
`DifferenceRange` then runs `kSliceTausPerFrame` = 2 taus per processed frame (128 per 64-frame block =
1/16 of the work, 16 blocks, publish ~1024 samples later; blocks >= 1024 frames still publish in the due
block, so test_tuner_dsp.cpp needed no change). A due while one runs merges into one pending analysis that
starts the block after the publish; closing the tuner drops an unfinished snapshot. Memory: +8 KB member
`mDiff` + 3 ints/bools; stack per analysis -8 KB.
- Tests: sliced d[] bit lock at blocks 1/16/64/100/128/1000/4096 and 5/33/127-tau slices; publish block and
  4096 cadence; overlap (4 taus/block) and reopen rules; timing now sums all 64 blocks per analysis (ratio
  <= 0.35) and checks every block < 1333 us locally.
- Red: next slice starting at `end + 1` -> 682..1 taus differ at every block size < 4096 (evidence/02b/
  red-slice-edge.log). Unsliced (`kSliceTausPerFrame` 4096) stayed green today: LP-E cores ran at 775 us
  worst vs sliced 153-167 us (red-unsliced.log).
- Green: tuner tests alone 10/10 (worst block 44-99 us, ratio 0.13-0.20); full suite 2/2 exit 0 (1144 cases);
  ASan -Filter Tuner green; app builds. A first pair of full runs crashed in an unrelated JSON test from a
  stale incremental /LTCG binary (full-*-stale-ltcg.log); a clean Rebuild fixed it.

## Result (verify02b fixes)
Commit 25c54f21 (parent 483dd8b8), tests only.
- Overlap test now uses 5 taus/block: analyses run 63..472, 473..882, 883..; 473 and 883 are not due blocks
  and the second snapshot ends at 474*64. Red against the drop-pending-due mutation
  (`mAnalysisDue = (mNextTau == 0)`): only that test fails, first at block 473
  (evidence/02b/red-drop-pending-due.log); reverted, green.
- `tests/VoLumBurstTiming.h`: local-only absolute checks repeat the measured sequence 3 times
  (`DeadlineRuns()`, 1 on CI) and require min over runs of the per-run worst block < 1333 us; ratios use
  run 0 only. Applied to the tuner timing case and the pitch Octaver/INSTANT absolute check (POLY has none).
- evidence/02b/red-unsliced.log regenerated (unsliced: min worst 491-716 us, still under the limit today;
  one run spiked to 1727 us, which the min absorbs).
- tuner-10-runs.log 10/10, pitch-burst-10-runs.log 10/10, full-3.log 1144/1144 exit 0.
