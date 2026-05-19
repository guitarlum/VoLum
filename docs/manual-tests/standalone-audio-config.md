# Standalone Audio Configuration Manual Test

Run this checklist after changes to the standalone APP audio host path.

## Matrix

- Platforms: Windows 10/11, macOS.
- Drivers: Windows ASIO, Windows WASAPI/DirectSound, macOS CoreAudio.
- Devices: built-in device and at least one external interface when available.
- Buffer sizes: 32, 64, 96, 128, 192, 256, 512, 1024, 2048, 4096, 8192.
- Dialog actions: Apply and OK.

## Steps

1. Launch the standalone app.
2. Open Preferences.
3. For each buffer size, select the size and press Apply. Confirm audio keeps running and the app does not crash.
4. Repeat the buffer-size sweep using OK instead of Apply.
5. Change device and buffer size together, then press Apply. Confirm audio restarts cleanly.
6. Change sample rate and buffer size together, then press Apply. Confirm the selected amp still loads and audio resumes.
7. On a Windows machine without an ASIO interface, select ASIO. Confirm VoLum shows an audio error and reverts to the previous working driver.
8. Close and relaunch the app. Confirm the last working audio settings load without a startup crash.
