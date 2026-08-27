# Q2: VoLum 1.2.0 structure decomposition + UI dedup

Seeded from the 1.2.0 quality review. Pure structure/hygiene; no user-visible behavior change. Deferred from
the review pass because the overlay split is medium-to-high risk and benefits from
a focused session with app-smoke screenshots.

## Already landed (Phase 1 + part of Phase 2, changelog `06/29/2026`)

- `VoLumCustomUi.h` is now a 35-line umbrella over `VoLumPresetBar.h`,
  `VoLumListMenu.h`, `VoLumConfirmDialog.h` and `VoLumCustomOverlay.h`. The three
  controls were extracted.
- `DrawVoLumScrollbar` is shared out of `VoLumColorHelpers.h` (~294) and used by
  `VoLumAmpList.h` and `VoLumCustomOverlay.h`.
- Dead `RowAt()` and `mTextFileIdx` are gone.

## Problem — what is still wrong

The 1964-line file was split, but the biggest piece just moved: **
`VoLumCustomOverlay.h` is ~1946 lines** and still owns both full screens (Manage
CRUD + Builder), popups, import and the per-frame int-encoded hotspot registry.
That is the part the ~500-line discipline was aimed at, and the 1.2.1 audit found
several defects clustered in exactly this file, which is corroboration rather than
coincidence.

Other headers now over 500 lines, for context when deciding how far to go:
`VoLumUserSettingsIO.h` 1236, `VoLumFractalArt.h` 1076, `VoLumContentStore.h`
1025, `VoLumTriptych.h` 913, `VoLumPitchShifter.h` 700, `VoLumAmpList.h` 637,
`VoLumCustomContentApi.h` 605, `VoLumCustomModel.h` 565; and
`VoLumLayoutBuild.inc.cpp` 1441, `VoLumSceneRig.inc.cpp` 914,
`VoLumSettingsLocks.inc.cpp` 535.

## Scope — remaining

### Phase 2 remainder (low risk, visual)
- Extract the shared pen/bin/overwrite glyphs. Still duplicated as `static`
  methods in both `VoLumAmpList.h` (~658-672) and `VoLumCustomOverlay.h`
  (~1849-1873), where the comments say outright that they mirror each other.
- Unify `VoLumPreCaptureMenuControl` (`VoLumTriptychMenus.h`) onto
  `VoLumListMenuControl::Row`. The support picker migrated; PRE did not.
- Fix the builder speaker/channel `DrawPopup` (`VoLumCustomOverlay.h` ~1235-1254)
  to clip, cap height and wheel-scroll per `volum-ui.mdc`. It currently draws every
  item with no cap, so a long list overflows its own panel.

### Phase 3 — split the overlay (medium/high risk)
- Keep one `IControl`; move Manage and Builder draw/handle logic into
  `VoLumCustomOverlayManage.h` / `VoLumCustomOverlayBuilder.h` as free functions
  over a context struct (hotspots, scroll offsets, `CustomAmp&`, callbacks).
- Replace the `switch (mManageKind)` x10 with a `ManageKindTraits` table.
- Coordinate with `B6-multi-instance-content-library.md`: B6 wants row identity
  snapshotted in `ReloadList` and threaded through every action, which touches the
  same call sites. Doing them in the wrong order means doing the overlay twice.

## Acceptance Criteria

- No single new UI header exceeds ~500 lines without justification.
- One shared scrollbar + glyph implementation; one shared dropdown row model.
- Builder popup is clip/scroll compliant.
- Pixel-identical UI before/after (compare `win-screenshot.ps1` captures of
  Manage, Builder, PRE/support pickers, preset bar, confirm dialog).
- Tests green; app smoke clean.

## Verification

- `pwsh NeuralAmpModeler/scripts/run-app-win.ps1` + screenshot diff of each
  affected surface.
- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`.
- macOS build via CI.
