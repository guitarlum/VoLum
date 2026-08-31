# Chorus DSP (four modes)

Status: ready-for-agent
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
