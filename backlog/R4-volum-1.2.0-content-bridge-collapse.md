# R4: Rename + partial-collapse the VoLum content "Mock" bridge

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
section 3.2). Medium-risk refactor across ~40 call sites; deferred from the review
pass to its own session.

## Problem

`NeuralAmpModeler/VoLumCustomContentMock.h` is a production bridge over
`volum::content::GlobalContentStore()`, not a mock — its own header says so. Of
its ~44 symbols, ~50% are trivial projections/lookups (rebuilding `static`
vectors per call) and ~30% earn their keep (UI-index <-> opaque-id, name dedup on
write, preset-owner routing, DI hooks for capture/apply). The plugin already
bypasses the bridge in ~40 places via `GlobalContentStore().reg()` directly, so
there are two access styles over one backend, and new code drifts between them.

## Scope (sequence matters; stop at any phase if risk grows)

1. **Rename (low risk):** `VoLumCustomContentMock.h` -> `VoLumCustomContentApi.h`
   (or `VoLumContentSession.h`); drop the `Mock` prefix on getters; fix the stale
   "throwaway display data" comment in `test_volum_custom_content.cpp`.
2. **Move per-instance state off globals (medium):** move `ActivePresetOwnerKey`,
   `PresetCaptureHook`, `PresetApplyHook` from process-global statics onto the
   `NeuralAmpModeler` instance (`_VolumActiveOwnerKey()` already exists). This
   also fixes a multi-instance-host race.
3. **Delete projection getters (medium):** have the UI iterate
   `GlobalContentStore().reg()` structs directly for lists; document that the
   registry is the canonical read path.
4. **Move mutations into the store (medium):** fold `AddCustomAmp`/`AddIR`/
   `AddPedal`/preset CRUD + name dedup into `content::ContentStore` (or a small
   `ContentMutations.h`); unify import so one minted id is used for both the
   filename prefix and the registry entry (today they diverge).
5. **(Optional, high risk) drop the index-based API** in favor of opaque ids
   everywhere, removing the delete-shifts-indices fragility.

## Acceptance Criteria

- No symbol named `Mock*` remains for production content access.
- One canonical read path (the registry); projection getters removed.
- Preset owner key / hooks are per-instance, not process-global.
- Import mints exactly one id per asset (filename and registry entry agree).
- Tests green (including session tests, isolated per-case); no behavior change in
  standalone or VST3.

## Verification

- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- Scripted standalone regression of BYO amp/IR/pedal import + presets.
- macOS build/tests via CI.
