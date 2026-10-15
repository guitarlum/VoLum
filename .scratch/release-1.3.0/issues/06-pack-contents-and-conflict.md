# What is in a Pack, and what happens on conflict?

Type: grilling
Status: resolved
Blocked by: 04

## Question

There is no portable export of machine settings + custom content today. `volum-settings.json`, the content library (`volum-content.json` + copied captures), and the DAW project chunk are three different stores. “Export settings” is overloaded: users mean backup of custom amps, presets, and preferences.

Prior art: `backlog/F10-settings-import-export.md`. Individual preset export in `backlog/F5-presets-full-rig.md` is the same portability problem — fold it into this decision, do not invent a second format.

Already locked on [How does the content library survive two writers?](04-two-writer-library.md) — do not re-ask:

- Import is a catalog writer under the same lock+merge.
- Catalog conflict: merge by stable id (new ids add; same id replaces that item; name-only collision is not a silent replace).
- The importing instance also applies the Pack to its live rig; other instances’ live rigs do not change.
- After they confirm an import that replaces a currently-playing library id, this instance **reloads** the new capture (not the delete-fallback). See [What happens when we delete content that is currently playing?](05-delete-while-playing.md).

Still decide:

1. **Payload.** Settings only, library only, or one bundle of settings + referenced content (amps, IRs, pedals, presets)? MIDI sound map lives with the library, so it rides in if the library is in the bundle.
2. **Failure.** Transactional stage+swap, prior tree left in a backup, never clobber on a bad bundle?
3. **Older/newer.** Bundle carries a schema version; importing newer into older degrades via the tolerant reader?
4. **Shape.** File extension; v1 export ALL vs selected amps+presets (also in the map’s Not yet specified).
5. **Scope UI.** Export and import both show scope: import names what will add vs replace/modify before confirm; export offers ALL or certain amps and their presets. Surfaced while resolving delete-while-playing — decide it here, not there.

Recommend: one bundle of settings + referenced content, transactional import. DAW chunks stay the host’s problem — a Pack is a machine/library backup, not a project file.

## Answer

**Jobs, one format.** A **Pack** is a `.volumpack` archive (zip under the hood). Two jobs, not two formats: **SAVE/RESTORE** (Everything) and **SHARE/IMPORT** (choose items). F5’s “export one preset file” folds in — no second extension. Never `settings.ini` (audio/MIDI devices). Never the DAW chunk. Marketplace of custom amps/presets is later.

**Where.** Gear → Settings: Export Pack… / Import Pack…. Not File → Preferences, not Manage, not PLAY-only.

**Export.** Everything, or selected amps and/or named presets. Export computes the **closure** and auto-includes it (custom amp `.nam` files; IRs and pedals those presets/amps reference). The export preview lists “also including”; a requirement cannot be unchecked. Factory-amp presets do not pack the factory capture (shipped). Unused IRs/pedals only travel in Everything.

**SHARE pack payload.** Library items + files only. No `volum-settings.json`, no MIDI sound map.

**SHARE import.** Catalog merge: extras stay. Preview names each add vs replace/modify (and the in-use reload case). Name-only collision is not a replace: keep both, same names, preview warns. Does not switch the sounding rig to a newly added amp. If they confirmed replace of a currently-playing library id, this instance **reloads** that capture ([What happens when we delete content that is currently playing?](05-delete-while-playing.md)). Other instances never change.

**Everything pack payload.** Library + files + MIDI sound map + `volum-settings.json` (last scenes, Lite, calibration, last amp, dual-amp sidecar).

**FULL import (library items only).** Three verbs — custom amps, IRs, pedals, named presets. Factory amps cannot be deleted.

| Verb | Same library id already present | Local-only items |
| --- | --- | --- |
| **Overwrite** | Pack wins | Stay |
| **Add** | Skip (keep mine) | Stay |
| **Reset** | Pack wins | Deleted; confirm names them |

**Machine settings.** Separate standalone-only check: “Also restore machine settings” (writes `volum-settings.json`, applies last-amp/scenes/Lite/calibration, replaces the MIDI sound map). Plugins Import the library; they never write `volum-settings.json`. MIDI map rides this check, not the three verbs. Reset without it can leave **invalid** MIDI slots (numbers stay, red).

**Failure.** Transactional: validate, stage, swap. Prior tree left in a backup. A corrupt or incomplete Pack changes nothing.

**Contract.** Pack format version is a **contract**, not the app version. Newer contract → older app refuses with “needs VoLum X.Y”. Older Packs always import. Same contract from a newer app: import; unknown keys ignored (today’s tolerant readers). Do not refuse merely because the exporting build is newer.

**Already locked, not re-opened.** Import is a catalog writer under lock+merge ([How does the content library survive two writers?](04-two-writer-library.md)). Same-id replace is Overwrite’s rule; Add is the opt-out; Reset is the wipe-extras opt-in. DAW projects stay the host’s problem.
