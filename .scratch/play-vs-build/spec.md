# PLAY vs BUILD (and the factory preset bank)

Locked map: `.scratch/release-1.3.0/map.md`. Do not reopen product calls.

## Outcome

Standalone + VST3 + AU: PLAY / BUILD toggle. PLAY is Opus D at 900×600. One Factory named Preset per factory amp (read-only). Empty PLAY until Add. First launch BUILD.

## Locked decisions (do not invent)

- [What is PLAY vs BUILD in 1.3.0?](../release-1.3.0/issues/11-play-vs-build.md)
- [Does the PLAY surface fit at 900×600?](../release-1.3.0/issues/12-play-surface-mockup.md) — **Opus D**. Mock: `.scratch/release-1.3.0/play-proto/opus2/index.html?variant=D`
- [Does 1.3.0 ship a factory preset bank?](../release-1.3.0/issues/13-factory-preset-bank.md)

MIDI decoder/handoff is midi-control. This spec **consumes** `midiSoundMap` and `VolumRecallSound`. If those are missing, implement against the same API names in the midi-control spec rather than inventing a second map.

## Factory bank (ticket 01, first)

Exactly one Factory named Preset per factory amp (15). Custom amps: no Factory row. Snapshot = that amp's **current factory-default `VoLumAmpSettings`** (owner may re-voice later via upgrade). Display name: **"Ready"** (amp name is secondary in PLAY Add; BUILD menu is already per-amp). Stable id: `factory:<idx>:v1`. Shipped, not content-library items. Pack Reset cannot strip them. Manage lists User only. Dirty Factory recall: Save writes a User copy; Factory row unchanged.

BUILD preset menu: Default (factory settings) pinned **above** Factory and User. Empty User (or Factory on a custom amp) hides the header.

PLAY Add / reassign: one Sound list, Factory vs User sections; Preset name primary, amp name secondary; Default omitted; scrolls; no cap.

Do not seed the MIDI map.

## PLAY chrome (Opus D)

Same window, toggle **PLAY** | **BUILD**. Remember last mode **per instance** (plugin: DAW id-tail `uiMode` = `"play"`|`"build"`; standalone: settings). Missing key → BUILD.

**PLAY shows:** centre live recalled amp art with lighting-breathe (not bounce); right PC thumb rail in Program Change order (invalid stays numbered); rail scrolls; LIVE thumb sticky; Add pinned under the list; empty state centred "No Sounds assigned" + Add; bottom PRE/POST stomps (Pitch, Comp, NAM 1/2, Chorus, Delay, Reverb, Tremolo) + meters; header PLAY/BUILD left of tuner / metronome / gear.

**PLAY hides:** sidebar, hero, triptych, knob row, preset bar, Manage.

**PLAY can change:** Sound recall + PRE/POST **bypass** on those stomps. Not knobs, not cab/channel, not Manage, not Dual Amp graph (NAM 2 stomp still silences SUPPORT). Bypass marks named preset `(unsaved)` like BUILD; next Sound recall restores the snapshot. Switching BUILD → PLAY **never** recalls. Highlight last-recalled slot if that Sound is still the origin (dirty OK); else no row selected. Two slots, same Sound: last recalled PC wins.

Channel stays in Settings. Port stays in Preferences. Pack stays in Settings. Gear in both modes (update badge).

Dual Amp on/off in PLAY is parked. Jewel pedals from proto B are optional polish, not a lock.

Iterate standalone until it matches variant D. Do not ship GPT/Grok layouts.

## Tests (gate)

Factory: 15 ids present; Manage cannot delete them; Save-on-dirty-Factory mints User; PLAY Add lists Factory Sounds without seeding MIDI.

PLAY: mode persistence; empty vs assigned vs invalid; recall does not run on toggle; bypass dirty; PC order.

Prove revert-fail. Windows suite. Launch `run-app-win.ps1` and judge layout.

## Docs

Changelog: PLAY/BUILD, factory Ready presets, empty PLAY until Add. EN/DE. Refresh main/toolbar screenshots and add a PLAY shot. Update screenshot recipes (toggle click target).
