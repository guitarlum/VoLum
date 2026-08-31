# `.volumpack` export / import

Status: done
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

## Result

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` → 787 cases, 0 failed.

- `VoLumPackArchive.h`: the zip half, and nothing else. STORE method 0 only, own
  CRC32, local headers + central directory + EOCD, no new dependency. Reading is
  all-or-nothing: any bad record, unsupported method, unsafe name or checksum
  mismatch returns zero entries with a reason, because a half-read Pack that looks
  like a small Pack is how a truncated download becomes a partial library.
- `VoLumPack.h`: the contract. `contractVersion` starts at 1 and a newer one is
  refused by name ("needs VoLum 1.3.0") rather than guessed at. `BuildExportPlan`
  is the closure: a ticked amp brings its bank, and every preset drags in the IRs,
  pedals and support amp it references - transitively, since a partner amp has its
  own bank with its own requirements. A factory-amp preset pulls its custom IR and
  pedal but no amp entry, so a Pack never ships VoLum's own captures back.
- `ApplyPack` is transactional: payload files land first, the registry is copied to
  `volum-content.json.pre-import.bak`, and the merged catalog goes out through
  `ContentStore::Save()` - the locked read-modify-write from ticket 01 - so an
  import is just another catalog writer and a sibling's unflushed item survives it.
  A Pack that does not open changes nothing at all.
- Pedal PRE indices are local, not portable. An incoming pedal whose `legacyIndex`
  is already taken by a *different* pedal is renumbered, and the presets inside
  that Pack are rewritten to follow it. Re-importing the same pedal keeps the index
  local presets already point at.
- `VoLumPackOverlay.h`: the modal. Export offers Everything vs a tick list and
  names what the ticks dragged in ("Also including: IR ..."), locked, not
  uncheckable. Import opens the picker straight away, then previews add / replace /
  reload-in-use / same-name / delete. The three verbs appear only for an Everything
  Pack - offering Reset for a Pack a friend sent would invite wiping your library.
  The machine-settings checkbox is standalone-only.
- `VoLumSettingsPackRowControl` puts the two buttons on a full-width "Content
  library" row rather than a fourth card: a Pack is about the whole library, not
  one of the three per-instance settings the cards hold.

Standalone check at 900x600 (`.scratch/pack/shots/`): the Settings row sits between
the card row and the shortcut box with no crowding; Export (Everything and
selection + closure line) and Import (six-row preview, three verbs with
sub-captions, settings checkbox) all fit the 560x408 box without overlap. A real
round trip through the native Save-As dialog wrote a 1.3 MB Pack of the seeded
library and the Import preview read it back as `Everything Pack - 1 amp, 1 IR,
1 pedal`.

## Revert-fail proof

Three batches, each `run-tests-win.ps1` with the fixes reverted and then restored.

**Batch A - reader-side gates** (6 cases fail):

| Revert | Failing cases |
| --- | --- |
| Skip the CRC check on extract | `A corrupt Pack is refused with a reason, never read as a short one`, `A corrupt Pack changes nothing at all` |
| Accept any entry name on write and on read | `An entry name that escapes the archive root is refused on write and on read` |
| Ignore `contractVersion` and read a newer Pack anyway | `A newer contract is refused by name; an older one always imports` |
| A manifest file that is not in the archive is skipped instead of refused | `A Pack whose manifest promises a file it does not carry is incomplete` |
| Trust the payload instead of the manifest's job (Share keeps settings + MIDI map) | `A Share Pack that arrives with settings or a MIDI map still imports as Share` |

**Batch B - export side** (4 cases fail):

| Revert | Failing cases |
| --- | --- |
| `BuildExportPlan` returns the raw ticks, no closure | `Export closure auto-includes what a selection references` (3 subcases) |
| Share carries the MIDI map and the settings document | `A Share Pack carries no settings and no MIDI sound map; Everything carries both`, `Share import merges by id: new ids add, extras stay` |
| An unreadable capture is skipped instead of refused | `Export refuses when a capture file is missing rather than packing a hole` |

**Batch C - apply side** (8 cases fail):

| Revert | Failing cases |
| --- | --- |
| `Add` behaves like `Overwrite` | `Overwrite, Add and Reset differ exactly where the ticket says they do` |
| `Reset` keeps local-only items | `Overwrite, Add and Reset differ exactly where the ticket says they do`, `Reset without the settings box can leave a MIDI slot invalid, never renumbered` |
| A plugin writes machine settings too | `Machine settings and the MIDI map ride the standalone checkbox, not the verbs` |
| Never renumber an imported pedal; always take the Pack's index | `An imported pedal whose PRE index is taken is renumbered, and the Pack's presets follow`, `Re-importing the same pedal keeps the PRE index local presets already point at` |
| No backup before the registry is rewritten | `A successful import leaves the prior library in a backup` |
| Apply without validating the Pack | `A corrupt Pack changes nothing at all` |
| Write the registry straight out instead of through the locked merge | `An import is a catalog writer: a sibling's unflushed item survives it` |
