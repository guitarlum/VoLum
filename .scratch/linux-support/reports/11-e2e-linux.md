# 11 Linux standalone e2e

Status: **PASS** on code head `b6d23c42`, with one skip. MIDI recall is implemented and fails closed when the sequencer is missing (`--require-midi`). The kernels available for the box and for GitHub Actions were built without an ALSA sequencer module, so that scenario is a skip, the same way the Windows harness skips a missing loopMIDI port.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject code | `feature/linux-11-e2e` | `b6d23c42694b8cfb9d5709f8b16c6dd0a500e0cc` |
| iPlug2 | `volum/linux-11` (off `volum/linux` `ee4ce0e69`) | `3d894cd8e7a4185e7b5b28d24e7c149d587ab1ba` |
| This report | a later commit on the same branch | see `git log -1` |

iPlug2 `3d894cd8e` only documents that `VoLumLinuxAppDataDir` is the one XDG helper. The function body is unchanged. Not merged into `volum/linux`, `feature/linux-support`, or `dev`.

## What runs

`NeuralAmpModeler/scripts/e2e-standalone-linux.sh`. Its own Xvfb, `jackd -d dummy` (realtime, then again without realtime when the kernel refuses it), a null ALSA PCM, a dead Pulse socket, and a sandbox `XDG_DATA_HOME` per scenario. On the box each scenario holds `gui.lock` and drops it before the next one. Clicks are xdotool on the 900x600 editor. Screenshots are xwd plus ImageMagick. The draft row and the Preferences dialog are read with tesseract.

Scenario names match the Windows harness where that scenario exists.

| Scenario | What it checks |
| --- | --- |
| `fresh` | Empty library, PLAY, first Factory Sound, program 0 recalled, cleared board not filled again, SIGTERM exit 0 |
| `presets` | An amp that was never visited keeps its preset; clicking PLAY sound 1 recalls SLO Crunch; clicking sidebar amp 3 selects it |
| `pack` | Everything export and open through `VOLUM_PACK_SAVE_PATH` / `VOLUM_PACK_OPEN_PATH`. Log lines `export wrote` and `opened` |
| `import` | Manage > Add for IRs and pedals through `VOLUM_IMPORT_OPEN_PATHS`. Four of five long near-identical names, the identical copy skipped |
| `midi` | Program Change and CC 102 on an ALSA virtual port named like RtMidi (`VoLum Loop:out client:port`), including an empty program, a burst, and One channel = 2. Needs `/dev/snd/seq` |
| `preferences` | `--list-audio-devices` and `--preferences`. The dialog text contains JACK and the dummy device `system` |
| `shutdown` | SIGTERM writes settings and both teardown lines. A second SIGTERM exits 3 |
| `dialog-export` | Fake zenity prints a path with a space. The pack is written and the log is `export wrote`. Cancel writes nothing |
| `dialog-open` | Fake zenity returns a pack. The log is `opened` and the import screen is up. Cancel leaves the overlay hidden and the library unchanged |
| `dialog-import` | No zenity on `PATH`, so kdialog runs and returns two WAVs. Both are imported. Cancel imports nothing |
| `dialog-builder` | Fake zenity returns `V30-E2E-1.nam`. The draft row lists it (tesseract reads the V30 chip that name fills in). Cancel leaves the row unchanged |

The four dialog scenarios are the ones carried from ticket 09. The `VOLUM_*` hooks return before `PromptForFile` / `PromptForOpenFiles`, so they never run those completions. A fake `gdbus` fails immediately so the chain reaches zenity or kdialog.

## XDG

`LinuxVoLumDataDir` (`NeuralAmpModeler/VoLumPaths.h`) calls `iplug::VoLumLinuxAppDataDir`. A relative `XDG_DATA_HOME` falls back to `$HOME/.local/share/VoLum`, and the library path is that directory. The doctest reads the function body and requires the call. Ticket 10 had already taught each copy to ignore a relative value; they were still two copies.

## Evidence

Box: WSL2 Ubuntu 26.04, GCC 15.2. Jobs through `box.ps1`. `gui.lock` released between scenarios.

