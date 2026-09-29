# MIDI control (decoder + audio-thread handoff + Settings)

Locked map: `.scratch/release-1.3.0/map.md`. Do not reopen product calls.

## Outcome

VST3 + AU + standalone, MIDI **in** only. Program Change slot N recalls a machine-global Sound (amp + named preset). Channel filter per instance (Omni default). Audio thread never touches registry/filesystem/UI.

## Locked decisions (do not invent)

- [What MIDI control does 1.3.0 include?](../release-1.3.0/issues/01-midi-control-scope.md)
- [How do we prove MIDI without a controller?](../release-1.3.0/issues/02-midi-without-hardware.md)
- [How does MIDI leave the audio thread?](../release-1.3.0/issues/03-midi-audio-thread-handoff.md)

PLAY owns the assignment list chrome (play-vs-build spec). This spec still needs a **headless** assign/clear/recall API and a **Settings** home for MIDI **channel** (and, until PLAY lands, the assigned-slot list + Add so MIDI is usable). If PLAY has already relocated the list, do not duplicate it in Settings — channel stays in Settings either way.

## Formats

`PLUG_DOES_MIDI_IN` = 1, `PLUG_DOES_MIDI_OUT` stays 0. AU `aufx` → `aumf` is accepted. One decoder for all formats. Standalone Preferences keep the **port** picker only.

## Player model

128 slots 0–127. No CC0/CC32, no notes, no pitch bend, no Learn. Unassigned / out of range / missing amp or preset → **ignore** (stay put). Default (factory settings) is not on MIDI. Invalid slots (deleted Sound) stay numbered, show red in the list, still ignore on PC.

Sound map is machine-global and lives in the content library under the pack spec's lock (`midiSoundMap`). If pack's persistence is not merged yet, implement the same JSON field so the merge is trivial — do not put the map in `volum-settings.json`.

MIDI **channel** is per plugin instance, Omni default, stored in the DAW **id-tail JSON** (`midiCh`: `0` = Omni, `1–16` = channel). Standalone: same field in the instance settings / id tail equivalent so it survives restart; not in the port picker.

## Audio thread

`ProcessMidiMsg` (audio thread): channel filter, then Program Change → enqueue slot `0–127`. Capacity **1**, latest-wins overwrite. No registry, no map, no filesystem, no UI.

Drain in `OnIdle` (same as model load). Apply with editor closed; if chrome exists, refresh; else set the existing UI-sync pending flag.

Mouse/keyboard do **not** enqueue. Extract `VolumRecallSound(ampId, presetId)` (headless) that the drain calls. Sidebar / preset-bar callbacks must use the same apply guts.

## Tests (gate)

CI, no hardware:

1. Decoder doctests: `IMidiMsg` Program Change → slot; channel filter Omni vs 1–16; wrong channel ignore; CC/notes ignored; unassigned/missing → ignore. **Not** CC0/CC32.
2. Queue: latest-wins; RT path does not call registry/filesystem (source pin / sanitizer-friendly unit).
3. Headless recall: slot → amp + named preset; Factory preset id vs User id.

pluginval is not semantic proof. Windows OS-MIDI loopback is human-only.

Prove each new test fails with the fix reverted, then passes.

## File ownership

`config.h`, `NeuralAmpModeler.cpp` / `.h` (`ProcessMidiMsg`, `OnIdle`), new `VoLumMidi.h` (decoder + queue, no iPlug UI), Settings overlay (channel + interim list), `VoLumChunkIdTail.h` (`midiCh`), tests `test_volum_midi_decoder.cpp`, `test_volum_midi_handoff.cpp`.

Do not take full B7 RT hardening. Do not enable MIDI out.

## Docs

Changelog: MIDI Program Change recalls Sounds; channel in Settings; AU type change note. EN/DE guides. No screenshot required unless Settings MIDI chrome is visible in an existing shot.
