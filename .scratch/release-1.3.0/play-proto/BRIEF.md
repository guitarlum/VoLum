# PLAY surface prototype brief

Throwaway. Question: **does PLAY fit at 900×600, and which structure feels like VoLum rather than a settings menu?**

Do not implement product C++ / plugin UI. One self-contained HTML file. Double-click to open.

Do not save tokens, time, or polish. Iterate until the 900×600 frame looks like it could sit next to today’s BUILD screenshots.

## Rejected

The first draft was a spreadsheet of PC rows + a chip dock. Owner: “looks like an enterprise settings menu”, “include the amp art”, “sexy, modern, appealing”. Do not ship a list-of-rows as the primary surface.

## Locked (do not reopen)

- PLAY = assigned MIDI Sounds in Program Change order + PRE/POST bypass. BUILD = today’s editor.
- Toggle labels: **PLAY** / **BUILD**. Placement: header-right, **left of** tuner / metronome / gear.
- Empty PLAY: centered “No Sounds assigned” + Add. Add also available when slots exist.
- Invalid slot: stays numbered, labeled invalid, not recallable. Neighbour does not inherit the number.
- Hide in PLAY: amp sidebar, hero-as-BUILD-triptych, knob row, preset bar, Manage.
- Keep: meters, tuner, metronome, gear (update badge on gear).
- 8 bypasses: PITCH, COMP, NAM 1, NAM 2, CHORUS, DELAY, REVRB, TREM.
- Shipping size: **exactly 900×600** plugin frame (standalone and plugin share it).

## Gig loop (this is the product)

PLAY has two jobs on one sounding rig. If a still cannot answer “how do I pick a Sound?” and “how do I stomp PRE/POST?”, it is rejected.

1. **Pick a Sound** by clicking its amp art (or an obvious named/art slot in PC order). That is Sound recall — the same target MIDI Program Change hits. Last-recalled is unmistakable. Invalid slots stay numbered and do nothing.
2. **Stomp PRE/POST** on the *live* rig: PITCH, COMP, NAM 1, NAM 2, CHORUS, DELAY, REVRB, TREM. These are the existing cards, not a settings toolbar and not the BUILD triptych. Stomping marks the last-recalled named preset `(unsaved)`.
3. **Recall another Sound** → that Sound’s bypass snapshot returns (the previous stomps are gone). Demo this. Dual Amp is not edited in PLAY; it rides inside the Sound.

Outside the 900×600 frame, a 3-step walkthrough is required: Recall 00 → Stomp CHORUS → Recall 01, with a live state line (`recalled PC`, `dirty`, `chorus on/off`, `dual yes/no`).

## Stereo / Dual Amp (required)

One demo Sound **is** Dual Amp, because PLAY must show how stereo looks, not only how mono looks.

- **PC 00** THC Sunset · Crunch — **mono**. One hero art (`hero-thc-sunset.png`). NAM 2 dim/off. One OUT meter (or L only).
- **PC 01** Marshall 2204 · Lead — **Dual Amp**. MAIN art `hero-marshall-2204.png` + SUPPORT art `hero-jmp-2203.png`, labeled MAIN / SUPPORT like BUILD. NAM 2 is live. **OUT L** and **OUT R** meters. Recalling this Sound is how stereo appears — not a Dual Amp editor.
- **PC 04** invalid.
- **PC 07** Orange ORS100 · Clean — mono (`hero-jmp-2203.png` is JMP art; use it only as a stand-in if no ORS crop exists, and label the Sound correctly).
- **PC 12** Monomyth Skele1 · Room — mono custom.

When Dual Amp is recalled, both art layers **light** from the IN envelope (SUPPORT may lag a few degrees so stereo reads). When mono, SUPPORT is gone.

## Motion (required, cheap, **visible**) — lighting, not bounce

Owner: the cabinet should **breathe through illumination** (lamps coming up and down with the playing). It must **not** translate or scale like a boombox jumping to the beat. Art stays planted.

