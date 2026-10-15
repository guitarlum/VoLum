# 07 Fixed-degree filter core (AudioDSPTools)

Status: resolved
Blocked by: 01
Report: P8 (audio-03, own-10). Measured: 3 biquads 4.5 us -> 1.1 us per 64 frames, 0 mismatching blocks of
100k (`bench/bench.cpp` `FixedPeaking`). Used by the always-on DC high-pass, the tone stack, PRE EQ, PRE
levels and IR shaping on every instance.

## Paths
- Submodule `AudioDSPTools/dsp/RecursiveLinearFilter.cpp` / `.h`, branch `volum/perf-safe-wins` from
  `446b8977`, pushed to `https://github.com/guitarlum/AudioDSPTools.git`
- New or extended VoLum test (e.g. `NeuralAmpModeler/tests/test_volum_filter_core.cpp`)

## Change
In `recursive_linear_filter::Base::Process`, dispatch once per call on (input degree, output degree) to
fixed kernels for (3,3) biquads, (2,2) high-pass, (1,2) low-pass and (1,0) level, keeping the generic loop for
anything else. Each kernel keeps today's arithmetic exactly: `out = 0.0;` then one `out +=` statement per
term in today's term order (input terms current -> oldest, then output terms newest -> oldest), the
`isnan -> 0` scrub, and the same history ring (`mInputStart` / `mOutputStart` and `mInputHistory` /
`mOutputHistory` contents stay valid, so switching kernels or reading state is unaffected). Replace runtime
`%` with compare-and-wrap.

## Acceptance
- Bit-equality doctest: each fixed kernel vs the generic loop (kept as a reference) over random blocks, random
  coefficient sets, 1 and 2 channels, block sizes 1..512, including NaN input.
- `test_golden_dsp.cpp` `tone-stack` / `pre-eq` / compressor hashes and all goldens unchanged.
- Speed check: tone stack (3 biquads, 64 frames) <= 0.5x the generic path.

## Proof
Bit-equality red against a perturbed kernel (two terms swapped); speed check red against the generic loop.

## Verifier conditions
Term order and `0.0 +` start identical on arm64 (clang contracts per expression, so keep one term per
statement); ring state stays consistent for any caller; pointer points at a pushed commit.

## Result
- AudioDSPTools `volum/perf-safe-wins` 6aeac76 (parent 446b8977, pushed, ls-remote matches); parent c4313e20
  (pointer + `tests/test_volum_filter_core.cpp` + vcxproj/CMake + changelog).
- Kernels `_ProcessFixedDegree<3,3|2,2|1,2|1,0>`: same 0.0 start, one `out +=` per term in the loop's order, NaN
  scrub; rings written every sample exactly as before (previous samples read from registers holding the same
  values). Any other degree pair and `_ProcessGeneric` (test reference) use the untouched loop.
- Lock: fixed vs generic vs alternating, output bits + ring state (starts and contents), 60 random coefficient
  sets per pair (NaN in unused out[0], 1/8 unstable), 1<->2 channel flips, blocks 0..512, NaN/inf/-0/denormal
  input; plus LowShelf/Peaking/HighShelf/HighPass/LowPass/Level via SetParams at 44.1/48/96 kHz.
- Red: output terms swapped (1225 + 1390 block mismatches); input ring store removed (state mismatches);
  (3,3) dispatch off -> speed ratio 1.00 > 0.5. Green after revert. Logs in `evidence/07/`.
- Speed (tone stack 3 biquads, mono, 64 frames, best of 41 rounds): 2.40-2.83 us -> 0.64-1.03 us, ratio
  0.26-0.36 (this Core Ultra 7 155H). Enforced <= 0.5 on x86-64 only; printed elsewhere (not measured on arm64).
- Full suite: 1151/1152; all goldens unchanged. The one failure is test_volum_pitch_burst's local-only absolute
  deadline (worst blocks 1-7 ms, non-burst blocks up to 4.5 ms = preemption). It fails the same way on the
  pre-change build (`evidence/07/baseline-prechange-burst.log`) and passes 1 of 3 isolated reruns; not caused here.
