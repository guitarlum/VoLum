---
name: volum-ui-change
description: Implement VoLum UI/layout changes efficiently. Use when modifying VoLum controls, PRE/AMP/POST triptych behavior, tuner/metronome overlays, sidebar/hero art, knob rows, or visual styling.
---

# VoLum UI Change

## Read Order

1. `AGENTS.md` for routing only.
2. `.cursor/rules/volum-ui.mdc`.
3. Smallest owner file:
   - `VoLumTriptych.h` for PRE/AMP/POST and pedal cards.
   - `VoLumCoreControls.h` for base controls and overlays.
   - `VoLumColorHelpers.h` for shared colors/helpers.
   - `VoLumFractalArt.h` only for fractal art changes.

## Implementation Rules

- Do not dump new controls into `VoLumControls.h`; it is an umbrella.
- Keep UI ownership local: triptych in `VoLumTriptych.h`, base controls in `VoLumCoreControls.h`, fractal art in `VoLumFractalArt.h`.
- For new files, update the UI file map or relevant rule.
- Preserve standalone/VST3 differences around `APP_API`.

## Verification

- Run `pwsh NeuralAmpModeler/scripts/run-app-win.ps1` for layout/visual changes.
- If params, keyboard steps, or state changed, also run `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`.
- Add/update `NeuralAmpModeler/installer/changelog.txt` for user-visible UI changes.
