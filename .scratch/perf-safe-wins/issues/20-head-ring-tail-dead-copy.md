# 20 A2 head ring: drop the dead tail-mirror refresh (NeuralAmpModelerCore)

Status: resolved
Blocked by: none (follows 05)
Source: ticket 05 follow-up ("head-ring tail copy is dead work").

## Paths
- Submodule `NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp` (head ring `refresh_tail_mirror` call, ~L383-384
  at 2d31bbf; re-find by name), fork branch `volum/perf-safe-wins` (pushed over the explicit HTTPS URL in spec.md,
  no force-push).
- Parent: submodule pointer bump, `NeuralAmpModeler/tests/test_volum_nam_exact.cpp` (reuse the existing locks).

## Change
Ticket 05 established that head reads mask per column and never touch the head ring's tail mirror. Stop
refreshing that mirror (keep the layer rings' refresh, which their reads depend on). If the head tail storage
then has no reader at all, leave its allocation alone in this ticket (memory is out of scope).

## Acceptance
- NAM exact-output locks (`test_volum_nam_exact.cpp`, all block-size cases incl. 512 / 2048 / varied) and the
  realtime-budget test stay green with no hash touched.
- A source or behaviour check fails if the head refresh comes back (optional if a clean check is not possible;
  say so in the Result).

## Proof
Show the head tail is never read: perturb it (e.g. fill the head tail with NaN after each refresh in a scratch
build) and the exact locks stay green; perturb a layer tail the same way and they fail.

## Verifier conditions
Only the head ring's refresh removed; layer rings untouched; submodule pointer on the pushed fork branch.

## Result (2026-09-29, worker)
- NAMCore b06165a on volum/perf-safe-wins (parent of 9c4d8ad; pushed over HTTPS, ls-remote shows b06165a):
  `_head_ring_write` drops its two `refresh_tail_mirror` calls (and the unused `mbs`); layer `_ring_write`
  untouched; head tail allocation left alone.
- Proof: head tail filled with NaN every block (no refresh) -> 11/11 exact cases + 17 golden/budget cases green;
  layer tail filled with NaN after its refresh -> all 11 exact cases fail (hash + finite). Green on the real change.
- Source check added to `test_volum_nam_exact.cpp`: fails if `_head_ring_write` calls `refresh_tail_mirror`
  again (red against 9c4d8ad), and asserts the layer write still does.
- Full run-tests-win.ps1: 1198/1198, no hash touched.
- CPU: not measurable. Soldano 10 s renders, median of 7: before FULL/64 874-1025 ms, after 975-1000 ms; run-to-run
  drift (10-15%) dwarfs a few KB of memcpy on every other block. Evidence: `evidence/20/`.
