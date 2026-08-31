# What happens when we delete content that is currently playing?

Type: grilling
Status: resolved
Blocked by: 04

## Question

Deleting a custom amp that is loaded updates the UI toward a factory amp and clears the custom index, but does not reload/clear the live model — DSP can keep the dead capture. Pedal delete updates the registry only; a live PRE capture can keep playing. Pack import/delete and MIDI preset recall make this more common.

This wants the same removal transaction as [How does the content library survive two writers?](04-two-writer-library.md) (identity, not row position; graph swap like cab-source changes). That ticket is resolved: catalog mutations are by stable id; the sounding rig lives on the instance, not in shared `customScenes`. This ticket is only what DSP/UI do when the deleted or Pack-replaced id is currently sounding.

What does 1.3.0 guarantee when the user deletes (or a Pack import replaces) the amp, IR, pedal, or preset that is currently sounding?

## Answer

**Language.** The **sounding rig** is this instance’s live graph. A **library id** is one content-library item (custom amp, IR, pedal, named preset). Factory amps are shipped; they cannot be deleted and do not need UUIDs for this (YAGNI). **Sound** remains the MIDI recall target.

**This instance, delete of a graph capture.** If this VoLum deletes a library id its sounding rig is using, that slot reverts *now* to an available default. UI and DSP agree. Staging is the same deferred family as cab ↔ IR (no dry burst, no ghost capture behind factory chrome). Confirm copy names the in-use case and the destination. Unused library rows do not touch the rig. Identity, not row: deleting a neighbour must not retarget this slot.

| Deleted id in use | Available default |
| --- | --- |
| Custom amp on MAIN | The factory amp already in the sidebar, as if clicked (this instance’s last knobs on that amp, **not** Default) |
| Dual Amp | Only the lane that used that id: MAIN reverts and/or SUPPORT drops; the other lane stays |
| Pedal in a PRE slot | That slot empty; live PRE model dropped |
| IR | Baked cab of the current amp (today’s path, kept) |

**Named preset deleted while selected.** Forget the name (preset bar / dirty tracking). Keep the sound. Deleting “My Lead” does not rewind PRE/AMP/POST.

**Sibling instances.** A catalog delete does not rewrite another VoLum’s sounding rig. They keep RAM until *they* next need that id (pick it, sound recall, capture reload). Then it is gone: same revert as above, or MIDI ignore. Files may already be deleted; the RAM copy is allowed until that next need.

**MIDI sound map.** Slot numbers stay. A slot whose Sound no longer exists is **invalid** (red in the allocation list) until the player reassigns or clears it. The neighbour does not inherit that number. Program Change on an invalid or unassigned slot still ignores and stays put ([What MIDI control does 1.3.0 include?](01-midi-control-scope.md)).

**Pack replace is not a delete.** Import/export *scope* (preview add vs replace; export ALL vs selected amps+presets) lives on [What is in a Pack, and what happens on conflict?](06-pack-contents-and-conflict.md). After they confirm an import that replaces a currently-playing library id, **reload** the new capture on this instance — do not bounce to the delete-fallback.

**Tests are a gate** (owner: this is error-prone). A test that only checks the library JSON is not enough. At least:

- Loaded custom MAIN deleted → DSP + chrome are that factory amp’s instance scene; dead capture is gone; factory scene was not overwritten by the custom knobs.
- SUPPORT-only custom deleted → SUPPORT dropped, MAIN unchanged.
- Unused custom amp deleted → sounding rig unchanged; selection does not jump to a neighbour row.
- PRE-1 pedal deleted → PRE-1 empty, PRE-2 unchanged; live PRE is not the deleted capture.
- Active IR deleted → baked cab, convolver gone.
- Selected named preset deleted → bar forgets, DSP unchanged.
- Sibling still playing the deleted amp → this instance reverted, sibling RAM unchanged until it next needs the id; then revert or ignore, no crash.
- MIDI slot 5 → deleted preset or amp: slot 5 invalid/red, slot 6 unchanged; PC 5 stays put; reassign works; clear → unassigned.
- Confirmed Pack replace of a currently-playing amp/IR/pedal → new payload loaded, not factory/empty/baked-cab fallback.
- Confirm dialog names the in-use case and the destination amp/slot.
