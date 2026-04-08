# VoLum -- NAM Player

A fork of [Neural Amp Modeler Plugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin) by Lum, rebuilt as a personal amp collection player. Ships 14 bundled guitar amp profiles with a custom UI for instant browsing and switching -- available as both standalone app and VST3 plugin.

See [NeuralAmpModeler/README.md](NeuralAmpModeler/README.md) for the full feature list, amp inventory, rig file structure, and developer guide.

## Quick overview

- **14 bundled amps** with 4 speaker modes (AMP/G12/G65/V30) and 2-6 gain stages each (~224 NAM profiles)
- **Custom dark-theme UI** (900x600) with sidebar amp browser, hero image, speaker mode buttons, channel stepper, grouped knobs
- **Per-amp settings** -- EQ, gain, speaker, channel remembered per amp and persisted across sessions
- **Fast switching** -- background model loading + parsed data cache; no UI freezing
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
