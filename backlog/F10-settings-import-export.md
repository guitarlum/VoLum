# F10 — Settings + custom-content import/export

Add a way to export and import a user's VoLum configuration so they can move it
between machines, back it up, or share a rig pack. Today `volum-settings.json`
and the user content (custom-amp manifests + copied `.nam`, IR library + copied
wavs, imported pedal captures, preset banks) live only in the local user-content
directory; there is no portable bundle.

Scope to decide in the ticket:

- **Bundle format** — a single archive (e.g. `.volumpack` = zip) that contains
  `volum-settings.json` plus the referenced custom content (manifests + `.nam` +
  IR wavs), addressed by the stable opaque IDs in `volum-content.json`, so
  references survive re-import even if local ordering differs.
- **Export** — "Export settings…" in the settings overlay: writes the bundle;
  optionally lets the user pick subsets (all / presets only / a single custom
  amp + its content).
- **Import** — "Import settings…": validates the bundle, copies content in under
  fresh local paths, **re-keys by stable ID** (skip IDs that already exist or
  offer merge/replace), and rewrites references. Must reuse the tolerant reader
  (ignore unknown keys, clamp/skip bad values, drop orphaned refs) and never
  clobber the existing config on a bad bundle — import is transactional (stage
  to temp, swap on success, leave a `.bak` of the prior settings).
- **Conflict policy** — same-name/different-ID custom amps, duplicate IRs/pedals,
  preset-bank merges. Define merge vs replace vs rename-on-import.
- **Versioning** — bundle carries the settings `schemaVersion`; importing a newer
  bundle into an older build degrades gracefully (tolerant reader rules).

Deliverables: format spec + schema, export/import UX in `VoLumSettingsOverlay.h`,
stable-ID re-keying logic as a pure helper with doctests (round-trip,
conflict/merge, orphaned-ref drop, corrupt-bundle rejection leaves config
intact), docs EN/DE + changelog. Do not implement here.

Depends on the 1.2.0 BYO + presets backend (stable IDs, settings JSON schema,
tolerant reader, removal matrix in `VoLumContentStore.h`).
Work on a dedicated `feature/settings-import-export` branch off the latest
`dev`; merge back into `dev` only after acceptance criteria + tests/docs/
changelog are in place. Never promote to `main` outside of a release.
