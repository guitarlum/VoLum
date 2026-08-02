# VoLum Backlog

This folder holds planning prompts for open work. It is a tracked planning
artifact: each open item below is a ready-to-paste prompt for a fresh Cursor
planning chat.

**Only open work lives here.** When an item ships, delete its prompt file and add
a one-line summary to the Done archive at the bottom. When an item ships
*partially*, trim the prompt to the remainder and say at the top what already
landed, so nobody re-plans shipped behaviour.

## How to use

1. Pick one item. Open a fresh Cursor chat per item.
2. Paste the prompt body into the chat.
3. Let the planning session produce a scoped ticket
   (problem, scope, acceptance criteria, tests/docs/changelog).
4. Open a second chat to implement on the named feature branch.
5. Merge back into `dev` once acceptance criteria are met.
   Never promote to `main` outside of a release.

## Open items

Verified against the code on 2026-07-30, not against the prompts' own claims.
Items marked **partially shipped** have a status block at the top of the file
naming what already exists.

### Bugfix
- `B6-multi-instance-content-library.md` — two VoLum instances, or a DAW instance
  plus the standalone, silently overwrite each other's custom library. Highest
  deferred severity from the 1.2.1 audit: it loses user content. Do this with `R4`
  phase 2, which proposes the same per-instance ownership from the refactor side.
- `B7-audio-thread-rt-violations.md` — the audio callback allocates, frees
  megabytes, constructs and destroys models, calls host/UI APIs, takes blocking
  mutexes and can throw. One ticket because they share one root cause: the model
  handoff is done *by* the audio thread. Owns the three real-time items that used
  to be in `P2`. Also unblocks `F9`, whose MIDI handoff needs the same machinery.
- `B8-pdb-symbol-mismatch.md` — the shipped installer's binaries and the CI PDB
  artifact come from two different link steps, so symbols do not resolve for the
  build most users run. Debuggability only; fold into the next packaging change.

### Features
- `F5-presets-full-rig.md` — **partially shipped.** Preset save/recall/rename/
  delete and shared banks landed in 1.2.0. Remaining: a factory preset bank, and
  export/import of individual presets (which has to decide what happens to the
  custom content a preset references).
- `F8-import-your-own-pedals.md` — **partially shipped.** Import and stable
  indices landed in 1.2.0. Remaining: let the user assign a pedal type group
  (Klon / TS-Boost / Distortion / Fuzz / Other) instead of one flat CUSTOM list.
  The `PedalItem.group` field already exists and is never written.
- `F9-midi-support.md` — Program Change recalls presets, CC maps to key params via
  MIDI-learn. Depends on `F5` for preset ordering and on `B7` for a real-time-safe
  handoff — the 1.2.1 spike was gated out precisely because `ProcessMidiMsg()`
  runs on the audio thread. Lowest priority.
- `F10-settings-import-export.md` — a portable `.volumpack` bundle of settings plus
  the custom content it references, for moving between machines or sharing a rig
  pack. Overlaps `F5`'s export half; settle the id re-keying story once, in
  whichever ships first.
- `F11-octaver-deep-research.md` — give the Octaver mode the same
  measured-reference deep-research treatment the transpose engine got (uses the
  local `deep-research` skill; reference the reference Chaos Bed as black-box
  reference). Ship the best octaver defensible with numbers; no gratuitous knobs.
- `F12-tremolo-deep-research.md` — validate/sharpen the three POST Tremolo voices
  (Optical/Bias/Harmonic) against measured real-tremolo references (same skill).
  Fix any voice that isn't an honest emulation; only add controls the research
  proves. The 1.2.1 audit also parked four Tremolo *voicing* findings for this
  pass — see `1.2.1-audit-deferred.md` § "Voicing changes".
- `F13-custom-amp-artwork-reroll.md` — expand the procedural custom-amp art beyond
  today's six styles and add a deterministic reroll. Fully open: it builds on the
  six styles 1.2.0 shipped, and needs a seed field, since only a style index is
  stored right now. Generated art only, no user image import.
- `F14-update-notifier.md` — subtle in-app "a new version exists" notice in
  standalone and plugin, fed by a static `appcast.json` published on
  `release: published`. Notify-only; auto-update is explicitly out of scope and
  blocked on code signing. Design is settled, ~2-3 days. Ships value one release
  *after* the one that introduces it, so it wants to be early.

