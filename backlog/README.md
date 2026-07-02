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
- `F1-transpose-octaver-pedal.md` — **STALE / shipped + superseded.** Pedal built; the Signalsmith phase-vocoder DSP in the doc was reversed for a low-latency WSOLA period-sync granular shifter with a DROP/FAST character (changelog 06/27/2026). Kept only as the original UI/state design reference. Octaver follow-up is now `F11`.
- `F11-octaver-deep-research.md` — give the Octaver mode the same measured-reference deep-research treatment the transpose engine got (uses the local `deep-reverse-engineering` skill; NDSP Rabea X Chaos Bed as black-box reference). Ship the best octaver defensible with numbers; no gratuitous knobs.
- `F12-tremolo-deep-research.md` — validate/sharpen the three POST Tremolo voices (Optical/Bias/Harmonic) against measured real-tremolo references (uses the local `deep-reverse-engineering` skill). Fix any voice that isn't an honest emulation; only add controls the research proves.
- `F4-a2-lite-mode-support.md` — optional opt-in A2-Lite execution; default stays A2-Full. **Implemented on `feature/a2-lite-mode` (in test / pending merge to `dev`)**; archive + prune the prompt once merged. Seeded from the A2 retraining work (see `docs/a2-training-runbook.md`).
- `F5-presets-full-rig.md` — save / load / recall a full rig (amp(s) + PRE + POST) as a named preset, with factory presets and export/import.
- `F6-bring-your-own-amp.md` — custom-amp builder + the shared "Base / Custom" area and user-content storage (foundation for F7 / F8).
- `F7-bring-your-own-ir.md` — surface the existing hidden NAM IR convolver as a first-class `.wav` cabinet-IR feature in the Custom area.
- `F8-import-your-own-pedals.md` — import custom PRE NAM captures into the PRE pedal slots.
- `F9-midi-support.md` — minimal MIDI: Program Change recalls presets, CC maps to key params via MIDI-learn. Lowest priority.

### OS Support
- `O1-linux-support-spike.md`

### Quality / Testing
- `Q1-regression-pin-safety-net.md` — fast in-CI doctest pins locking 1.2.0 UI/state behavior (thumbnail scaling, hover highlight, standalone preset restore) so a later perf/refactor pass can't silently regress them. Low-to-medium; pull the scaling pin in alongside the performance-optimization ticket.
- `Q2-volum-1.2.0-structure-decompose.md` — decompose the 1964-line `VoLumCustomUi.h`, dedup scrollbar/glyph/PRE-menu primitives, and make the builder popup list-disciplined. Pure hygiene, phased low->high risk.
- `Q3-volum-1.2.0-custom-content-correctness.md` — 1.2.0 BYO/presets hardening, re-verified down from the quality review: **none are urgent user-facing bugs** (the one real bug, `RemoveCustomAmp` orphaning, already landed). Remaining items are niche/latent: preset values relabelling a drifted custom-amp scene (niche), `_VolumActiveScene()` read/write split (latent), pedal index-cap collision at 127 (edge); `customSupportId` id-tail field is dead-code cleanup, not a bug. From `docs/design/1.2.0-quality-review.md`.
- `P2-volum-1.2.0-rt-and-perf-hardening.md` — lock-free staging drain (no audio-thread mutex/alloc), main I/O buffer capacity assert, async IR load, field-wise dirty compare, throttled settings save.
- `Q4-screenshot-harness-custom-amp-seed.md` — deterministic custom-amp seeding so the local `win-screenshot`/`win-click` harness can capture states behind runtime-grown lists (the amp-library scrollbar), plus a real scrollbar/gutter geometry doctest and an AGENTS.md/`volum-ui.mdc` review for screenshot guidance. Found while fixing the sidebar scrollbar (the `+` builder overlay isn't scriptable, so the scrollbar couldn't be screenshotted).

### Docs
- `D1-docs-input-level-and-polish.md`

### Project / Repo
- `R2-volum-ampete-product-flag-removal.md`
- `R3-upstream-nam-player-sync-review.md` — periodic sweep of upstream NAM Player repo + submodules for bug fixes / A2 / perf changes worth cherry-picking. Use the `upstream-sync` skill.
- `R4-volum-1.2.0-content-bridge-collapse.md` — rename + partially collapse the mis-named `VoLumCustomContentMock.h` production bridge (move preset hooks/owner-key onto the instance, delete projection getters, fold mutations into the store). From `docs/design/1.2.0-quality-review.md`.

### Reference (not a prompt)
- `1.0.1-review-findings.md` — engineering review notes captured during the 1.0.1 cycle.

## Suggested order

The "bring your own / presets" cluster is the current focus:

1. F6 — Bring Your Own Amp + the Custom-area foundation (unblocks F7 and F8).
2. F5 — Presets (can reference custom amps from F6).
3. F7, F8 — BYO IR and custom pedals (build on the F6 foundation).
4. F9 — MIDI (depends on F5 for Program Change → preset).
5. F11 / F12 — Octaver + Tremolo deep-research passes (F1 transpose+octaver pedal already shipped).
6. O1 — Linux spike (can run in parallel).
7. R2, R3 — repo hygiene and upstream sync sweeps.
8. D1 — docs, folded in continuously as items land.
9. Q1 — regression pins; ideally folded into the next perf/refactor pass rather than run standalone.

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
