# F8 — Custom pedals: assignable type groups

Importing custom PRE pedals shipped in 1.2.0. Only the grouping half of the
original ticket is still open.

## Already shipped (do not re-plan)

Import of `.nam` captures into the PRE pedal slots from the Manage panel, listed
under a **CUSTOM** group in the PRE capture menu, with a stable per-pedal index
(`PedalItem.legacyIndex` / `nextPedalIndex`) so existing projects keep pointing at
the same capture. Changelog `06/20/2026`; pool-exhaustion handling `06/29/2026`;
1.2.1 added write-failure reporting and stopped a failed import from consuming a
slot. See `docs/user-guide.en.md` and `VoLumContentStore.h`.

## Still open

Custom pedals all land in one flat **CUSTOM** group. The original ticket wanted
the user to assign a type — Klon / TS-Boost / Distortion / Fuzz / Other — so
imports file themselves next to the curated pedals of the same character instead
of in an undifferentiated list that grows without structure.

The schema is already there and unused: `PedalItem.group` exists in
`VoLumContentStore.h`, but the import path passes an empty group
(`AddPedal(base, rel)` at `VoLumCustomOverlay.h` ~735) and no UI ever sets it.
So this is a UI + menu-grouping change, not a storage change.

Decide:

- Fixed group list or free text? Fixed is testable and keeps the menu tidy; free
  text repeats the naming problem the custom-amp cab slots deliberately avoided.
- Where the group is chosen: at import time, or editable afterwards in Manage.
  Editable means the PRE menu has to regroup live.
- Whether custom pedals interleave with curated pedals of the same group, or sit
  in a `CUSTOM` sub-section beneath each. Interleaving reads better but makes the
  delete-while-loaded case (a deferred 1.2.1 finding) harder to reason about.
- Migration: every existing pedal has an empty group. `Other` on read is the
  obvious default; confirm that leaves projects sounding identical.

## Acceptance criteria

- Group choice survives a save/reload and a library re-read.
- Pedals imported before this change still load and still resolve to the same
  capture; no PRE index churn.
- Doctests for group persistence, the empty-group migration default, and menu
  grouping as a pure helper.
- EN/DE user guide + changelog.

Work on a branch off latest `dev`. Do not commit to `dev` or `main` directly.
