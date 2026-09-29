# How does a current build read a future DAW chunk?

Type: grilling
Status: resolved

## Question

The current chunk reader consumes exactly this build’s `kNumParams` param doubles, then the per-amp tail (`Unserialization.cpp`). A later 1.3.0 build that appends Chorus (or any) params will write a longer list. Opening that project in 1.2.1/1.2.2 misaligns the tail — the same shape as the 1.2.0 “every project resets” bug. The id-tail JSON is already skippable; unknown **param doubles** are not.

This cannot fire until 1.3.0 writes extra params, and it has to land **before** that write. Chorus is the first planned param bump.

What skip/version rule does 1.3.0 ship so:

- 1.2.2 can open a future chunk without scrambling the rig (or fail cleanly),
- and 1.3.0 still reads every older chunk?

Recommend: reader-first, writers second. Do not add Chorus params until this lands.

## Answer

**UX.** A shipped 1.2.2 (or 1.2.0/1.2.1) plugin opening a song saved by 1.3.0 keeps the 1.2.2-era sounding rig. Chorus and other 1.3.0-only instance fields are simply absent. Not a scrambled amp/cab, not an empty track.

**Why the usual param bump is out.** 1.2.2 is frozen. It always reads 93 knob doubles, then treats what follows as the per-amp tail. Extra doubles at the front of a 1.3.0 save are the 1.2.0 “project resets / garbage rig” bug again. A new binary block after the id tail also leaves that reader short of the VST3 bypass int. The format 1.2.2 already skips: extra keys inside the existing length-prefixed id-tail JSON.

**Skip rule (what 1.3.0 writes).** Freeze the param prefix at the 1.2.2 list (93 doubles, same names and order) and freeze the byte-counted per-amp tail. New instance state travels only in id-tail JSON. User-settings, named-preset, and content-registry JSON stay additive (unknown keys ignored, no version bump). Once `kNumParams` grows, `SerializeState` must not write every param as prefix doubles.

**EParams.** New automatable knobs (Chorus is the first) are real EParams, appended so indices 0–92 stay put. The DAW can automate them. Their saved values live in id-tail JSON, not as extra prefix doubles. On 1.3.0 load: apply the 93 prefix, then overlay JSON onto the new params and per-amp scenes.

**Reader-first.** For chunk version ≥ 1.2.0, consume those 93 prefix doubles, not this build’s live `kNumParams`. Older version branches unchanged. This lands before any EParam is appended. A 1.2.x save then loads with new knobs at defaults.

**Same channel later.** Per-instance MIDI channel ([What MIDI control does 1.3.0 include?](01-midi-control-scope.md)) is instance state in the id tail, not an EParam and not `volum-settings.json`. Later minors add JSON keys, not prefix doubles or binary-tail bytes, until a future map redraws this.

**Tests are a gate.** Writer prefix stays 93 even when `kNumParams` is larger. A 1.2.2-shaped reader loads extra JSON keys with selection intact and `pos` at size−4. A 1.3.0-shaped reader loads a 1.2.2-shaped chunk. pluginval is not this coverage. JSON key names and Chorus DSP stay on their own tickets.

