# VoLum -- NAM Player

![VoLum standalone UI](docs/volum-ui.png)

A guitar amp collection player built on [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerPlugin). Ships 14 amp profiles with a custom UI for instant browsing and switching -- standalone app and VST3 plugin.

## Features

<p align="center">
  <img src="NeuralAmpModeler/resources/Images.xcassets/AppIcon.appiconset/iOSAppIcon.png" alt="VoLum app icon" width="160">
</p>

- **14 bundled amps** with 4 speaker modes and multiple gain stages each (~224 profiles total)
- **Dark-theme UI** with sidebar amp browser, speaker buttons, channel stepper, and grouped knobs
- **Per-amp settings** -- knobs, toggles, speaker mode, and channel are saved per amp and restored on next launch
- **Fast amp switching** -- models load on a background thread; switching back to a previously loaded amp is instant
- **Keyboard shortcuts** -- Up/Down: switch amp; Left/Right: switch channel; click a knob for keyboard fine-tuning
- **Standalone + VST3** -- same UI and features in both formats

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml)

Most users should download VoLum from [**Releases**](https://github.com/guitarlum/VoLum/releases):

- `VoLum-vX.Y.Z-windows-setup.exe` for the recommended Windows install
- `VoLum-vX.Y.Z-windows-portable.zip` for portable Windows use
- `VoLum-vX.Y.Z-macos-standalone.dmg` for the recommended macOS standalone app install
- `VoLum-vX.Y.Z-macos-vst3.zip` for macOS VST3 plugin installation

If you want the newest preview build before a release, open [**Actions > Build Native**](https://github.com/guitarlum/VoLum/actions/workflows/build-native.yml), pick the latest green run, and download **VoLum-win** or **VoLum-mac** from **Artifacts**.

### Which download should I choose?

- **Windows installer** if you want the easiest setup for standalone plus VST3
- **Windows portable** if you want a zip you can unpack anywhere
- **macOS standalone** if you just want to play through the app
- **macOS VST3** if you want to use VoLum inside a DAW

## Install

### Windows (recommended)

Run `VoLum-vX.Y.Z-windows-setup.exe`.

It installs:

- `VoLum.exe` in `C:\Program Files\VoLum`
- `VoLum.vst3` in `C:\Program Files\Common Files\VST3`
- `VoLumRigs` in the VoLum install folder

The VST3 plugin finds the bundled rigs automatically through the installer registry entry, so no manual copying is needed.

### Windows (portable)

Use `VoLum-vX.Y.Z-windows-portable.zip` from a release, or the Windows artifact from **Build Native**.

1. Unzip the portable package.
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

### macOS standalone app (recommended)

1. Download `VoLum-vX.Y.Z-macos-standalone.dmg`.
2. Open it and drag `VoLum.app` into `Applications`.
3. Launch `VoLum.app`.

The standalone already contains the bundled rigs inside the app.

Because the macOS builds are currently unsigned, macOS may ask you to right-click `VoLum.app` and choose `Open` the first time.

### macOS VST3 plugin

1. Download `VoLum-vX.Y.Z-macos-vst3.zip`.
2. Unzip it.
3. Copy `VoLum.vst3` and `VoLumRigs` side by side into `~/Library/Audio/Plug-Ins/VST3/`:

```
~/Library/Audio/Plug-Ins/VST3/
  VoLum.vst3/
  VoLumRigs/
```

### macOS preview builds

If you are installing from the **Build Native** artifact instead of a tagged release:

1. Download **VoLum-mac** from the latest green run.
2. Use the `*-standalone.dmg` or `*-app.dmg` file for the standalone app.
3. Use the `*-vst3.zip` file for the plugin.
4. Install them the same way as the macOS release downloads above.

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

## Keyboard controls

- With no knob selected: `Up/Down` switches amp, `Left/Right` switches channel
- Click a knob to select it for keyboard control
- Selected knob: `Up/Down` adjusts, `Left/Right` selects the next knob
- Hold `Shift` for finer `0.1` adjustments
- Press `Enter` for exact numeric entry
- Press `Delete` or `Backspace` to reset the selected knob to its default value
- Press `Esc` to leave knob keyboard mode and return arrows to amp/channel navigation

## Build from source

See the [developer guide](NeuralAmpModeler/README.md).

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- upstream fork base
- [iPlug2](https://iplug2.github.io) -- plugin framework
- Amp profiles by Lum
