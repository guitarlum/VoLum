# How do we prove MIDI without a controller?

Type: research
Status: resolved

## Question

The owner has no MIDI hardware and still wants MIDI control proven for VST3 and standalone (AU if in scope). What test seams exist in this repo and in iPlug so an agent can 100% the decoder, the audio-thread handoff, Program Change → preset, and host MIDI-in — without a physical device?

Look at: `ProcessMidiMsg` in iPlug, pluginval MIDI, Windows virtual loopback, doctest injection of MIDI messages, standalone host MIDI device enumeration. Report what can be automated here vs what still needs a human DAW click.

Do not decide the MIDI feature shape (see [What MIDI control does 1.3.0 include?](01-midi-control-scope.md)).

## Answer

**Today:** `PLUG_DOES_MIDI_IN/OUT` are 0; no `ProcessMidiMsg` override. Existing tests only pin APP host MIDI open/close, not message injection.

iPlug delivers MIDI to `ProcessMidiMsg` on the **audio** thread (APP drain in `AppProcess`; VST3 `Process`; AU `DoMIDIEvent`). Setting `PLUG_DOES_MIDI_IN` to 1 advertises a VST3 event input and flips AU from `aufx` to `aumf`. APP device pickers already exist and do not wait on that flag. Windows WinMM cannot open a virtual port; macOS can.

**Automated, no hardware (ranked):**

1. Pure decoder doctests on `IMidiMsg` helpers (`MakeProgramChange`, `MakeControlChangeMsg`) — Bank Select + Program Change → indices, channel filter, bounds. CI via `run-tests-win.ps1`.
2. Unit-test a bounded audio→main command queue: RT path must not touch registry, filesystem, or UI.
3. Headless selection service shared with mouse/keyboard, fed by that queue.
4. pluginval/auval after MIDI-in is advertised — bus/scan only. Upstream pluginval sends note on/off to **instruments**; it does not prove Program Change or CC control for an effect.

**Still human:** DAW MIDI routing; one visual Program Change; first Preferences device pick; Windows standalone OS-MIDI e2e needs an installed loopback port (not in CI).

**1.3.0 pyramid:** decoder + handoff + headless selection in CI are mandatory. Do not treat pluginval or a physical controller as proof of the decoder.
