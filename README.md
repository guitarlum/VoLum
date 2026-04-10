# VoLum -- NAM Player

![VoLum standalone UI](docs/volum-ui.png)

A guitar amp collection player built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Ships 14 amp profiles with a custom UI for instant browsing and switching -- standalone app and VST3 plugin.

## Download

[![Build](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

**Windows** and **macOS** builds are produced automatically from the latest code (same approach as [upstream NAM CI](https://github.com/sdatkinson/NeuralAmpModelerPlugin/blob/main/.github/workflows/build-native.yml): portable **zip** artifacts, not installers).

1. Open the [**Build Native**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml) workflow and pick the latest green run.
2. Under **Artifacts**, download **VoLum-win** or **VoLum-mac**:
   - **VoLum-win** -- zip with the standalone `.exe`, VST3 binary, and changelog (extract and run / copy the VST3 where your host expects it).
   - **VoLum-mac** -- zip with the `.app` bundle and plug-in formats from the build.

**Installer builds** (Windows setup exe, macOS `.dmg` / `.pkg`) are produced when you cut a **tagged release** via the Release workflow (Windows runner installs Inno Setup there). Published downloads also show on [**Releases**](https://github.com/guitarlum/VoLum/releases).

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

See [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md) for the full developer guide, amp inventory, and rig file structure.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles by Lum
