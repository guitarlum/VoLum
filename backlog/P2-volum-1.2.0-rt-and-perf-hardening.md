# P2: VoLum 1.2.0 RT + performance hardening

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
sections 1 and 4). The audio path is already free of content-store/JSON/filesystem;
these are the remaining real-time risks and UI-thread perf hotspots.

## Problem

1. **Audio-thread mutexes in the staging drain.** `_ApplyDSPStaging()` (top of
   every `ProcessBlock`) calls `_VolumDrainLoaderResults()`, which takes
   `mVolumLoaderMutex` + `mStagingMutex` and may call `model->Reset()` (can
   allocate inside NAM). Pre-existing pattern that 1.2.0 extends.
2. **Main I/O buffer resize on the audio thread.** `_PrepareBuffers` may `resize`
   `mInputArray` / `mOutputArray` when `numFrames` changes mid-stream (dual-amp
   scratch already pre-reserves + asserts; main buffers do not).
3. **Sync IR load on the UI callback.** `_VolumSelectIR` -> `_StageIR` loads a WAV
   + allocates on the calling (UI) thread — a hitch, not an audio fault.
4. **Dirty-check serializes JSON per knob move.** `AmpSettingsEqual` builds two
   full JSON trees and string-compares; called from `_VolumRecomputePresetDirty`.
5. **Settings-save churn under automation.** Every param change sets
   `mVolumSettingsDirty` and `OnIdle` always calls `_VolumSaveCurrentToSettings()`;
   with custom scenes redirecting to `GlobalContentStore`, automation -> per-sample
   map writes + atomic JSON writes in standalone.

## Scope

- (1) Replace the staging hand-off with a lock-free SPSC queue of `unique_ptr`
  commits drained in `_ApplyDSPStaging` with moves only; call `Reset()` on the
  loader thread before enqueue. Remove the second mutex grab from the audio path.
- (2) Reserve `mInputArray`/`mOutputArray` to max block size in `OnReset`; assert
  capacity (no `resize`) in `ProcessBlock`.
- (3) Route IR loads through the same async loader path as NAM models.
- (4) Replace `AmpSettingsEqual`'s JSON compare with a field-wise / hashed compare
  for the dirty flag (keep the JSON codec for persistence only).
- (5) Set `mVolumSettingsDirty` only for VoLum-owned params; coalesce
  `_VolumSaveCurrentToSettings` (save when dirty, throttled, not every idle tick).

## Acceptance Criteria

- No mutex acquisition, allocation, or `Reset()` on the audio thread in the
  staging drain; verified by inspection + a stress test that loads models while
  audio runs.
- No buffer `resize` in `ProcessBlock`; capacity assert holds across block-size
  changes.
- Knob automation does not trigger per-sample disk writes in standalone.
- Tests green; macOS sanitizer (TSan/ASan via CI) clean on the staging change.

## Verification

- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- `bash NeuralAmpModeler/scripts/run-tests-mac.sh --sanitize` via CI.
- Manual: automate a knob in a DAW and confirm no disk thrash / no dropouts on
  rapid amp switching.
