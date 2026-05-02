# VoLum Agent Notes

Keep this file as a small routing index. Detailed guidance lives in scoped Cursor rules and skills so agents load only what the task needs.

## Scope

- Main product code: `NeuralAmpModeler/`.
- Vendored/submodule code: `iPlug2/`, `NeuralAmpModelerCore/`, `eigen/`. Avoid edits unless task explicitly targets them.
- `AudioDSPTools/` is a submodule, but VoLum actively depends on DSP there.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename it to `VoLumRigs/`.

## Start Here

- UI/layout work: `.cursor/rules/volum-ui.mdc` and skill `volum-ui-change`.
- Params, presets, state migration: `.cursor/rules/volum-state-params.mdc` and skill `volum-param-state-change`.
- CI, installers, releases, artifacts: `.cursor/rules/volum-release-packaging.mdc` and skill `release-manager`.
- Build/CI failures: skill `native-build-debugger`.
- Submodule or vendored code questions: `.cursor/rules/volum-submodules.mdc`.

## Fast Commands

- Windows tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- macOS tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh`
- macOS sanitizer tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh --sanitize`
- Windows app smoke check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- Windows portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Windows installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- macOS release-equivalent package: `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`
- Format: `bash format.bash`

## Test Map

- DSP helpers/effects: doctests in `NeuralAmpModeler/tests/` (`test_process_io.cpp`, `test_delay_reverb_dsp.cpp`, `test_volum_pre_effects.cpp`, `test_tone_stack.cpp`, tuner/metronome tests).
- Main signal-chain decisions: `VoLumProcessingPlan.h` plus `test_volum_processing_plan.cpp`.
- Params/keyboard/state: `test_eparam_order.cpp`, `test_keyboard_steps.cpp`, `test_volum_chunk_version.cpp`, `test_volum_chunk_codec.cpp`.
- User settings JSON: `test_volum_user_settings_io.cpp`.
- Main amp rigs: `test_nam_rigs.cpp`; new `.nam` files under `rigs/` must load there.
- PRE NAM captures: `VoLumPrePedalCaptures.h`, `test_volum_pre_pedal_captures.cpp`, and the PRE section of `test_nam_rigs.cpp`; new files under `rigs/PrePedals/` must discover, load, and package.
- UI/layout: `test_volum_ui_regressions.cpp`, especially pure layout checks before source-string locks.
- Packaging/installers/plugin validation: `verify-packaging-win.ps1`, `verify-packaging-mac.sh`, `verify-installer-win.ps1`, `validate-vst3-win.ps1`, `validate-vst3-mac.sh`, and `.github/workflows/ci.yml`.

## Non-Negotiables

- Write/update focused tests for confirmed feature or bugfix work.
- Append one dated line to `NeuralAmpModeler/installer/changelog.txt` for user-visible behavior.
- Do not reorder `EParams` or rename stable parameter names without state migration and tests.
- Keep unrelated local dirt, especially expected `iPlug2` ASIO patch dirt, out of commits.