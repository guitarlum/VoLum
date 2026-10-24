Status: ready-for-agent
Blocked by: 09, 10

# 11 Linux standalone e2e

## Scope

May write:
- `NeuralAmpModeler/scripts/e2e-standalone-linux.sh` (plus a helper `.py` if
  needed)
- `.github/workflows/ci.yml`: new job `linux-e2e`

## Work

Use Xvfb, xdotool and ImageMagick, with a sandbox `XDG_DATA_HOME` per run.

Scenarios:
- launch with a fresh library;
- amp and preset switch;
- custom NAM import and `.volumpack` export/import via the `VOLUM_*` env hooks;
- MIDI recall through an ALSA virtual port (`snd-virmidi` if the kernel has it,
  otherwise `aconnect` with a RtMidi virtual output or JACK MIDI);
- the Preferences dialog opens and lists the dummy JACK device;
- clean shutdown with settings persisted.

## Acceptance

- `ALL PASS` on the box and in the CI `linux-e2e` job, with the run id.
- Each scenario is shown to fail with its feature broken: one mutation per
  scenario, logged in the report.

## Carried from ticket 10

- The standalone takes `--list-audio-devices` and `--preferences` for these
  scenarios. A quit is SIGTERM (it closes like the close button); a second
  signal exits at once.
- `VoLumLinuxAppDataDir` (iPlug2 `IPlug/APP/VoLumAppLinuxPolicy.h`) repeats
  `LinuxVoLumDataDir` (`NeuralAmpModeler/VoLumPaths.h`). Keep one, used by both
  the app and the library. Both accept a relative `XDG_DATA_HOME`, which the XDG
  spec says to ignore. Add a doctest that a relative value falls back to
  `~/.local/share`, red against the current code.
