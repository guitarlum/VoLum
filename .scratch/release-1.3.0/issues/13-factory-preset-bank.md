# Does 1.3.0 ship a factory preset bank?

Type: grilling
Status: resolved

## Question

Cold start today: each factory amp’s preset list is only **Default (factory settings)** — a reset, not a named **Preset**, and not a **Sound**. [What MIDI control does 1.3.0 include?](01-midi-control-scope.md) keeps Default off MIDI. [What is PLAY vs BUILD in 1.3.0?](11-play-vs-build.md) therefore has empty PLAY until Add, and first launch is BUILD so that empty map is not the first screen.

Prior art: `backlog/F5-presets-full-rig.md` remainder. Individual preset *files* already folded into Pack ([What is in a Pack, and what happens on conflict?](06-pack-contents-and-conflict.md)) — do not invent a second format here. Marketplace of factory-amp presets stays out of this book.

Decide, for this minor:

1. **In or out.** Does 1.3.0 ship a curated factory named-preset bank (cold-start demos), or park it? Parking means Add has no legal target until the player saves a named preset.
2. **If in: mutability.** Read-only shipped bank (upgrade can refresh; Manage cannot overwrite/rename/delete those rows) vs seed into the user bank on first run (player can destroy them; upgrade cannot refresh).
3. **If in: PLAY / MIDI.** Bank only (BUILD lists + Add targets), or also seed the MIDI sound map so PLAY is not empty on a machine that has never Assigned? Ticket 11 locked first-launch BUILD; do not reopen that. Seeding the map would change the empty-PLAY story.
4. **If in: size.** A handful of Sounds across a few amps, vs one named preset on every factory amp (15), vs something else. Who records the snapshots is a follow-on task, not this ticket.

Recommend: **in, small, read-only, no MIDI seed.** Without some named presets, the MIDI / PLAY headlines cannot be used until the player saves. Keep Default as the reset row. Do not auto-fill the sound map — empty PLAY until Add still holds; Add immediately has factory Sounds to pick. Size: a handful (not 15×N). Authoring is a later task ticket if this stays in.

Do not implement presets or PLAY on this ticket.

## Answer

**In 1.3.0.** Exactly **one Factory named Preset per factory amp** (15). Custom amps get no Factory row. The owner records the snapshots; the conductor freeze into the install is implementation, not another map ticket.

**Shipped graph only.** A Factory Sound may use a factory amp, shipped cab/channel, shipped PRE captures, POST, and Dual Amp only with a factory partner. No custom amp, IR, or pedal. Those stay User presets (Pack SHARE).

**Factory vs User.** BUILD preset menu and PLAY Add/reassign use subsections **Factory** and **User**. Default (factory settings) stays the pinned reset **above** both in BUILD — not a Preset, not inside Factory, not a Sound. Empty User (or Factory on a custom amp) hides the header.

**Read-only Factory.** Manage lists **User only**. No overwrite, rename, or delete on Factory. Dirty Factory recall: Save writes a **User copy**; the Factory row is unchanged. An upgrade may refresh Factory snapshots. Factory presets are shipped, not content-library items — Pack Reset cannot strip them.

**Identity.** PLAY and MIDI point at a Factory Preset by a stable shipped id, not the row title, so a later refresh can rename or re-voice the snapshot without invalidating assigned slots.

**MIDI / PLAY.** Do **not** auto-fill the MIDI sound map. PLAY stays empty until Add. First launch stays BUILD ([What is PLAY vs BUILD in 1.3.0?](11-play-vs-build.md)).

**PLAY Add / reassign** (supersedes ticket 11’s amp-then-preset). One **Sound** list: Factory vs User sections; each row is Preset name primary, amp name secondary. Default is omitted. No cap; the list scrolls. The assigned-slot rail is still Program Change order.

**Not this ticket.** Individual preset files stay Pack. Marketplace of factory-amp presets stays out of this book. Snapshot authoring is the owner, then a conductor spec.
