# VoLum -- NAM Player

A fork of [Neural Amp Modeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Lum, rebuilt as a personal amp collection player. VoLum ships 14 of Lum's guitar amp profiles with a custom UI designed for instant browsing and switching -- no file hunting, no setup, just pick an amp and play.

## What's different from NAM

| | NAM Plugin | VoLum |
|---|---|---|
| **Purpose** | General-purpose NAM model loader | Curated amp collection player |
| **Model selection** | File browser (find your own `.nam` files) | Sidebar amp gallery with 14 bundled amps |
| **Speaker / cab** | Separate IR loader | Built-in speaker modes (AMP / G12 / G65 / V30) per amp |
| **Channel switching** | N/A | Discrete gain-stage stepper per amp+speaker combo |
| **Per-amp settings** | N/A | All knobs, toggles, speaker & channel remembered per amp |
| **Session persistence** | DAW project only | Settings saved to JSON, restored on next launch |
| **Model loading** | Blocks UI | Background thread + per-amp dspData cache |
| **UI** | 600x400, file-browser focused | 900x600 dark theme, amp gallery sidebar, hero image, grouped knobs |

## Features

- **14 bundled amps** -- Ampete One, Bad Cat, Brunetti, Fryette, H&K TriAmp, Lichtlaerm, Marshall 2204/2203/JVM, Orange OD120/ORS100, Sebago, Soldano SLO100, THC Sunset
- **4 speaker modes per amp** -- AMP (direct), G12, G65, V30 (cabinet simulations baked into the NAM profiles)
- **2-6 gain stages per amp** -- channel stepper dynamically discovers available channels from the rig files
- **Per-amp snapshots** -- switch amps and your EQ, gain, gate, speaker, channel settings are saved and restored automatically
- **Fast switching** -- parsed model data is cached per amp folder; switching channels/speakers skips JSON re-parsing
- **Non-blocking UI** -- model loading runs in a background thread so the interface stays responsive
- **Cross-session persistence** -- settings saved to `rigs/volum-settings.json`, last-used amp restored on startup
- **NDSP-style amp images** -- hero image area displays amp illustrations (Ampete, Brunetti, Marshall 2203 so far)
- **Original NAM preserved** -- set `VOLUM_AMPETE_PRODUCT 0` in `config.h` to build the stock NAM plugin

## Bundled amps

| Amp | Channels | Notes |
|-----|----------|-------|
| Ampete One | 4 | |
| Bad Cat mini Cat | 3 | |
| Brunetti XL 2 | 3 | |
| Fryette Deliverance 120 | 2 | Channels 3-4 only |
| H&K TriAmp Mk2 | 6 | |
| Lichtlaerm Prometheus | 3 | |
| Marshall 2204 1982 | 6 | |
| Marshall JMP 2203 1976 | 6 | Includes FatBee (f) and FatBee+Clone (x) |
| Marshall JVM 210H OD1 | 6 | |
| Orange OD120 1975 | 5 | Includes FatBee (f) |
| Orange ORS100 1972 | 2 | |
| Sebago Texas Flood | 2 | |
| Soldano SLO100 | 3 | |
| THC Sunset | 5 | |

Each amp x 4 speaker modes x channels = ~224 `.nam` files total.

## Rig file structure

```
rigs/
  {AmpFolder}/
    {Speaker}-{AmpCode}-{Channel}.nam
```

- **Speaker prefix:** `AMP` (direct), `G12`, `G65`, `V30`
- **AmpCode:** short identifier (e.g. `Ampt`, `2203`, `BadC`)
- **Channel suffix:** gain stage (`1`-`6`) or special (`f`, `x`)

Example: `rigs/Marshall JMP 2203 1976/V30-2203-f.nam`

## Quick start (dev)

1. Open `NeuralAmpModeler/NeuralAmpModeler.sln` in Visual Studio 2022 (Build Tools or Community).
2. Select **NeuralAmpModeler-app** | **Release** | **x64**.
3. Build and run. The standalone app reads rigs from `rigs/` relative to the repo root.

## Build requirements

- Windows 10+ (x64)
- Visual Studio 2022 Build Tools (MSVC v143)
- All dependencies (iPlug2, Eigen, NAMCore) are vendored in the repo

## Key source files

| File | Role |
|------|------|
| `config.h` | `VOLUM_AMPETE_PRODUCT`, window size, version |
| `VoLumAmpeteCatalog.h` | Amp metadata (folder names, display names, speaker prefixes) |
| `VoLumPaths.h` | Rig directory discovery + channel file scanning |
| `VoLumControls.h` | Custom iPlug2 UI controls for the VoLum layout |
| `NeuralAmpModeler.h/cpp` | Plugin class with VoLum state, layout, loading, per-amp settings |
| `Unserialization.cpp` | Version-aware state deserialization (v0.7.15 adds per-amp settings) |

## Credits

- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) by Steven Atkinson
- [NAM Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) -- the upstream this fork is based on
- [iPlug2](https://iplug2.github.io) -- the plugin framework
- Amp profiles created by Lum
