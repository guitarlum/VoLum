# VoLum 1.2.1 — residual manual tests

Everything that could be automated this pass is automated. Run these three
commands first; only the checks below them still need a human.

```pwsh
pwsh NeuralAmpModeler/scripts/run-tests-win.ps1              # 677 doctests
pwsh NeuralAmpModeler/scripts/e2e-standalone-win.ps1         # 127 on-disk state checks
pwsh NeuralAmpModeler/scripts/smoke-standalone-audio-config-win.ps1
pwsh NeuralAmpModeler/scripts/reaper-harness/run-reaper-harness.ps1  # real-host render
```

Each item below says **why** it is still manual, so the list shrinks over time
instead of growing by habit.

## 1. Audio you have to hear

*Why manual: no automated listening, and CI runners have no audio device.*

- **IR to cab swap has no gap.** On a custom amp with a DIRECT capture, select a
  custom IR, play a sustained chord, and switch to a baked cab mid-ring. The cab
  should change with no burst of raw, cab-less amp. Switch back and forth a few
  times. This is the one fix whose whole point is inaudibility.
- **Nothing clicks or drops** when changing channel, amp, or cab while playing.
- **Dual Amp** MAIN/SUPPORT still sum in phase after the above.

## 1b. Things only your eyes can confirm

*Why manual: the screenshot harness needs an unlocked desktop. `PrintWindow` cannot
composite VoLum's GL surface while the session is locked - the capture comes back
as a blank white client area - so the scripted UI sweep that would normally cover
these could not run overnight. Each item below is a fix from this pass whose whole
point is what appears on screen.*

- **`H` closes the Settings page** it opened (so does `Esc`, so does the gear).
- **Leave a custom amp, come back to a factory amp:** **No Cab** and **Custom IR**
  must be clickable again, not greyed out. Reach the bad case by picking a custom
  amp whose current gain stage has no DIRECT capture, then clicking a factory amp.
- **Dual Amp with no support amp:** turn Dual Amp on with `Tab`/the hero toggle
  while SUPPORT is still `(none)`. Focus must stay on MAIN - the cab row must keep
  showing MAIN's cabs and the channel stepper must not read `---`.
- **Dual Amp, both lanes on custom amps:** recall a preset, then check the cab row
  describes the *focused* lane. Switch focus and check it follows.
- **Press `S` on a custom amp:** it must step that custom amp's cabs (skipping
  slots it has no capture for), exactly like clicking them, and must not relabel
  the channel stepper with the factory amp's channels.
- **Exact-value box:** type `abc`, then an empty string, into a knob's exact-value
  entry. The knob must not move. Then type `1,5` and a value with its unit
  (`3 dB`, `50 %`) and confirm both are accepted.
- **Manage list with 100+ items:** the overwrite / rename / delete / IR icons on
  rows past the 100th must act on their own row.
- **Double-click Delete in a confirmation:** the row underneath the button must not
  also be activated.
- **A selected knob that a mode switch hides** (Tremolo `CROSSOVER`, Delay `TIME`
  under Sync, the pitch modes) must stop taking arrow keys.

## 2. Real DAW hosting

*Why manual: the REAPER harness now renders through the plugin for real and checks
a project save/reopen round-trip, but it cannot drive the rig - which amp, cab, or
IR is loaded is GUI-only state. The checks below are exactly the part it cannot
reach, and the plugin's state path differs from the standalone's: the DAW chunk,
not the settings file, is authoritative.*

- **Reopen with a custom IR.** In REAPER, load VoLum, select a custom IR, close
  the plugin window, reopen it. The cab row must still show the copper **Custom
  IR** chip, not **No Cab**. (This was the headline 1.2.1 report.)
- **Reopen with a custom amp on an upper channel.** Same, with a custom amp
  whose captures sit on non-contiguous gain stages (e.g. 1 and 5) on stage 5.
  It must come back on 5, and the amp you hear must be the stage-5 capture.
- **Save, close, reopen the project.** Both of the above survive.
- **Two instances on two tracks** keep separate rigs; neither overwrites the
  other.
- **Project made on a factory amp** opens on that factory amp even if the
  standalone was last used on a custom one. (This is the chunk-beats-settings
  rule; the unit tests cover the decision, not the host wiring.)

## 3. Hardware round trip

*Why manual: needs a real interface with a loopback cable. Measured on this
laptop — see "Measured round trip" below.*

- With a cable from output 1 to input 1, the interface's input gain up, and its
  **master output knob up** — at zero, the loop is silent and every reading is
  `no-signal`:

```pwsh
pwsh NeuralAmpModeler/scripts/loopback-latency-win.ps1 -Api wasapi -OutDevice "Speakers (UA-2X2)" -InDevice "Line (UA-2X2)" -Buffer 128
pwsh NeuralAmpModeler/scripts/loopback-latency-win.ps1 -Api asio -OutDevice "ASIO4ALL v2" -InDevice "ASIO4ALL v2" -Buffer 128
```

  It reports measured round-trip milliseconds, or `no-signal` with the observed
  peak so a dead path is obvious rather than silently reported as a number. The
  ASIO run is the one that matches what VoLum plays through; it needs the ASIO4ALL
  pin setup described below.
