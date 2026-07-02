---
name: volum-ui-change
description: Implement VoLum UI/layout changes efficiently. Use when modifying VoLum controls, PRE/AMP/POST triptych behavior, tuner/metronome overlays, sidebar/hero art, knob rows, or visual styling.
---

# VoLum UI Change

Follow `.cursor/rules/volum-ui.mdc` for the file map, layout ownership, `APP_API`
standalone/VST3 rules, and verification. It auto-attaches when editing
`NeuralAmpModeler/VoLum*.h`, so read the smallest owner file it names first.

- Never dump new controls into the `VoLumControls.h` umbrella; keep ownership local.
- Verify with the Windows app smoke check (`AGENTS.md` "Fast Commands"); also run the
  Windows tests if params, keyboard steps, or state changed.
- Add an `NeuralAmpModeler/installer/changelog.txt` line for user-visible UI changes.
