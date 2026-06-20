# VoLum UI Modernization Ideas (parking lot)

Running list of larger visual/UX modernization ideas spotted while iterating on
the 1.2.0 BYO + presets UI shell. These are intentionally **out of scope** for
the current slices - they are candidates for a future dedicated "UI refresh"
pass. Nothing here should block shipping the BYO/presets work.

## Observed during BYO + presets refinement (Jun 2026)

- **Overall flatness / depth.** The palette is mostly flat fills on a near-black
  background. A future pass could add subtle depth: a faint vignette/gradient on
  the main panel, soft inner shadows on inset controls (knob wells, the cab row),
  and a quiet grain/texture so large dark regions don't read as empty.
- **Knob rendering.** The bottom-row knobs are simple arc + pointer. Modern amp
  sims use layered knobs (metallic ring, tick marks, value-arc glow on the
  active lane). Worth a dedicated knob restyle.
- **Typography hierarchy.** Section captions, value readouts, and the hero amp
  name all sit at similar weights. A clearer type scale (one display face for the
  hero, a tighter mono/condensed face for numeric readouts) would sharpen the IA.
- **Hero art framing.** The procedural hero art floats in a plain bordered box.
  Could gain a more intentional frame (corner brackets already exist elsewhere in
  the UI - reuse them) and a faint reactive glow tied to output level.
- **Manage / dropdown panels.** Pre-redesign these read as utilitarian list
  boxes. Slice 5 addresses the worst of it (inline icons, scrollbar, sizing); a
  later pass could add row hover transitions and iconography per item type.
- **Empty states.** "No files yet", "No presets yet", and the builder coverage
  empty state are plain text. Friendly illustrated/iconographic empty states
  would feel more finished.

## Observed during BYO follow-up fixes (Jun 20 2026)

- **Tooltips are functional, not styled.** We now have hover tooltips on all the
  new icon buttons, but they use the stock iPlug tooltip chrome. A future pass
  could give them the VoLum panel look (dark rounded chip, cream text, slight
  delay/fade) and consistent placement so they don't cover the thing they
  describe.
- **Confirm modal is generic.** The "Are you sure?" dialog is reused for delete
  and overwrite with only the confirm-button label changing. It still reads as a
  plain box — could gain an icon (trash vs overwrite), color-coded confirm
  (red destructive vs amber caution), and a keyboard affordance (Enter =
  confirm, Esc = cancel) hint.
- **Inline error banners.** Name-collision errors show as an amber text banner
  that replaces the hint line. Works, but a small inline field-level treatment
  (red outline on the offending input + icon) would be clearer than a banner far
  from the field.
- **Custom-art selection language.** Settled on cyan art + gold border/badge for
  the selected swatch. The broader question of how "active/selected" is signalled
  is still inconsistent app-wide (gold border here, copper for active Custom IR,
  teal for No Cab, gold for stock cabs) — a future pass should codify one
  selection-state visual language and a documented accent palette.
- **Cabinet row vs Custom IR affordance.** The single cabinet row reads well, but
  `Custom IR` is a text button sitting next to cab chips; a dedicated
  IR/upload-style control (with the impulse glyph more prominent and a clear
  "edit/swap" affordance) would make BYO-IR more discoverable.
- **Builder coverage grid.** The (slot × channel) grid is information-dense and
  functional; a future pass could add column/row hover highlight, a clearer
  "duplicate" treatment than the small `x2` badge, and drag-to-assign.

> Add new ideas here as they come up rather than acting on them mid-slice.
