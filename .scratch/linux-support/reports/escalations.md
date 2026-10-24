# Escalations (owner)

## E1 2026-10-24 05:20: cloud agents cannot start

`Task environment=cloud` failed with: "The Cursor app needs to be installed on
your repository."

- **Needed from the owner:** install the Cursor GitHub app on `guitarlum/VoLum`
  and `guitarlum/iPlug2`, plus the other submodule forks `guitarlum/NeuralAmpModelerCore`
  and `guitarlum/AudioDSPTools`.
- **Until then:** every worker runs locally. Builds and gates go through the box
  (`scripts/linux/box.ps1`, 2 build locks) and GitHub Actions.
  - The ticket 07 arena runs both candidates on the box, one per build lock.
- Nothing is blocked; the loop is only slower.
- **Closed 2026-10-24 10:35:** the owner declined. Workers stay local (the laptop,
  plus builds on the box and in GitHub Actions) for the whole effort. Do not
  dispatch cloud agents or ask again.

## E2 2026-10-25: Babyface capture is not a clean loopback of Pulse playback

Read-only. No Windows audio setting, default device, TotalMix route, or volume
was changed. Playback probes were a 440 Hz tone at -24 dBFS, under the -18 dBFS cap.

WSLg Pulse (`unix:/mnt/wslg/PulseServer`, server pulseaudio 17.0) defaults:

- Sink `RDPSink`, s16le 2ch 44100 Hz, description "RDP Sink"
- Source `RDPSource`, s16le 1ch 44100 Hz, description "RDP Source"

Windows defaults on `LUM_DESKTOP` (user `steff`), console / multimedia / communications:

- Render: **Analog (3+4) (RME Babyface Pro)** — not Speakers, and not Analog 1+2
- Capture: **ADAT (7+8) (RME Babyface Pro)**

The Babyface is attached. The defaults are not the main outs the spec describes,
and the capture is not a clean loop of what Pulse plays:

- `paplay` of 1.2 s at -24 dBFS came back on `RDPSource` at -23.2 dBFS and again
  at -22.8 dBFS, and it was the 440 Hz tone. The sink monitor was -24.0 dBFS.
  At those moments the path did loop the tone back.
- The standalone on Pulse, writing digital silence (`VOLUM_LINUX_GATE_TONE=mute`,
  4 s), recorded `RDPSource` peaking at **-12.4 dBFS**. Silence in did not come
  back as silence.
- A 20 s sine from the same standalone recorded a peak of **-1.1 dBFS** that did
  not null against the sine (fitted gain -0.08). That is louder than -18 dBFS,
  so the 5-minute runs were not continued on this device.

**Needed from the owner:** a Windows default playback and recording pair that is
only the TotalMix loopback of the main outs, with nothing else mixed into that
capture. The box will not retarget the devices.

The gate script is still delivered. Align / null / dropout checks run on a
private Pulse null sink that does not touch WSLg. Ticket 12 stays `escalated`.
