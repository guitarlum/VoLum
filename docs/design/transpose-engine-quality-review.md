# VoLum Transpose-Engine — Thermo-Nuclear Code-Quality Review

Branch: `feature/transpose-engine` (56 commits ahead of `dev`). Review date: 2026-06-29.

Scope: the full branch delta plus a whole-codebase structural pass focused on
**testability** and **AI-traversability**. The brief named two recurring bug
classes — "sometimes we missed highlighting on UI components" and "sometimes
params were not handled well in the settings.json" — and asked for the structure
to be fixed so those classes cannot recur, with the AI rules/docs reviewed too.

This is the prioritized findings record. Items are tagged:

- **LANDED** — fixed in this quality pass (test-covered).
- **BACKLOG** — folded in as a later phase of this same pass, or kept as a ticket.
- **NOTE** — observation, no action required.

Severity: P0 (correctness/RT), P1 (structure/maintainability), P2 (polish).

Baseline before the pass: `run-tests-win.ps1` green — 459 cases / 1,944,881
assertions; source parity 39 tests.

---

## 0. Headline: the two named bug classes are *latent*, not live

Both named symptoms were investigated and are **not currently broken**:

- All 19 new PRE-pitch / POST-tremolo params **are** persisted in
  `volum-settings.json` (verified across all four JSON IO paths and the
  param<->struct bridge).
- Every new mode picker (Pitch TRANSPOSE/OCTAVER, Tremolo OPTICAL/BIAS/HARMONIC)
  **does** draw its active state — in the shared `VoLumModePickerControl`'s amber
  treatment, consistent with Delay/Reverb.

The real problem is **structural fragility**: the way params and selection are
wired makes it easy for the *next* addition to silently drop a field or miss a
highlight, and **no test fails when that happens**. This pass fixes the
structure and adds the enforcement tests, rather than patching non-bugs.

---

## 1. Settings persistence — no single source of truth

### P1 / LANDED (Phase 1+2) — adding one persisted param touches ~9–13 sites

