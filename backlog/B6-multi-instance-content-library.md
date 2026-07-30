# B6 — The custom content library is not safe against two writers

Deferred out of the 1.2.1 pre-release audit. Full evidence:
`audit-notes/gpt56/P4-content-store.md` (`F-P4-6`, `F-P3-3`),
`audit-notes/opus5/P-custom-content-library.md` (findings 2, 7),
`audit-notes/opus5/P-state-serialization-content.md` (findings 3, 4, 9, 10),
`audit-notes/opus5/P-neuralampmodeler.md` (finding 7).

Ledger: `backlog/1.2.1-audit-deferred.md`.

## Why this is first in the queue

1.2.1 stopped the library from being *erased* by a file VoLum could not read,
and stopped a delete from destroying a capture before the registry commit. It
did not touch the case where two writers both hold a valid snapshot. That case
still loses content, and it loses it silently: the payload files stay on disk as
orphans while their names, settings and preset banks disappear.

Everyone who uses VoLum in a DAW *and* opens the standalone to audition
something is exposed. It gets worse as libraries grow, because a full-file
last-writer-wins rewrite drops proportionally more each time.

## The defects, as measured

**Across processes.** `ContentStore::Save()` rewrites the whole registry from
this process's snapshot with no lock, no generation check, and no reload
(`VoLumContentStore.h:749-755`). Standalone plus a DAW instance: import an IR in
one, save a preset in the other, and the second write erases the first import.

**Inside one host.** `GlobalContentStore()` is a process-wide singleton
(`VoLumContentStore.h:917-927`) and every plugin constructor reloads it
(`NeuralAmpModeler.cpp:497-500`) with no mutex. Instantiating a second VoLum
discards the first one's unflushed scene edits. `mReg` is then read and mutated
from whatever thread the host runs UI callbacks on — a plain data race.

**One live scene for many instances.** A focused custom amp keeps its scene in
the shared store rather than in the DAW chunk, so two instances on two tracks
focused on the same custom amp write over each other's knobs, and
`SerializeState` reaches out to disk to flush it
(`audit-notes/opus5/P-neuralampmodeler.md` finding 7).

**Failures still reported as success.** Most mutators ignore the `Save()` return
(`audit-notes/opus5/P-custom-content-library.md` finding 8, gpt56 `F-P3-5`), so
the UI lists content that will not exist next launch. 1.2.1 fixed this for IR
and pedal *import* only.

**Settings, same shape.** Any instance can rewrite the whole machine-global
settings file through the Lite toggle, and a failed settings write silently
discards custom-amp scene edits (opus5 `P-state-serialization-content` 4, 10).

## Scope for the planning session

Decide, with reasons, between:

- **A single owner.** One process/instance owns the library; everyone else reads.
  Cheapest, and the least like what users expect from a shared library.
- **Read-modify-write under a cross-process lock, merged by stable id.** Reload
  under the lock, apply this writer's mutations by id, write, release. Needs a
  real answer for a stale lock left by a crashed process.
- **Per-item files instead of one registry.** Removes whole-file
  last-writer-wins by construction and makes merges trivial, at the cost of a
  migration and a slower enumerate.

Whatever is chosen must also answer:

- Same-process synchronization: a mutex around `mReg`, or a single-threaded
  owner with a queue.
- Where a focused custom amp's live scene belongs. If it stays in the shared
  store, two instances need a defined winner; if it moves into the chunk, that
  is a chunk-format change and needs migration.
- What the UI says when a write fails. "Success" is not available any more.
- Whether the `.bak` / `.pre-1.2.1.bak` snapshots still make sense under the new
  scheme.

## Acceptance criteria

- Two stores over one base directory, each adding a different item, both saving:
  a reload contains both. This is gpt56's proposed
  `Two_content_store_writers_merge_nonconflicting_imports` and it fails today.
- Two plugin instances constructed in sequence: the first one's unflushed edits
  survive the second's construction.
- A forced write failure surfaces in the UI and the in-memory list does not
  claim the item exists.
- A crashed writer does not leave the library permanently locked.
- The migration path from a single `volum-content.json` is covered by an upgrade
  fixture, including the 1.2.0 → 1.2.1 schema-v2/v3 fixtures already in
  `NeuralAmpModeler/tests/`.
- `e2e-standalone-win.ps1` gains a two-writer scenario, since it already runs the
  real app against a sandboxed `LOCALAPPDATA`.

## Out of scope

Deleting content that is currently playing. That is the sibling cluster (gpt56
`F-P6-3`, `F-P4-5`, opus5 `P-custom-content-library` 3, `P-overlay-manage-ui` 5)
and it is a removal transaction reaching into the audio graph. Plan it in the
same session if the ownership model turns out to decide it; implement it
separately.
