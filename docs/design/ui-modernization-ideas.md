# VoLum UI Modernization Ideas (parking lot)

Running list of larger visual/UX modernization ideas spotted while iterating on
the 1.2.0 BYO + presets UI shell. These are intentionally **out of scope** for
the current slices - they are candidates for a future dedicated "UI refresh"
pass. Nothing here should block shipping the BYO/presets work.

## Status: 1.2.0 UI modernization sweep (Jun 20 2026)

A dedicated visual/UX-only sweep ran on `feature/1.2.0-ui-polish`. It codified a
design-token system in `VoLumColorHelpers.h` (one brass selection language,
spacing/type scales, reusable depth helpers) and applied it across every view.
Items below are tagged **[DONE]**, **[PARTIAL]**, or left untagged (still
parked). Deferred items were intentionally out of scope: reactive/audio-driven
motion (the locked "subtle motion" constraint) and anything touching params,
routing, or layout structure (the backend session owns those).

## Observed during BYO + presets refinement (Jun 2026)

- **[DONE] Overall flatness / depth.** Added `FillVGradient` / `DrawPanelDepth` /
  `DrawInsetWell` / `DrawKnobWell` / `DrawVignette` helpers and applied them to
  the canvas, sidebar, AMP/PRE/POST/SUPPORT panels, pedal cards, meters, and
  overlay panels. (Grain/texture was skipped on purpose - gradient + vignette
  already kill the empty-black read.)
- **[DONE] Knob rendering.** New `VoLumDialKnobControl` (layered metallic body,
  recessed well shadow, tick ring, value-arc with an active-lane glow, accent
  pointer) replaces the plain arc + pointer on all knob rows.
- **[DONE] Typography hierarchy.** Codified `VoLumType` (Display/Caption/Label/
  Value/Body). Hero amp name and effect titles use the Poiret display face;
  numeric readouts stay on the calmer body face for legibility at small sizes.
- **[PARTIAL] Hero art framing.** Panels now carry the depth treatment and the
  existing corner-bracket frame reads more intentionally. The **reactive glow
  tied to output level** is deliberately deferred (audio-reactive motion is out
  of this sweep's locked "subtle motion" scope).
- **[PARTIAL] Manage / dropdown panels.** List menus and manage panels now use
  the inset-well background and the shared brass selection language. Per-row
  hover transitions and per-item-type iconography are still parked.
- **Empty states.** "No files yet", "No presets yet", and the builder coverage
  empty state are plain text. Friendly illustrated/iconographic empty states
  would feel more finished. (Untouched by the sweep.)

## Observed during BYO follow-up fixes (Jun 20 2026)

- **Tooltips are functional, not styled.** We now have hover tooltips on all the
  new icon buttons, but they use the stock iPlug tooltip chrome. A future pass
  could give them the VoLum panel look (dark rounded chip, cream text, slight
  delay/fade) and consistent placement so they don't cover the thing they
  describe. (Stock iPlug tooltip chrome isn't cleanly restyleable from our draw
  code - left parked.)
- **[DONE] Confirm modal is generic.** The dialog is now color-coded (red
  destructive vs amber caution via `_IsDestructive()`), draws a caution glyph
  and panel depth, and shows an "Enter to confirm / Esc to cancel" hint with a
  real `OnKeyDown` handler for both keys.
- **Inline error banners.** Name-collision errors show as an amber text banner
  that replaces the hint line. Works, but a small inline field-level treatment
  (red outline on the offending input + icon) would be clearer than a banner far
  from the field. (Still banner-based after the sweep.)
- **[DONE] Custom-art selection language.** Codified one selection-state language:
  brass `SEL_*` tokens (wash + bright border + soft glow + cream text) now mark
  the active item in every mutually-exclusive group (cab row, sidebar, list
  menus, metronome time-sigs, builder art picker). Teal stays the SUPPORT-lane
  identity and copper stays the BYO/Custom-IR identity, documented as such in
  `VoLumColorHelpers.h`.
- **[PARTIAL] Cabinet row vs Custom IR affordance.** Custom IR keeps its copper
  identity and impulse glyph and now shares the unified active treatment, but a
  dedicated upload/edit-swap control is still parked.
- **Builder coverage grid.** The (slot × channel) grid is information-dense and
  functional; a future pass could add column/row hover highlight, a clearer
  "duplicate" treatment than the small `x2` badge, and drag-to-assign. (Cosmetic
  depth only from the sweep; interaction unchanged.)

> Add new ideas here as they come up rather than acting on them mid-slice.
