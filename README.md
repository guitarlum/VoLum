# VoLum -- NAM Player

![VoLum standalone UI](docs/volum-ui.png)

A guitar amp collection player built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Ships 14 amp profiles with a custom UI for instant browsing and switching -- standalone app and VST3 plugin.

## Features

- **14 bundled amps** with 4 speaker modes and multiple gain stages each (~224 profiles total)
- **Dark-theme UI** with sidebar amp browser, speaker buttons, channel stepper, and grouped knobs
- **Per-amp settings** -- knobs, toggles, speaker mode, and channel are saved per amp and restored on next launch
- **Fast amp switching** -- models load on a background thread; switching back to a previously loaded amp is instant
- **Keyboard shortcuts** -- Up/Down: switch amp; Left/Right: switch channel
- **Standalone + VST3** -- same UI and features in both formats

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

Grab the latest build from [**Actions > Build Native**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml) -- pick the latest green run, scroll to **Artifacts**, and download **VoLum-win** (Windows) or **VoLum-mac** (macOS).

Stable releases with a Windows installer and macOS `.dmg` will appear under [**Releases**](https://github.com/guitarlum/VoLum/releases) once tagged.

## Install

### Windows (portable zip from CI)

1. Download and unzip the **VoLum-win** artifact. Inside you will find two zips -- open the **main** one (not the `-pdbs` symbols archive).
2. You should see this layout:

```
VoLum_x64.exe                    (standalone app)
VoLum.vst3/                      (VST3 plugin bundle)
  Contents/x86_64-win/VoLum.vst3
VoLumRigs/                       (amp profiles -- required!)
  Ampete One/
  Marshall JMP 2203 1976/
  ...
```

3. **Standalone** -- just run `VoLum_x64.exe`. It finds `VoLumRigs` next to itself.
4. **VST3 in a DAW** -- copy the `VoLum.vst3` **folder** and the `VoLumRigs` **folder** into your VST3 scan path so they sit side by side:

```
C:\Program Files\Common Files\VST3\
  VoLum.vst3/                    <-- the whole folder, not just the inner file
  VoLumRigs/                     <-- amp profiles, right next to it
```

### Windows (installer from a tagged release)

Run `VoLum-Setup.exe`. It places the standalone in `Program Files\VoLum`, the VST3 bundle in `Common Files\VST3`, and the amp profiles under the install directory. The VST3 finds models automatically via registry -- no manual copying needed.

### macOS (portable zip from CI)

1. Unzip the **VoLum-mac** artifact, open the main zip.
2. Keep `VoLum.app`, `VoLum.vst3`, and `VoLumRigs/` in the same folder.
3. **Standalone** -- double-click `VoLum.app`.
4. **VST3** -- copy `VoLum.vst3` and `VoLumRigs` side by side into `~/Library/Audio/Plug-Ins/VST3/`.

## Bundled amps

| Amp | Channels |
|-----|----------|
| Ampete One | 4 |
| Bad Cat mini Cat | 3 |
| Brunetti XL 2 | 3 |
| Fryette Deliverance 120 | 2 |
| H&K TriAmp Mk2 | 6 |
| Lichtlaerm Prometheus | 3 |
| Marshall 2204 1982 | 6 |
| Marshall JMP 2203 1976 | 6 |
| Marshall JVM 210H OD1 | 6 |
| Orange OD120 1975 | 5 |
| Orange ORS100 1972 | 2 |
| Sebago Texas Flood | 2 |
| Soldano SLO100 | 3 |
| THC Sunset | 5 |

Each amp has 4 speaker modes (AMP direct, G12, G65, V30) and a number of gain-stage channels.

## Settings

Your per-amp knob, toggle, speaker, and channel settings are stored automatically:

- **Windows:** `%LOCALAPPDATA%\VoLum\volum-settings.json`
- **macOS:** `~/Library/Application Support/VoLum/volum-settings.json`

Settings persist across sessions for both standalone and VST3.

## Build from source

See the [developer guide](NeuralAmpModeler/README.md).

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles by Lum
