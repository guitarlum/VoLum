# Locked read-modify-write content library

Status: done
Blocked by: none

## Goal

Two catalog writes of different items both survive. Concrete: import an IR in standalone and save a named preset in the DAW → next launch still has both.

## Do this

Implement the two-writer mechanism in `.scratch/pack/spec.md`. Touch `VoLumContentStore.h` and the process-global `GlobalContentStore()` constructor path in the plugin. Drop `customScenes` from the registry write; migrate them onto the instance / standalone settings.

Add `midiSoundMap` (schema in the spec) under the same lock. No Settings MIDI chrome in this ticket — persistence only, with doctests.

In-process: mutex; second constructor does not `Load()` over unflushed state.

OS lock dies with the process. Tests: two stores, two constructors, write failure visible, lock file not stuck after a killed holder (simulate by closing the lock handle without writing a stale cookie).

## Tests (must fail with this ticket reverted)

- Two `ContentStore` instances on two temp dirs are **not** the case — use one shared dir, two store objects (or two processes if you can do it headlessly). Different items both present after both saves.
- Process-global: constructing a second store/client does not wipe the first's unflushed catalog.
- Failed `Save()` → `TakeWriteFailure()` / equivalent; memory does not claim the unsaved item.
- MIDI map round-trips in the registry JSON; unknown extra keys ignored.

## Done when

Windows tests green. Revert-fail proven. No Pack UI yet.

## Result

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` → 747 cases / 4211804
assertions, 0 failed.

Mechanism (`VoLumContentStore.h`):

- `RegistryFileLock`: `LockFileEx` / `flock` on a `volum-content.lock` **sibling**
  of the registry (the registry is replaced by rename, so a lock on it is a lock
  on a file that no longer exists). Blocking with a 4 s ceiling; a caller that
  gives up reports a write failure instead of hanging the UI thread.
- `ContentStoreMutex()`: one process-wide recursive mutex, because the OS lock is
  per handle and does not serialize threads inside one process.
- `Save()` = locked read-modify-write: re-read the file, `MergeRegistries(disk,
  baseline, current)` by stable id, write, adopt the merge as both live catalog
  and new baseline. A failed write leaves the baseline alone.
- `EnsureLoaded()` replaces the constructor's `Load()`; it reads only when this
  process has no catalog in memory and no unflushed changes.
- Schema v4: `midiSoundMap` added, `customScenes` no longer written (read once as
  `legacyCustomScenes` for migration; the sounding rig is per-instance state now,
  carried in the DAW chunk id tail and in `volum-settings.json`).

## Revert-fail proof

Each mechanism was reverted in isolation, the suite rebuilt, and the named cases
observed failing; then restored and re-run green.

| Revert | Failing cases |
| --- | --- |
| `Save()` writes `mReg` instead of the merge (pre-1.3.0 whole-catalog overwrite) | `Two writers on one library: both new items survive both saves` (0 == 1), `Two writers: merges are by id, not by whole-list replacement` (2 == 3), `Two writers: preset banks merge per preset, not per bank` (1 == 2), `Two writers: MIDI slot assignments merge per slot` (1 == 2), `Concurrent saves from two threads keep the library parseable and complete` (25 == 50) |
| `EnsureLoaded()` → unconditional `Load()` | `A second EnsureLoaded does not read over an unflushed catalog` (1 == 2) |
| `EnsureLoaded()` → `return true` (never reads) | `EnsureLoaded still performs the first read` (`IsLoaded()` false, 0 == 1) |
| `mBaseline = mReg` moved before the write lands | `A failed save stays pending and is carried by the next successful one` (`HasUnflushedChanges()` false, 1 == 2) |
| OS lock → naive cookie lock (`CREATE_NEW` + delete on release) | `A killed lock holder does not stuck-lock the library` (`Save()` false) |
| `LOCKFILE_EXCLUSIVE_LOCK` dropped (shared lock) | `The library lock is exclusive while held` (second `Acquire` succeeded) |
| `LockPath()` → `RegistryPath()` | `The lock lives beside the registry, never on it` |
| `midiSoundMap` not serialized, `customScenes` written as in 1.2.0 | `Registry round-trips amps, IRs, pedals, and presets` (0 == 2), `Combined BYO project ... round-trips on disk` (0 == 1), `Two writers: MIDI slot assignments merge per slot` (0 == 2), `The registry no longer writes shared custom scenes` (`customScenes` present) |
| MIDI reader drops its shape checks (`e.value(...)`) | `MIDI sound map reader ignores unknown keys and malformed slots` (threw `type_error.302`) |
| `IsFactoryPresetId` branch removed from `MidiPresetIdResolves` | `MIDI slot resolution reports gone content as invalid, never as empty` (factory slot read as Invalid) |
| `RemoveCustomAmp` erases matching MIDI slots | `Deleting content leaves its MIDI slot assigned but invalid` (0 == 1) |
