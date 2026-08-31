# What Chorus card motif ships with the four modes?

Type: prototype
Status: resolved
Blocked by: 08

## Question

[Where does Chorus sit in POST, and which voices?](08-chorus-placement-and-voices.md) locked modes **CLASSIC / WARPED / CLEAR / ENSEMBLE** and a fourth POST card. Existing PRE/POST cards draw a generated motif (`DrawEffectMotif` in `VoLumTriptychMotifs.h`): Pitch already switches **variant** (Transpose helix vs Octaver chevrons); Delay / Reverb / Tremolo are one drawing each.

The owner wanted generated card art once the voices exist. Decide the Chorus motif language before the Chorus spec draws pixels:

1. **One Chorus glyph, or a variant per mode** (Pitch-style)?
2. **Quiet-slot vs focused-card.** Same drawing at ~20 px thumb and card size (gold/glow already gated on size for other effects)?
3. **Accept or reject** a cheap sketch (HTML or stills), not product C++.

Do not bind DSP or reopen the mode names. Local chorus measurement notes stay off this repo.

Recommend: one Chorus family drawing plus **variant per mode**, matching Pitch; throwaway sketches at card + Quiet sizes for the owner to accept or reject.

## Comments

- Prototype (throwaway, not product C++): [chorus-motif/index.html](../chorus-motif/index.html).
- Round 1 ribbons rejected: too many POST motifs are LFO curves. Quiet-size contract locked (same drawing, gold gated &gt; 40 px). One family glyph first; per-mode variants wait.
- Round 2: Prism / Twin gem / Vesica / Spark. Twin gem was ok; owner asked for more, 80s-themed, and an Opus pass.
- Round 3 80s families declined (option G). Owner asked for a wormhole.
- Round 4 wormhole throats: Throat (twisted hyperboloid), Ring Well, Gate, Lens. Still: [chorus-motif/shot-v6.png](../chorus-motif/shot-v6.png). Owner locked **Throat**. One glyph for all four modes (not Pitch-style variants).

## Answer

**Glyph.** One family drawing for CLASSIC / WARPED / CLEAR / ENSEMBLE — Delay-style, not Pitch-style. Quiet label stays **CHORUS**. Per-mode motif variants are out of this book.

**Family: Throat.** A wireframe wormhole: near mouth (large teal ellipse), far mouth (small gold ellipse + core), latitude ellipses in between, straight generator lines with a twist (~0.55 rad) so the throat warps without plotting an LFO. Same `DrawEffectMotif` at card and Quiet ~20 px; gold/glow only when `min(w,h) > 40`. Bypass uses the existing product veil, not a second glyph.

**Reference (throwaway, not product C++):** [chorus-motif/index.html](../chorus-motif/index.html) with Throat selected; still [chorus-motif/shot-v6.png](../chorus-motif/shot-v6.png). The chorus spec copies this construction into `VoLumTriptychMotifs.h`; it does not invent a fifth POST waveform.

**Rejected on the way.** Detuned-ribbon LFO plots (too many POST motifs are already curves). Round 2 Prism / Vesica / Spark. Round 3 80s props (Chrome Ghost, Sun Slip, Tape Slip, Neon Twin, Reel Pair); Twin gem was “ok” then set aside for a wormhole. Ring Well, Gate, and Lens lost to Throat.

