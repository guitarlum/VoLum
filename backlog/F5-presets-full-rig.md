# F5 — Presets: factory bank + shareable preset files

The bulk of F5 shipped in 1.2.0. This is the remainder, re-scoped after verifying
what is actually in the code.

## Already shipped (do not re-plan)

Named full-rig presets — amp(s), cab/channel, PRE, POST, dual amp — with save,
recall, overwrite, rename, delete, `<`/`>` cycling and an `(unsaved)` dirty
marker. Banks live in `volum-content.json` (`presetBanks`), shared by the
standalone and every plug-in instance. Orphaned references to deleted content are
dropped on recall rather than crashing. See changelog `06/20/2026`,
`docs/user-guide.en.md` § Presets, and `VoLumSettingsPresets.inc.cpp` /
`VoLumPresetBar.h`.

1.2.1 additionally made preset ops claim the live rig per window, resolve
rename/delete by identity rather than row position, and report library write
failures.

## Still open

### 1. A factory preset bank

There is no curated starting set. The only reset affordance is the
`Default (factory settings)` row (`VoLumAmpMenus.inc.cpp` ~61), which restores
shipped defaults for the current amp — not a named preset library.

Decide: are factory presets shipped as a read-only bank alongside the user's
banks, or seeded into a user bank on first run? Read-only is safer (an upgrade can
refresh them) but needs the UI to refuse overwrite/rename/delete on those rows,
which currently assume mutability. They also have to survive the identity-based
lookup 1.2.1 introduced.

### 2. Export / import of individual presets

No way to hand a preset to another player. A preset references content by opaque
id (custom amps, IRs, pedals), so an exported file is only meaningful with the
captures it names: either export must refuse presets that reference custom
content, or the format has to carry the payloads and the importer has to
de-duplicate against the receiving library. That decision is the whole ticket.

Note `B6-multi-instance-content-library.md` is a prerequisite in spirit: import
writes the shared library, which is exactly the surface B6 is fixing.

## Acceptance criteria

- Factory presets are reachable on a clean install and cannot be silently
  destroyed by the normal Manage actions.
- An exported preset re-imported on another machine either reproduces the rig or
  says precisely which content is missing — never loads a half-rig silently.
- Import cannot corrupt or duplicate existing library content.
- Doctests for the export/import round trip and for factory-bank immutability.
- EN/DE user guide + changelog.

Work on a branch off latest `dev`. Do not commit to `dev` or `main` directly.
