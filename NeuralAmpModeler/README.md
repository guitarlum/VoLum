# VoLum -- NAM Player

VoLum is a custom build of the [Neural Amp Modeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) that ships 14 bundled guitar amp profiles with a purpose-built UI. It targets guitarists who want a plug-and-play experience without hunting for `.nam` model files.

## Quick start (dev)

1. Open `NeuralAmpModeler/NeuralAmpModeler.sln` in Visual Studio 2022 (Build Tools or Community).
2. Select **NeuralAmpModeler-app** | **Debug** | **x64**.
3. Build and run. The standalone app reads rigs from `rigs/` relative to the repo root.

The VoLum-specific code is compiled when `VOLUM_AMPETE_PRODUCT` is `1` (set in `config.h`). Setting it to `0` builds the original NAM plugin.

## Rig file structure

All rig files live under `rigs/` with one subfolder per amp:

```
rigs/
  {AmpFolder}/
    {Speaker}-{AmpCode}-{Channel}.nam
```

- **Speaker prefix:** `AMP` (direct), `G12`, `G65`, `V30` (cabinet simulations)
- **AmpCode:** short identifier derived from amp name (e.g. `Ampt`, `2203`, `BadC`)
- **Channel suffix:** gain stage number (`1`-`6`) or special labels (`f`, `x`)

Example: `rigs/Marshall JMP 2203 1976/V30-2203-f.nam` = Marshall 2203 with V30 cab, FatBee gain stage.

Every amp folder has the same 4 speaker prefixes. The number of channels varies per amp (2 to 6). See the [plan document](.cursor/plans/volum_full_rig_catalog.plan.md) for the full inventory.

## Bundled amps (14)

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

## UI design

The VoLum UI is a single-view layout (900x600):

- **Left sidebar (200px):** Scrollable amp list. Click to select an amp.
- **Right panel:** Speaker mode buttons (AMP / G12 / G65 / V30), hero image placeholder, amp name, channel stepper, standard NAM controls (Input, Gate, Bass, Mid, Treble, Output knobs), Noise Gate and EQ toggles, footer showing the currently loaded `.nam` filename.

The channel stepper (`< 1 >`) cycles through only the gain stages available for the selected amp + speaker combination. Changing amp or speaker resets the channel to the first available.

## Architecture notes

- **No file browser:** VoLum does not expose NAM's file browser controls. Rig selection is entirely through the sidebar, speaker buttons, and channel stepper.
- **Callback-based state:** Amp/speaker/channel are plain member variables, not iPlug2 params. Controls use `std::function` callbacks to update state and trigger loads.
- **Background loading:** NAM model parsing (~200-500ms) runs in a detached `std::thread` to keep the UI responsive. An atomic flag prevents concurrent loads.
- **Dynamic channel discovery:** `volum::DiscoverChannels()` scans the amp folder at runtime to find matching `.nam` files for the selected speaker prefix.
- **Conditional compilation:** All VoLum code is gated behind `#if VOLUM_AMPETE_PRODUCT`. The original NAM plugin builds unmodified with `VOLUM_AMPETE_PRODUCT 0`.

## Key source files

| File | Role |
|------|------|
| `config.h` | `VOLUM_AMPETE_PRODUCT`, window size, version |
| `VoLumAmpeteCatalog.h` | Amp metadata (folder names, display names, speaker prefixes) |
| `VoLumPaths.h` | Rig directory discovery + channel file scanning |
| `VoLumControls.h` | Custom iPlug2 UI controls for the VoLum layout |
| `NeuralAmpModeler.h/cpp` | Plugin class with VoLum state, layout, and loading logic |

## Build requirements

- Windows 10+ (x64)
- Visual Studio 2022 Build Tools (MSVC v143)
- No external package manager -- all dependencies (iPlug2, Eigen, NAMCore) are vendored in the repo
