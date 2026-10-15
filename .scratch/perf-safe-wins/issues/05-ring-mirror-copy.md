# 05 A2 ring tail-mirror copies only written columns (NeuralAmpModelerCore)

Status: resolved
Blocked by: 04
Report: P5 (nam-07). Verified impact: under 1% of a FULL lane at 64 frames; more at large host blocks.

## Paths
- Submodule `NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp` (~L351-352 layers, ~L383-384 head), same fork
  branch as 04
- `NeuralAmpModeler/tests/test_volum_nam_exact.cpp` (extend with a 512-frame and a 2048-frame host block
  case so the mirror boundary is crossed many times)

## Change
Replace the unconditional `memcpy(hist + pow2_size*C, hist, mbs*C*4)` with a copy of only the columns this
block wrote inside `[0, mbs)` (the wrapped part `[0, num_frames-first)` or `[wp, wp+first)` intersected with
`[0, mbs)`). Most blocks copy nothing.

## Acceptance
- Exact lock (all block sizes) green; goldens green; budget medians at 64 / 128 logged.

## Proof
Lock red against a perturbed copy range (one column short), green on the real change.

## Verifier conditions
Every column any later read can touch in `[pow2_size, pow2_size+mbs)` still mirrors `[0, mbs)`; covers the
first block after Reset/prewarm and block sizes above and below `mbs`.

## Escalation (2026-09-26, worker; left uncommitted, Status stays claimed)
- Change done and bit-exact: `refresh_tail_mirror` re-mirrors only `[wp, wp+first)` and `[0, n-first)` clipped
  to `[0, mbs)` (layers + head). Invariant by induction: `SetMaxBufferSize` (also on every `Reset`) zero-fills
  ring + tail; a ring write changes only those columns, all in `[0, pow2)`; so after every write the whole tail
  mirrors `[0, mbs)` before the same block's reads. Layer reads span `[base, base+n)` with `base < pow2`, so
  they touch at most `pow2+mbs-1`; head reads mask per column and never touch its tail.
- Lock extended (512, 2048, and a varied 400-reserve cycle `{400,1,399,37,805,200,3}`); pre-change hashes
  equal the 64-frame ones (A2 output is block-size independent). Green after the change. Red: copy one column
  short -> 11/11 fail; skip the unwrapped-segment refresh -> 9/11 fail.
- **Blocker:** full suite fails `test_volum_realtime_budget` ratio guard (`ratio <= 0.6`, production vs
  8192-reserve Reset): 0.89-0.99 in every row after, 64/128 rows pass before. The 8192-reserve penalty that
  guard detects *was* this unconditional copy (8192 cols x 24 rings = 6 MB memcpy per block). Keeping the
  change needs a new signal for that guard (e.g. assert the Reset size directly) = threshold/test change ->
  owner call per spec "Escalate and stop".
- Work saved: `_05-core.patch` (NAMCore) and `_05-parent-tests.patch`; also still in the working tree.

## Owner decision (2026-09-26 16:35)
Accept the change. Replace the realtime-budget 'ratio <= 0.6' assertion (production reset vs 8192 reserve) with a direct check that production resets at NamResetBlockSize (the call sites are already source-locked in test_volum_ui_regressions.cpp ~L2379-2385); keep the absolute deadline-share check (Myth -> Soldano < 0.50); keep printing the ratio as INFO. This is the only threshold change the owner has approved.

## Result
- NAMCore 2d31bbf on volum/perf-safe-wins (pushed, ls-remote verified; parent f9836c5): refresh_tail_mirror
  copies only written columns inside [0, mbs), layers + head. Parent: pointer bump + lock cases + guard change.
- Lock (test_volum_nam_exact.cpp) +7 cases (512, 2048, varied 400-reserve cycle), pinned from pre-change core.
  Red: copy one column short -> 11/11 fail; skip unwrapped-segment refresh -> 9/11 fail. Green on the change.
- Budget guard per owner decision: ratio <= 0.6 replaced by NamResetBlockSize(block) == block, <= reserve/16,
  and Chain.resetBlock == NamResetBlockSize; ratio still printed, Myth -> Soldano share < 0.50 kept.
  Red: production Chain reset at ReservedAudioBlockSize -> 8/8 reset checks fail. Green after revert.
- Full run-tests-win.ps1: 1144/1144. No changelog line (tiny internal speedup, nothing a player notices).
- Timing: removed memcpy is ~2 KB per ring per block at 64 frames, 16 KB at 512. Budget-test medians
  (FULL/64 335 -> 267 us) are confounded by the alternated 8192-reserve chain's copy traffic; not the real gain.
