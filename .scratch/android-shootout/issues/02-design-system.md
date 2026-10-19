# 02 VoLum design system and art port

Status: blocked
Blocked by: 01
Branch: `feature/android-design` off `feature/android-shootout`
Worker: claude-opus-5-5-high (with `/frontend-design` and `.cursor/rules/volum-ui.mdc`)

## Goal

Port the VoLum desktop identity and moving art to a Compose `:design` module once,
so every candidate inherits the same look and motion.

## Scope

May write: `NeuralAmpModeler/projects/android/design/**`, and a `design-gallery`
screen id + fixture registration in `:core`/`:harness` (only that).
Must not write: `ui-*/`, `engine/`, desktop sources.

## Deliverables

- Tokens: colours, typography (fonts copied from
  `NeuralAmpModeler/resources/fonts/` into `res/font`), spacing, corner radii,
  stroke weights, matched to `VoLumColorHelpers.h` / `Colors.h` and the desktop
  screenshots.
- Components: `VoLumSelection` (AmberPicker / Brass / ListTeal), knob (vertical
  drag, fine adjust on hold, value readout, haptic tick), toggle, mode picker,
  sub-mode pill, meters (IN/OUT), channel stepper, gold corner-bracket frame,
  PRE crosshatch and AMP circuit-board surfaces, list row with thumbnail,
  scrollbar, confirm dialog, toast/error banner.
- Art: Compose Canvas ports of the sunset hero (`VoLumHero.h`), per-amp
  procedural art (`NeuralAmpModeler/art/VoLumArt*.h` via `VoLumArtDispatch.h`),
  the animator (`VoLumArtAnimator.h`, same timing/easing and energy input),
  fractal art (`VoLumFractalArt.h`), pedal-card motifs
  (`VoLumTriptychMotifs.h`, `VoLumPedalCardControl.h`). Frame-rate independent,
  allocation-free per frame, pauses when off screen.
- A `design-gallery` screen showing every token, component state and art piece.

## Acceptance

- `verify-candidate.ps1 -Candidate design` green (build, unit, captures of the
  gallery at both font scales, motion sheets of hero, PLAY art at energy
  0/0.5/1, a pedal toggle).
- Cold Opus review with `rubric.md` criteria 1 (identity) and 6 (motion) both
  ≥ 4 against the desktop references; verdict in `verdicts/design.md`.
- Pushed to GitHub. The coordinator then tells candidate workers to merge
  `feature/android-design` into their branches.

## Verifier checklist

- No Roboto/system font anywhere; no Material default colours leaking.
- Art timing compared frame-by-frame with `reference/` sheets, not by feel.
- Per-frame allocations: none in draw lambdas (check for `Path()`/`Paint()`
  construction inside `onDraw`/`drawBehind` without `remember`).
