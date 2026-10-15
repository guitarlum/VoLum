# 10 Lanczos resamplers only when resampling

Status: resolved
Blocked by: 06
Report: B4 (nam-08). Each model allocates and clears two 2 MiB Lanczos buffers even when host rate equals
model rate (48 kHz): ~16 MiB per instance and extra Reset time, no steady-state CPU.

## Paths
- `NeuralAmpModeler/VoLumResamplingNam.h` (`Reset` ~L105-117)
- Test (extend an existing ResamplingNAM test or new)

## Change
Call `mResampler.Reset(sampleRate, maxBlockSize)` only when `NeedToResample()`. A later Reset at a different
rate creates the resamplers on demand through the existing path (44.1 -> 48 -> 44.1 reuses clear-buffers).

## Acceptance
- A test-visible accessor or observable shows no resampler allocation at matching rates, and correct output
  after 48 -> 44.1 -> 48 -> 96 Resets.
- 44.1 / 96 kHz golden groups unchanged; exact lock from 04 unchanged.

## Proof
Accessor assertion red against the old `Reset`.

## Verifier conditions
Every path that processes through the resampler is preceded by a Reset at a resampling rate; no audio-thread
allocation introduced.

## Parked (owner, 2026-09-26 22:00)
Budget: lean plan. Not in this loop; stays specified for a later effort.

## Result (2026-09-29, worker)
- `ResamplingNAM` holds the container in a `std::optional`: built and Reset only when `NeedToResample()`,
  dropped on a Reset at the model's rate. Dropping (not keeping) it makes 44.1 -> 48 -> 44.1 rebuild fresh, which
  is exactly what the old always-Reset container did (a kept one takes ClearBuffers and keeps its phase).
  Allocation pattern on every Reset is the old one or less; nothing new on the audio thread.
- New `tests/test_volum_resampling_nam.cpp` (both build descriptors): `HasResampler()` false at 48 kHz, true at
  44.1; output after 48/44.1/48/96/44.1/48@128/44.1/88.2@256 Resets bit-equal to a wrapper built at that rate, plus
  the same-args ClearBuffers path.
- Red: old always-Reset -> 2 accessor checks fail; keep-the-container variant -> accessor + output lock fail at
  44.1/64 after 48/128. Green after. Full suite 1197/1197 (goldens incl. 44.1/96 and NAM exact lock untouched).
- Memory (test INFO, private bytes): 4 wrappers Reset at 48 kHz 16.07 MiB before, 0 MiB after.
  Evidence: `evidence/10/`.
