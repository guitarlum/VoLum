# Chorus DSP (four modes)

Status: resolved
Blocked by: 01

## Goal

`VoLumChorus.h` header-only POST chorus, processed **before** Delay in `_VolumProcessPostChain`. Modes and knobs per the spec. MIX=0 passthrough. Finite, bounded. `Reset()` on bypass edge and mode change.

## Do this

Follow `.scratch/post-chorus/spec.md` DSP binding table. Mirror `VoLumTremolo.h` (in-place stereo, `SetParams` + `Process` + `Reset`). Wire into `VoLumProcessBlock.inc.cpp` after the freeze ticket: Chorus → Delay → Reverb → Tremolo. Bypass-edge `Reset()` tracking (`mPostChorusWasActive`) like Delay/Reverb/Tremolo.

Do not add `EParams` in this ticket if you can drive DSP from a test harness / internal setters first — but landing params in ticket 03 is fine as long as 01 is resolved. Prefer: DSP + unit tests here; params + scene + UI in 03.

## Tests (must fail with DSP reverted)

`test_volum_chorus.cpp` (register in both `tests/CMakeLists.txt` and `NeuralAmpModeler-Tests.vcxproj`):

- Every mode produces a different wet signature at MIX=1 (not identical buffers).
- MIX=0 is passthrough (bit-identical or tight epsilon on a silence+impulse fixture).
- No NaN/Inf; output bounded.
- `Reset()` clears a ringing delay so re-engage does not replay a tail.

## Done when

Windows suite includes the new file. Revert-fail proven. No motif/UI required here.

## Result

`NeuralAmpModeler/VoLumChorus.h` (header-only, `Prepare` / `SetParams` / `Process` /
`Reset`, in-place stereo like `VoLumTremolo.h`). Runs first in
`_VolumProcessPostChain`; `mPostChorusWasActive` drives the bypass-edge `Reset()`
alongside Delay/Reverb/Tremolo, and the non-finite scrub and missing-model paths
clear it with the rest of POST. `test_volum_chorus.cpp` is registered in both
`tests/CMakeLists.txt` and `NeuralAmpModeler-Tests.vcxproj`.

Suite: 750 cases / 7,689,176 assertions, all passing.

### Revert-fail proof

Each revert was applied to the shipped code, the full Windows suite was run, and
the code restored. No test in this file passes against a broken engine.

**Pass C - MIX blend loses its equal-power law and its dry-at-zero identity**
(2 failed):

- `Chorus MIX 0 is bit-identical passthrough (all modes)`
- `Chorus output stays finite and bounded (all modes, knob extremes)`

**Pass D - `Process()` returns early, i.e. the card is lit but nothing happens**
(6 failed):

- `Chorus alters the signal at MIX 1 (all modes)`
- `Chorus modes produce distinct wet signatures at MIX 1`
- `Chorus WIDTH decorrelates the two channels (all modes)`
- `Chorus TONE darkens the wet bus counter-clockwise`
- `Chorus without Reset does replay the buffered tail (guards the Reset test)`
- `Chorus is stable across a live mode switch every block`

**Pass III - `Reset()` stops clearing the line, two modes share a name, and the
out-of-range mode / zero sample rate healing is removed** (4 failed):

- `Chorus Reset clears the line so re-engaging does not replay a tail`
- `Chorus mode change resets the line (Delay/Tremolo contract)`
- `Chorus mode names cover every mode and default safely`
- `Chorus survives an out-of-range mode and degenerate sample rate`

Two tests were weaker than the behaviour they claimed and were tightened until
the matching revert failed them: the mode-name test now requires the four labels
to be pairwise distinct, and the out-of-range test now requires that a healed
zero sample rate still processes audio and that an unknown mode renders
bit-identically to the default voice (it previously only asked for finite
output, which a zero-length line satisfies by passing audio straight through).
