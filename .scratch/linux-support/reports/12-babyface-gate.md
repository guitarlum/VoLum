# 12 Real-device gate

Status: **ESCALATED**. The gate script, its red proofs, and three 5-minute passes are done on a private Pulse null sink. The Babyface capture is not a clean loop of that playback, so the 5-minute runs were not put through the interface. Details are in escalations E2. No CI job runs this gate, and `ci.yml` was not changed, so CI was not dispatched.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject | `feature/linux-12-babyface` | code `e566c38e05aeac15d09b6388c658655b9304fa61`; this report is a later commit |
| iPlug2 | `volum/linux-12` (off `volum/linux` `95da4e747`) | `b3347aadfe40b84d925407130f5d212c9a80c542` |

Box: WSL Ubuntu 26.04, the app built there from `e566c38e`. WSLg Pulse stayed on `RDPSink` / `RDPSource` after every run.

Beyond the ticket's may-write list: `IPlugAPP_linux.cpp` / `.h` and one `OS_LINUX` call in `IPlugAPP_host.cpp` (the gate tone), `provision-linux.sh` (`pulseaudio-utils`, `pulseaudio`, and not leaving the session daemon enabled), and `scripts/linux/smoke/volum-pulse-null.c` (the aligner). No Windows or macOS path was edited. The tone is off unless `VOLUM_LINUX_GATE_TONE` is set.

## What the gate does

`NeuralAmpModeler/scripts/linux/real-device-gate.sh`

- Holds `locks/gui.lock`. Does not call `pactl set-*` and does not change a default device or a volume.
- Seeds `settings.ini` (`driver=2`, PulseAudio, 48000 / 256) under a throwaway `XDG_DATA_HOME`.
- Launches the standalone with `--preferences`. SIGTERM quits it. `--list-audio-devices` must list PulseAudio.
- `VOLUM_LINUX_GATE_TONE` writes a 440 Hz sine at -24 dBFS and does not run the amp, so a hardware loopback of the output cannot feed back into it. `--mute` writes silence. `--drop` zeroes one buffer (callback 400, about 2.1 s).
- `parecord` records the Pulse source in parallel. The aligner lags the recording against the sine, fits a gain, and counts samples whose error exceeds half the fitted tone.
- Null peak must be at or under **-40 dBFS**. That number is fixed from the calibration below. Dropout samples must be at most 32. Gain must sit in 0.3..3, which is what rejects a mute. A callback gap over 50 ms with `stream_time` ≥ 0.5 s is an xrun and fails a normal run. The gap at stream open (`stream_time` under 0.5 s) is printed and is not a failure.
- `--private` starts its own `pulseaudio -n` on a private socket and a 48 kHz null sink. WSLg is not reconfigured. `--self-test` checks the aligner on synthetic wavs and opens no device.

## Device prerequisite (failed)

Read-only Windows query on `LUM_DESKTOP`: render default **Analog (3+4) (RME Babyface Pro)**, capture default **ADAT (7+8) (RME Babyface Pro)**. WSLg names those `RDPSink` and `RDPSource` (44100 Hz). Not the Speakers / main outs.

- `paplay` of 1.2 s at -24 dBFS came back on `RDPSource` at -23.2 dBFS and later -22.8 dBFS, and it was the 440 Hz tone. The sink monitor was -24.0 dBFS. At those moments the tone did loop.
- Standalone writing digital silence for 4 s: `RDPSource` peaked at **-12.4 dBFS**. Silence did not come back as silence.
- Standalone sine for 20 s: recording peak **-1.1 dBFS**, fitted gain -0.08. That is louder than -18 dBFS, so the long runs were not continued on this device.

## Calibration (private null sink, 15 s)

`~/volum-linux/out/12/private-cal` at `d9fddfbb` (same gate code, before the tolerance constant was filled in):

| | |
| --- | --- |
| peak | -24.00 dBFS |
| gain | 1.0002 |
| overlap | 15.55 s |
| residual peak | **-76.57 dBFS** |
| residual rms | -82.40 dBFS |
| dropout samples | 0 |
| xruns after 0.5 s | 0 |

