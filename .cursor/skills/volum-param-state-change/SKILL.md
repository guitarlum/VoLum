---
name: volum-param-state-change
description: Safely change VoLum parameters, preset migration, user settings, or serialization. Use when adding/reordering params, renaming parameter names, changing chunk versions, or editing settings persistence.
---

# VoLum Param And State Change

## Checklist

1. Read `.cursor/rules/volum-state-params.mdc`.
2. Identify every affected contract:
   - `EParams` order
   - stable `GetName()` strings
   - `Unserialization.cpp`
   - `VoLumChunkVersion.h`
   - keyboard step sizes
   - per-amp JSON settings
3. Add migration instead of modifying old readers.
4. Update tests before calling work done:
   - `test_eparam_order.cpp`
   - `test_keyboard_steps.cpp`
   - `test_volum_chunk_version.cpp`
   - settings/DSP-specific tests as needed.

## Invariants

- Old DAW sessions must still load.
- Old param names must remain readable through migration.
- Per-amp settings write to user profile; rigs folder is legacy read fallback.
- New DSP behavior needs bounded/no-NaN/passthrough-style tests where applicable.

## Verification

- Run `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`.
- Add a dated changelog line for user-visible behavior or breaking state format changes.
