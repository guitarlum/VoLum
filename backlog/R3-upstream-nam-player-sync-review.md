# R3: Review upstream NAM Player repo for changes worth adopting

## Problem

VoLum is a heavily customized fork of NeuralAmpModelerPlugin (the upstream "NAM
Player" / standalone+plugin repo) and depends on NeuralAmpModelerCore. Upstream
keeps moving — notably the whole A2 / `SlimmableContainer` work, trainer
recipes, DSP fixes, iPlug2 bumps, installer/CI tweaks, and bug fixes — and we
have not done a structured sweep of what landed upstream since our last sync.
Without a periodic review we risk missing free bug fixes, performance work, and
A2-related improvements, and we let our drift from upstream grow (making every
future cherry-pick harder).

This was seeded from the A2 retraining work, where we discovered our local
understanding of the core (`ContainerModel` default slice) was based on a stale
assumption — a reminder that upstream-equivalent files need periodic
reconciliation.

## Scope

Use the **`upstream-sync` skill** (`.cursor/skills/upstream-sync/SKILL.md`) — it
documents the sync strategy, upstream-equivalent file map, and cherry-pick flow.
Do NOT blindly merge; this is a curate-and-cherry-pick review, not a fast-forward.

- Identify the upstream baselines and what we currently track:
  - `NeuralAmpModelerPlugin` (the app/plugin repo) — find the commit/tag VoLum
    last synced from and diff `HEAD` against it for the files we mirror.
  - Submodules `NeuralAmpModelerCore`, `AudioDSPTools`, `iPlug2`, `eigen` — note
    current pinned SHAs (`git submodule status`) vs upstream latest tags.
- Triage upstream changes into buckets, each with a one-line "adopt / skip / defer"
  call and rationale:
  - **Bug fixes** (audio glitches, threading, serialization, file loading).
  - **A2 / slimmable** improvements (trainer recipe, container handling,
    `SetSlimmableSize`, metadata sync) relevant to our packed-container rigs.
  - **DSP / performance** (resampler, NAM core, dsp.cpp hot paths).
  - **Build / CI / installer** changes that reduce our maintenance burden.
  - **UI** changes — usually SKIP (VoLum UI is a full rewrite), but flag any
    shared-control or iPlug2-API changes that affect our code.
- For anything in the "adopt" bucket, list the exact upstream commits/files and
  whether it cherry-picks cleanly onto VoLum's customized versions; note conflicts
  in the upstream-equivalent files (`VoLum*.inc.cpp`, `NeuralAmpModeler.cpp`, etc.).
- Explicitly confirm whether upstream changed anything about the A2 export format
  or container defaults since we set our `PackedWaveNet.export()` pipeline (see
  `docs/a2-training-runbook.md`), so our shipped rigs stay format-current.

## Acceptance Criteria

- A written review (this is a planning/triage ticket) listing, per bucket, the
  upstream changes since our last sync with an adopt/skip/defer decision and
  reason for each.
- Concrete follow-up tickets (or a prioritized list) for every "adopt" item,
  each scoped enough to implement on a feature branch.
- Current vs upstream SHA/tag table for all four submodules + the plugin repo.
- No code changes in this review chat — adoption happens in separate
  implementation chats on `feature/<topic>` branches off the latest `dev`.

## Verification

- N/A for the review itself (no code change). Each spawned adoption ticket
  carries its own verification (`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`,
  macOS parity, app smoke as relevant) per `.cursor/rules/vo-lum-workflow.mdc`.
