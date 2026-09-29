# Where does Chorus sit in POST, and which voices?

Type: grilling
Status: resolved
Blocked by: 09

## Question

POST today is Delay → Reverb → Tremolo (`_VolumProcessPostChain`). Tremolo is last so it modulates the wet wash. There is no Chorus param, DSP, or card. The POST triptych is hard-coded for three slots. User ask: [Chorus effect](https://github.com/guitarlum/VoLum/issues/25) (standalone cannot host a separate chorus).

The owner wants Chorus as the last effect, 2–3 modes, and a generated motif once voices exist.

Decide:

1. **Order.** Fourth stage after Tremolo (Chorus hears the tremolo’d wash), or insert before Tremolo (Tremolo stays last)? “Last” in the original ask may mean last **added**, not last **in the chain**.
2. **Voices.** Pick 2–3 mode names and behaviours from [Which chorus voice families fit VoLum POST?](09-chorus-voice-families.md). Prefer few knobs, matching Delay/Tremolo.
3. **Params / state.** New `EParams` wait on [How does a current build read a future DAW chunk?](07-forward-compatible-chunks.md).

Recommend: wait for ticket 09 before naming modes. For order, prefer insert **before** Tremolo unless the owner’s “last” is literal — a chorus after a harmonic tremolo is an unusual guitar chain.

## Answer

**Chain.** Chorus → Delay → Reverb → Tremolo. Fixed. Tremolo still last, still pulses the whole wash. “Last effect” in the original ask meant last **added**, then the owner put Chorus first as guitar best practice. Analog delay `Wear` can still wiggle the repeats; Chorus + Analog delay is stacked on purpose.

**Chrome.** Fourth POST card. Visual order matches the bus. PRE already fits four cards in the same expanded strip; POST goes from three equal peers to four. Tab/arrow cycling reaches all four. No drag-to-reorder in this book — see Out of scope on the map; [What is PLAY vs BUILD in 1.3.0?](11-play-vs-build.md) does not grow a chain editor.

**Knobs.** Shared row from [Which chorus voice families fit VoLum POST?](09-chorus-voice-families.md): **RATE · DEPTH · TONE · WIDTH · MIX**. No tempo sync (Delay and Tremolo already own that). No extra per-mode knob. WIDTH always spreads the wet (a stereoizer on historically-mono flavors; native L/R offset on stereo flavors). Same five knobs on every mode; meaning can shift per mode the way Delay’s last knob is Grit / Wear / Bloom.

**Modes (four families, not 09’s trio).** Compact analog choruses of the same family sound alike, so this is four *diverse* modes, not four analog twins. Player-facing names are VoLum **modes** (same noun as Optical / Hall / Digital), never a donor:

| Mode | Family |
| --- | --- |
| **CLASSIC** | Analog bucket-brigade chorus: triangle LFO, short modulated delay, no extra page. |
| **WARPED** | Dark vinyl / tape warble. RATE = warble speed, DEPTH = amount, TONE = darkness. MIX blends chorus → vibrato (100% wet). |
| **CLEAR** | Clean stereo interpolated taps, no feedback. Kept from ticket 09. |
| **ENSEMBLE** | Three phase-spaced taps, denser / less cyclic. Kept from ticket 09. |

**Default.** Mode **WARPED**. The card itself still ships **bypassed**, like Delay / Reverb / Tremolo.

**What 09 still owns / what it does not.** Knob row, CLEAR, and ENSEMBLE stand. 09’s **Warm** and default **Clear** do not ship — WARPED is a different family than Warm (vinyl-warble, not triangle-delay + feedback), and the factory mode is WARPED.

**POST contract (inherited).** Per-amp scene, named presets, POST lock + Store, bypass-edge `Reset()`, per-mode snapshots like Delay/Tremolo. New automatable knobs are real `EParams`; saved values live in id-tail JSON, not extra prefix doubles ([How does a current build read a future DAW chunk?](07-forward-compatible-chunks.md)).

**Algorithm binding is not this ticket.** Each mode ships a copyable algorithm with high confidence (published signal flow, compatible-license code, expired-or-citable patents). A literature vibe-family that cannot be nailed does not win. Binding constants and any local measurement notes stay **off this repo** (map Notes). The chorus spec’s first job is that binding; this ticket only locks roles, names, knobs, and order.

**Motif.** Generated card art once the modes exist — still fog, not this ticket.

**Tests are a gate.** DSP (every mode, MIX=0 passthrough, bounded, no NaN), UI (four POST cards + four-mode picker), state (per-amp + lock + chunk JSON). pluginval is not this coverage.
