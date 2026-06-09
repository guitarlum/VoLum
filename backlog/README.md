# VoLum Backlog

This folder holds planning prompts seeded from post-1.0 user feedback. It is a
tracked planning artifact: each open item below is a ready-to-paste prompt for a
fresh Cursor planning chat.

## How to use

1. Pick one item. Open a fresh Cursor chat per item.
2. Paste the prompt body into the chat.
3. Let the planning session produce a scoped ticket
   (problem, scope, acceptance criteria, tests/docs/changelog).
4. Open a second chat to implement on the named feature branch.
5. Merge back into `dev` once acceptance criteria are met.
   Never promote to `main` outside of a release.

## Open items

### Features
- `F1-transpose-octaver-pedal.md` — combined Transpose + Octaver PRE pedal before the compressor.
- `F4-a2-lite-mode-support.md` — optional opt-in A2-Lite execution; default stays A2-Full. Seeded from the A2 retraining work (see `docs/a2-training-runbook.md`).
- `F5-presets-full-rig.md` — save / load / recall a full rig (amp(s) + PRE + POST) as a named preset, with factory presets and export/import.
- `F6-bring-your-own-amp.md` — custom-amp builder + the shared "Base / Custom" area and user-content storage (foundation for F7 / F8).
- `F7-bring-your-own-ir.md` — surface the existing hidden NAM IR convolver as a first-class `.wav` cabinet-IR feature in the Custom area.
- `F8-import-your-own-pedals.md` — import custom PRE NAM captures into the PRE pedal slots.
- `F9-midi-support.md` — minimal MIDI: Program Change recalls presets, CC maps to key params via MIDI-learn. Lowest priority.

### OS Support
- `O1-linux-support-spike.md`

### Docs
- `D1-docs-input-level-and-polish.md`

### Project / Repo
- `R2-volum-ampete-product-flag-removal.md`
- `R3-upstream-nam-player-sync-review.md` — periodic sweep of upstream NAM Player repo + submodules for bug fixes / A2 / perf changes worth cherry-picking. Use the `upstream-sync` skill.

### Reference (not a prompt)
- `1.0.1-review-findings.md` — engineering review notes captured during the 1.0.1 cycle.

## Suggested order

The "bring your own / presets" cluster is the current focus:

1. F6 — Bring Your Own Amp + the Custom-area foundation (unblocks F7 and F8).
2. F5 — Presets (can reference custom amps from F6).
3. F7, F8 — BYO IR and custom pedals (build on the F6 foundation).
4. F9 — MIDI (depends on F5 for Program Change → preset).
5. F1 — Transpose + Octaver pedal.
6. O1 — Linux spike (can run in parallel).
7. R2, R3 — repo hygiene and upstream sync sweeps.
8. D1 — docs, folded in continuously as items land.

## Done (archive)

One-line summaries of completed items; the individual prompt files were pruned
once shipped.

### Bugfix
- `B1` — done 2026-05-19: Standalone audio config no longer crashes on 32/96-sample buffers, ASIO no-device fallback reverts safely, and CI is green on `dev`.
- `B2` — done 2026-05-20: macOS trackpad scrolling now follows system natural-scrolling direction for knobs and wheel-driven controls; CI is green on `dev`.
- `B3` — done 2026-05-20: Arrow keys and mouse wheel now share consistent 0.5 dB / 0.1 dB fine steps for amp, PRE NAM, support amp, and COMP level knobs; CI is green on `dev`.
- `B4` — done 2026-05-19: PRE NAM, compressor, main amp, and support amp output-style levels now fully mute at their minimum setting; CI is green on `dev`.
- `B5` — done 2026-05-20: Standalone audio configuration now hardens device/channel selection and keeps mismatched or invalid I/O choices from destabilizing the app; CI is green on `dev`.

### UX
- `U1` — done 2026-05-20: Standalone audio setup was simplified and tightened, with clearer configuration behavior, safer channel/device choices, and spacing polish; CI is green on `dev`.

### Performance
- `P1` — done 2026-05-20: Hero fractal redraws are cached, fixed DC-blocker coefficients moved out of `ProcessBlock`, active tuner analysis was optimized, and CI is green on `dev`.

### Features
- `F2` — done 2026-05-26: PRE/POST lock overlay with store-to-amp, live-lock snapshot persistence, and settings backward compatibility; shipped in v1.0.1.
- `F3` — done 2026-05-26: macOS Audio Unit (AUv2) plugin ships alongside standalone and VST3; installer + portable component zip; Logic Pro / GarageBand / REAPER compatible; shipped in v1.0.1.

### Project / Repo
- `R1` — done 2026-05-19: Issues page is public, docs link to issue intake.
