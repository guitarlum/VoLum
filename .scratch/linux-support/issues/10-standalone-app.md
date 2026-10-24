Status: claimed
Blocked by: 07

# 10 Standalone app on Linux

## Scope

May write:
- `iPlug2/IPlug/APP/**`, inside `OS_LINUX` sections or new files
- `iPlug2/WDL/swell` build glue
- `NeuralAmpModeler/resources/main.rc_mac_dlg` / `main.rc_mac_menu`, only if
  SWELL needs Linux variants
- `NeuralAmpModeler/projects/linux/**`
- `NeuralAmpModeler/NeuralAmpModeler.cpp`, only `OS_LINUX` guards next to the
  existing `OS_WIN`/`OS_MAC` ones

## Work

- **SWELL-GDK `main()`:** use `IPlugAPP_main.cpp` (the commented sketch) and
  PR #1336.
- **Main window:** hosts `IGraphicsLinux` on the app run loop from ticket 07.
- **Preferences dialog** (`IDD_DIALOG_PREF`): lists the RtAudio JACK, ALSA and
  Pulse devices and the RtMidi ALSA inputs. Settings persist in the XDG library.
- **Missing libraries:** a missing `libjack` or `libpulse` only removes that API
  from the list.
- **Shutdown:** a clean shutdown path, the Linux equivalent of `VoLumAppShutdown`.
  No hang, and settings are written.

## Acceptance

- With `jackd -d dummy` under Xvfb, `VoLum` launches, processes audio for 60 s
  with zero xruns reported by JACK, switches amps from the command-line harness
  env, and quits cleanly.
- `smoke-standalone-audio-linux.sh` exists and is wired into the `linux-plugin`
  job (or a new `linux-app` job).
- Windows `run-tests-win.ps1`, `e2e-standalone-win.ps1` and the macOS CI job are
  green: the shared APP code is unchanged for them.
