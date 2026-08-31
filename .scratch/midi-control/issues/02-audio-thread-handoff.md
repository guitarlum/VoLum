# MIDI audio-thread handoff + headless recall

Status: resolved
Blocked by: 01

## Goal

[How does MIDI leave the audio thread?](../../release-1.3.0/issues/03-midi-audio-thread-handoff.md): capacity-1 latest-wins queue; drain in `OnIdle`; headless `VolumRecallSound`.

## Do this

1. Queue: lock-free or mutex-free latest-wins slot (`std::atomic<int>` with a sentinel empty, or a one-slot overwrite). Audio thread only stores the int.
2. `ProcessMidiMsg` override: decoder then enqueue. Enable `PLUG_DOES_MIDI_IN`.
3. `OnIdle`: drain; resolve slot against `midiSoundMap`; unassigned/invalid → ignore; else `VolumRecallSound`.
4. Headless recall shared with mouse/keyboard apply guts (stable amp identity + named preset, including Factory ids `factory:<idx>:v1` once play-vs-build ships; until then User presets + factory amps are enough). Custom-amp cabs must not diverge from the sidebar path.
5. Instance `midiCh` in id-tail JSON. Reader: missing key = Omni.

If pack's `midiSoundMap` field is not merged, add the same JSON key on the content registry so you are not blocked.

## Tests (must fail with this ticket reverted)

`test_volum_midi_handoff.cpp`: enqueue A then B → drain sees B; empty drain is no-op; recall of missing preset stays put; recall of assigned User preset applies amp+scene in a headless fixture (content store temp dir). Source pin: `ProcessMidiMsg` does not call `GlobalContentStore` / filesystem.

## Done when

Windows suite. Revert-fail proven. Settings chrome is ticket 03.
