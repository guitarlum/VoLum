# Settings MIDI channel + assigned-slot list

Status: resolved
Blocked by: 02

## Goal

Gear → Settings → MIDI: channel (Omni default, per instance) and, unless PLAY already owns the list, assigned slots + Add (pick Sound: amp + named preset). Standalone port picker stays in Preferences.

## Do this

Channel control writes `midiCh` on this instance only.

List: slots 0–127 that are assigned; empty holes omitted; invalid (deleted Sound) stay numbered and red. Add / reassign / clear. Default is not assignable. No Learn.

If play-vs-build has already shipped the PLAY rail, **do not** keep a second assignment UI in Settings — channel only.

Keep Settings readable at 900×600. Launch `run-app-win.ps1` and judge.

## Tests

Headless assign/clear/invalid-red (may live in handoff tests). UI source pins for the channel control. No hardware.

## Done when

Standalone: set channel, assign a slot, (with a loopback or injected `ProcessMidiMsg` in a debug path not required for CI). Revert-fail for any new tests. Docs in ticket 04.
