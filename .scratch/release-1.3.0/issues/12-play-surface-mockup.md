# Does the PLAY surface fit at 900×600?

Type: prototype
Status: resolved
Blocked by: 11

## Question

[What is PLAY vs BUILD in 1.3.0?](11-play-vs-build.md) locked the brief. This ticket raises fidelity: a cheap mockup at the shipping size (900×600 — standalone and plugin share it) so the owner can accept or reject the layout.

Do not implement product PLAY. An HTML preview is enough (same idea as throwaway art previews). Do not reopen the navigation cut, formats, dirty model, MIDI assignment home, or mode names.

Show, at 900×600:

1. **Assigned-slot list** in Program Change order: a few valid Sounds, one **invalid** slot still numbered, and an **empty** PLAY (no slots) with Add.
2. **PRE/POST bypass chips**: Pitch, Comp, NAM 1/2, Chorus, Delay, Reverb, Tremolo — not the BUILD triptych.
3. **Kept chrome**: meters, tuner, metronome, gear, PLAY/BUILD toggle.
4. **Hidden**: amp sidebar, hero, triptych, knob row, preset bar, Manage.

Ask: does this ship as PLAY chrome, or what changes (chip overflow, empty state, invalid row, toggle placement)?

## Comments

- Prototype (throwaway, not product): Opus won. Round 2 six variants in [opus2/](../play-proto/opus2/index.html). Owner locked **D (thumb rail)** as the structure. Overflow rail is in (Long bank scene): scroll, sticky LIVE thumb, pinned Add. Dual Amp stays a Sound property in 1.3.0 PLAY (ticket 11) — NAM 2 stomp already silences SUPPORT; a PLAY Dual Amp on/off is parked (nice, not a must).
- Hero PNGs are BUILD crops: `MAIN`/`SUPPORT` tags, captions and corner ticks sit near the edges. Framed layouts hide that with zoom; edge-to-edge (F) has to crop past it.

## Answer

**Yes — D ships as the PLAY chrome.** Reference: [opus2/index.html?variant=D](../play-proto/opus2/index.html?variant=D). 900×600 holds it.

**Layout.** Centre: live recalled amp art, lighting-breathe (not bounce). Right: named PC thumbs in Program Change order (invalid stays numbered). Bottom: PRE/POST stomps + meters. Header: PLAY/BUILD left of tuner / metronome / gear. Hide BUILD chrome as ticket 11 already locked.

**Bank.** The rail scrolls a long assigned list. LIVE thumb stays visible (sticky). Add is pinned under the list, not a 30px stub. Empty PLAY is still centred “No Sounds assigned” + Add.

**Not PLAY in 1.3.0.** Dual Amp graph / on-off (it rides inside the Sound; NAM 2 bypass mutes SUPPORT). Jewel pedals from variant B are optional polish, not a lock. GPT/Grok directions are out.

Throwaway HTML is not product code. Product PLAY waits for a conductor spec after this map.