| Check | Result |
| --- | --- |
| Full suite, code head `b6d23c42` | **ALL PASS 96 checks**. One skip: MIDI, no `/dev/snd/seq` |
| Windows `run-tests-win.ps1` | **exit 0**. 1476 passed, 0 failed, 1 skipped. The C++ under test is this head; the later commits are the shell script and `ci.yml` only |
| XDG doctest, fixed tree | 2 cases, 17 assertions, `GREEN_RC=0` |
| Preferences OCR on the box | The dialog text contains `JACK` and `system` |

## Red proofs

Each break was restored afterwards. `git diff` on the touched sources was empty, and the factory banks were put back.

| Scenario | Break | Red result |
| --- | --- | --- |
| XDG doctest | `LinuxVoLumDataDir` is a second copy again and accepts a relative `XDG_DATA_HOME` | `CHECK( body.find("VoLumLinuxAppDataDir(") != npos )` failed. `RED_RC=1` |
| `fresh` | Both `factory-presets.json` files (repo `rigs/` and the one beside the binary) hidden | `factory:12:v1` missing, program 0 not recalled, prefill empty. `RED2_fresh_RC=1` |
| `presets` | The per-amp map is dropped on load | `preset_e2e_away` missing. `RED2_presets_RC=1` |
| `pack` | `writeChosen` returns before it writes | No `.volumpack`, no `export wrote`. `RED2_pack_RC=1` |
| `import` | `StartImport` returns before it copies | IR and pedal batches import 0 of 5. `RED2_import_RC=1` |
| `midi` | No sequencer (`--require-midi`, the old fail-closed check) | `ALSA sequencer /dev/snd/seq is present` failed on the box and in CI [40153669537](https://github.com/guitarlum/VoLum/actions/runs/40153669537) |
| `preferences` | The driver label `JACK` renamed to `Jack` | `--list-audio-devices`, the log, and the dialog all miss `JACK`. `RED2_preferences_RC=1` |
| `shutdown` | The second-signal `_exit(3)` removed | The process is still up after 20 s (`got hung`). `RED_shutdown_RC=1` |
| `dialog-export` | The completion returns before `writeChosen` | Zenity ran; the pack was not written; no `export wrote`. `RED_dialog-export_RC=1` |
| `dialog-open` | The completion returns before `openChosen` | No `opened` line. `RED_dialog-open_RC=1` |
| `dialog-import` | The completion returns before `StartImport` | 0 files imported. `RED_dialog-import_RC=1` |
| `dialog-builder` | The completion returns before the draft is filled | The draft row does not change and does not list the file. `RED_dialog-builder_RC=1` |

## CI

[Run 40154331174](https://github.com/guitarlum/VoLum/actions/runs/40154331174) on `b6d23c42`. **Success. Every `ci.yml` job**, including macOS tests, macOS installer upgrade, macOS standalone e2e, Linux tests, Linux standalone, and Linux standalone e2e.

Linux standalone e2e: **ALL PASS 96 checks**, and

`SKIP [midi] Program Change and CC 102 recall - no /dev/snd/seq (snd-seq did not load; this kernel has no sequencer module)`

An earlier run on `090d81bf`, [40153669537](https://github.com/guitarlum/VoLum/actions/runs/40153669537), failed only that MIDI check (`--require-midi` was the fail-closed behaviour then). Preferences OCR on that run already contained `JACK` and `system`.

## Open questions

1. **MIDI cannot be executed here.** The box kernel (`6.6.114.1-microsoft-standard-WSL2`) has `# CONFIG_SND_SEQUENCER is not set`, so `snd-seq` and `snd-virmidi` do not exist to load. The GitHub `ubuntu-22.04` image (`6.8.0-1064-azure`) still has no `snd-seq.ko` after `linux-modules-extra`. `aconnect` and an RtMidi virtual port both need that device. The app's MIDI is ALSA, so a JACK MIDI port would not reach it. The scenario skips, and `--require-midi` fails. A kernel with the sequencer module is what would turn the skip into the Program Change run.
