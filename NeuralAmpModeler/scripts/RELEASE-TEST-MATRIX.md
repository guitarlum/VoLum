# VoLum Release Test Matrix

Automated CI covers fresh builds, upgrade-install smoke (when a prior published release exists), standalone launch, and non-empty audio device dropdown checks. Run this short manual matrix before publishing a release draft.

## macOS

### Fresh install
1. Remove any prior VoLum install and reset Privacy -> Microphone for VoLum if testing TCC from scratch.
2. Install from the release installer DMG or PKG.
3. Launch VoLum. Grant microphone access if prompted.
4. Open Preferences. Confirm separate Input Device and Output Device lists are populated on built-in audio.
5. Close and relaunch. Confirm no repeated mic prompt and devices still appear.

### Upgrade install
1. Install the previous published release (currently v1.0.0).
2. Create or edit `~/Library/Application Support/VoLum/volum-settings.json` with a non-default value.
3. Install the new release over the old one.
4. Confirm the app version updated and `volum-settings.json` preserved.

## Windows

### Fresh install
1. Run the release installer or portable zip on a clean machine/profile.
2. Launch VoLum standalone.
3. Open Preferences. Confirm separate Input Device and Output Device lists are populated.

### Upgrade install
1. Install the previous published release.
2. Seed `%LOCALAPPDATA%\VoLum\volum-settings.json` with a non-default value.
3. Install the new release over the old one.
4. Confirm version updated and settings preserved.

## Host smoke (optional)

- REAPER: load VST3/AU, confirm audio works. If keyboard shortcuts do not reach VoLum, enable **Send all keyboard input to plug-in** on the FX header.
- Logic Pro / GarageBand: load AU, confirm audio and preset recall.
