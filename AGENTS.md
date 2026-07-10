# VoLum Agent Notes

Keep this file as a small routing index. Detailed guidance lives in scoped Cursor rules and skills so agents load only what each task needs.

## Scope

- Main product code: `NeuralAmpModeler/`.
- Vendored/submodule code: `iPlug2/`, `NeuralAmpModelerCore/`, `eigen/`. Avoid edits unless the task explicitly targets them.
- `AudioDSPTools/` is a submodule, but VoLum actively depends on DSP there.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename it to `VoLumRigs/`.

## Android Landscape Prototype File Map

- `ui/prototype/VoLumPrototype.kt`: root routing, safe back, landscape entry points, and stage font-scale policy.
- `ui/prototype/PrototypeModels.kt`: fixed signal-path model, catalogs, deterministic reducer, and controller.
- `ui/prototype/SignalPathWorkspace.kt`: persistent PRE/AMP/POST path, performance rail, summaries, and block editors.
- `ui/prototype/PrototypeJourney.kt`: first-launch setup, split catalog browser, loading/recovery, and tuner.
- `ui/prototype/PrototypeSettings.kt`: audio/performance/library settings, metronome, and staged content editors.
- `ui/prototype/PrototypeComponents.kt`, `PrototypeTheme.kt`: accessible controls and neutral structural tokens.

## Start Here

- UI/layout work: `.cursor/rules/volum-ui.mdc` and skill `volum-ui-change`.
- Params, presets, state migration: `.cursor/rules/volum-state-params.mdc` and skill `volum-param-state-change`.
- C++/DSP/audio-chain work: `.cursor/rules/neural-amp-modeler-native.mdc`.
- CI, installers, releases, artifacts: `.cursor/rules/volum-release-packaging.mdc` and skill `release-manager`.
- Build/CI failures: skill `native-build-debugger`.
- Submodule or vendored code questions: `.cursor/rules/volum-submodules.mdc`.
- Upstream sync with NAMCore or NeuralAmpModelerPlugin: skill `upstream-sync`.

## Fast Commands

- Windows tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- macOS tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh`
- macOS sanitizer tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh --sanitize`
- Windows app smoke check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- Windows portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Windows installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- macOS release-equivalent package: `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`
- Format: `bash format.bash`