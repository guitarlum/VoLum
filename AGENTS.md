# VoLum Agent Notes

## Scope
- Main product code lives in `NeuralAmpModeler/`.
- Treat `iPlug2/`, `AudioDSPTools/`, `NeuralAmpModelerCore/`, and `eigen/` as vendored/submodule dependencies unless the task is explicitly about them.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename that folder to `VoLumRigs/`.

## Fast Verification
- Windows doctests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- Windows standalone UI check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- macOS fast local build: `bash NeuralAmpModeler/scripts/makedist-mac.sh dev`
- macOS CI/release-matching build: `bash NeuralAmpModeler/scripts/makedist-mac.sh full zip`
- Windows portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Windows installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- C++ formatting: `bash format.bash`
- First local native build may need the same setup CI uses: `iPlug2/Dependencies/IPlug/download-iplug-sdks.sh` and `iPlug2/Dependencies/download-prebuilt-libs.sh`.
- If a Windows rebuild fails in postbuild because the exe is in use, close `VoLum.exe` and rerun; `run-app-win.ps1` calls this out.

## Real Entry Points
- `NeuralAmpModeler/NeuralAmpModeler.cpp` and `.h` are the real app/plugin entrypoints.
- VoLum-specific UI controls live in `NeuralAmpModeler/VoLumControls.h`.
- Amp metadata lives in `NeuralAmpModeler/VoLumAmpeteCatalog.h`.
- Rig lookup and settings-path logic live in `NeuralAmpModeler/VoLumPaths.h`.
- State migration logic lives in `NeuralAmpModeler/Unserialization.cpp`.
- `NeuralAmpModeler/config.h` still controls product identity, version, window size, and feature forks; `VOLUM_AMPETE_PRODUCT` is enabled here.

## Build Variants
- `APP_API` means standalone. VST3-only behavior stays under `#ifndef APP_API`.
- The standalone and DAW paths intentionally differ; `NeuralAmpModeler/tests/test_process_io.cpp` covers APP-vs-plugin input/output behavior.
- On macOS, `makedist-mac.sh` defaults to the shipped targets only: `APP` and `VST3`. AU is opt-in via `MACOS_BUILD_ALL_TARGETS=1` and is not part of current CI.
- Xcode project path is `NeuralAmpModeler/projects/NeuralAmpModeler-macOS.xcodeproj`; the standalone scheme is `macOS-APP`.

## State And Persistence
- Do not reorder `EParams` or rename parameter `GetName()` strings without updating `Unserialization.cpp`; deserialization matches params by name and version-specific readers preserve old sessions.
- VoLum `0.1.x` state is intentionally read through the NAM `0.7.15` path in `Unserialization.cpp`. If the chunk format changes, add a new version branch instead of changing old readers.
- Per-amp settings write to the user profile, not the rigs folder:
  - Windows: `%LOCALAPPDATA%\VoLum\volum-settings.json`
  - macOS: `~/Library/Application Support/VoLum/volum-settings.json`
- The app still reads `<rigsRoot>/volum-settings.json` as a legacy fallback.

## Rigs And Packaging
- `VoLumPaths.h` looks for `VoLumRigs` before `rigs`.
- Windows installer writes `HKLM\Software\VoLum\NeuralAmpModeler\VoLumRigsRoot`; legacy installs may still expose `AmpeteRigsPath`.
- Portable packaging assumes `VoLum.vst3` and sibling `VoLumRigs/` stay side by side.
- macOS standalone embeds `VoLumRigs` inside `VoLum.app/Contents/Resources`; the macOS VST3 zip keeps rigs as a sibling folder instead.
- `NeuralAmpModeler/tests/test_nam_rigs.cpp` expects bundled repo rigs under `rigs/Ampete One/...`.

## Release Housekeeping
- For user-visible or behavior changes, append one dated line to `NeuralAmpModeler/installer/changelog.txt`; `release-native.yml` uses that file as the GitHub release body.
- If bumping version, use `python3 bump_version.py patch|minor|major`; this updates `config.h`, installer metadata, and plist versions.
- macOS plist/resource filenames still use the `NeuralAmpModeler-*` prefix even though the shipped product name is `VoLum`.
