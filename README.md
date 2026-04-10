# VoLum -- NAM Player

![VoLum standalone UI](docs/volum-ui.png)

A guitar amp collection player built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Ships 14 amp profiles with a custom UI for instant browsing and switching -- standalone app and VST3 plugin.

## Download

[![Build](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

VoLum is built in two ways on GitHub:

| Workflow | When it runs | What you get |
|----------|----------------|----------------|
| [**Build Native**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml) | Every push to `main`, PRs, or manual run | **Portable zips** (CI artifacts): binaries + bundled **`.nam`** profiles under **`VoLumRigs/`**, plus a separate **debug symbols** archive (Windows `.pdb` / macOS dSYM). **No installer** — this job does not run Inno Setup or build a macOS `.dmg`. |
| [**Release Native**](https://github.com/guitarlum/VoLum/actions/workflows/release-native.yml) | Git tag `v*` or manual dispatch | **Release assets**: Windows **`VoLum-Setup.exe`** (Inno Setup: app + VST3 bundle under Common Files + **`VoLumRigs`** under the install dir + registry so VST3 finds models), macOS **`.dmg`**, plus symbol zips. Draft appears under [**Releases**](https://github.com/guitarlum/VoLum/releases). |

### Build Native artifacts (quick try)

1. Open **Build Native** and pick the latest green run.
2. Under **Artifacts**, download **VoLum-win** or **VoLum-mac**. Each artifact folder contains **two** zips: the **main** package and a **symbols**-only zip (for crash debugging; not needed to run).
3. Unzip the **main** zip and keep this layout:
   - **Windows:** `VoLum_x64.exe`, the **`VoLum.vst3`** module file, and a **`VoLumRigs/`** folder (amp subfolders with `.nam` files) **in the same directory**. Do not split only the `.vst3` into `%CommonProgramW6432%\VST3\` without also giving the plugin a way to find **`VoLumRigs`** (see below).
   - **macOS:** `VoLum.app`, `VoLum.vst3`, and **`VoLumRigs/`** together (same folder).

The app discovers profiles by checking **`VoLumRigs`** first, then legacy **`rigs`**, relative to the plugin/standalone binary and current working directory (see `NeuralAmpModeler/VoLumPaths.h`). **Installer builds** also set **`HKLM\Software\VoLum\NeuralAmpModeler\VoLumRigsRoot`** so the VST3 in Program Files **Common Files** can load models even though they live under the VoLum install directory.

### Recommended: full install (VST3 in a DAW)

For everyday use with a host, prefer a **tagged release** and run **`VoLum-Setup.exe`** (Windows) or the **`.dmg`** (macOS). You do **not** need to place **`VoLumRigs`** next to the `.vst3` in Common Files; the installer wires that path through the registry (Windows) and bundles data under the application install location.

## Features

- **14 bundled amps** with 4 speaker modes and 2-6 gain stages each (~224 profiles)
- **Dark-theme UI** with sidebar amp browser, hero art, speaker buttons, channel stepper, and grouped knobs
- **Per-amp settings** -- knobs, toggles, speaker mode, and channel are saved per amp and restored on next launch
- **Fast amp switching** -- models load on a background thread; parsed DSP data is cached
- **Keyboard shortcuts** -- Up/Down: switch amp; Left/Right: switch channel
- **Standalone + VST3** -- same UI and features in both formats

## Build from source

Requires Windows 10+ (x64) with Visual Studio 2022 Build Tools, or macOS with Xcode. All dependencies are vendored.

```
NeuralAmpModeler/NeuralAmpModeler.sln        (Windows)
NeuralAmpModeler/projects/NeuralAmpModeler.xcodeproj  (macOS)
```

The repo keeps amp models under **`rigs/`** at the tree root for development; shipping layouts use **`VoLumRigs/`** as above. See [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md) for the full developer guide, amp inventory, and rig file structure.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles by Lum