`volum-settings.json` (`%LOCALAPPDATA%\VoLum\`) is written/read by manual,
duplicated field lists. The data-model SSOT is the `VoLumAmpSettings` struct
(`VoLumAmpeteCatalog.h`), but serialization is scattered:

| Layer | Files / functions |
|-------|-------------------|
| param <-> struct | `_VolumSavePreToSlot` / `_VolumRestorePreFromSlot` / `_VolumSavePostToSlot` / `_VolumRestorePostFromSlot` (`VoLumSettings.inc.cpp`) |
| struct <-> JSON (block helpers) | `PreBlockToJson`/`FromJson`, `PostBlockToJson`/`FromJson` (`VoLumUserSettingsIO.h`) |
| struct <-> JSON (**duplicate** inline) | the per-amp loops in `VolumUserSettingsToJson`/`FromJson` (`VoLumUserSettingsIO.h` ~L506–547 / ~L790–854) |
| equality (lock chrome) | `PreBlockEquals`/`PostBlockEquals` (`VoLumPrePostLock.h`) |
| DAW chunk (parallel path) | `Unserialization.cpp` id-tail |

The **4-way JSON duplication** is the trap: `PreBlock*` helpers AND the inline
`VolumUserSettings*` loops both enumerate every PRE field. A new field added to
the block helper but forgotten in the inline loop persists in lock snapshots but
silently fails to persist per-amp — and no test catches it.

A composed codec already exists and is unused by the main file:
`AmpSettingsToJson`/`AmpSettingsFromJson` in `VoLumAmpSettingsJson.h` compose
`WriteAmpCoreBlock` + `PreBlockToJson` + `PostBlockToJson` cleanly. Its header
comment says the `volum-settings.json` serializer is "intentionally left
untouched" — that note is the source of the drift.

**Fix (Phase 2):** route the `VolumUserSettingsToJson`/`FromJson` per-amp blocks
through the composed codec so there is **one** field list. Collapses ~13 edit
sites toward ~3–4.

### P1 / LANDED (Phase 1) — no test enforces a full round-trip

`test_volum_user_settings_io.cpp` round-trips a *hand-picked* subset (main test
stops at `preNam2Level`; tremolo is covered, **`prePitch*` is not**).
`CopyPreBlock` in `test_volum_pre_post_lock.cpp` itself omits every `prePitch*`
field — copy-paste drift in the test code. A param can be dropped from
persistence with the suite still green.

**Fix (Phase 1):** an exhaustive round-trip doctest that sets every
`VoLumAmpSettings` field to a non-default value and asserts equality after
`VolumUserSettingsToJson` -> `FromJson`. This pins the Phase-2 refactor.

---

## 2. UI selection / highlight — three dialects, no shared helper

### P1 / LANDED (Phase 2) — every control hand-draws its active state

`VoLumColorHelpers.h` documents one brass selection language (`SEL_BG`,
`SEL_BORDER`, `SEL_GLOW`, `SEL_TEXT`) "for any mutually-exclusive group", but
there is **no `DrawSelection` helper**. Three dialects coexist:

| Dialect | Tokens | Used by |
|---------|--------|---------|
| Brass | `SEL_*` | cab row (`VoLumSpeakerRow.h`), metronome time-sigs |
| Teal | `ITEM_SEL_*` | sidebar amp list, list menus, PRE-capture menu, builder art picker |
| Amber | `AMBER` fill | `VoLumModePickerControl`, `VoLumSubModePillControl` (all mode pickers + sub-pills) |

Each control re-implements the fill/border/glow by hand, so a new
mutually-exclusive control can ship with no highlight or the wrong one. The new
Pitch/Tremolo pickers use the amber dialect (correct for pickers) — the "missed
highlighting" perception is the dialect mismatch next to the brass cab row.

`test_volum_ui_regressions.cpp` asserts only wired strings + geometry; **zero**
tests cover selection visuals, so an omission stays green.

**Fix (Phase 2, visuals unchanged):** add
`DrawVoLumSelection(g, rect, active, hovered, SelectionStyle)` with an explicit
`SelectionStyle { Brass, ListTeal, AmberPicker }`; route every mutually-exclusive
control through it; add a source-guard test that each picker calls it.

---

## 3. Structure — files too large for cheap AI traversal

### P1 / PARTIALLY LANDED (Phase 3) — `NeuralAmpModeler.cpp` is ~5570 lines (~8100 in the TU)

Largest VoLum-owned files (lines on disk):

| File | Lines | Note |
|------|-------|------|
| `NeuralAmpModeler.cpp` | ~5570 | + tail-includes -> ~8100-line TU |
| `VoLumCustomUi.h` | 1895 | `VoLumCustomOverlayControl` ~73% of it (Q2) |
| `NeuralAmpModelerControls.h` | 1150 | upstream-derived |
| `VoLumUserSettingsIO.h` | 1062 | the duplication above |
| `VoLumSettings.inc.cpp` | 1014 | scene + locks + preset bank |
| `VoLumTriptych.h` | 918 | |

`NeuralAmpModeler.cpp` god-functions: the `mLayoutFunc` lambda (~1450 lines),
`_UpdateVoLumLayout` (~401), the keyboard block (~490), `OnIdle` (~189),
scene/rig helpers (~1100). The repo already proves a low-risk decomposition
mechanism — tail-`#include`d `.inc.cpp` siblings sharing the one TU.

**LANDED (Phase 3) — dead-code + dedup:** dead-code removal (`RowAt()`,
write-only `mTextFileIdx`, unused `StartTextEntry` `fileIdx` param in
`VoLumCustomUi.h`) and the scrollbar primitive dedup (shared `DrawVoLumScrollbar`
in `VoLumColorHelpers.h`, routed through the sidebar amp list and the Manage
overlay list).

**LANDED (Phase 3) — `NeuralAmpModeler.cpp` tail-`#include` split:**
**5599 -> 3523 lines on disk (-2076)**, four new tail-`#include`d siblings (the
compiled TU is byte-identical after preprocessing — `#include` pastes each block
at its original position):

| New sibling | Lines | Contents |
|-------------|-------|----------|
| `VoLumKeyboard.inc.cpp` | 496 | on-screen keyboard navigation + exact-entry handlers |
| `VoLumLayoutRuntime.inc.cpp` | 419 | `_HideControlGroup` + `_UpdateVoLumLayout` |
| `VoLumSceneRig.inc.cpp` | 762 | tuner/metronome toggles + channel/PRE-capture/custom-amp/IR rig helpers |
| `VoLumAmpMenus.inc.cpp` | 420 | factory reset + preset/support-amp menus + `_VolumApplyDualAmpFocus` |

The move was done with a raw-substring slice anchored on unique function
signatures (byte-exact for retained text; `git diff` confirmed a single
2076-line deletion + 4 `#include` insertions), verified by app build + the full
469-case suite (the source-string locks read the siblings via `ReadPluginSource`;
five tests that read `NeuralAmpModeler.cpp` directly were repointed to the blob).

**LANDED (Phase 3e) — `mLayoutFunc` lambda extracted:** the ~1450-line build
body became `NeuralAmpModeler::_BuildVoLumLayout(IGraphics*)` in the tail-
`#include`d `VoLumLayoutBuild.inc.cpp` (the lambda now forwards). The earlier
"captures constructor locals" worry was disproven *by the compiler*: a stored-
and-later-called lambda using a constructor local is UB, and converting the body
to a member makes any such use a hard compile error — it built clean, so there
were none. **NeuralAmpModeler.cpp: 3523 -> 2075 lines (5599 at pass start).**
Screenshot-verified the standalone renders an identical layout.

**LANDED (Phase 3d) — `VoLumSettings.inc.cpp` split:** the 1090-line sibling is
now a thin umbrella over `VoLumSettingsLocks.inc.cpp` / `VoLumSettingsScene.inc.cpp`
/ `VoLumSettingsPresets.inc.cpp` (all the same TU; byte-identical after
preprocessing).

**LANDED (Phase 3c/3f) — `VoLumCustomUi.h` decomposed:** the four controls each
moved to their own header — `VoLumPresetBar.h` (227), `VoLumListMenu.h` (219),
`VoLumConfirmDialog.h` (173), `VoLumCustomOverlay.h` (1480) — and `VoLumCustomUi.h`
is now a **40-line umbrella** that just includes them. Consumers needing only the
preset bar / dropdown no longer drag in the overlay. The preamble-control-class
concern was moot: the three small controls were self-contained and moved cleanly
with per-header includes; no top-include shuffle was needed.

**DEFERRED (logic refactor, not decomposition — intentionally not forced):**
- `VoLumCustomOverlayControl` is now isolated in its own file but still ~1480
  lines. Carving its *internals* (out-of-lining methods + a `switch(mManageKind)`
  traits table) is a behavior-changing logic refactor on a multi-state overlay
  (Manage list for presets/IRs/pedals + Builder grid) that yields **no** file-
  size/token win (an AI editing the class reads it whole regardless) and cannot
  be screenshot-verified in every state cheaply. It is the one remaining item
  whose risk/reward argues for a dedicated, opt-in pass rather than bundling.

### P1 / LANDED (Phase 4, R4) — the "Mock" content bridge is mis-named

`VoLumCustomContentMock.h` is a production bridge over `GlobalContentStore()`,
half pass-through, with process-global preset hooks.

**LANDED:** git-tracked rename to `VoLumCustomContentApi.h` + header/comment
refresh across all referencing files (the "Mock" name made readers and agents
repeatedly treat production code as throwaway test scaffolding — a direct
AI-traversability tax). No symbols renamed (`volum::custom::*` unchanged).

**DEFERRED (higher-churn, ~40 sites):** moving `ActivePresetOwnerKey` / preset
hooks off process-global statics onto the instance; deleting the projection
getters so the UI reads `GlobalContentStore().reg()` directly; folding mutations
+ name-dedup into the store.

---

## 4. Correctness (custom content) — niche/latent (Phase 5, Q3)

- **LANDED** — `AddPedal` previously clamped every pedal past the 64-slot pool
  (indices 64..127) to index 127, aliasing multiple pedals onto one PRE-capture
  index. It now returns `-1` once the pool is exhausted (within-pool behavior
  unchanged) and the Manage-pedals importer surfaces a "slots are full" error.
  Pinned by a boundary doctest in `test_volum_custom_content.cpp`.
- **DEFERRED (needs plugin-level test infra)** — `_VolumActiveScene()` uses
  `customScenes[id]` (`operator[]`), so read paths *can* insert phantom default
  scenes. A const or-default read accessor is the fix, but it is a plugin-member
  method with ~8 read sites and **no unit harness instantiates the plugin**, so
  it can only be guarded by source pins + app build. Worth doing in a pass that
  either adds that harness or accepts source-pin verification.
- **DEFERRED** — `activePresetId` restore relabels the live scene rather than
  applying the bank preset's `pr.settings` (diverges only for drifted custom
  scenes); subtle three-way behavior, defer with the read/write split.
