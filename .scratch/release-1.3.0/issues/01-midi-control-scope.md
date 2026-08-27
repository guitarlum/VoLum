# What MIDI control does 1.3.0 include?

Type: grilling
Status: open

## Question

VoLum does not control the rig from MIDI today. `NeuralAmpModeler/config.h` sets `PLUG_DOES_MIDI_IN` and `PLUG_DOES_MIDI_OUT` to 0. There is no `ProcessMidiMsg` in product code. Standalone Preferences already expose MIDI **device pickers** (iPlug host I/O). That is not MIDI **control**.

User-facing ask: [Preset setting and MIDI control](https://github.com/guitarlum/VoLum/issues/15) (standalone presets + MIDI; also “no MIDI even in the DAW”). Prior art: `backlog/F9-midi-support.md`. A 1.2.1 spike was gated because iPlug delivers MIDI on the audio thread.

Decide, for this minor:

1. **Formats.** VST3 is required. Standalone is the GitHub ask; the device picker already exists, so the extra cost is “same decoder, different source.” AU already ships as a plug-in — include MIDI-in there too, or VST3-only?
2. **Layer 1 vs Learn.** Bank Select (`CC0` amp, `CC32` channel) + Program Change → preset is the specified first layer. MIDI Learn / arbitrary CC maps were explicitly a later layer. Does 1.3.0 ship layer 1 only, or Learn as well?
3. **Channel filter.** Omni, saved channel, or both?

Recommend: VST3 + AU + standalone, one decoder; layer 1 only in 1.3.0; saved MIDI channel with Omni as default. Do not drop standalone — issue 15 is standalone-first, and a decoder that cannot be driven from the existing picker is wasted work.

Do not design the audio-thread handoff here (see [How does MIDI leave the audio thread?](03-midi-audio-thread-handoff.md)). Do not design the no-hardware test harness here (see [How do we prove MIDI without a controller?](02-midi-without-hardware.md)).
