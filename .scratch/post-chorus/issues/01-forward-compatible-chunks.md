# Freeze the 93-double DAW-chunk prefix

Status: resolved

## Answer

Landed on `feature/1.3.0` before Chorus EParams. `kVoLumChunkParamPrefixCount = 93`. `SerializeState` writes that many doubles (not `SerializeParams` / live `kNumParams`). The ≥ 1.2.0 reader consumes the same count. Extra instance state stays in id-tail JSON. Tests in `test_volum_state_roundtrip.cpp` pin alignment, extra-prefix derail, and unknown JSON keys.
Blocked by: none

## Goal

Land the skip rule from [How does a current build read a future DAW chunk?](../../release-1.3.0/issues/07-forward-compatible-chunks.md) **before** any Chorus (or other) `EParams` are appended.

## Do this

1. Introduce `kVoLumChunkParamPrefixCount = 93` next to the real `kNumParams` (keep `kNumParams == 93` for this ticket; the constant is what later tickets grow past).
2. `SerializeState`: write exactly those 93 param doubles, not `NParams()` / live `kNumParams`. Same encoding as today's `SerializeParams` (the raw `IParam` values). Freeze the byte-counted per-amp tail; do not add a new binary block.
3. `UnserializeState` for chunk version ≥ 1.2.0: consume those 93 prefix doubles by the **frozen 1.2.2 names/order**, not a live `for (i = 0; i < kNumParams)` loop. Older version branches unchanged.
4. New automatable knobs later overlay from id-tail JSON. This ticket does not add Chorus keys yet — it only makes that overlay possible (unknown JSON keys already ignored).
5. Replace the source pin in `test_volum_ui_regressions.cpp` that requires `for (int i = 0; i < kNumParams; ++i)` on the current-version reader. Pin the freeze constant instead.

## Tests (must fail with this ticket reverted)

- Writer prefix stays 93 even if a helper serializes as if `kNumParams` were larger.
- A 1.2.2-shaped reader (93 doubles, then binary tail, then id-tail JSON) loads extra JSON keys with selection intact and `pos` at `size - 4` (VST3 bypass int).
- A 1.3.0-shaped reader loads a 1.2.2-shaped chunk; new knobs would be defaults (N/A until Chorus exists — pin that missing JSON keys leave existing params alone).
- pluginval is not this coverage.

## Done when

- Tests green on Windows; revert-fail proven for the new freeze cases.
- No Chorus `EParams` yet.
- Changelog: skip (not user-visible). Docs: skip.
