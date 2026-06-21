# VoLum -- Developer Guide

This is the build and architecture reference for contributors. For download and install instructions, see the [root README](../README.md).

## Product shape

VoLum is an independent NAM-based amp collection app. It still keeps selected upstream-compatible files and paths so future NAM Plugin fixes can be cherry-picked, but the shipped product is the curated VoLum workflow.

| Area | VoLum 1.2 behavior |
| --- | --- |
| **Amp catalog** | 15 bundled amps with sidebar browsing, 4 speaker modes, and per-amp channel steppers. All captures are NAM Architecture 2 (A2) packed containers, played at full size (A2-Lite opt-in is backlog `F4`). |
| **Rig building** | PRE compressor + two NAM pedal slots, AMP controls, Dual Amp, and POST Delay/Reverb in the triptych UI. |
| **Custom content (BYO)** | Import your own NAM amp captures (A1 or A2), IRs, and pedal captures into a VoLum-owned content library (`volum-content.json`); referenced from DAW projects by stable opaque ids. |
| **Presets** | Per-amp named snapshots of the full rig (capture/recall/overwrite/rename/delete) with an exact-compare "(unsaved)" indicator. |
| **Persistence** | Per-amp speaker, channel, knobs, PRE, POST, Dual Amp, custom-IR/support ids, and the active preset stored in user profile JSON; VST3 also serializes state (chunk 1.2.0 with an append-only id tail). |
| **Practice tools** | Chromatic tuner and metronome in standalone and VST3. |
| **Keyboard workflow** | Section switching, focus movement, knob edit mode, exact entry, toggles, tuner/metronome/settings shortcuts. |
| **Runtime behavior** | Background model loading, per-amp DSP cache, NAM output NaN scrub, and final-bus output safety. |
| **UI** | 900x600 dark UI with amp gallery, procedural art, grouped knobs, pedal cards, and settings overlay. |


## Quick start

1. Open `NeuralAmpModeler.sln` in Visual Studio 2022 (Build Tools or Community), or `projects/NeuralAmpModeler-macOS.xcodeproj` in Xcode.
2. Select **NeuralAmpModeler-app** | **Release** | **x64** (Windows) or **APP** target (macOS).
3. Build and run. The standalone reads rigs from `rigs/` at the repo root.

## Build requirements

- **Windows:** Windows 10+ (x64), Visual Studio 2022 Build Tools (MSVC v143)
- **macOS:** macOS 11+, Xcode 15+
- All dependencies (iPlug2, Eigen, NAMCore) are vendored in the repo

## CI and packaging


| Path                                      | What it does                                                                                                                                                                      |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `scripts/makedist-win.bat full zip`       | Build + portable zip (exe + VST3 bundle + VoLumRigs)                                                                                                                              |
| `scripts/makedist-win.bat full installer` | Build + Inno Setup installer                                                                                                                                                      |
| `scripts/makedist-mac.sh dev`             | Fast local macOS standalone DMG for UI iteration (host arch only, no installer, no VST3 zip, no dSYM zip)                                                                         |
| `scripts/makedist-mac.sh full zip`        | Standalone DMG + VST3 zip only (no `.pkg` installer). Also copies `VoLumRigs` next to the installed `VoLum.vst3` under `~/Library/Audio/Plug-Ins/VST3/` (same layout as the zip). |
| `scripts/makedist-mac.sh full installer`  | Installer DMG (contains `VoLum Installer.pkg`) only — no standalone DMG or VST3 zip.                                                                                              |
| `scripts/makedist-mac.sh full all`        | One Xcode build: installer DMG **and** standalone DMG **and** VST3 zip (used by **Release Native**).                                                                              |
| `scripts/package-portable.ps1`            | Local portable zip from an existing Windows build                                                                                                                                 |
| `scripts/run-tests-win.ps1`               | Build and run the doctest suite                                                                                                                                                   |
| `scripts/run-tests-mac.sh`                | Build and run the same doctest suite with CMake/clang on macOS                                                                                                                    |
| `scripts/run-tests-mac.sh --sanitize`     | Run the macOS doctest suite with ASan/UBSan                                                                                                                                        |
| `scripts/run-app-win.ps1`                 | Build and launch the standalone (for UI iteration)                                                                                                                                |
| `scripts/validate-vst3-win.ps1`           | Validate the built Windows VST3 with pluginval and the Steinberg validator when available                                                                                         |
| `scripts/validate-vst3-mac.sh`            | Validate the built macOS VST3 with pluginval and the Steinberg validator when available                                                                                           |


