# P2 — UI-thread performance: dirty-check and settings-save churn

Originally a five-part RT + performance ticket seeded from the 1.2.0 quality
review. **Three of the five parts are now owned by
`B7-audio-thread-rt-violations.md`**, which came out of the 1.2.1 audit with a
wider scope and the correct root cause (the audio thread performs the model
handoff). This file keeps only the two parts B7 does not cover, because they are
UI-thread performance rather than real-time safety.

Moved to B7: the lock-free staging drain, the main I/O buffer capacity assert, and
async IR loading. All three verified still open in the code — do not treat them as
fixed, just plan them in B7.

## Problem

1. **The preset dirty check serializes JSON on every knob move.**
   `AmpSettingsEqual()` (`VoLumAmpSettingsJson.h` ~98) is
   `AmpSettingsToJson(a) == AmpSettingsToJson(b)`: two full JSON trees built and
   compared to answer one boolean. It is called from
   `_VolumRecomputePresetDirty()`, i.e. per parameter change, so a knob drag or a
   host automation lane pays two tree builds per event to decide whether to draw
   `(unsaved)`.
2. **Settings-save churn under automation.** `mVolumSettingsDirty` is set from many
   param and selection paths, and `OnIdle` writes the settings file whenever it is
   set (`NeuralAmpModeler.cpp` ~956). Coalescing to once per idle tick already
   landed, but with automation running that is still an atomic JSON write of the
   whole settings document every tick, and custom scenes route through the shared
   content store.

## Scope

- (1) Replace the JSON compare with a field-wise or hashed compare for the dirty
  flag only. The JSON codec stays the persistence format; nothing about the file
  changes. A cheap hash has to include every field the codec writes, or the dirty
  marker starts lying — which is worse than being slow, so this wants a test that
  walks each field.
- (2) Set `mVolumSettingsDirty` only for VoLum-owned params, and put a time-based
  throttle in front of `_VolumSaveCurrentToSettings` rather than saving on every
  idle tick that happens to find the flag set. Exit must still flush, or the
  throttle turns into data loss.

## Acceptance criteria

- A knob drag does not build JSON per event; the `(unsaved)` marker still appears
  and clears exactly as it does today.
- Automating a parameter in a DAW does not produce a settings write per idle tick;
  closing the app or the project still persists the final state.
- Doctests: field-wise equality agrees with the JSON compare across every field
  (including the ones added in 1.2.1), and the throttle flushes on shutdown.
- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` green; macOS via CI.

## Verification

- Manual: automate a knob in a DAW, watch for disk thrash and for a stale
  `(unsaved)` state.
