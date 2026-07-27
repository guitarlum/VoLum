# VoLum 1.2.1 — residual manual tests

Everything that could be automated this pass is automated. Run these three
commands first; only the checks below them still need a human.

```pwsh
pwsh NeuralAmpModeler/scripts/run-tests-win.ps1          # 592 doctests
pwsh NeuralAmpModeler/scripts/e2e-standalone-win.ps1     # 118 on-disk state checks
pwsh NeuralAmpModeler/scripts/smoke-standalone-audio-config-win.ps1
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

## 2. Real DAW hosting

*Why manual: no REAPER/Cubase automation harness exists, and the plugin's state
path differs from the standalone's — the DAW chunk, not the settings file, is
authoritative.*

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

*Why manual: the loopback harness is built and works, but the physical signal
path on this laptop never passed audio — see "Known gap" below.*

- With a cable from output 1 to input 1 and the interface's input gain up:

```pwsh
pwsh NeuralAmpModeler/scripts/loopback-latency-win.ps1 -Api asio -OutDevice "ASIO4ALL v2" -InDevice "ASIO4ALL v2" -Buffer 128
```

  It reports measured round-trip milliseconds, or `no-signal` with the observed
  peak so a dead path is obvious rather than silently reported as a number.
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

## Known gap — the loopback on this laptop

The harness, the tone-burst detection, and the reporting all work; the physical
path does not. With both endpoints unmuted and at full volume, input peaked at
0.00044 (pure noise) while a 0.9-amplitude burst played out. Nothing in software
explains that, so it is the interface's front-panel gain, the monitor level, or
which jack the cable is in. One minute with the box in hand settles it.

Also unresolved: the Swissonic UA-2X2 has no native ASIO driver available — the
Thomann page offers none for this model, and the current Thomann USB audio
package targets a newer device. The fallback is FlexASIO, already downloaded to
`C:\tmp\volum-loopback\FlexASIO-1.10b.exe`. It needs an elevated install, which
this session could not perform. After installing it, re-run the loopback command
above with `-Api asio -OutDevice FlexASIO -InDevice FlexASIO` and compare against
the ASIO4ALL number.

## Not tested, by decision

- **MIDI.** `PLUG_DOES_MIDI_IN` is 0 and there is no `ProcessMidiMsg`; the
  feature does not exist yet (backlog F9, 1.3.0). Nothing to test.
- **Linux.** Out of scope for 1.2.1.
