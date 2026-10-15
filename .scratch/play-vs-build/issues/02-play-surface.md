# PLAY surface (Opus D)

Status: resolved
Blocked by: 01

## Goal

PLAY vs BUILD toggle and the Opus D layout at 900×600. Reference: `.scratch/release-1.3.0/play-proto/opus2/index.html?variant=D`.

## Do this

New owner file(s) `VoLumPlaySurface.h` (do not dump into `VoLumControls.h`). Wire toggle in the header (left of tuner/metronome/gear) on **both** modes. Hide BUILD chrome in PLAY; hide PLAY chrome in BUILD. Persist `uiMode` per instance.

Empty PLAY: centred "No Sounds assigned" + Add. Assigned: thumb rail PC order, invalid numbered, LIVE sticky, Add pinned, scroll a long list. Centre: recalled amp art, lighting-breathe. Bottom: stomps + meters.

Add/reassign picker: one Sound list (Factory/User) from ticket 01. Writes `midiSoundMap` via the content-library field (pack/midi specs).

Launch `run-app-win.ps1`. Iterate until it matches D. Do not invent a Dual Amp PLAY switch.

## Tests

Mode round-trip in id-tail / standalone settings. Empty vs assigned vs invalid list helpers. Toggle does not call recall.

## Done when

Standalone judged against D. Revert-fail for new tests. Bypass wiring is ticket 03 if you split; may land here if cheaper.
