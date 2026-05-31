# Standalone Audio Configuration Manual Test

Run this checklist after changes to the standalone APP audio host path.

## Matrix

- Platforms: Windows 10/11, macOS.
- Drivers: Windows ASIO, Windows WASAPI/DirectSound, macOS CoreAudio.
- Devices: built-in input/output and at least one external interface when available.
- Visible buffer sizes: 48, 64, 96, 128, 256, 512, 1024, 2048, 4096, 8192.
- Legacy saved buffer sizes to smoke-test by editing `settings.ini`: 32, 192. They should reopen as the next visible size.
- Dialog actions: Apply and OK.

## Steps

1. Launch the standalone app.
2. Open Preferences.
3. Confirm the audio section has separate Input Device and Output Device selectors plus Input/Output channel selectors.
4. Confirm the buffer-size dropdown is exactly ordered as `48, 64, 96, 128, 256, 512, 1024, 2048, 4096, 8192` with no duplicates.
5. For each visible buffer size, select the size and press Apply. Confirm audio keeps running and the app does not crash.
6. Repeat the buffer-size sweep using OK instead of Apply.
7. Change input device, output device, and buffer size together, then press Apply. Confirm audio restarts cleanly.
8. Change sample rate and buffer size together, then press Apply. Confirm the selected amp still loads and audio resumes.
9. On a Windows machine without an ASIO interface, select ASIO. Confirm VoLum shows an audio error and reverts to the previous working driver.
10. Close and relaunch the app. Confirm the last working audio settings load without a startup crash.