`-40` is looser than -76.57 and still under a dropped buffer, which lands near -24 dBFS.

## Red proofs

| Proof | Result |
| --- | --- |
| Aligner with the dropout check compiled out | `self-test` exit **1**. The dropped synthetic still had 167 bad samples, and the test rejected the run because that case was accepted. |
| `--private --mute --seconds 6` | exit **1**. `REASON: recording is silent or too short; playback did not reach the source`. Peak -999 dBFS. App exit 0. |
| `--private --drop --seconds 8` | exit **1**. `REASON: null residual peak is above the tolerance`. Residual peak **-23.99 dBFS**, **158** dropout samples, gain 0.9995. App exit 0. |

Outputs: `~/volum-linux/out/12/red-mute` and `red-drop`.

## Three private passes (300 s each, `e566c38e`)

Preferences stayed open. One open-gap each time (`stream_time=0.005`, about 1.55 s) and no later xrun, including quit.

| Run | Peak dBFS | Gain | Overlap s | Residual peak | Residual rms | Dropouts | Xruns after 0.5 s |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | -24.00 | 1.0002 | 301.19 | -76.60 | -81.86 | 0 | 0 |
| 2 | -24.00 | 1.0002 | 301.31 | -72.04 | -76.57 | 0 | 0 |
| 3 | -23.37 | 1.0002 | 301.23 | -76.57 | -82.40 | 0 | 0 |

Run 3's sample peak is 0.6 dB above the sine; the fitted gain and the residual did not move. All three exits were 0. WSLg defaults were still `RDPSink` / `RDPSource`, and no pulse, VoLum, or Xvfb process was left.

## Xruns

### Pulse (the gate)

Every launch logs one `gate-xrun` of about 1.55 s at `stream_time=0.005`. That is the first `pa_simple` write while the stream connects: device open. The three 5-minute runs, the mute, and the drop each had **zero** gaps after 0.5 s, with Preferences open the whole time and through SIGTERM. Pulse's simple API does not report underruns on the callback status; a dropped buffer shows up in the recording, which is what the drop proof measures.

### JACK dummy (real plug-in, not the gate tone)

`smoke-standalone-audio-linux.sh --seconds 30` at the same app, `gui.lock` held, real-time `jackd -d dummy` (priority 10, rlimit 95). 55/55. Eight `xrun` lines in `jackd.log`:

- **Launch (runs 2 and 3), one each.** `JackEngine::XRun: client = VoLum was not finished, state = Running`. The client has just been activated and the first callbacks run while the capture loads. This is the device open, the same class as the Pulse open gap. `RtApiJack::getDeviceCount` / `getDeviceInfo` do open `RtApiJackCount` and `RtApiJackInfo`, but those clients are gone before VoLum is in the graph, and the log names VoLum, not those probe clients.
- **Run 3, measured seconds 6 and 8, four xruns, DSP 5.7–9.1%.** Three are `state = Running` (the callback had not returned). One cycle is `state = Triggered` for both VoLum and `volum-probe`, plus `JackTimedDriver::Process XRun = 4 usec`: the dummy server woke late, and both clients were only scheduled, not stuck in a lock. `mStagingMutex` is taken on the audio thread only to publish a pointer; the long hold is not there, and that mutex is shared with Windows and macOS, so it was not changed.
- **Run 4 quit, Preferences open, two xruns.** `state = Triggered`. The client is leaving the graph (device close). Run 1's close had zero.

No OS_LINUX change was made for these. A fix would have to show a red run against a cause in our code. The mid-window cluster is the dummy server missing a 5 ms period, not a probe client and not a lock the callback waits on.

## Open questions

1. The owner still needs a Windows default pair that is only the TotalMix loopback of the main outs, with that capture quiet when we play silence. Until then the 5-minute Babyface numbers cannot be taken. E2.
2. The 1.55 s Pulse gap at stream open is counted as device open and does not fail the gate. Same treatment as a JACK activate xrun.
