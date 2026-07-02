# R2: Remove legacy VOLUM_AMPETE_PRODUCT fence

## Problem

`VOLUM_AMPETE_PRODUCT` is always `1` in `NeuralAmpModeler/config.h`, but many source files still read like there are multiple product modes. That makes the code harder to reason about and can mislead agents into preserving a toggle that no longer exists.

## Scope

- Audit all `VOLUM_AMPETE_PRODUCT` references in:
  - `NeuralAmpModeler/config.h`
  - `NeuralAmpModeler/NeuralAmpModeler.cpp`
  - `NeuralAmpModeler/NeuralAmpModeler.h`
  - `NeuralAmpModeler/NeuralAmpModelerControls.h`
  - `NeuralAmpModeler/Unserialization.cpp`
  - `NeuralAmpModeler/VoLumUserSettingsIO.h`
  - `NeuralAmpModeler/VoLumAmpeteCatalog.h`
  - `NeuralAmpModeler/projects/NeuralAmpModeler-Tests.vcxproj`
  - `NeuralAmpModeler/tests/CMakeLists.txt`
- Collapse guards only after confirming the guarded code is truly VoLum-only and no upstream sync path still depends on the visual fence.
- Preserve upstream cherry-pick clarity with explicit `// VoLum:` comments or extraction into `VoLum*.inc.cpp` where useful.

## Acceptance Criteria

- No remaining `#if VOLUM_AMPETE_PRODUCT` or `#ifdef VOLUM_AMPETE_PRODUCT` branches.
- No behavior change in standalone or VST3 builds.
- Upstream-equivalent files still make VoLum-only additions easy to distinguish.
- `NeuralAmpModeler/installer/changelog.txt` records the cleanup if any user-visible behavior changes; otherwise no changelog needed.

## Verification

- `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- Targeted app or VST3 MSBuild for any touched build target.
- If UI attachment or layout guards change, run `pwsh NeuralAmpModeler/scripts/run-app-win.ps1` and inspect the standalone app.
