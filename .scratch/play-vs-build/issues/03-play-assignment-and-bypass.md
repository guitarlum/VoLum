# PLAY recall, stomps, dirty

Status: resolved
Blocked by: 02

## Goal

Clicking a PLAY row calls the same `VolumRecallSound` MIDI uses. PRE/POST stomps toggle bypass only. Dirty / last-recalled highlight per ticket 11.

## Do this

Stomps: Pitch, Comp, NAM 1/2, Chorus, Delay, Reverb, Tremolo — existing params. NAM 2 bypass mutes SUPPORT. Bypass marks `(unsaved)` when a named preset is the origin; next recall restores snapshot. BUILD→PLAY never recalls. Last-recalled PC highlighted if still the origin (dirty OK).

If midi-control has not merged, still call a single headless recall function both can share.

## Tests (must fail with this ticket reverted)

Recall from slot applies amp+preset. Toggle PLAY/BUILD does not. Bypass sets dirty; recall clears to snapshot. Invalid slot click does not crash; PC on invalid ignores (if MIDI tests live here, otherwise midi-control owns PC).

## Done when

Standalone: assign two Sounds, switch, stomp, confirm dirty. Docs in ticket 04.
