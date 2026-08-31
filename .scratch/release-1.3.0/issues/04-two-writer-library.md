# How does the content library survive two writers?

Type: grilling
Status: resolved

## Question

Custom amps, IRs, pedals, and presets live in one process-wide `GlobalContentStore()` that writes the whole `volum-content.json` via temp+rename. There is no cross-process lock or merge. Two VoLum instances, or a DAW instance plus standalone, last-write-wins and can silently drop the other session’s library. User-visible: imported content vanishes from the registry (orphans on disk).

A Pack importer is another writer. MIDI Program Change onto a clobbered preset bank is the same wound. Prior art: `backlog/B6-multi-instance-content-library.md` and R4 phase 2 (per-instance ownership) in `backlog/R4-volum-1.2.0-content-bridge-collapse.md`.

What ownership model does 1.3.0 ship before Pack?

- Single-owner (refuse the second writer),
- Locked read-modify-write merge,
- Or per-item files?

Recommend: decide this before Pack format. Pair the same ownership story with [What happens when we delete content that is currently playing?](05-delete-while-playing.md). Do not silently skip this gate — code still has the defect.

## Answer

**Guarantee.** Two catalog writes of different items both survive. Concrete: import an IR in standalone and save a named preset in the DAW → next launch still has both. Same for two plugin tracks each adding a different preset. Silent last-write-wins of the whole library is out.

**Mechanism.** Locked read-modify-write on the existing one-file registry. Cross-process OS advisory lock that dies with the process (crash must not stuck-lock). Under the lock: reload, apply this writer’s mutations by stable id, write, release. Same-id last writer of *that item* wins. In-process: mutex around the store; a second plugin constructor must not `Load()` over a live sibling’s unflushed catalog. Per-instance preset-owner key (R4 phase 2) rides with this — no ambient process-global owner. Per-item registry files and vector clocks are out of 1.3.0: two VoLums, one machine, both online; a lock serializes them. Tests are mandatory (two stores add different items; two constructors; write failure visible; crashed writer does not stuck-lock).

**Library vs instance.** Custom amps behave like factory amps. The content library is the catalog (captures, IRs, pedals, named presets, MIDI sound map). The sounding rig lives on the VoLum instance (DAW chunk / standalone settings). Drop shared `customScenes`. A catalog write never rewrites another instance’s live knobs. New plugin insert still inherits initial scenes from the machine settings file the way factory amps already do.

**MIDI sound map.** Persisted with the content library (or a sibling under the same lock) so every format can write it. Settings UI may still edit it. `volum-settings.json` stays standalone-write-only and is not this gate.

**Pack as a writer** (so [What is in a Pack, and what happens on conflict?](06-pack-contents-and-conflict.md) does not re-ask this). Import merges by stable id into the library: new ids add; same id replaces that item; a different item that only shares a name is not silently replaced. The VoLum you imported into also applies the Pack to its live rig. Other tracks stay as they are. Standalone import updates the library and the standalone window. Payload, file extension, transactional failure, and schema version stay on that Pack ticket.

**Write failure.** A failed save is visible. The in-memory list does not claim an item that never reached disk.

DSP-on-delete stays on [What happens when we delete content that is currently playing?](05-delete-while-playing.md).
