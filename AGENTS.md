# VoLum Agent Notes

Keep this file as a small routing index. Detailed guidance lives in scoped Cursor rules and skills so agents load only what each task needs.

## Scope

- Main product code: `NeuralAmpModeler/`.
- Vendored/submodule code: `iPlug2/`, `NeuralAmpModelerCore/`, `eigen/`. Avoid edits unless the task explicitly targets them.
- `AudioDSPTools/` is a submodule, but VoLum actively depends on DSP there.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename it to `VoLumRigs/`.

## Start Here

Rules below auto-attach on the files they own; you do not need to open them by hand.

- UI/layout work: `.cursor/rules/volum-ui.mdc`.
- Custom NAM import transactions: `NeuralAmpModeler/VoLumCustomNamImport.h`.
- Params, presets, state migration: `.cursor/rules/volum-state-params.mdc`.
- C++/DSP/audio-chain work: `.cursor/rules/neural-amp-modeler-native.mdc`.
- CI, installers, releases, artifacts: `.cursor/rules/volum-release-packaging.mdc` and skill `release-manager`.
- Build/CI failures: skill `native-build-debugger`.
- Submodule or vendored code questions: `.cursor/rules/volum-submodules.mdc`.
- Upstream sync with NAMCore or NeuralAmpModelerPlugin: skill `upstream-sync`.
- Writing or trimming rules/skills/`AGENTS.md`: `.cursor/rules/ai-artifact-authoring.mdc`.

## Agent skills

### Issue tracker

Planning lives in tracked `.scratch/<effort>/` (maps, specs, tickets). Foggy
multi-session work starts with `/wayfinder`. GitHub Issues stay user-facing.
See `docs/agents/issue-tracker.md`.

### Domain docs

Single-context. `CONTEXT.md` is a lazy glossary, not a spec. See
`docs/agents/domain.md`.

## Fast Commands

- Windows tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- macOS tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh`
- macOS sanitizer tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh --sanitize`
- Windows app smoke check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- Windows portable package: `cd NeuralAmpModeler\scripts; cmd /c makedist-win.bat full zip` (the script resolves its helpers relative to the working directory, so it must run from `scripts`)
- Windows installer package: `cd NeuralAmpModeler\scripts; cmd /c makedist-win.bat full installer`
- Windows standalone end-to-end scenarios: `pwsh NeuralAmpModeler/scripts/e2e-standalone-win.ps1`
- Windows standalone rate/buffer switching stress: `pwsh NeuralAmpModeler/scripts/stress-standalone-rate-switch-win.ps1` (needs a real ASIO device)
- macOS release-equivalent package: `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`
- Watch/dispatch CI: `pwsh NeuralAmpModeler/scripts/ci-watch.ps1 -Ref <branch> [-Dispatch] [-NoWait]`
- Format: `bash format.bash`