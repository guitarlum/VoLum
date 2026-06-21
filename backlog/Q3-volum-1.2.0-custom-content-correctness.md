# Q3: VoLum 1.2.0 custom-content correctness fixes

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
section 2). These are verified correctness gaps in the BYO/presets backend that
were left to backlog because each changes load/restore behavior and needs a
focused test, unlike the contained `RemoveCustomAmp` fix already landed.

## Problem

1. **Preset settings not restored on load.** On chunk/session restore, a matched
   `activePresetId` sets the preset label + "clean" bar and snapshots
   `_VolumActiveScene()`, but never applies `pr.settings`. The user sees the
   preset name while the knobs reflect the per-amp scene, not the saved preset.
   See `NeuralAmpModeler/Unserialization.cpp` (~682) and
   `_VolumRestoreSessionSelection` (`NeuralAmpModeler.cpp` ~2220).
2. **`customSupportId` serialized but never applied.** The id-tail writes
   `customSupportId` (`NeuralAmpModeler.cpp` ~2153) but unserialize only applies
   `perAmpSupportId` / `customMainId`. A custom-main + custom-support project can
   lose its support partner on reload.
3. **`_VolumActiveScene()` inserts empty scenes on read.** It returns
   `customScenes[id]` via `operator[]` (`NeuralAmpModeler.cpp` ~4172). Any read
   path with a stale/erased id silently persists a default scene. It is used in
   many write paths too, so the fix needs a read/write split.
4. **Pedal legacy-index cap collides at 127.** Once `nextPedalIndex > 127`,
   `AddPedal` (`VoLumCustomContentMock.h` ~357) clamps every new pedal to 127 ->
   colliding PRE-capture indices, silently. `PedalLegacyIndexAt` (~319) returns
   `0` (== EMPTY) for out-of-range.

## Scope

- Fix (1): on `activePresetId` match, apply the bank preset's `pr.settings`
  (via the existing recall/apply path) before snapshotting, in both
  `Unserialization.cpp` and `_VolumRestoreSessionSelection`.
- Fix (2): after `_VolumSelectCustomAmp`, if `idTail.customSupportId` is non-empty,
  resolve it and set `mVolumCustomSupportIdx` / scene support field.
- Fix (3): add a const `_VolumActiveSceneOrDefault()` for read sites; keep the
  mutable `operator[]` accessor only where a scene must be created.
- Fix (4): reject `AddPedal` when the 64-slot pool is exhausted (surface a
  reason like the builder's `SaveDisabledReason`); return a sentinel (-1) from
  `PedalLegacyIndexAt` for OOR and guard callers.

## Acceptance Criteria

- Reloading a project/standalone session with an active preset restores the
  preset's actual knob values, not just its label.
- A custom-main + custom-support project round-trips the support partner.
- No phantom `customScenes` entries appear from read-only paths.
- Adding pedals past the pool cap fails cleanly without index collisions.
- New doctests cover each path; existing 1.2.0 tests stay green.

## Verification

- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- Scripted standalone regression: import stock rigs as custom amps, save a preset,
  reload, confirm knobs match (use `win-click.ps1` / `win-screenshot.ps1`).
- `bash NeuralAmpModeler/scripts/run-tests-mac.sh` via CI for parity.
