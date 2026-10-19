# 07 Gallery

Status: blocked
Blocked by: 02, 03, 04, 05, 06 (or the 07:30 cut-off, whichever comes first)
Worker: claude-opus-5-5-high

## Goal

One page the owner opens in the morning to compare the candidates and pick.

## Scope

May write: `.scratch/android-shootout/gallery/**`, `.scratch/android-shootout/apk/**`,
`.scratch/android-shootout/reports/07-gallery.md`.

## Deliverables

- `gallery/index.html` (static, opens from disk, relative paths into
  `evidence/`): one row per screen id, one column per candidate (latest
  evaluated commit), desktop reference beside where one exists; font-scale 1.3
  toggle; motion contact sheets; verdict and scores per candidate with the
  commit and round; "not approved by cut-off" shown plainly when true.
- APKs for each candidate's latest commit in `apk/<x>-<shortsha>.apk` plus a
  three-line install note (`adb install -r ...`, laptop `adb` is at
  `%LOCALAPPDATA%\Android\Sdk\platform-tools\adb.exe`).
- An advisory ranking from a cold second-family reviewer (gpt-5.6-sol-high)
  with reasons. It ranks; it does not pick.
- Engine spike summary (three lines) linking `reports/06-engine-spike.md`.

## Acceptance

- `gallery/index.html` references every candidate's latest commit sha and every
  screen id from the registry; no broken image paths (a script checks).
