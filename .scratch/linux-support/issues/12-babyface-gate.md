Status: ready-for-agent
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
