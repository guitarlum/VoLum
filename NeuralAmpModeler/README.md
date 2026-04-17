# VoLum -- Developer Guide

This is the build and architecture reference for contributors. For download and install instructions, see the [root README](../README.md).

## What's different from NAM


|                         | NAM Plugin                                | VoLum                                                                                                           |
| ----------------------- | ----------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| **Purpose**             | General-purpose NAM model loader          | Curated amp collection player                                                                                   |
| **Model selection**     | File browser (find your own `.nam` files) | Sidebar amp gallery with 14 bundled amps                                                                        |
| **Speaker / cab**       | Separate IR loader                        | Built-in speaker modes (AMP / G12 / G65 / V30) per amp                                                          |
| **Channel switching**   | N/A                                       | Discrete gain-stage stepper per amp+speaker combo                                                               |
| **Per-amp settings**    | N/A                                       | All knobs, toggles, speaker & channel remembered per amp                                                        |
| **Session persistence** | DAW project only                          | Settings saved to user profile JSON, restored on next launch; VST3 also serializes per-amp bank in plugin state |
| **Model loading**       | Blocks UI                                 | Background thread + per-amp DSP cache                                                                           |
| **UI**                  | 600x400, file-browser focused             | 900x600 dark theme, amp gallery sidebar, procedural hero art, grouped knobs                                     |


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
| `scripts/run-app-win.ps1`                 | Build and launch the standalone (for UI iteration)                                                                                                                                |


**CI** (`.github/workflows/ci.yml`) runs on **pull requests to `main`**, on **pushes to `main`**, and manually (**Actions → CI → Run workflow**), and builds the same macOS set as releases (`full all`: installer DMG with `.pkg`, standalone DMG, VST3 zip; no dSYM zip) plus Windows tests/portable zip. Artifacts: **VoLum-mac** (`build-mac/out/`, including **`VoLum-v*-mac.dmg`** for **`VoLum Installer.pkg`**) and **VoLum-win** (`build-win/out/`). **Release Native** (`.github/workflows/release-native.yml`) is the one-button draft release flow: it defaults to the next minor version, accepts `minor`/`patch`/`major`/`manual` input, creates the tag and draft release, then uploads the user-facing assets only.

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


Each amp x 4 speaker modes x channels = ~224 `.nam` files total.

## Key source files


| File                     | Role                                                                                    |
| ------------------------ | --------------------------------------------------------------------------------------- |
| `config.h`               | `VOLUM_AMPETE_PRODUCT`, window size, version                                            |
| `installer/VoLum.iss`    | Windows installer (Inno): standalone + VST3 + `VoLumRigs`; sets `VoLumRigsRoot` in HKLM |
| `VoLumAmpeteCatalog.h`   | Amp metadata (folder names, display names, speaker prefixes)                            |
| `VoLumPaths.h`           | Rig directory discovery, channel file scanning, settings path                           |
| `VoLumControls.h`        | Custom iPlug2 UI controls (sidebar, knobs, speaker buttons, channel stepper)            |
| `NeuralAmpModeler.h/cpp` | Plugin class with VoLum state, layout, model loading, per-amp settings                  |
| `Unserialization.cpp`    | Version-aware state deserialization (per-amp settings bank)                             |


## Building the original NAM plugin

Set `VOLUM_AMPETE_PRODUCT 0` in `config.h` to build the stock NAM plugin UI (600x400, file browser).

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles created by Lum

