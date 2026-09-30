# 04 A2 LeakyReLU `cwiseMax` (NeuralAmpModelerCore) + NAM exact-output lock

Status: resolved
Blocked by: 01
Report: P4 (nam-01). Measured: activation 17-25 us -> 10-14 us per FULL block (-35..-47%, ~3% of a FULL
lane); only subnormal inputs differ, which DAZ reads as 0 downstream.

## Paths
- Submodule `NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp` (~L598), branch `volum/perf-safe-wins` from
  `27027cc5`, pushed to `https://github.com/guitarlum/NeuralAmpModelerCore.git`
- New VoLum test `NeuralAmpModeler/tests/test_volum_nam_exact.cpp` (registered in both build descriptors)
- Parent submodule pointer + changelog

## Change
`ztile = (ztile.array() < 0.0f).select(ztile.array() * kLeakySlope, ztile.array());` becomes
`ztile = ztile.cwiseMax(ztile * kLeakySlope);` with a `// VoLum:` comment explaining why it is identical
(slope in (0,1)).

## New exact-output lock (used again by 05 and 06)
`test_volum_nam_exact.cpp`: render 1 s of a fixed guitar-like signal through a bundled amp (FULL and LITE)
and a PRE capture at 48 kHz in 64-frame blocks with FTZ/DAZ set as in `ProcessBlock` (restore MXCSR after),
and compare a SHA-256 of the output bytes against a per-OS constant, following `test_golden_dsp.cpp`
(Windows hash pinned now from the **pre-change** build; macOS hash printed on mismatch and pinned from the
first CI run, as a370773d did for ping-pong). Land the lock **before** the LeakyReLU change in the same
ticket so the pinned value comes from today's code.

## Acceptance
- Lock green before and after the change; `test_golden_rigs`, `test_volum_golden_render`, realtime-budget
  green. Budget medians (FULL 64/128) logged before/after in the ledger.
- Submodule commit exists on the fork branch before the parent pointer commit.

## Proof
Lock red against a perturbed slope (e.g. 0.0100001f), then green on the real change.

## Verifier conditions
Identity argument holds for finite, +-0, NaN; subnormals only differ under DAZ; no other code in the file
changed; pointer points at a pushed commit.

## Result
- NAMCore `f9836c5` on fork branch `volum/perf-safe-wins` (pushed, ls-remote verified); parent pointer bumped.
- Lock `test_volum_nam_exact.cpp`: Soldano AMP FULL/LITE + Myth PRE FULL/LITE, 1 s at 48k/64, FTZ/DAZ
  scoped with MXCSR restore, production Reset size + ProcessNamInChunks. Windows hashes pinned from 27027cc5.
  Helpers `RenderNam(path, full, block, input)` / `LockCases()` / `PinnedHash()` for tickets 05 and 06.
- Red: slope 0.0100001f in the cwiseMax line -> both FULL hashes fail (LITE uses the scalar 3-ch path). Green on
  the real change. Full suite 1140/1140.
- Budget medians (Myth -> Soldano, median of 9 runs, us): FULL/64 253.4 -> 250.2, FULL/128 458.8 -> 463.9,
  LITE/64 60.4 -> 61.3 (within noise). Isolated op (perf-sweep bench): 12.5 -> 6.9 us per FULL block.
