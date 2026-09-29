# Pack docs, changelog, screenshots

Status: done
Blocked by: 03

## Goal

User-visible finish for the library lock, delete-in-use, and Pack.

## Do this

- Changelog: one line each for (1) two VoLum windows no longer dropping each other's library items, (2) deleting in-use content reverts this instance cleanly, (3) Export/Import Pack.
- EN/DE user guides: Settings Pack, SHARE vs Everything, conflict verbs, MIDI map rides Everything + settings checkbox. Keep both languages in sync.
- Screenshot recipe + PNG if Settings chrome grew (Pack row).
- Mark tickets 01–04 resolved. No `ready-for-agent` left.

## Done when

EN/DE match. Windows tests still green.

## Result

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` → 787 cases, 0 failed.

- `installer/changelog.txt`: three entries, newest first — Pack export/import (new),
  delete-in-use moving the rig (fix), two windows no longer dropping each other's
  library items (fix, including the per-amp scenes moving out of the shared file).
- `docs/user-guide.en.md` / `docs/user-guide.de.md`: new "Content Library Packs" /
  "Packs der Inhaltsbibliothek" section under Settings covering scope (Everything vs
  selection + the closure that cannot be unticked), the import preview, the three
  FULL verbs, why a shared Pack has no Reset, the standalone-only settings box, the
  backup file, contract refusal, and PRE-slot renumbering. The Custom Content
  section gained the two-writer and delete-while-playing paragraphs and a link
  across. Both languages carry the same paragraphs in the same order.
- `docs/user-guide-pack-import.png`: the import preview on the seeded library, at
  the shared 900x600 docs framing, referenced from both guides.
- `docs/screenshot-recipes.md`: recipe row for that PNG, plus the caveat that the
  Pack file dialogs are separate top-level windows and need `SendKeys` against the
  foreground window rather than `win-key.ps1`.

Tickets 01-04 are all `done`; nothing is left `ready-for-agent`.
