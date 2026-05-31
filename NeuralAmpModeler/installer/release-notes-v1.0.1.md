# VoLum 1.0.1

Stability and platform patch. **No intentional DSP or tone changes.**

## New

**PRE / POST lock:** lock a PRE or POST scene and carry it while browsing amps. Locked sections keep the same live sound across amp switches, do not overwrite other amps automatically, and show a Store arrow when you can save the carried scene to the current amp.

**macOS Audio Unit:** VoLum now ships as AUv2 (`VoLum.component`) for Logic Pro, GarageBand, REAPER, and other AU hosts.

## Standalone audio

- Separate Input Device and Output Device dropdowns remain, so macOS built-in microphone and speakers work correctly as separate CoreAudio devices.
- Input routing is mono again: choose one guitar input channel; VoLum mirrors it internally as needed.
- Output routing remains stereo with separate Output L/R channel choices.
- Bundled Amp Rigs stay selected in the macOS installer because the standalone app, VST3, and AU all require the shared VoLumRigs content.

## Performance

- Faster amp browsing under load: stale support/PRE model loads are dropped when newer loads replace them.
- Safer DSP staging: NAM, IR, PRE, and POST updates publish through one synchronized staging path.
- Lower real-time risk: processing helpers were split up and app I/O buffers are pre-reserved on reset.

## Fixes

- macOS microphone permission persists more reliably across launches.
- macOS VST3/AU archives are verified for valid bundle sealing/signing before release.
- Windows standalone version metadata now reports 1.0.1 for upgrade installs.
- Standalone audio settings recover cleanly from failed driver/device probes.
- Buffer size choices are stable and normalized to common pro-audio sizes.
- Knob wheel/keyboard steps are more consistent; macOS trackpad scrolling follows system direction.
- Amp/PRE output controls fully mute at `-∞ dB`.

## Compatibility

- 1.0.0 DAW sessions and `volum-settings.json` files load unchanged.
- The experimental single shared audio-device dropdown was dropped; separate Input/Output devices are the supported 1.0.1 layout.

## Downloads

- Windows installer: `VoLum-v1.0.1-windows-setup.exe`
- Windows portable: `VoLum-v1.0.1-windows-portable.zip`
- macOS installer: `VoLum-v1.0.1-macos-installer.dmg`
- macOS standalone: `VoLum-v1.0.1-macos-standalone.dmg`
- macOS VST3: `VoLum-v1.0.1-macos-vst3.zip`
- macOS AU: `VoLum-v1.0.1-macos-component.zip`

Full changelog: [changelog.txt](https://github.com/guitarlum/VoLum/blob/v1.0.1/NeuralAmpModeler/installer/changelog.txt)
