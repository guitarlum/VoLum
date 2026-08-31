# Locked read-modify-write content library

Status: ready-for-agent
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