**CI** (`.github/workflows/ci.yml`) runs on pull requests and pushes to `dev` or `main`, and can also run manually. It covers formatting, NAMCore regression tests, Windows doctests, Windows installer/portable verification, macOS doctests, macOS sanitizer doctests, release-equivalent macOS packaging, and VST3 validation with pluginval plus the Steinberg validator when available. Artifacts are **VoLum-mac** and **VoLum-win**. **Release Native** (`.github/workflows/release-native.yml`) creates the tagged draft release and uploads the user-facing assets.

## Test suite map

Both Windows and macOS run the doctest suite. Windows uses the Visual Studio project `projects/NeuralAmpModeler-Tests.vcxproj`; macOS uses `tests/CMakeLists.txt`. When adding a new `test_*.cpp`, register it in both places.

| Change area | Tests to update |
| ----------- | --------------- |
| DSP/process helpers | `tests/test_process_io.cpp`, `tests/test_delay_reverb_dsp.cpp`, `tests/test_volum_pre_effects.cpp`, `tests/test_tone_stack.cpp` |
| Main signal-chain decisions | `VoLumProcessingPlan.h`, `tests/test_volum_processing_plan.cpp` |
| Params, keyboard steps, serialized state | `tests/test_eparam_order.cpp`, `tests/test_keyboard_steps.cpp`, `tests/test_volum_chunk_version.cpp`, `tests/test_volum_chunk_codec.cpp` |
| User settings JSON | `tests/test_volum_user_settings_io.cpp` |
| Main amp `.nam` files under `rigs/` | `tests/test_nam_rigs.cpp` |
| PRE captures under `rigs/PrePedals/` | `VoLumPrePedalCaptures.h`, `tests/test_volum_pre_pedal_captures.cpp`, `tests/test_nam_rigs.cpp`, packaging verify scripts |
| UI/layout behavior | `tests/test_volum_ui_regressions.cpp` and pure layout helpers before source-string locks |
| Packaging/installers/VST3 validity | `scripts/verify-packaging-win.ps1`, `scripts/verify-packaging-mac.sh`, `scripts/verify-installer-win.ps1`, `scripts/validate-vst3-win.ps1`, `scripts/validate-vst3-mac.sh` |

Current release asset names:

- `VoLum-vX.Y.Z-macos-standalone.dmg`
- `VoLum-vX.Y.Z-macos-vst3.zip`
- `VoLum-vX.Y.Z-macos-installer.dmg` (contains `VoLum Installer.pkg`)
- `VoLum-vX.Y.Z-windows-setup.exe`
- `VoLum-vX.Y.Z-windows-portable.zip`

## Rig file structure

In the repo, profiles live under `rigs/` at the tree root. Shipped builds use `VoLumRigs/` (same amp subfolders).

```
rigs/                               (repo / dev)
VoLumRigs/                          (portable zip / Windows installer)
  {AmpFolder}/
    {Speaker}-{AmpCode}-{Channel}.nam
```

- **Speaker prefix:** `AMP` (direct), `G12`, `G65`, `V30`
- **AmpCode:** short identifier (e.g. `Ampt`, `2203`, `BadC`)
- **Channel suffix:** gain stage (`1`-`6`) or special (`f` = FatBee, `x` = FatBee+Clone)

Example: `rigs/Marshall JMP 2203 1976/V30-2203-f.nam`

## Rig discovery

`VoLumPaths.h` > `FindRigsRootDirectory()` checks in order:

