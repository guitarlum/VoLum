# Q3: VoLum 1.2.0 custom-content correctness fixes

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
section 2). NOTE: re-verified after the review — **none of these are urgent
user-facing bugs** (the one real bug in this cluster, `RemoveCustomAmp` orphaning
files, was already fixed and tested). These are niche/latent hardening items kept
here for a future quality pass. Each changes load/restore behavior, so each needs
a focused test.

## Already landed

Item 4, first half: `AddPedal` now refuses once the 64-slot PRE-capture pool is
exhausted instead of clamping every new pedal to index 127
(`VoLumCustomContentApi.h` ~429), covered by "AddPedal refuses once the PRE-capture
index pool is exhausted" in `test_volum_custom_content.cpp`. Changelog
`06/29/2026`. 1.2.1 additionally made a failed import stop consuming a slot.

Items 1, 2, 3 and the second half of 4 are all still open, verified against the
current code — the notes below are accurate, not stale.

## Problem

1. **Preset values relabel a drifted custom-amp scene (niche).** Restore
   re-applies the live *saved* scene and relabels the matched preset
   (`Unserialization.cpp` ~682, `_VolumRestoreSessionSelection`
   `NeuralAmpModeler.cpp` ~2220). In the normal case the knobs are correct. It
   only diverges for a **custom** amp whose shared-library scene was changed by
   another project between saves (custom scenes are library-global, not stored in
   the DAW chunk), so the chunk relabels the drifted library scene as the preset.
   Applying `pr.settings` on the matched-preset path would harden this.
2. **`customSupportId` is written and round-tripped but has no consumer on
   restore.** The id-tail writes it (`NeuralAmpModeler.cpp` ~1078) and the codec
   round-trips it under test (`VoLumChunkIdTail.h`, `test_volum_chunk_codec.cpp`,
   `test_volum_state_roundtrip.cpp`), so it is *not* dead in the "delete it" sense
   — `Unserialization.cpp` simply never reads it. Support is restored from the
   scene's `supportCustomId` instead (`_VolumApplyAmpSettings`,
   `VoLumSettings.inc.cpp` ~618) and from `perAmpSupportId` for factory slots. So
   the choice is to either wire the restore path (making the chunk authoritative
   for the support partner, which is the point of storing it) or drop the field and
   its tests. Do not "clean it up" without picking one deliberately.
3. **`_VolumActiveScene()` inserts empty scenes on read (latent).** It returns
   `customScenes[id]` via `operator[]` (`NeuralAmpModeler.cpp` ~4172). Any read
   path with a stale/erased id silently persists a default scene. The scene is
   created at selection so it doesn't bite in practice, but it is a footgun. Used
   in many write paths too, so the fix needs a read/write split.
4. **`PedalLegacyIndexAt` still returns `0` for an out-of-range row.** The
   exhaustion half is fixed (see above), but `PedalLegacyIndexAt`
   (`VoLumCustomContentApi.h` ~375) still answers `0` — which is the EMPTY sentinel
   — for an index it cannot resolve, so a caller cannot distinguish "no pedal" from
   "bad row". Should return `-1` with the callers guarded.

## Scope

- Fix (1): on `activePresetId` match, apply the bank preset's `pr.settings`
  (via the existing recall/apply path) before snapshotting, in both
  `Unserialization.cpp` and `_VolumRestoreSessionSelection`.
- Fix (2): after `_VolumSelectCustomAmp`, if `idTail.customSupportId` is non-empty,
  resolve it and set `mVolumCustomSupportIdx` / scene support field.
- Fix (3): add a const `_VolumActiveSceneOrDefault()` for read sites; keep the
  mutable `operator[]` accessor only where a scene must be created.
- Fix (4): return a sentinel (-1) from `PedalLegacyIndexAt` for out-of-range and
  guard the callers. (Rejecting `AddPedal` on an exhausted pool already landed.)

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
