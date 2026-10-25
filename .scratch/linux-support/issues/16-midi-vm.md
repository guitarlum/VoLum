Status: claimed
Blocked by: 11

# 16 Linux MIDI recall through the real ALSA stack, in a CI guest

Ticket 11's `midi` scenario is a SKIP. The WSL box kernel and the GitHub azure
kernel have no ALSA sequencer (`CONFIG_SND_SEQUENCER` unset, no `snd-seq.ko`).
The app's RtMidi is ALSA-only, so a JACK MIDI port never reaches it. The recall
logic is covered by the platform-independent doctests; the gap is RtMidi ALSA
plus the app's MIDI thread on a real kernel.

## Scope

May write:
- `.github/workflows/ci.yml`: one `linux-midi-vm` job
- `NeuralAmpModeler/scripts/linux/midi-vm/**` (guest image recipe, boot and run scripts)
- `NeuralAmpModeler/scripts/e2e-standalone-linux.sh`: only what the guest needs
  (paths, a virtual-port helper); the scenario's checks stay as they are

Must not touch the owner's machine: no WSL kernel swap, no box kernel modules.

## Work

- **Guest:** in a CI job, boot a QEMU guest with Ubuntu's generic kernel
  (`CONFIG_SND_SEQUENCER=m`). Use KVM when `/dev/kvm` exists on the runner, and
  TCG otherwise if the time fits.
  - Cloud image plus cloud-init, or a debootstrap rootfs; whichever boots
    reliably. Cache the image with `actions/cache`.
- **Run:** in the guest, `modprobe snd-seq`; build or copy the CI-built
  standalone; run `e2e-standalone-linux.sh --require-midi -Scenario midi` (or
  the script's equivalent) under Xvfb. The send side is `aconnect` or an RtMidi
  virtual port.
- **Coverage:** Program Change, CC 102, empty program, latest-wins burst, One
  channel, and the Active Sensing crowding that the doctests do not cover.

## Acceptance

- A green `linux-midi-vm` job on the exact head, with its run id. The log shows
  `/dev/snd/seq` present and the midi scenario PASS (not SKIP).
- **Red proof:** break the app's MIDI recall from the run script (for example,
  drop the Program Change handoff), restore afterwards, and show the job red.
  No switches in product code.
- The job's wall time is in the report. If it exceeds 25 minutes, say so; the
  coordinator decides whether it runs on every push or on dispatch only.

## Escalate instead of forcing

If no guest boots with a sequencer after a reasonable try (KVM missing and TCG
too slow, or the image won't boot), write the attempts to the report and stop.
The coordinator records it in `reports/escalations.md`; the skip then stands as
a documented gap in the beta notes.
