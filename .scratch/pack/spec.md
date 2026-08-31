# Pack (two-writer library + delete-while-playing + `.volumpack`)

Locked map: `.scratch/release-1.3.0/map.md`. Do not reopen product calls.

## Outcome

The content library survives two writers. Deleting (or Pack-replacing) an in-use library id does the locked DSP/UI thing. Users can Export / Import a `.volumpack` from Gear → Settings.

## Locked decisions (do not invent)

- [How does the content library survive two writers?](../release-1.3.0/issues/04-two-writer-library.md)
- [What happens when we delete content that is currently playing?](../release-1.3.0/issues/05-delete-while-playing.md)
- [What is in a Pack, and what happens on conflict?](../release-1.3.0/issues/06-pack-contents-and-conflict.md)

## Serial order

`01` two-writer (including MIDI sound-map persistence schema and dropping shared `customScenes`) → `02` delete-while-playing → `03` Pack format/UI → `04` docs. MIDI *control* (decoder, queue, Settings list chrome) is **not** this spec; only the library field the MIDI map lives in.

## Two-writer mechanism

- Keep one `volum-content.json`. Cross-process OS advisory lock that **dies with the process** (Windows `LockFileEx` on a sibling lock file or the registry handle; POSIX `flock`). Crash must not stuck-lock.
- Under the lock: reload, apply this writer's mutations **by stable id**, write, release. Same-id last writer of *that item* wins.
- In-process: mutex around the store. A second plugin constructor must **not** `Load()` over a live sibling's unflushed catalog.
- Per-instance preset-owner key (no ambient process-global owner).
- Write failure is visible; in-memory list does not claim an item that never reached disk.
- **Drop shared `customScenes`.** Custom amps behave like factory amps: the sounding rig lives on the instance (DAW chunk / standalone `volum-settings.json`). Catalog writes never rewrite another instance's live knobs. New plugin insert still inherits initial scenes from the machine settings file the way factory amps already do. Migrate any existing `customScenes` on read into standalone settings / the instance; stop writing that map to the registry.
- **MIDI sound map** persists with the content library (or a sibling file under the **same lock**). Schema:

```json
"midiSoundMap": [
  { "slot": 5, "ampId": "factory:7", "presetId": "preset_abc" },
  { "slot": 6, "ampId": "factory:7", "presetId": "factory:7:v1" }
]
```

`ampId` is `factory:<idx>` or a custom-amp library id. `presetId` is a User preset library id or a Factory preset shipped id (`factory:<idx>:v1`). Empty / missing map = no assignments. Factory preset ids are not library items; they still serialize here. Pack SHARE does **not** include this map; Pack Everything does.

## Delete-while-playing

This instance, in-use graph slot → available default **now** (UI and DSP agree). Staging same family as cab ↔ IR (no dry burst, no ghost capture). Confirm copy names the in-use case and the destination.

| Deleted id in use | Available default |
| --- | --- |
| Custom amp on MAIN | Factory amp already in the sidebar, as if clicked (this instance's last knobs on that amp, **not** Default) |
| Dual Amp | Only the lane that used that id |
| Pedal in a PRE slot | That slot empty; live PRE model dropped |
| IR | Baked cab of the current amp |
| Named preset selected | Forget the name; **keep the sound** |

Siblings keep RAM until they next need the id. MIDI slots stay numbered; missing Sound → **invalid** (red). Pack replace of an in-use id **reloads** the new capture after confirm (not the delete-fallback).

## Pack

One `.volumpack` = zip, **STORE method 0** (no compression dependency). Contract version integer inside `manifest.json`, **not** the app version. Newer contract → older app refuses with "needs VoLum X.Y". Older Packs always import. Same contract from a newer app: import; unknown keys ignored.

Gear → Settings: **Export Pack…** / **Import Pack…**. Not File → Preferences, not Manage, not PLAY-only.

**Export:** Everything, or selected amps and/or named presets. Compute closure (custom `.nam`, IRs, pedals those presets/amps reference). Preview lists "also including"; requirements cannot be unchecked. Factory-amp presets do not pack the factory capture. Unused IRs/pedals only in Everything.

**SHARE payload:** library items + files only. No `volum-settings.json`, no MIDI sound map.

**SHARE import:** merge by id; extras stay; name-only collision keeps both and warns. Does not switch the sounding rig to a newly added amp. Confirmed replace of an in-use id → reload.

**Everything payload:** library + files + MIDI sound map + `volum-settings.json`.

**FULL import verbs** (library items only; factory amps cannot be deleted): Overwrite / Add / Reset as in ticket 06.

**Machine settings:** standalone-only checkbox "Also restore machine settings". Plugins never write `volum-settings.json`. MIDI map rides that checkbox, not the three verbs.

**Failure:** validate, stage, swap. Prior tree left in a backup. Corrupt Pack changes nothing.

Never pack `settings.ini` (audio/MIDI devices). Never the DAW chunk.

## Tests (gate)

Two-writer: two stores add different items; two constructors; write failure visible; crashed writer does not stuck-lock.

Delete-while-playing: every row in ticket 05's test list (DSP, not only JSON). Pack replace vs delete-fallback.

Pack permutations: SHARE add/replace/name-collision; FULL Overwrite/Add/Reset; settings checkbox standalone vs plugin; transactional failure; contract version refuse; closure auto-include.

Prove each new test fails with the fix reverted, then passes. Windows suite.

## Docs

Changelog lines for library merge, delete-in-use, Pack export/import. EN/DE user guides. Screenshot of Settings Pack row if chrome changed.

## Out of scope

Marketplace. Individual preset files (folded in). MIDI decoder/UI (midi-control spec). Factory preset authoring (play-vs-build spec) — this spec only stores whatever preset ids exist.
