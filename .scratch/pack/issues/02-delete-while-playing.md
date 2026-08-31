# Delete (and Pack-replace) of an in-use library id

Status: ready-for-agent
Blocked by: 01

## Goal

Every guarantee in [What happens when we delete content that is currently playing?](../../release-1.3.0/issues/05-delete-while-playing.md). Tests are a gate — JSON-only tests are not enough.

## Do this

Headless apply path (same guts mouse/MIDI will use later): given a sounding-rig snapshot + a deleted/replaced id, produce the new graph + confirm-copy strings.

Wire real delete in Manage / custom API to that path on **this instance**. Siblings are not rewritten.

Pack replace reload can be a testable function now and hooked from import in ticket 03.

Confirm dialog names the in-use case and the destination amp/slot.

## Tests (must fail with this ticket reverted)

Implement each bullet in ticket 05's list (custom MAIN, SUPPORT-only, unused neighbour, PRE-1 pedal, IR, named preset, sibling RAM, MIDI slot invalid/red, Pack replace reload, confirm copy). MIDI slot tests may use the map schema from 01 even if the MIDI decoder does not exist yet (mark invalid by resolving ids).

## Done when

Windows suite. Revert-fail proven.
