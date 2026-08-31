# Delete (and Pack-replace) of an in-use library id

Status: done
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

## Result

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` → 767 cases, 0 failed.

- `VoLumRigRepair.h`: headless planner. `PlanDelete` / `PlanReplace` take a
  `SoundingRig` snapshot plus the item going away and return the repairs, the rig
  that is left, and the confirm copy. No iPlug, no content store, so the same
  answers are reachable from the sidebar, the Manage panel, a Pack import, and a
  unit test.
- `VoLumRigRepair.inc.cpp`: the mechanical half. Snapshots the live rig, and pushes
  a plan back out through the ordinary select/clear paths so a repair is
  indistinguishable from the user having done it by hand. Also
  `_VolumRepairRigForMissingContent()` for the sibling case.
- `_VolumSelectFactoryAmp(ampIdx, snapshotOutgoing)` extracted from the sidebar
  click callback, so the MAIN fallback really is "as if clicked" (scene restore +
  capture reload + chrome). `snapshotOutgoing=false` on the delete path: folding
  the deleted amp's knobs into the factory slot would overwrite the knobs the user
  left on that factory amp.
- Sidebar bin and the Manage panel both plan before the catalog mutation (a
  pedal's rig reference is its capture index, which the delete takes with it) and
  apply after.
- IR teardown uses `deferToCabSwap`, so the convolver keeps running until the
  baked-cab capture is staged - no burst of raw, cab-less amp.

Tests are DSP-level, not JSON-level: `tests/test_volum_rig_repair.cpp` derives the
real graph from the repaired rig through `volum::MakeProcessingPlan` (the function
`ProcessBlock` uses) and asserts on `runMainModel` / `runFallback` / `runDualAmp` /
`runIR` / `runPreNam[]`, plus the deferred-swap steps from `VoLumDspStaging.h`.

Ticket 05 bullet coverage:

| Bullet | Case |
| --- | --- |
| Loaded custom MAIN deleted | `Deleting the custom amp on MAIN sends MAIN to the sidebar factory amp` |
| factory scene not overwritten by the custom knobs | `Deleting content that is playing moves the sounding rig, not just the list` (`snapshotOutgoing=false`) |
| SUPPORT-only custom deleted | `Deleting the custom amp used only on SUPPORT drops that lane and leaves MAIN alone`, `Deleting a custom amp that is on both lanes moves both` |
| Unused custom amp deleted, no neighbour jump | `Deleting an unused library row leaves the sounding rig alone` (4 subcases) |
| PRE-1 pedal deleted | `Deleting the pedal in PRE 1 empties that slot and leaves PRE 2 running`, `A pedal loaded in both PRE slots empties both`, `A pedal loaded but bypassed still counts as in use` |
| Active IR deleted | `Deleting the active IR falls back to the baked cab and the convolver goes`, `An IR convolving both lanes is dropped from both`, `An IR on a switched-off SUPPORT lane is not treated as sounding` |
| Selected named preset deleted | `Deleting the selected preset forgets the name and changes nothing else` |
| Sibling RAM | `A sibling keeps its RAM copy until it next needs the deleted id` |
| MIDI slot invalid/red, neighbour intact, reassign, clear | `A MIDI slot whose Sound was deleted goes invalid and its neighbour is untouched` (4 subcases) |
| Pack replace reload, not fallback | `A confirmed Pack replace reloads the lane instead of falling back` (4 subcases), `Replacing a preset does not touch the sounding rig`, `Replacing an unused library row touches nothing` |
| Confirm copy names the in-use case and the destination | `Confirm copy names the item, the in-use case, and the destination` |
| No dry burst on the swap | `The IR teardown a delete triggers waits for the replacement cab`, `A replacement capture that never arrives does not strand the lane` |

## Revert-fail proof

| Revert | Failing cases |
| --- | --- |
| `PlanDelete` returns catalog-only (no repairs, generic copy) — the pre-1.3.0 behaviour | all 13 delete cases: MAIN, SUPPORT-only, both lanes, PRE 1, both PRE slots, bypassed pedal, active IR, both-lane IR, selected preset, sibling next-need, confirm copy, unused-row graph equality |
| `PlanReplace` delegates to `PlanDelete` (replace bounces to the fallback) | `A confirmed Pack replace reloads the lane instead of falling back` (all 4 subcases), `Replacing a preset does not touch the sounding rig`, `Replacing an unused library row touches nothing` |
| Custom-amp match by "any custom lane is set" instead of by id | `Deleting an unused library row leaves the sounding rig alone` |
| Plugin wiring reverted: hardcoded confirm copy, no `_VolumApplyPendingRigRepair()`, `_VolumClearIR(false)` | `Deleting content that is playing moves the sounding rig, not just the list` |
| `_VolumSelectFactoryAmp(mVolumAmpIdx)` with the outgoing snapshot | `Deleting content that is playing moves the sounding rig, not just the list` |
| `RemoveCustomAmp` erases matching MIDI slots (from ticket 01) | `A MIDI slot whose Sound was deleted goes invalid and its neighbour is untouched` |