### OS Support
- `O1-linux-support-spike.md` — feasibility spike; can run in parallel with
  anything.

### Quality / Testing
- `Q2-volum-1.2.0-structure-decompose.md` — **partially shipped.**
  `VoLumCustomUi.h` is now a 35-line umbrella and the scrollbar is shared, but the
  overlay it split out is itself 1814 lines and still owns both Manage and Builder.
  Remaining: that split, the duplicated pen/bin glyphs, the PRE capture menu, and a
  builder popup that draws without clipping or a height cap.
- `Q3-volum-1.2.0-custom-content-correctness.md` — **partially shipped.** Latent
  BYO/preset hardening, none of it urgent. The pedal-pool exhaustion half landed;
  still open are applying a matched preset's values (not just its label) on
  restore, the `customSupportId` restore path, the `_VolumActiveScene()` read/write
  split, and an out-of-range `PedalLegacyIndexAt` returning the EMPTY sentinel.
- `Q4-screenshot-harness-custom-amp-seed.md` — **mostly shipped.** Seeding, the
  scrollbar geometry test and the runbook all landed. Remaining three small gaps:
  a name-area shrink-to-fit test, an `AGENTS.md` pointer to the capture harness,
  and one stale cross-link.
- `P2-volum-1.2.0-rt-and-perf-hardening.md` — **re-scoped.** Now only the two
  UI-thread performance items: the preset dirty check builds two JSON trees per
  knob event, and settings still write on every idle tick that finds the flag set.
  Its three real-time items moved to `B7`.

### Project / Repo
- `R3-upstream-nam-player-sync-review.md` — recurring sweep of the upstream NAM
  Player repo + submodules for fixes worth cherry-picking. Never "done"; last sync
  was 2026-06-22 (NAMCore `f4a6cc0` → `27027cc`). Use the `upstream-sync` skill.
- `R4-volum-1.2.0-content-bridge-collapse.md` — **partially shipped.** The rename
  to `VoLumCustomContentApi.h` landed; phases 2-5 did not. Phase 2 (per-instance
  preset hooks and owner key) is the same change `B6` needs — pair them.

### Loose nits

Carried over from the retired 1.0.1 review doc; all cosmetic, none blocking. Kept
as one list rather than five files.

- A duplicate step in `.github/workflows/ci.yml`.
- Changelog date ordering is inconsistent with the file's own style in places.
- `PrePostLockSim` in the tests is a hand-maintained mirror of the production lock
  state machine and can drift from it. The only one with real substance — it wants
  a shared fixture rather than a parallel implementation.
- Vendored headers under `tests/third_party/` carry no provenance comments.
- `DbToAmpWithMuteFloor` is asserted in two test files.

### Reference (not a prompt)
- `1.2.1-audit-deferred.md` — **start here for anything about the 1.2.1 audit.**
  Triage ledger over all 25 review passes: every finding, its severity, and
  whether 1.2.1 fixed it or deferred it, plus a suggested order for 1.3.0. The
  evidence itself lives in `audit-notes/` (`opus5/`, `gpt56/`, `selfreview/`,
  `selfreview-gpt/`), keyed by the report and finding ids the ledger cites.
  `B6`, `B7` and `B8` above are the three deferred clusters big enough to have
  been promoted out of it into their own prompts. Most of the deferred work from
  that audit is *only* in this ledger, so read it before planning 1.3.0 scope.

## Suggested order

The 1.2.1 audit reshuffled this: content-safety bugs now come before features,
and two of the highest-value items are not files in this folder at all — they live
in `1.2.1-audit-deferred.md`.

1. **`B6` + the "deleting content that is currently playing" cluster** from the
   ledger, with `R4` phase 2 folded in. All three want the same content-removal
   transaction and the same per-instance ownership; done separately it gets
   designed three times. This is the only deferred cluster that destroys user
   content.
2. **The two one-shot IR calibration bugs** (ledger: `opus5
   P-custom-content-library` 1 and `P-state-serialization-content` 1). Both
   permanently mis-set IR loudness, so every day they stay open is more libraries
   to re-measure later.
3. **Forward-compatible chunk reading** (ledger: `gpt56 P2-9`). Same shape as the
   bug that reset every project in 1.2.0. It cannot fire today, but it fires the
   first time a 1.3.0 project is opened in 1.2.1 — so it has to land *before*
   1.3.0 writes chunks.
