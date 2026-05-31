# VoLum 1.0.1

Stability and platform patch. **No intentional DSP or tone changes** — golden tests still match 1.0.0.

## New: PRE / POST lock (global overlay)

Lock **PRE** or **POST** from the section header to carry one scene while you browse amps.

- Locked sections keep the same live sound across amp switches.
- Other amps' saved PRE/POST blocks are not overwritten automatically.
- If the carried scene differs from the **current** amp's saved block, a **Store** arrow appears — click it to commit the overlay to that amp only.
- Unlock restores this amp's saved scene without touching other amps.
- Lock state and the exact live scene persist across standalone restart and DAW project reload.

See the user guide: [PRE](https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md) and [POST](https://github.com/guitarlum/VoLum/blob/main/docs/user-guide.en.md) sections.

## New: macOS Audio Unit

VoLum now ships as **AUv2** (`VoLum.component`) for Logic Pro, GarageBand, REAPER, and other AU hosts. Installed via the macOS installer or the portable component zip.

## Performance

- Unified DSP staging under one mutex; NAM, IR, and PRE/POST commits publish atomically across the audio thread.
- Loader queue drops stale support/PRE loads when a newer request for the same target is queued (less CPU when switching amps quickly).
- ProcessBlock PRE/POST helpers extracted; I/O buffers pre-reserved on reset to avoid RT allocation.

## Bug fixes

- PRE/POST lock: live locked scene survives app/DAW reload (dedicated snapshot in settings + plugin state).
- PRE/POST lock: store arrow no longer sticks on the **origin** amp after reload when its saved scene matches the overlay.
- Settings file forward/backward compatibility between 1.0.0 and 1.0.1 (additive lock fields, version stays at 6).
- macOS: restore populated Input/Output device lists in standalone Preferences (reverted experimental single-device picker that emptied lists on Mac).
- macOS installer: Bundled Amp Rigs stay selected in Customize because the standalone app, VST3, and AU require VoLumRigs.
- macOS: mic permission persistence, VST3/AU zip signing verification, AU bundle sealing.
- Windows: standalone exe version resource matches 1.0.1 for upgrade installs.
- Standalone audio: robust buffer sizes, failed driver probe reverts to last working config.
- macOS trackpad scroll direction; consistent knob wheel/keyboard steps; output mutes at −∞ dB.

## Not in this release

- **Single shared audio device** — dropped; separate Input/Output device dropdowns remain.
- **Mono-only input routing** — reverted to stereo Input L/R for now; may return in a future patch.

## Compatibility

- 1.0.0 DAW sessions and `volum-settings.json` files load unchanged.
- Downgrading to 1.0.0 after using lock fields keeps other settings intact; lock snapshots are ignored by 1.0.0.

## Artifacts

| Platform | File |
|----------|------|
| Windows installer | `VoLum-v1.0.1-windows-setup.exe` |
| Windows portable | `VoLum-v1.0.1-windows-portable.zip` |
| macOS installer | `VoLum-v1.0.1-macos-installer.dmg` |
| macOS standalone | `VoLum-v1.0.1-macos-standalone.dmg` |
| macOS VST3 | `VoLum-v1.0.1-macos-vst3.zip` |
| macOS AU | `VoLum-v1.0.1-macos-component.zip` |

Full changelog: [changelog.txt](https://github.com/guitarlum/VoLum/blob/v1.0.1/NeuralAmpModeler/installer/changelog.txt)

## Quick test checklist

1. **PRE/POST lock:** lock on Amp A → switch to B (Store arrow if different) → quit → reopen on B → switch back to A (no Store arrow if scenes match).
2. **macOS standalone Preferences:** Input and Output device dropdowns are populated; audio works after Apply.
3. **AU/VST3:** insert in Logic or REAPER; rigs load; audio passes.
4. **Upgrade:** install over 1.0.0; version shows 1.0.1; existing settings preserved.
