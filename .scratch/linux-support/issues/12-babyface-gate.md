Status: escalated
Blocked by: 10

# 12 Real-device gate: Babyface through WSLg (box only)

## Scope

May write: `NeuralAmpModeler/scripts/linux/real-device-gate.sh`, `reports/`.

## Work

- **Route:** the standalone on RtAudio Pulse goes through WSLg's PulseServer to
  Windows and then the Babyface, which is the Windows default device with
  TotalMix loopback on the main outs.
- **Test signal:** play a fixture DI through a bypass-equivalent rig, or a test
  tone if needed, and record the Pulse source in parallel (`parecord`).
- **Measure:** align the recording with the reference, null-test it within a
  tolerance set from a first calibration run, and count dropouts and xruns over
  5 minutes.
- Hold `locks/gui.lock` and pass `localai-guard.sh`.

## Acceptance

- Three consecutive green runs on the box, with the numbers in the report.
- The gate fails when the playback is muted or glitched on purpose, for example
  by injecting a dropped buffer.
- **If the device is not the Windows default, or loopback is off:** escalate,
  because that is an owner prerequisite. Keep the ticket `escalated` and continue
  the others; do not block.

## Carried from ticket 10

- The JACK-dummy smoke gates xruns only in a 60 s window after a 5 s warm-up.
  Single xruns were seen at launch, at quit and while Preferences was open, at
  6–49 % DSP load. On the Babyface, count xruns from stream open to close,
  including launch, opening Preferences and quit. Each xrun outside a device
  open or close needs a root cause in the report, for example a JACK probe
  client from the device list, or a lock the audio callback waits on. Fix any
  cause that is VoLum's, and show the fix with a red run.

## Resume notes (coordinator, 2026-10-25)

- Work so far is on `feature/linux-12-babyface` (code e566c38e, report 791a2a3b; iPlug2
  `volum/linux-12` b3347aadf) and escalation E2 in its `reports/escalations.md`.
  The gate passed 3x300 s on a private Pulse null sink; `--mute` and `--drop` fail.
- Before it resolves: set `NULL_PEAK_DB` from a calibration run on the Babyface
  loopback (the -40 now is a placeholder over a -76.6 dBFS null-sink calibration),
  and decide `VOLUM_LINUX_GATE_TONE` once at host start, not on the first audio
  callback.
