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

## Carried from ticket 09 (coordinator, 2026-10-24)

The Linux dialogs are asynchronous. The `VOLUM_*` harness hooks return before
`PromptForFile` / `PromptForOpenFiles`, so they do not exercise the completion.
These scenarios use a fake `zenity` (and, for one case, a missing `zenity` so
`kdialog` runs) on `PATH`. Each must be shown red with the completion not
invoked.

- **Pack export.** Settings, Export, choose a path the fake `zenity` prints
  (spaces in the name). The `.volumpack` is written, and the log line is the
  same `export wrote` text as the synchronous path. Cancel (zenity exit 1)
  writes nothing.
- **Pack open.** Import, the fake returns a pack path. The import screen shows
  that pack. Cancel leaves the overlay hidden and the library unchanged.
- **Manage import.** Add IRs or pedals. The fake returns two paths. Both are
  imported. Cancel imports nothing.
- **Builder add.** Add `.nam`. The fake returns one path. The draft lists that
  file. Cancel leaves the draft unchanged.

## Acceptance

- `ALL PASS` on the box and in the CI `linux-e2e` job, with the run id.
- Each scenario is shown to fail with its feature broken: one mutation per
  scenario, logged in the report.