- Fake **IN envelope** 0–1 on `requestAnimationFrame` (swell + jitter, silence → 0). IN meter **and** OUT meter(s) paint from it.
- Recalled amp art **does not move**. Drive **brightness / lamp wash / corona** from `--in`:
  - `filter: brightness(...)` (and optional slight saturate) on the live art — quiet is dim, loud is lit.
  - A gold (MAIN) / teal (SUPPORT) radial wash, `mix-blend-mode: soft-light` or overlay, opacity tied to `--in`.
  - Frame **corona** box-shadow opacity/spread tied to `--in`.
  - No translate, no scale-as-meter, no bounce. No `blur`, no canvas particles, no fractal redraw.
- Dual Amp: both heroes **light** from the envelope (SUPPORT may lag a few degrees so stereo reads). OUT L/R can be slightly different heights.
- `will-change: filter` on live art only. Pause rAF when the tab is hidden.
- `?freeze=0.15` and `?freeze=0.85` must produce **visibly different illumination** in screenshots (dim vs lit — orchestrator compares brightness, not position). Default live (no freeze) must be obviously breathing in the browser.

This is a gimmick. In product it samples the existing IN peak, not an FFT.

## Look like VoLum (do not invent a new product)

Today’s BUILD (Read these PNGs with the image tool before drawing):

- `c:\dev\VoLum\.scratch\release-1.3.0\play-proto\ref\build-amp-thc.png`
- `c:\dev\VoLum\.scratch\release-1.3.0\play-proto\ref\build-dual-marshall.png`
- `c:\dev\VoLum\.scratch\release-1.3.0\play-proto\ref\build-post-ors.png`
- `c:\dev\VoLum\.scratch\release-1.3.0\play-proto\ref\build-pre-slo.png`

Palette: charcoal `rgb(17,17,24)`, gold `rgb(252,222,145)`, teal `rgb(91,196,196)`, cream, brass selection, Art Deco corner ticks. Fonts: **Poiret One**, **Josefin Sans**, **Michroma**.

Amp art to use as `<img src="../art/...">` (not fake CSS fractals):

- `../art/hero-thc-sunset.png`
- `../art/hero-marshall-2204.png`
- `../art/hero-jmp-2203.png`
- `../art/card-delay.png`, `card-reverb.png`, `card-trem.png`, `card-pre-strip.png`

Demo Sounds are listed under **Stereo / Dual Amp**. Do not invent a sixth Sound.

## Owner direction (round 2 — Opus only, six variants)

- **D is locked** as PLAY chrome (`opus2/index.html?variant=D`). Overflow rail: Long bank scene. Dual Amp is a Sound property, not a PLAY toggle. GPT/Grok out. Do not overwrite `opus/index.html`.

New work lives in `opus2/index.html`, `?variant=A|B|C|D|E|F`.

## Prototype skill (UI branch)

Six **structurally different** variants in ONE html file. All six obey art-middle + pedals-bottom. Difference is how you pick a Sound, how Dual Amp splits the middle, and how much poster vs stage you take from round-1 A/B. Floating bar **outside** the 900×600: ← label → (six names), Gig / Empty, walkthrough, state line.

## Self-iterate (mandatory — **until happy for each variant**)

Do not oneshot. Do not do three loops for the whole set and call it done. **Finish one variant before starting the next.**

For **each** of A–F:

1. Write / update that variant.
2. Screenshot with `shot.ps1` (`-Html ...\opus2\index.html -Variant X -Freeze 0.75 -Out ...\shots\opus2-X-vN.png`). If Edge Access Denied, absolute `--screenshot=` under `$env:TEMP`, then copy.
3. **Read the PNG** with the image Read tool. Dual Amp `freeze=0.15` vs `0.85` must look dim vs lit. Hunt: cannot pick a Sound, stomps look like a settings toolbar, no Dual Amp pair, lighting invisible, art bouncing, spreadsheet residue, stomps not at the bottom, art not the middle.
4. Fix. Repeat **until you would put that still next to BUILD without flinching.** Minimum **three** screenshot-and-fix loops **per variant**.

Do not recommend shipping. The owner picks pieces.