- **CORRECTION (not dead — do NOT drop)** — the earlier note that
  `customSupportId` is "written but never read" is **wrong**: it is written at
  `NeuralAmpModeler.cpp` L2412 and has chunk round-trip tests
  (`test_volum_chunk_codec.cpp` L679/697/913/931). Dropping it would be a
  breaking chunk-format change. No action.

---

## 5. Real-time / performance (Phase 6, P2 — riskiest, audio thread) — DEFERRED (gated on macOS TSan CI)

This phase is **deliberately not implemented in this pass**. The plan gated it on
"macOS sanitizer CI green, checkpoint first", and this pass ran on a Windows host
with **no ThreadSanitizer available**. Converting audio-thread locking to
lock-free without TSan verification risks shipping a subtle data race — the worst
outcome for a real-time audio plugin — so it is left as a CI-backed follow-up.

Current state of each item:

- **Already done (pre-pass)** — settings save is coalesced: param changes set
  `mVolumSettingsDirty` and `OnIdle` flushes once (`NeuralAmpModeler.cpp` L2312);
  the dirty compare already uses the struct `AmpSettingsEqual`, not a per-knob
  JSON build (`VoLumSettings.inc.cpp` L1088). No work needed.
- **Incorrect as originally stated** — "pre-reserve I/O buffers + assert no
  `resize` in `ProcessBlock`": hosts legitimately change the block size, which
  must `resize` `mInputArray`/`mOutputArray` mid-stream (`_PrepareBuffers`
  L5439). An unconditional no-resize assert would fire on a normal host buffer
  change. Any reservation here must allow the resize-on-blocksize-change path.
