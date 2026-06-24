# Q3: VoLum 1.2.0 custom-content correctness fixes

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
section 2). NOTE: re-verified after the review — **none of these are urgent
user-facing bugs** (the one real bug in this cluster, `RemoveCustomAmp` orphaning
files, was already fixed and tested). These are niche/latent hardening items kept
here for a future quality pass. Each changes load/restore behavior, so each needs
a focused test.

## Problem

1. **Preset values relabel a drifted custom-amp scene (niche).** Restore
   re-applies the live *saved* scene and relabels the matched preset
   (`Unserialization.cpp` ~682, `_VolumRestoreSessionSelection`
   `NeuralAmpModeler.cpp` ~2220). In the normal case the knobs are correct. It
   only diverges for a **custom** amp whose shared-library scene was changed by
   another project between saves (custom scenes are library-global, not stored in
   the DAW chunk), so the chunk relabels the drifted library scene as the preset.
   Applying `pr.settings` on the matched-preset path would harden this.
2. **`customSupportId` id-tail field is dead (cleanup, not a bug).** The id-tail
   writes `customSupportId` (`NeuralAmpModeler.cpp` ~2153) but unserialize never
   reads it. Support is correctly restored from the scene's `supportCustomId`
   (`_VolumApplyAmpSettings`, `VoLumSettings.inc.cpp` ~618) and from
   `perAmpSupportId` for factory slots. The unused field can simply be removed.
3. **`_VolumActiveScene()` inserts empty scenes on read (latent).** It returns
   `customScenes[id]` via `operator[]` (`NeuralAmpModeler.cpp` ~4172). Any read
   path with a stale/erased id silently persists a default scene. The scene is
   created at selection so it doesn't bite in practice, but it is a footgun. Used
   in many write paths too, so the fix needs a read/write split.
4. **Pedal legacy-index cap collides at 127 (edge).** Once `nextPedalIndex > 127`,
   `AddPedal` (`VoLumCustomContentMock.h` ~357) clamps every new pedal to 127 ->
   colliding PRE-capture indices, silently (needs 64 imported pedals to hit).
   `PedalLegacyIndexAt` (~319) returns `0` (== EMPTY) for out-of-range.

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
