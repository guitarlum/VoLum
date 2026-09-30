# 18 Loader prefetch without building models; cap to cache size

Status: resolved
Blocked by: 17
Report: B2 (bg-03, bg-04). Each sibling prefetch builds and prewarms two full NAM models and discards both;
each load queues ~15 siblings into an 8-entry cache, evicting the model just loaded.

## Paths
- `NeuralAmpModeler/VoLumLoader.inc.cpp` (`makeModel` ~L285-297, `storeCache` ~L273-281, prefetch queueing
  ~L333-369)
- `NeuralAmpModeler/NeuralAmpModeler.h` (`kVolumDspCacheMaxEntries` ~L722)
- Loader tests (new or extended)

## Change
- Prefetch: read and parse the `.nam` and fill `nam::dspData` directly (version, architecture, config,
  metadata, weights, expected sample rate), mirroring `NAM/get_dsp.cpp` without building a DSP. Keep the file
  exists check, the `ifstream >> j` parse and the version check.
- Cache miss for a real load: fill `dspData` the same way, store it, then build once with
  `nam::get_dsp(dspData&)`, the path a cache hit already takes.
- Queue at most `kVolumDspCacheMaxEntries - 1` prefetches, ordered by the likely next pick (same speaker
  with adjacent channels first, then the same channel on other cabs), so the loaded model is never evicted.

## Acceptance
- A model built via the new path is bit-identical to one built via `get_dsp(path, conf)` (exact lock from
  ticket 04 style render, or compare outputs of both builders).
- Hit/miss counter test: after loading channel X, re-selecting X and its neighbors hits the cache.
- Loader-thread time per amp switch logged before/after.

## Proof
Builder-equivalence red against a perturbed field (e.g. dropped expected sample rate); cache test red against
the old queueing (loaded model evicted).

## Verifier conditions
No field of `dspData` lost (metadata / loudness / sample rate); loader thread only; no change to what the
audio thread receives.
