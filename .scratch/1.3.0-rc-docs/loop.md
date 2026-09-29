# 1.3.0-rc-docs

## Goal

Finish the 1.3.0 RC user how-to on `feature/1.3.0`: PLAY empty → picker → board, Pack export then import, README five headlines with PLAY then BUILD heroes, recipes, ticket 04, one changelog docs line. Then restore the personal library and dispatch full CI. A second agent must review EN/DE/README against this goal and leave no blocking mismatches. Do not bump version, merge, or commit unless asked.

## Predicate

- `pwsh .scratch/1.3.0-rc-docs/verify.ps1` exit 0

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1`.
Do not trust chat memory. Continue until every Predicate line passes.
Escalate and stop on new product, new sound / golden retune, irreversible
git, or a real dead end.

Required figures: play-empty, play-picker, play, main, settings-signal,
settings-midi, settings-system, pack-export, pack-import. Recapture chorus
and post if they still show PLAY|BUILD words. Generate any Pack from
docs/screenshot-seed into `%TEMP%\volum-docs\seed.volumpack`; do not commit
a .volumpack. Restore `%LOCALAPPDATA%\VoLum` from VoLum-backup and dispatch
`ci-watch.ps1 -Ref feature/1.3.0 -Dispatch` after verify is green. No commit
unless the owner asks.

## Ledger

- 2026-09-09 14:57 — Owner picked predicate A (file checks) plus a second-agent docs review. Wrote this loop. Predicate not run yet.
- 2026-09-09 15:02 — Recaptured pack-import from `%TEMP%\volum-docs\seed.volumpack` (Everything Pack preview with Overwrite/Add/Reset). Recaptured post + chorus on ORS100 with compact header. Predicate not yet green (prose still old).
- 2026-09-09 15:07 — EN/DE PLAY three figures and Pack export-then-import; README five bullets + PLAY then BUILD; recipes canvas coords; ticket 04 resolved; changelog docs line. `verify.ps1` exit 0. Restored `%LOCALAPPDATA%\VoLum` from backup. Dispatched full CI https://github.com/guitarlum/VoLum/actions/runs/34355112067 on e07287b. Second-agent review in flight.
- 2026-09-09 15:13 — Docs review PASS (no blocking mismatches). Optional nits not applied. Predicate still green. Loop complete.
