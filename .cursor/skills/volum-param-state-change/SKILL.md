---
name: volum-param-state-change
description: Safely change VoLum parameters, preset migration, user settings, or serialization. Use when adding/reordering params, renaming parameter names, changing chunk versions, or editing settings persistence.
---

# VoLum Param And State Change

## Checklist

1. Read `.cursor/rules/volum-state-params.mdc`.
2. Identify every affected contract listed there (param order, stable names, serialization, chunk version, keyboard steps, settings).
3. Add migration instead of modifying old readers.
4. Update the param/state tests listed in that rule before calling work done, plus settings/DSP-specific tests as needed.
5. If a post-plan fix would change user-facing display wording or symbols,
   ask before changing the planned wording (for example `-∞ dB` vs `OFF`).

## Invariants

- Old DAW sessions must still load.
- Old param names must remain readable through migration.
- Per-amp settings write to user profile; rigs folder is legacy read fallback.
- New DSP behavior needs bounded/no-NaN/passthrough-style tests where applicable.

## Do NOT bump `kVoLumUserSettingsVersion` for purely additive fields

`VolumUserSettingsToJson` / `FromJson` is forward-tolerant by design: a reader
in an older build ignores unknown keys, and a reader in a newer build accepts
any `version` greater than its `kVoLumUserSettingsVersion` without triggering
legacy migration. That means **adding** new optional keys (a new bool, a new
optional sub-object) does **not** require a version bump.

Only bump `kVoLumUserSettingsVersion` when the change is non-additive:

- An existing key changes meaning (units, scale, encoding).
- An existing key is removed.
- The reader needs to actively rewrite or relocate old data.

When in doubt, leave the version alone and write a focused test in
`test_volum_user_settings_io.cpp` that:

- Confirms a JSON written by the current build still loads cleanly in a
  simulated "older reader" (load without the new fields, assert no `healed`).
- Confirms a JSON written by a simulated "future" build (`version` = current
  + 1, with extra unknown keys) loads cleanly with no `healed` and no data
  loss in the keys we do know.

History: 1.0.1 RC bumped 6 -> 7 only to gate two booleans (`preLocked` /
`postLocked`). That triggered destructive legacy-v1 migration when a user
downgraded to 1.0.0 because the old clamp logic mapped any unknown version
back to v1. Fix was to revert the bump and rely on additive forward
tolerance. The bug class is: **"locked the version field too eagerly."**

## Verification

- Run `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`.
- Add a dated changelog line for user-visible behavior or breaking state format changes.