- **Gated (the real RT work)** — `_ApplyDSPStaging()` takes
  `std::lock_guard<std::mutex>(mStagingMutex)` **on the audio thread**
  (`NeuralAmpModeler.cpp` L2959, called from `ProcessBlock` L1987); `OnReset`
  also locks `mPrePitchMutex` and calls `Reset()`. Replacing these with a
  lock-free SPSC hand-off (allocation/`Reset()` on the loader thread only) is the
  P0 RT-safety fix and **must** land with macOS TSan CI green.
- **Gated** — `_VolumSelectIR` does a synchronous WAV load on the UI callback;
  route through the existing async loader.

---

## 6. Tests & verification gaps

- No exhaustive settings round-trip (section 1) — **Phase 1**.
- No selection-visual coverage (section 2) — **Phase 2 guard**.
- Custom-amp UI states (sidebar scrollbar overflow) are unreachable by the
  scripted harness — **Phase 1, Q4** (`VOLUM_SEED_CUSTOM_AMPS` + scrollbar
  geometry helper/test).
- Q1 regression pins (thumbnail-scaling invariant, hover-highlight wiring,
  `volumActivePresetId` round-trip) — **Phase 1**.

---

## 7. AI documentation / rules drift

- `.cursor/rules/volum-ui.mdc` says layout attaches in `_AttachVoLumGraphics`;
  it is actually the `mLayoutFunc` lambda. File-map omits several `VoLum*` files.
- No rule encodes the settings SSOT contract or the selection-language contract,
  which is *why* both bug classes are easy to reintroduce.
- No onboarding "how to add a param / add a control" checklist enumerating the
  edit sites — **Phase 7** adds one, plus the SSOT + selection rules and the
  file-map refresh.

---

## Action summary

| # | Finding | Sev | Phase / Status |
|---|---------|-----|------|
| 1.1 | Settings duplication -> composed write codec (SSOT) | P1 | Phase 2 — LANDED |
| 1.2 | Exhaustive settings round-trip test | P1 | Phase 1 — LANDED |
| 2 | Shared `DrawVoLumSelection` + source guard | P1 | Phase 2 — LANDED |
| 3.1 | Decompose `NeuralAmpModeler.cpp` + `VoLumCustomUi.h` | P1 | Phase 3 — dead-code LANDED; bulk excise DEFERRED (map in §3) |
| 3.2 | Dedup scrollbar primitive (`DrawVoLumScrollbar`) | P1 | Phase 3 — LANDED |
| 3.3 | R4 content-bridge rename | P1 | Phase 4 — rename LANDED; collapse DEFERRED |
| 4 | Q3: `AddPedal` pool cap | P1 | Phase 5 — LANDED |
| 4b | Q3: `_VolumActiveScene` split / preset apply | P2 | DEFERRED (needs plugin test infra) |
| 4c | Q3: `customSupportId` "dead" | — | NOT a bug (has round-trip tests) |
| 5 | P2 RT/perf hardening (audio-thread locks) | P0/P1 | Phase 6 — DEFERRED (gated on macOS TSan CI) |
| 6 | Q1 pins + Q4 harness | P1 | Phase 1 — LANDED |
| 7 | Rules/docs SSOT + selection + checklist + file-map | P1 | Phase 7 — LANDED |