- Kill any leftover `volum_loopback.exe` first. It holds the ASIO device
  exclusively, and it also locks its own binary so the rebuild fails with
  `LNK1104`.
- Compare that measurement with the **Latency** line in Settings. They should be
  in the same ballpark; a large mismatch means the driver is misreporting.

## 4. Input calibration with a calibrated capture

*Why manual: needs a `.nam` whose trainer recorded `input_level_dbu`, which no
bundled rig has.*

- Load a capture from tone3000 that declares an input level. Settings should
  stop saying the model has no capture level, and the switch should become
  usable. Toggling it should audibly change drive.
- Load any bundled rig: the explanation returns and the switch is unavailable.

## 5. macOS

*Why manual: no macOS machine in this session.*

- Build and run `bash NeuralAmpModeler/scripts/run-tests-mac.sh` and
  `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`. Both now apply the
  iPlug2 patch first; the standalone will not compile without it, so a build
  failure here means the patch step did not run.
- Standalone Settings shows the round-trip latency line.
- AU and VST3 load in Logic/Live and pass the section 2 checks.

## Measured round trip — about 64-68 ms over WASAPI

The dead path was the interface's own master output knob sitting at zero. With it
up, the loop passes audio and the harness measures:

```
out='Speakers (UA-2X2)' in='Line (UA-2X2)' api=wasapi rate=48000 buffer=128
measured_ms=63.73  spread_frames=0  peak_in=0.0670  overflows=10
```

Five bursts, identical to the sample within a run; two runs read 63.73 and 67.73
ms, so WASAPI's shared-mode buffering shifts by a few ms between stream opens.
That is the WASAPI shared-mode path,
consistent with the ~48-60 ms measured for FlexASIO below rather than with
ASIO4ALL's 21.2 ms, so it is not the number a player on ASIO feels. Input peaks
at 0.067 for a 0.9 burst (about -23 dB), so the interface's input gain has plenty
of headroom left if a hotter measurement is ever wanted.

Two bugs in the harness had to be fixed before this read out at all, and they
both reported the failure as a dead cable:

- The burst was timed in stream frames but searched for by capture-buffer index.
  Those two counters diverge exactly when a driver drops input blocks (the 10
  overflows above), so the search began past the burst and never saw it. The
  emit point is now marked in the capture buffer's own index.
- A hit at index 0 was indistinguishable from "nothing found", which would read a
  genuinely zero-latency loop as silence.

`test_volum_loopback_detect.cpp` pins both.

## Still open — ASIO4ALL never reaches the interface

On the same cable, `-Api asio` returns literal digital silence (`peak_in` exactly
0.00000, nothing anywhere in the buffer) while WASAPI measures fine. ASIO4ALL has
no saved configuration on this machine — no `HKCU\Software\ASIO4ALL v2` key at all
— so it is running on defaults and exposes one stereo pair (`in=2 out=2`) that is
not the UA-2X2's.

To finish this: open `C:\Program Files (x86)\ASIO4ALL v2\a4apanel.exe`, enable the
UA-2X2 input and output pins, disable the built-in Realtek and Intel devices, then
re-run the ASIO command in section 3. Expect roughly 21 ms if the driver's own
figure is honest, which is the cross-check VoLum's Settings latency line needs.

## Driver comparison — settled, keep ASIO4ALL

The Swissonic UA-2X2 has no native ASIO driver: Thomann offers none for this
model and the current Thomann USB audio package targets a newer device. FlexASIO
was installed and measured as the fallback. Driver-reported round trip at 48 kHz
with a 128-frame buffer:

| Driver                        | Round trip |
| ----------------------------- | ---------- |
| ASIO4ALL v2                   | 21.2 ms    |
| FlexASIO, WASAPI exclusive    | 48.3 ms    |
| FlexASIO, WASAPI shared       | 49.4 ms    |
| FlexASIO, MME (its default)   | 60.0 ms    |
| FlexASIO, WDM-KS              | fails to open |

Exclusive mode does engage — that is the 1 ms between the two WASAPI rows — but
the ~48 ms floor is the UA-2X2's generic Windows USB-audio class driver, which
WASAPI must go through and ASIO4ALL buffers more tightly around. So FlexASIO is
not the win, and 21 ms is this interface's practical floor on this machine. The
tuned config is kept at `%USERPROFILE%\FlexASIO.toml` so the comparison is
reproducible; deleting it returns FlexASIO to its 60 ms MME default.

ASIO4ALL's 21.21 ms also matches VoLum's own Settings readout of 21.2 ms
exactly, which independently confirms the new latency line is honest.

## Not tested, by decision

- **MIDI.** `PLUG_DOES_MIDI_IN` is 0 and there is no `ProcessMidiMsg`; the
  feature does not exist yet (backlog F9, 1.3.0). Nothing to test.
- **Linux.** Out of scope for 1.2.1.
