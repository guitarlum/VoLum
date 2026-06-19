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

> Add new ideas here as they come up rather than acting on them mid-slice.
