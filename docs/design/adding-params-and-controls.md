# Adding a param / adding a control — VoLum checklist

The two recurring bug classes in VoLum are **a persisted field that silently
fails to save** and **a mutually-exclusive control that ships with a missing or
mismatched highlight**. Both happen because the relevant logic has several edit
sites. These checklists enumerate every site so nothing is missed. Follow the
matching scoped rule for detail: `.cursor/rules/volum-state-params.mdc` (state)
and `.cursor/rules/volum-ui.mdc` (UI).

---

## Adding a persisted parameter / `VoLumAmpSettings` field

Do these in order; the round-trip test at the end is what proves it is done.

1. **Data model** — add the field to the `VoLumAmpSettings` struct in
   `VoLumAmpeteCatalog.h` (this is the single source of truth).
2. **JSON codec (write)** — add it to the matching *block* helper, never to a
   second inline list:
   - core amp field -> `WriteAmpCoreBlock` / `ReadAmpCoreBlock`
     (`VoLumUserSettingsIO.h`)
   - PRE field -> `PreBlockToJson` / `PreBlockFromJson`
   - POST field -> `PostBlockToJson` / `PostBlockFromJson`
   `VolumUserSettingsToJson` composes these, so you do **not** touch its body.
3. **param <-> struct bridge** (only if the field is driven by an `EParam` knob):
   `_VolumSavePreToSlot` / `_VolumRestorePreFromSlot` /
   `_VolumSavePostToSlot` / `_VolumRestorePostFromSlot` (`VoLumSettings.inc.cpp`).
4. **PRE/POST lock chrome** (only if the field is inside a lockable block):
   `PreBlockEquals` / `PostBlockEquals` (`VoLumPrePostLock.h`).
5. **DAW chunk path** (only if it must survive in a saved DAW project):
   add a version branch in `Unserialization.cpp`; never rewrite old readers.
6. **EParam** (only if it is a host-automatable knob): append to `EParams`
   (order is serialization-sensitive — never reorder), give it a stable
   `GetName()` string, set its range/step, and add a keyboard step in
   `VoLumKeyboardModel.h`.

Tests (mandatory — see `vo-lum-workflow.mdc`):

- **Always** extend the exhaustive pin "User settings IO round-trips EVERY
  VoLumAmpSettings field" in `test_volum_user_settings_io.cpp` to set the new
  field to a non-default, in-range value. If it is not in that test, it is not
  done.
- New `EParam`: pin its index/step in `test_eparam_order.cpp` /
  `test_keyboard_steps.cpp`.
- New chunk branch: `test_volum_chunk_version.cpp` + `test_volum_chunk_codec.cpp`.

---

## Adding a mutually-exclusive (selectable) control

1. **Draw selection through the SSOT** — never hand-draw the active/hover
   background. Use `DrawVoLumSelection(g, rect, active, hovered, style)` and tint
   the label with `SelectionInkColor(style, active)` (both in
   `VoLumColorHelpers.h`).
2. **Pick the right `VoLumSelectionStyle`** for the surface (keep the dialect):
   - `AmberPicker` — mode pickers / sub-pills (Pitch, Tremolo, Delay, Reverb).
   - `Brass` — cab/speaker row, metronome time-sigs.
   - `ListTeal` — sidebar amp list, list-menu rows.
3. **Scrollbar** (only if the control is a runtime-growable list): draw it with
   `DrawVoLumScrollbar`, clip to the rect, cap height to the window, and wire
   mouse-wheel scrolling. Put the geometry math in a pure helper with a doctest
   (see `VoLumAmpListScroll.h` / `test_volum_amp_list_scroll.cpp`).
4. **File placement** — put the control in the correct owner file (see the file
   map in `.cursor/rules/volum-ui.mdc`); do not add it to a monolith.

Tests:

- Extend the selection source-guard in `test_volum_ui_regressions.cpp` to assert
  the new control routes through `DrawVoLumSelection`.
- Prefer a pure-helper test for any geometry/state before a source-string lock.
- Run the Windows app smoke check and inspect the highlight on screen.

---

## Verification (every change)

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`; for UI changes also
`pwsh NeuralAmpModeler/scripts/run-app-win.ps1` and inspect; append a line to
`NeuralAmpModeler/installer/changelog.txt`.
