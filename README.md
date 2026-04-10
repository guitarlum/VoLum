# VoLum -- NAM Player

![VoLum standalone UI](docs/volum-ui.png)

A guitar amp collection player built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Ships 14 amp profiles with a custom UI for instant browsing and switching -- standalone app and VST3 plugin.

## Download

[![Build](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

**Windows** and **macOS** installers are built automatically from the latest code.

1. Go to the [**latest build**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml) page.
2. Click the top run with a green checkmark.
3. Scroll down to **Artifacts** and download:
   - **VoLum-win** -- contains `VoLum-Setup.exe`
   - **VoLum-mac** -- contains the `.dmg` installer

Stable releases will appear on the [**Releases**](https://github.com/guitarlum/VoLum/releases) page.

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
