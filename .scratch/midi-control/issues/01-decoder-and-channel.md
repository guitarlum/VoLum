# MIDI decoder + channel filter

Status: resolved
Blocked by: none

## Goal

Pure decoder: channel filter + Program Change → slot 0–127. No hardware. No audio thread yet.

## Do this

`VoLumMidi.h` (or `.h` + test-only helpers): given `IMidiMsg` (status, data1, data2) + saved channel (0=Omni), return `std::optional<int>` slot. Ignore notes, CC, pitch bend, out-of-range, wrong channel.

Do **not** look up the sound map here. That is the handoff/recall layer.

`PLUG_DOES_MIDI_IN` can wait for ticket 02 if flipping it requires AU/project work; decoder tests must not depend on the flag.

## Tests (must fail with decoder reverted)

`test_volum_midi_decoder.cpp` (CMake + vcxproj): MakeProgramChange on Omni/channel; wrong channel; CC0/CC32 ignored; note-on ignored; slot 0 and 127 legal; slot via data1.

## Done when

Windows suite. Revert-fail proven.
