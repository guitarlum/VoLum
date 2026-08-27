# Q4 — Screenshot harness: name-area shrink-to-fit pin

## Already landed

Most of this ticket is done. Kept open only for the one remaining pin.

- **Deterministic seeding:** `VOLUM_SEED_CUSTOM_AMPS=N` (`NeuralAmpModeler.cpp`),
  `#ifndef NDEBUG`, seeded into a temp sandbox so it never touches the real
  content store. The seam is `volum::custom::AddCustomAmp` in
  `VoLumCustomContentApi.h`. The `+` builder overlay is not scriptable; do not
  try to complete it with `win-click.ps1`.
- **Scrollbar geometry test:** `VoLumAmpListScroll.h` + `test_volum_amp_list_scroll.cpp`
  (scrollable state, `RowRightX`, thumb size/position, drag-to-offset).
- **Runbook:** `docs/screenshot-recipes.md` plus `docs/screenshot-seed/`.
- **`volum-ui.mdc`:** documents the seeding recipe.
- **`AGENTS.md` Fast Commands:** `win-screenshot.ps1` / `win-click.ps1` /
  `win-key.ps1` pointer (added 2026-08-27).

Note the harness's practical ceiling: `PrintWindow` cannot composite VoLum's GL
surface while the desktop is locked, so the capture comes back as a blank white
client area. Any overnight or unattended screenshot sweep needs an unlocked
session.

## Remaining

The **name-area shrink-to-fit** geometry is still unpinned. `VoLumAmpList.h`
shrinks the label when the scrollbar gutter appears; `VoLumAmpListScroll.h`
covers the gutter/`RowRightX` half, not the text-fit half. Extract a pure helper
and add a doctest next to `test_volum_amp_list_scroll.cpp`.

## Acceptance

- A doctest fails if the name area does not shrink when the list becomes
  scrollable, and fails if it stays shrunk when the list fits.
- No user-visible change.
- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` green.