4. **`F14`** — cheap, settled, and only pays off a release after it ships.
5. **`F11` / `F12`** — the deep-research voicing passes, which also clear the
   Tremolo voicing findings the audit parked.
6. **`F5` → `F10` → `F8`** — presets remainder, then the settings bundle that
   reuses its id re-keying, then pedal groups.
7. **`F9`** — MIDI, after `F5` (preset ordering) and `B7` (real-time handoff).
8. **`B7` + `P2`** — a deliberate real-time and performance pass, not a slot
   squeezed between features.
9. **Hygiene: `Q2`, `Q3`, `Q4` remainder, the UI polish table** in the ledger,
   which is cheap and highly visible.
10. **`O1`** Linux spike, in parallel whenever. **`R3`** periodically. **`B8`**
    whenever the packaging scripts are next touched. **`F13`** any time.

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
- `U1` (audio setup) — done 2026-05-20: Standalone audio setup was simplified and tightened, with clearer configuration behavior, safer channel/device choices, and spacing polish; CI is green on `dev`.
- `U1` (native window resize) — **won't do**, decided 2026-06-21. A native-border resize was built and worked, then reverted at the user's request: all-edge resize lets an off-aspect drag leave a letterbox strip. The bottom-right corner grip is aspect-locked and stays the only resizer (`PLUG_HOST_RESIZE 0`). Reopen only if free-axis resize with a `WM_SIZING` aspect lock is ever wanted, which is an iPlug2 mirror change. (Two unrelated tickets shared the id `U1`.)

### Performance
- `P1` — done 2026-05-20: Hero fractal redraws are cached, fixed DC-blocker coefficients moved out of `ProcessBlock`, active tuner analysis was optimized, and CI is green on `dev`.

### Features
- `F1` — done 2026-06-27, shipped in v1.2.0: PRE Pitch pedal with TRANSPOSE and OCTAVER. The prompt's Signalsmith phase-vocoder design was reversed during implementation for a low-latency WSOLA period-sync granular shifter (`VoLumPitchShifter.h`); the octaver follow-up is now `F11`.
- `F2` — done 2026-05-26: PRE/POST lock overlay with store-to-amp, live-lock snapshot persistence, and settings backward compatibility; shipped in v1.0.1.
- `F3` — done 2026-05-26: macOS Audio Unit (AUv2) plugin ships alongside standalone and VST3; installer + portable component zip; Logic Pro / GarageBand / REAPER compatible; shipped in v1.0.1.
- `F4` — done 2026-06-24, shipped in v1.2.0: opt-in A2 Lite execution mode with a Settings switch and persistence; default stays A2-Full. `feature/a2-lite-mode` is merged into `dev`.
- `F6` — done 2026-06-20, shipped in v1.2.0: custom-amp builder plus the CUSTOM sidebar area and the user-content store (`volum-content.json` under the per-user content dir) that `F7` and `F8` were built on. Shipped with a deliberate narrowing: DIRECT plus three renameable cab slots rather than the prompt's free-form autocomplete names.
- `F7` — done 2026-06-20, shipped in v1.2.0: `.wav` cabinet IRs as a first-class Custom-area feature, convolved against the DIRECT capture, shared by standalone and plug-in, with per-IR level and low/high cut added 2026-07-07.

### Quality / Testing
- `Q1` — done 2026-06-29: in-CI doctest pins locking the three named 1.2.0 behaviours (thumbnail scale-invariant blitting and `OnRescale` invalidation, overlay hover highlight via `mHoverAction`, standalone active-preset id persistence), in `test_volum_ui_regressions.cpp`.

### Docs
- `D1` — done 2026-05-20, extended 2026-06-22: EN and DE user guides document the +4 dBu interface input level and cross-reference A2 / Lite mode, plus a broader polish pass.

### Project / Repo
- `R1` — done 2026-05-19: Issues page is public, docs link to issue intake.
- `R2` — done: the legacy `VOLUM_AMPETE_PRODUCT` build fence is gone; the identifier now appears nowhere in the product source. No changelog entry, since behaviour was unchanged.

### Retired reference docs
- `1.0.1-review-findings.md` — removed 2026-07-30. All 13 crash / data-loss / audio findings from the 1.0.1 cycle were fixed (phases 3a-4a); the five cosmetic leftovers are now the "Loose nits" list above.
