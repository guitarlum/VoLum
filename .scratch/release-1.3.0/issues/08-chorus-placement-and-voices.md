# Where does Chorus sit in POST, and which voices?

Type: grilling
Status: open
Blocked by: 09

## Question

POST today is Delay → Reverb → Tremolo (`_VolumProcessPostChain`). Tremolo is last so it modulates the wet wash. There is no Chorus param, DSP, or card. The POST triptych is hard-coded for three slots. User ask: [Chorus effect](https://github.com/guitarlum/VoLum/issues/25) (standalone cannot host a separate chorus).

The owner wants Chorus as the last effect, 2–3 modes, and a generated motif once voices exist.

Decide:

1. **Order.** Fourth stage after Tremolo (Chorus hears the tremolo’d wash), or insert before Tremolo (Tremolo stays last)? “Last” in the original ask may mean last **added**, not last **in the chain**.
2. **Voices.** Pick 2–3 mode names and behaviours from [Which chorus voice families fit VoLum POST?](09-chorus-voice-families.md). Prefer few knobs, matching Delay/Tremolo.
3. **Params / state.** New `EParams` wait on [How does a current build read a future DAW chunk?](07-forward-compatible-chunks.md).

Recommend: wait for ticket 09 before naming modes. For order, prefer insert **before** Tremolo unless the owner’s “last” is literal — a chorus after a harmonic tremolo is an unusual guitar chain.
