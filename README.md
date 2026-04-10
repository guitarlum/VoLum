# VoLum -- NAM Player

![VoLum standalone UI — amp sidebar, speaker modes, hero art, and controls](docs/volum-ui.png)

A fork of [Neural Amp Modeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Lum, rebuilt as a personal amp collection player. Ships 14 bundled guitar amp profiles with a custom UI for instant browsing and switching -- available as both standalone app and VST3 plugin.

See [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md) for the full feature list, amp inventory, rig file structure, and developer guide.

## Downloads (CI and releases)

[![Build Native](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

- **Latest installers from `main`:** open the [**Build Native**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml) workflow, click the most recent **green** run, scroll to **Artifacts**, and download **VoLum-win** and **VoLum-mac**. Inside **VoLum-win** you will find `VoLum-Setup.exe` (and a zip of the installer plus changelog). Inside **VoLum-mac** you will find the `.dmg` (and dSYM zip).
- **Draft release builds (tagged):** push a version tag matching `v*` (for example `v0.1.0`), or run [**Release Native**](https://github.com/guitarlum/VoLum/actions/workflows/release-native.yml) manually and set the **tag** input. Then open [**Releases**](https://github.com/guitarlum/VoLum/releases) and open the new **Draft** to download the `.dmg` and Windows zip assets.

There is no permanent “latest installer” URL until you publish a release; use the workflow links above or attach binaries to a GitHub Release when you want a stable download page.

## Quick overview

- **14 bundled amps** with 4 speaker modes (AMP/G12/G65/V30) and 2-6 gain stages each (~224 NAM profiles)
- **Custom dark-theme UI** (900x600) with sidebar amp browser, hero image, speaker mode buttons, channel stepper, grouped knobs
- **Per-amp persistent settings** -- all knobs, toggles, speaker mode, and channel are saved per amp to a **user-writable** JSON file and restored when you return to that amp or on the next launch (VST3 state chunk includes the same data). On Windows this is `%LOCALAPPDATA%\VoLum\volum-settings.json`; on macOS, `~/Library/Application Support/VoLum/volum-settings.json`. If an older `rigs/volum-settings.json` exists next to the resolved rigs folder, it is still read once for migration.
- **Super-fast amp changes** -- click another amp (or use **Up/Down** arrow keys): the UI stays responsive because NAM models load on a **background thread**, and parsed DSP data is **cached per amp folder** so repeat visits avoid re-parsing JSON
- **Keyboard** -- **Up/Down**: previous/next amp; **Left/Right**: previous/next channel on the channel stepper (standalone focused; host may capture keys in VST3)
- **Standalone + VST3** -- same codebase, same UI, same features in both formats

## Upstream

This is a fork of [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin). All VoLum-specific code is gated behind `#if VOLUM_AMPETE_PRODUCT` so the original NAM plugin can still be built by setting `VOLUM_AMPETE_PRODUCT 0` in `config.h`.

## Build

Requires Windows 10+ (x64) and Visual Studio 2022 Build Tools (MSVC v143). All dependencies are vendored.

```
NeuralAmpModeler/NeuralAmpModeler.sln
  -> NeuralAmpModeler-app   (standalone, Release x64)
  -> NeuralAmpModeler-vst3  (VST3 plugin, Release x64)
```

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles by Lum
