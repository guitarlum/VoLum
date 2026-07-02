# Q2: VoLum 1.2.0 structure decomposition + UI dedup

Seeded from the 1.2.0 thermo-nuclear review (`docs/design/1.2.0-quality-review.md`,
section 3). Pure structure/hygiene; no user-visible behavior change. Deferred from
the review pass because the overlay split is medium-to-high risk and benefits from
a focused session with app-smoke screenshots.

## Problem

`VoLumCustomUi.h` is 1964 lines and violates the repo's ~500-line header
discipline. `VoLumCustomOverlayControl` alone is ~1407 lines (73% of the file)
and owns two full screens (Manage CRUD + Builder), popups, import, and a
per-frame int-encoded hotspot registry. UI primitives are duplicated across
files.

## Scope

### Phase 1 — low risk, pure file moves
- Turn `VoLumCustomUi.h` into an umbrella (like `VoLumControls.h`).
- Extract `VoLumPresetBarControl`, `VoLumListMenuControl`,
  `VoLumConfirmDialogControl` into their own `VoLum*.h` headers.
- Update the file-map table in `AGENTS.md` and `.cursor/rules/volum-ui.mdc`.

### Phase 2 — dedup primitives (low risk, visual)
- Extract `DrawVoLumScrollbar` into `VoLumColorHelpers.h`; replace the 3 copies in
  `VoLumCustomUi.h` and the one in `VoLumAmpList.h`.
- Extract shared pen/bin/overwrite glyphs (triplicated with `VoLumAmpList.h`).
- Unify `VoLumPreCaptureMenuControl` (`VoLumTriptychMenus.h`) onto
  `VoLumListMenuControl::Row` (the support picker already migrated; PRE did not).
- Fix the builder speaker/channel `DrawPopup` to clip + cap height + wheel-scroll
  per `volum-ui.mdc`.

### Phase 3 — split the overlay (medium/high risk)
- Keep one `IControl`; move Manage and Builder draw/handle logic into
  `VoLumCustomOverlayManage.h` / `VoLumCustomOverlayBuilder.h` as free functions
  over a context struct (hotspots, scroll offsets, `CustomAmp&`, callbacks).
- Replace the `switch (mManageKind)` x10 with a `ManageKindTraits` table.
- Remove dead `RowAt()` and `mTextFileIdx`.

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
