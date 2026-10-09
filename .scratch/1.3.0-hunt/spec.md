# 1.3.0 overnight bug hunt: spec

Base: `dev` at a86cfedf plus the stuck-click fix. Last public release: `v1.2.3`
(180 commits to dev). Plan: `c:\Users\SteffenDangmann\.cursor\plans\1.3.0_overnight_bug_hunt_57f3e428.plan.md`.

## Authority

- Fix and merge into `dev`: anything with a clear expected behavior (user guide,
  changelog, spec, or obvious defect: crash, hang, stuck UI, data loss, wrong recall)
  and a regression test proven red without the fix.
- Report only (`findings.md`, disposition `reported`): changes that move golden renders
  or the preset/state format.
- Questions (`questions.md`): UX judgment calls. No behavior change.
- Refactors/optimizations (`opportunities.md`): into dev only when golden renders and
  state format stay bit-identical and no UI behavior changes (audio thread: ASan green
  and before/after timings). Everything else on `feature/post-1.3.0-opt`.

## Finding format (`findings.md`)

```
### F-NN short title
- Source: lane + agent
- Severity: crash | hang | stuck-ui | data-loss | wrong-behavior | cosmetic | docs
- Repro: exact steps or test
- Expected / Actual
- Disposition: open | fixed (commit) | reported | question (Q-NN) | duplicate (F-NN) | not-a-bug
```

## Lanes

See `loop.md` Lanes. Each lane's agent reports back to the coordinator; the coordinator
alone writes `loop.md`, triages findings, and merges.