1. **Windows registry** `HKLM\Software\VoLum\NeuralAmpModeler\VoLumRigsRoot` (set by the Inno installer so VST3 under Common Files can reach models in the VoLum install directory)
2. Walk up from the **plugin module** (VST3 DLL or standalone exe -- uses `GetModuleHandleEx` on Windows so it resolves to the plugin, not the host process). Checks for `VoLumRigs/` then `rigs/` at each level.
3. **macOS .app bundle** `Contents/Resources/VoLumRigs` (then `rigs`) for the standalone app
4. **macOS Application Support** lookup in `~/Library/Application Support/VoLum/VoLumRigs` and `/Library/Application Support/VoLum/VoLumRigs` (then legacy `rigs`) for installed rigs outside the app bundle
5. Walk up from the module / extracted archive and check sibling `VoLumRigs/` then `rigs/` (used by portable VST3 packaging)
6. **CWD** `./VoLumRigs` then `./rigs` (dev fallback)

Settings are stored under the user profile (`%LOCALAPPDATA%\VoLum\` on Windows, `~/Library/Application Support/VoLum/` on macOS) so they work regardless of install location.

## Bundled amps


| Amp                     | Channels | Notes                                    |
| ----------------------- | -------- | ---------------------------------------- |
| Ampete One              | 4        |                                          |
| Bad Cat mini Cat        | 3        |                                          |
| Brunetti XL 2           | 3        |                                          |
| Diezel Herbert Mk1      | 4        | Mid-cut captures                         |
| Fryette Deliverance 120 | 2        | Channels 3-4 only                        |
| H&K TriAmp Mk2          | 6        |                                          |
| Lichtlaerm Prometheus   | 3        |                                          |
| Marshall 2204 1982      | 6        |                                          |
| Marshall JMP 2203 1976  | 6        | Includes FatBee (f) and FatBee+Clone (x) |
| Marshall JVM 210H OD1   | 6        |                                          |
| Orange OD120 1975       | 5        | Includes FatBee (f)                      |
| Orange ORS100 1972      | 2        |                                          |
| Sebago Texas Flood      | 2        |                                          |
| Soldano SLO100          | 3        |                                          |
| THC Sunset              | 5        |                                          |


Each amp x 4 speaker modes x channels = ~240 `.nam` files total.

## Key source files


| File | Role |
| --- | --- |
| `config.h` | Window size, version, plugin identity |
| `installer/VoLum.iss` | Windows installer: standalone + VST3 + `VoLumRigs`; sets `VoLumRigsRoot` in HKLM |
| `VoLumAmpeteCatalog.h` | Amp metadata, default amp settings, POST snapshots |
| `VoLumPrePedalCaptures.h` | PRE NAM capture metadata, grouping, discovery |
| `VoLumPaths.h` | Rig discovery, channel file scanning, settings path |
| `VoLumCoreControls.h` | Sidebar, speaker row, knob row, meters, settings overlay frame |
| `VoLumTriptych.h` | PRE/AMP/POST strip, cards, chain connector umbrella |
| `VoLumKeyboardModel.h` | Keyboard focus rings and parameter step sizes |
| `VoLumTunerDSP.h` | Chromatic tuner pitch detection and smoothing |
| `VoLumMetronomeDSP.h` | Sample-accurate metronome click generator and accent patterns |
| `NeuralAmpModeler.h/cpp` | Plugin class with VoLum state, layout, model loading, processing |
| `VoLumLoader.inc.cpp` | Async model-loader thread and result draining |
| `VoLumSettings.inc.cpp` | Per-amp settings persistence |
| `Unserialization.cpp` | Version-aware state deserialization |


## Upstream sync

VoLum-only code in upstream-equivalent files (`NeuralAmpModeler.cpp`, `.h`, `NeuralAmpModelerControls.h`, `Unserialization.cpp`, `config.h`) is marked with `// VoLum:` comments or lives in `VoLum*.h` / `VoLum*.inc.cpp` tail-includes. See `.cursor/skills/upstream-sync/SKILL.md`.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- original plugin shell and upstream reference
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles created by Lum

