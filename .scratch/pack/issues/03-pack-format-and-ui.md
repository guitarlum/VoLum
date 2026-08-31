# `.volumpack` export / import

Status: ready-for-agent
Blocked by: 01, 02

## Goal

Gear → Settings: Export Pack… / Import Pack…. Format, verbs, previews, transactional import per `.scratch/pack/spec.md`.

## Do this

- Archive: zip STORE (method 0). `manifest.json` carries `contractVersion` (start at 1), job (`share` | `everything`), item lists, closure.
- Export preview: Everything vs selection; auto-included companions listed and not uncheckable.
- Import preview: add vs replace/modify; name-only collision warning; in-use reload case.
- FULL verbs Overwrite / Add / Reset. Standalone-only "Also restore machine settings".
- Transactional stage+swap; prior tree backed up; corrupt Pack changes nothing.
- Newer contract → refuse with "needs VoLum X.Y" (X.Y = the first VoLum that writes that contract; for v1 that is 1.3.0).

File picker via existing iPlug patterns. Keep Settings layout readable at 900×600.

## Tests (must fail with this ticket reverted)

Permutation tests in `test_volum_pack.cpp` (both CMake and vcxproj): SHARE add/replace/name-collision; FULL Overwrite/Add/Reset; settings checkbox; plugin path does not write `volum-settings.json`; transactional failure; contract refuse; closure auto-includes IR/pedal referenced by a selected preset; factory capture not packed for a factory-amp User preset.

## Done when

Standalone launch: Settings shows Export/Import; a round-trip Pack of a seeded library restores items. Revert-fail proven. Docs in ticket 04.
