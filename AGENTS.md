# VoLum Agent Notes

## Scope

- Main product code lives in `NeuralAmpModeler/`.
- Treat `iPlug2/`, `AudioDSPTools/`, `NeuralAmpModelerCore/`, and `eigen/` as vendored/submodule dependencies unless the task is explicitly about them. Note: We actively add new effect DSP (like `Reverb.h`/`Delay.h`) to `AudioDSPTools/dsp/`.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename that folder to `VoLumRigs/`.

## Fast Verification

- Windows doctests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- Windows standalone UI check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- macOS fast local build: `bash NeuralAmpModeler/scripts/makedist-mac.sh dev`
- macOS CI (PR to main / push to main): `bash NeuralAmpModeler/scripts/makedist-mac.sh full all` (same artifacts as release; `PACKAGE_DSYMS=0` in CI).
- Windows portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Windows installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- C++ formatting: `bash format.bash`
- First local native build may need the same setup CI uses: `iPlug2/Dependencies/IPlug/download-iplug-sdks.sh` and `iPlug2/Dependencies/download-prebuilt-libs.sh`.
- If a Windows rebuild fails in postbuild because the exe is in use, close `VoLum.exe` and rerun; `run-app-win.ps1` calls this out.

## Real Entry Points

- `NeuralAmpModeler/NeuralAmpModeler.cpp` and `.h` — plugin entrypoint, ProcessBlock, layout, keyboard handling.
- `NeuralAmpModeler/config.h` — product identity, version, window size, `VOLUM_AMPETE_PRODUCT` flag.

## UI File Map (read the right file first — saves tokens)

| File | What's inside | ~Lines |
|------|--------------|--------|
| `VoLumControls.h` | **Thin umbrella** — just includes the three below. Never edit directly. | 12 |
| `VoLumColorHelpers.h` | `VoLumColors` namespace, `DrawCornerAccent`, `DrawDiamond`, `ClearVoLumKnobSelection`. Includes `VoLumFractalArt.h`. | 82 |
| `VoLumFractalArt.h` | **All procedural fractal art** (rarely changes): `DrawStripMiniFractal` (14-variant auto-scaling), `DrawSidebarMiniFractal` (sidebar thumbnails), `DrawHeroFractalArt` (full-size hero). ~700 lines of pure drawing math — read only when changing fractals. | 705 |
| `VoLumCoreControls.h` | All "base" controls: sidebar list, speaker row, hero image (delegates to `DrawHeroFractalArt`), mode picker, knob labels, channel stepper, exact entry, settings backdrop, etc. | 1168 |
| `VoLumTriptych.h` | POST effects UI: `VoLumTriptychControl` (PRE/AMP/POST layout + mouse expand), `VoLumPedalCardControl` (delay/reverb cards with cached Lichtenberg/waveform art), `VoLumChainConnectorControl` | 390 |
| `VoLumTriptychState.h` | `EVoLumSection` and `EVoLumEffectFocus` enums (tiny, breaks circular deps) | 5 |
| `VoLumAmpeteCatalog.h` | Amp metadata (names, folders, speaker prefixes) | — |
| `VoLumPaths.h` | Rig root discovery, channel file enumeration | — |
| `VoLumUserSettingsIO.h` | Per-amp JSON settings read/write | — |
| `Unserialization.cpp` | State migration for DAW preset recall across versions | — |
| `NeuralAmpModelerControls.h` | `NAMKnobControl` (keyboard step sizes live here in `GetKeyboardStep`), `NAMSwitchControl`, `NAMSettingsPageControl`, `NAMFileBrowserControl` | — |

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

## Testing And Code Quality

- **Tests are mandatory.** Every confirmed-working feature or bugfix gets a test before the iteration is done. Don't wait for the user to ask.
- Test files live in `NeuralAmpModeler/tests/`. Framework: doctest (single-header, `test_main.cpp` has the runner).
- Test project: `NeuralAmpModeler/projects/NeuralAmpModeler-Tests.vcxproj`. Add new `.cpp` files there.
- DSP tests can run without IPlug2/IGraphics — include headers from `AudioDSPTools/dsp/` directly.
- Param-order and keyboard-step tests use a local `EParams` enum copy (avoids IGraphics dependency). If `EParams` changes, both the header and the test must update — that's the point.
- **Refactor proactively.** Files over ~500 lines or classes with unrelated responsibilities get split without being asked. Follow the file map below.

## Release Housekeeping

- For user-visible or behavior changes, append one dated line to `NeuralAmpModeler/installer/changelog.txt`; `release-native.yml` uses that file as the GitHub release body.
- If bumping version, use `python3 bump_version.py patch|minor|major`; this updates `config.h`, installer metadata, and plist versions.
- macOS plist/resource filenames still use the `NeuralAmpModeler-*` prefix even though the shipped product name is `VoLum`.

## UI Architecture (PRE / AMP / POST)

- VoLum's interface uses a triptych layout managed by `VoLumTriptychControl`. Only one section is expanded at a time: PRE (Boosts/Drives), AMP (Rig), or POST (Delay/Reverb pedalboard).
- The bottom knob row uses "replace mode" logic in `_AttachVoLumGraphics`: clicking a section tab or a pedal card (via `VoLumPedalCardControl`) hides the previously focused controls and shows the newly focused effect's controls.
- Effect identity artwork: Lichtenberg (Reverb) and echo waveform (Delay) in `VoLumTriptych.h` `_DrawFractalArt`. Amp sidebar/hero fractals in `DrawStripMiniFractal` (14 variants, auto-scales for 22px thumbnails vs 200px cards) in `VoLumColorHelpers.h`. Hero full-size fractals in `VoLumHeroImageControl::DrawGeometricArt` in `VoLumCoreControls.h`.
- PRE strip background: diagonal crosshatch. AMP collapsed strip: circuit-board grid. Both in `VoLumTriptych.h` `_DrawStrip` / `_DrawAmpStrip`.