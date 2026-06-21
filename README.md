**Languages:** English | [Deutsch](README.de.md)

# VoLum

<p align="center">
  <img src="docs/user-guide-main.png" alt="VoLum standalone UI" width="820">
</p>

VoLum is an open-source guitar amp collection for the stage, studio, and practice desk. It uses the [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore) core, but ships as its own focused app: 15 curated amps, built-in PRE pedals, Dual Amp, POST effects, your own amp/IR/pedal imports, per-amp presets, a tuner, a metronome, and a fast dark UI for standalone use or VST3 hosts.

[Download VoLum](https://github.com/guitarlum/VoLum/releases) or jump to the [user guide](docs/user-guide.en.md).

## Why It Stands Out

- **15 bundled amps, ready to play:** vintage, modern, and boutique captures with 4 speaker modes and multiple gain-stage channels each.
- **NAM Architecture 2 (A2) profiles:** every bundled amp, cab, and PRE NAM capture is an A2 model, trained to its best fit between 700 and 1200 epochs and always played at full size (never the lite slice).
- **Full rig workflow:** PRE compressor and NAM pedal slots, AMP controls, and POST Delay/Reverb live in one `PRE | AMP | POST` layout.
- **Dual Amp:** blend a main amp with a support amp, pan both lanes, and flip support polarity when a stack needs it.
- **Bring your own:** import your NAM amp captures (A1 or A2), impulse responses, and pedal captures into a managed library that keeps working after you move the originals.
- **Per-amp presets:** save named rig snapshots for each amp and cycle them in place with `‹` / `›`.
- **POST effects with character:** Digital, Analog, and Reverse Delay plus Hall, Plate, and Oktaverb Reverb with Halo, Shimmer, and Bloom voices.
- **Practice tools built in:** silent tuner and configurable metronome work in both standalone and VST3.
- **Fast switching and recall:** amp models load in the background, and each amp remembers its speaker, channel, knobs, PRE, POST, and Dual Amp setup.
- **Better keyboard control:** switch sections, move focus, edit knobs, toggle cards, open tools, and type exact values without reaching for the mouse.
- **Output safety:** a final safety stage catches runaway peaks and non-finite samples before they leave the plugin.

## Download

[![Build status](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)

Use **[Releases](https://github.com/guitarlum/VoLum/releases)** for stable packages. Use **[Actions -> CI](https://github.com/guitarlum/VoLum/actions/workflows/ci.yml)** only for preview builds from the latest development state.

| Platform | Recommended asset | When to choose it |
| --- | --- | --- |
| Windows | `VoLum-vX.Y.Z-windows-setup.exe` | Easiest install: standalone app, VST3, and bundled rigs. |
| Windows | `VoLum-vX.Y.Z-windows-portable.zip` | Portable or scripted setup. Keep `VoLum.vst3` and `VoLumRigs` together. |
| macOS | `VoLum-vX.Y.Z-macos-installer.dmg` | Easiest install when available. Contains `VoLum Installer.pkg`. |
| macOS | `VoLum-vX.Y.Z-macos-standalone.dmg` | Standalone app only. |
| macOS | `VoLum-vX.Y.Z-macos-vst3.zip` | Manual VST3 install. VoLum is VST3, so it does not appear in Logic Pro. |

Releases may not include every asset type. Open the release page and pick the package that matches your system.

## Important Security Notice

VoLum release signing is still being set up. See the [code signing policy](CODE_SIGNING.md).

- **Windows:** SmartScreen may warn that the app is from an unknown publisher. If you trust the build source, choose **More info -> Run anyway**.
- **macOS:** Gatekeeper may block unsigned or non-notarized builds. Use **right-click -> Open** on the app or installer, or **System Settings -> Privacy & Security -> Open Anyway**.
- **macOS VST3 zip:** if your DAW still hides the plugin after a rescan, remove quarantine:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/VoLum.vst3
```

Preview builds from CI are development artifacts and should be treated as unsigned test builds.

## Quick Install

### Windows Installer

Run `VoLum-vX.Y.Z-windows-setup.exe`. It installs:

- `VoLum.exe` to `C:\Program Files\VoLum`
- `VoLum.vst3` to `C:\Program Files\Common Files\VST3`
- `VoLumRigs` to the VoLum install folder

The VST3 finds the bundled rigs automatically.

### Windows Portable

Unzip `VoLum-vX.Y.Z-windows-portable.zip`. For standalone, run `VoLum_x64.exe`. For VST3, copy both folders into your VST3 scan path:

```text
C:\Program Files\Common Files\VST3\
  VoLum.vst3\
  VoLumRigs\
```

### macOS Installer

Open `VoLum-vX.Y.Z-macos-installer.dmg`, then run `VoLum Installer.pkg`. The installer can place the standalone app, VST3, and bundled rigs for you.

### macOS Standalone

Open `VoLum-vX.Y.Z-macos-standalone.dmg`, drag `VoLum.app` to **Applications**, then launch it. The app includes the bundled rigs.

### macOS VST3 Zip

Unzip `VoLum-vX.Y.Z-macos-vst3.zip`, then place both `VoLum.vst3` and `VoLumRigs` in your VST3 folder:

```text
~/Library/Audio/Plug-Ins/VST3/
  VoLum.vst3/
  VoLumRigs/
```

Rescan plugins in your DAW. Use a VST3-capable host such as REAPER, Ableton Live, Cubase, Studio One, or Bitwig.

### Linux

VoLum does not currently provide a native Linux build. Some users have reported that the Windows VST3 works well on Linux through [yabridge](https://github.com/robbert-vdh/yabridge), but that path is not officially tested by VoLum.

## Bundled Amps

| Amp | Channels |
| --- | --- |
| Ampete One | 4 |
| Bad Cat Mini Cat | 3 |
| Brunetti XL 2 | 3 |
| Diezel Herbert Mk1 | 4 |
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

Each amp ships with `AMP`, `G12`, `G65`, and `V30` speaker modes.

## Learn More

- [User guide](docs/user-guide.en.md): interface, Dual Amp, PRE pedals, POST effects, custom content, presets, tuner, metronome, keyboard controls, and settings.
- [Developer guide](NeuralAmpModeler/README.md): build, test, packaging, and architecture notes.
- [Report a bug or request a feature](https://github.com/guitarlum/VoLum/issues/new/choose): use the **Bug report** template for crashes or wrong behavior, and **Feature request** for ideas.
- Settings are stored locally at `%LOCALAPPDATA%\VoLum\volum-settings.json` on Windows and `~/Library/Application Support/VoLum/volum-settings.json` on macOS.

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin), the original plugin shell VoLum grew from
- [iPlug2](https://iplug2.github.io), the plugin framework
- Amp profiles by Lum
