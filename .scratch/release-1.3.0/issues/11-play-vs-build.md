# What is PLAY vs BUILD in 1.3.0?

Type: grilling
Status: resolved

## Question

Locked on [What MIDI control does 1.3.0 include?](01-midi-control-scope.md): the player navigates **sounds** over MIDI; the current UI navigates Amp → amp channel / preset (build). The owner put a PLAY vs BUILD split **in this 1.3.0 book** — not a later FR.

Today there is one view. MIDI config is parked in Gear → Settings so it is not a main-view button. PLAY may relocate that.

Decide:

1. **What each mode is.** PLAY = execution (sounds, MIDI, gig). BUILD = the current editor (sidebar, PRE/AMP/POST, Manage). Or a different cut.
2. **Which formats.** Standalone only, or VST3/AU too? Plugin chrome is already cramped; hosts have their own play/edit ideas.
3. **What chrome each mode shows or hides.** Sidebar, triptych, preset bar, meters, MIDI sound list, …
4. **Where MIDI config lives** once PLAY exists. Ticket 01 put it in Settings as the power-user home; PLAY might own the sound list.

Do not implement a PLAY view on this ticket. If the split is unclear until something is visible, graduate a **prototype** ticket rather than coding the product.

Do not design POST effect reorder here. [Where does Chorus sit in POST, and which voices?](08-chorus-placement-and-voices.md) parked that past 1.3.0: PLAY ships first (mockup if the split needs one); chain order is extra BUILD complexity only if PLAY earns it. POST stays Chorus → Delay → Reverb → Tremolo.

Recommend: standalone + plugin both get the split (MIDI is useless in the DAW if PLAY is APP-only). PLAY is a sound-centric surface; BUILD keeps Amp → preset. MIDI assigned-slot list lives in PLAY, channel + port stay out of the main chrome (channel in Settings, port in standalone Preferences).

## Comments

- From [Where does Chorus sit in POST, and which voices?](08-chorus-placement-and-voices.md): 1.3.0 PLAY is still in this book (prototype if needed). Putting “more complexity” such as POST reorder into BUILD is later, not a 1.3.0 PLAY feature.
- From [Is the update notifier in this 1.3.0 book?](10-update-notifier.md): the gear badge rides the settings control. PLAY and BUILD both need that control visible, or the notice has nowhere to land. MIDI channel stays in Settings; the sound list may move.
- Mockup: [Does the PLAY surface fit at 900×600?](12-play-surface-mockup.md).
- From [Does 1.3.0 ship a factory preset bank?](13-factory-preset-bank.md): PLAY Add / reassign is one Sound list (Factory/User, preset name + amp), not amp-then-preset. Empty PLAY until Add still holds; the MIDI map is not seeded.

## Answer

**Cut.** Two navigations, one sounding rig, same window, toggle. **PLAY** walks **Sounds** (amp + named preset, the same targets Program Change recalls). **BUILD** is today’s editor (sidebar Amp → named Preset, PRE/AMP/POST, Manage). Not a lock-down of BUILD and not a recall-only skin.

**Formats.** Standalone + VST3 + AU. 900×600 is cramped; PLAY hides BUILD chrome instead of dropping the mode in the plugin.

**PLAY list.** Assigned MIDI sound-map slots only, **Program Change order** (0–127). Empty holes omitted; **invalid** slots stay numbered and visible. No drag-reorder in this book (rewriting PC numbers is a floorboard footgun). No independent setlist order (MIDI-as-badges) — extra list, parked.

**PLAY can change.** Sound recall + **PRE/POST bypass** on the existing cards (Pitch, Comp, NAM 1/2, Chorus, Delay, Reverb, Tremolo). Not the knob row, not cab/channel, not Manage, not the Dual Amp graph (Dual Amp still rides inside the Sound). Bypass is the same live edit as BUILD: marks the named preset `(unsaved)`, does not overwrite until save; the next Sound recall restores the snapshot. Switching BUILD → PLAY **never** recalls. Highlight the **last-recalled** slot if that Sound is still the origin (dirty is OK); otherwise no row selected. Two slots, same Sound: last recalled PC wins.

**MIDI assignment.** PLAY owns the list + Add / reassign / clear (Default stays off MIDI). Add/reassign picker shape is [Does 1.3.0 ship a factory preset bank?](13-factory-preset-bank.md) — one Sound list, not amp-then-preset. **Channel** stays in Settings (per instance, Omni default). Standalone **port** stays in Preferences. Pack import/export stays in Settings. Gear stays in both modes for the update badge.

**Chrome.** PLAY: sound list + bypass chips + meters + tuner + metronome + gear + PLAY/BUILD toggle. Hide sidebar, hero, triptych, knob row, preset bar, Manage. BUILD: today’s editor plus the same toggle and gear. Labels on the toggle: **PLAY** and **BUILD**.

**Which mode opens.** Remember last mode **per instance** (plugin: DAW project; standalone: settings). First launch / never-toggled → **BUILD** so an empty map is not the first screen.

**Visual.** Layout fidelity is [Does the PLAY surface fit at 900×600?](12-play-surface-mockup.md), not this ticket. Empty PLAY until Add; factory bank is [Does 1.3.0 ship a factory preset bank?](13-factory-preset-bank.md).
