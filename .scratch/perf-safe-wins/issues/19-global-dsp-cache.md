# 19 Process-global model data cache

Status: resolved
Blocked by: 18
Report: B2 part bg-05 (owner included it despite the lifetime risk). Twenty instances of the same amp each
parse the same captures and keep their own 8-entry cache.

## Paths
- `NeuralAmpModeler/VoLumLoader.inc.cpp`, `NeuralAmpModeler/NeuralAmpModeler.h` (per-instance
  `mVolumDspCache` / `mVolumDspCacheOrder`)
- New small header for the shared cache (e.g. `NeuralAmpModeler/VoLumSharedDspCache.h`) + tests

## Change
Replace the per-instance cache with a process-global LRU of `std::shared_ptr<const nam::dspData>` keyed by
canonical path + file size + mtime, guarded by a mutex, used only by loader threads. `makeModel` copies from
the shared entry as it does today from the local one. Prefetch skips paths already cached or in flight
globally. Size it so N instances do not thrash it (e.g. 16-24 entries; memory stays within a few MB).

## Acceptance
- Unit tests: two "instances" (two loaders) share entries; an entry stays valid while one instance is
  destroyed mid-load; key changes when the file's size or mtime changes.
- `run-tests-win.ps1 -Asan` green for the loader and cache tests.
- Models built from shared entries are bit-identical to directly loaded ones.

## Proof
Sharing test red against the per-instance cache; mtime-change test red against a path-only key.

## Verifier conditions
Never touched by the audio thread; no lock held while building a model; lifetime safe via shared_ptr; memory
bound documented.

## Parked (owner, 2026-09-26 22:00)
Budget: lean plan. Not in this loop; stays specified for a later effort.

## Result (2026-09-29, worker)
- New `VoLumSharedDspCache.h`: process-global LRU of `shared_ptr<const nam::dspData>` keyed by weakly-canonical
  path + size + mtime, one mutex, in-flight set for prefetch claims; evicted entries freed outside the lock.
  16 entries (2 x the per-instance working set of 8); memory bound in the header comment: 16 bundled captures
  = 5.5-6.5 MiB private bytes (Windows), vs up to 8 entries (~2.7 MB) per instance before.
- Loader: per-instance `mVolumDspCache`/`Order`/`kVolumDspCacheMaxEntries` removed; `makeModel` copies the
  shared entry and builds with no lock held; prefetch queue and run skip keys cached or in flight globally.
  `VoLumNamDspData.h` comment-only (stale cap reference). Existing source locks updated to the new names
  (`test_volum_loader_prefetch.cpp`, `test_volum_ui_regressions.cpp` copy-before-build lock kept).
- Tests `test_volum_shared_dsp_cache.cpp` (both descriptors): two loader threads share one parse; rewrite in place
  (mtime, size, content) misses, alternate spelling hits; entry outlives eviction + its loader, 4 racing loaders on a
  cap-2 cache render bit-identical to direct `get_dsp(path)`; prefetch claims; source lock that only
  `_VolumQueueMainPrefetch` / `_VolumLoaderThreadMain` reach it.
- Red: `GlobalDspCache` thread_local (per instance) -> sharing test 3 checks fail; path-only key -> rewrite test
  4 checks fail. Green after.
- ASan (`-Asan -Filter` shared cache, prefetch, FillDspData, warm channel, cached dspData, ResamplingNAM): 12/12.
  Full run-tests-win.ps1 1204/1204. App Release build OK; standalone smoke: MAIN read logged, no failures.
- Load: parse 10-12 ms per capture, hit ~0.12 ms, copy ~0.2 ms, build ~40 ms (unchanged per instance).
- Note: one test build hit C1001 (stale LTCG) and a bogus SIGSEGV; clean int rebuild fixed both.
