# 16 Linux MIDI recall in a QEMU guest

Status: **PASS** on code head `3687a528`. The `linux-midi-vm` job boots Ubuntu's generic kernel under KVM, loads `snd-seq`, and the midi scenario passes with `--require-midi`. The host `linux-e2e` job still skips that scenario: the GitHub azure kernel has no sequencer module.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject code | `feature/linux-16-midi-vm` | `3687a528bd0546b9c8106d1b7a48edd30548abb0` |
| iPlug2 | unchanged from `feature/linux-support` | `3d894cd8e7a4185e7b5b28d24e7c149d587ab1ba` |
| This report | a later commit on the same branch | see `git log -1` |

No product code changed. The app's MIDI path is the one ticket 11 already shipped.

## What runs

Job **Linux MIDI (QEMU guest)** (`linux-midi-vm`) in `.github/workflows/ci.yml`, on `ubuntu-22.04`.

The runner has `/dev/kvm`. The job opens it (`udev` mode `0666`, plus `chmod 666`) and QEMU uses `accel=kvm` and `-cpu host`. The log line is `VOLUM_MIDI_VM_ACCEL=kvm cpu=host`.

The guest is the Ubuntu 22.04 cloud image. Cloud-init installs `linux-image-generic` when the image's own kernel is not `*-generic`, then the packages the scenario needs (Xvfb, jackd, a compiler, ALSA, Mesa, GTK). The baked disk is cached with `actions/cache`. A cache hit skips the bake. This run was a hit: kernel `5.15.0-198-generic`, module `/lib/modules/5.15.0-198-generic/kernel/sound/core/seq/snd-seq.ko`.

The guest runs `e2e-standalone-linux.sh --scenario midi --require-midi` under its own Xvfb. The send side is the script's existing ALSA virtual port (`VoLum Loop:out client:port`), the same RtMidi spelling as on Windows. `--require-midi` is unchanged: a missing `/dev/snd/seq` fails the job.

`ALSA_CONFIG_PATH` in the scenario still nulls the default PCM. It also defines `seq.default` and `seq.hw`, because that variable replaces the system ALSA config and both the sender and RtMidi open the sequencer as `"default"`.

## Result

CI run [40174970981](https://github.com/guitarlum/VoLum/actions/runs/40174970981) on `3687a528`. Every `ci.yml` job succeeded, including macOS.

The guest log shows `/dev/snd/seq` (`crw-rw-rw-` on `5.15.0-198-generic`) and:

`ALL PASS  20 checks`

Passed checks include Program Change 9, CC 102 value 42, 64 Active Sensing messages then the following Program Change, the empty program 7, the burst ending on program 101, and One channel = 2.

The host `linux-e2e` job on the same run still prints `SKIP [midi] Program Change and CC 102 recall` because that runner's kernel has no `snd-seq`. That skip is the azure-kernel gap. The guest job is the one that must not skip.

## Wall time

The job ran **12 minutes 0 seconds** (`2026-10-25T08:01:05Z` to `2026-10-25T08:13:05Z`). The image restore was a cache hit, so the bake step was skipped. The guest boot and the scenario are the `MIDI recall in the guest` step, about 6 minutes 45 seconds of that.

A cold bake on the first run of this branch finished in a few minutes under KVM and is cached after that. The steady-state job is under 25 minutes, so it stays on every `ci.yml` run.

## Red proof

Run [40170411862](https://github.com/guitarlum/VoLum/actions/runs/40170411862) on `68ae2a0d`. Before the guest build, the job replaced `mVolumMidiQueue.Drain()` with an empty `decltype(...){}` so the idle loop never takes a Program Change or CC. The sequencer was present. The scenario exited 1:

`FAILED  9 of 20 checks`

The failures are the recall checks: Program Change 9, CC 102 value 42, Active Sensing crowding, the empty program's ignored line, the burst, the saved PLAY cursor and preset, and both One-channel-2 recalls. The job is red. That sed is not on `3687a528`.

## Follow-up

Code head `b630bf32`, merged onto `feature/linux-support` `aec116b8`. CI run [40180499888](https://github.com/guitarlum/VoLum/actions/runs/40180499888). Every job succeeded, including macOS and Formatting.

The sender now closes its sequencer port and client on a `quit` line after the checks. The guest writes and syncs the result file, then `poweroff -f`. The host still requires `ALL PASS` and rc 0. On this run the guest log is `ALL PASS  20 checks` and `VOLUM_MIDI_VM_GUEST_DONE rc=0` at about 26 seconds of guest uptime. The `MIDI recall in the guest` step is 34 seconds. There is no `Waiting for process: volum-alsa-midi`. The host `linux-e2e` job still skips MIDI and its other scenarios passed.

The base image is `https://cloud-images.ubuntu.com/jammy/20261020/jammy-server-cloudimg-amd64.img`, SHA256 `2099e63da0b822ef8e05e3d596c1b402eaaae658fc08ab919cb8495bfcdc488b`, recorded in `image-key.txt`. That file is in the `actions/cache` key, so this run baked a new image (`image pin` prints that digest).

The job wall time is **7 minutes 47 seconds** (`2026-10-25T09:24:21Z` to `2026-10-25T09:32:08Z`), including that cold bake.
