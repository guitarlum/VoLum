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

### P1 / BACKLOG (Phase 3) — `NeuralAmpModeler.cpp` is ~5570 lines (~8100 in the TU)

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

**Fix (Phase 3):** extract `VoLumPluginUiStyles.inc.cpp`, `VoLumKeyboard.inc.cpp`,
`VoLumLayoutRuntime.inc.cpp`, `VoLumSceneRig.inc.cpp`; decompose `VoLumCustomUi.h`
(Q2); dedup the triplicated scrollbar/glyph/PRE-menu primitives.

### P1 / BACKLOG (Phase 4, R4) — the "Mock" content bridge is mis-named

`VoLumCustomContentMock.h` is a production bridge over `GlobalContentStore()`,
half pass-through, with process-global preset hooks. Rename + per-instance hooks
+ registry-as-canonical-read-path.

---

## 4. Correctness (custom content) — niche/latent (Phase 5, Q3)

- `_VolumActiveScene()` uses `customScenes[id]` (`operator[]`) — read paths can
  insert phantom default scenes. Needs a const or-default read accessor.
- `activePresetId` restore relabels the live scene rather than applying the
  bank preset's `pr.settings` (diverges only for drifted custom scenes).
- `customSupportId` id-tail field is written but never read (dead; support
  restores via the scene). Safe to drop.
- `AddPedal` clamps every pedal past the 64-slot pool to index 127 -> colliding
  PRE-capture indices; `PedalLegacyIndexAt` returns `0` (== EMPTY) for OOR.

---

## 5. Real-time / performance (Phase 6, P2 — riskiest, audio thread)

- Staging drain takes `mVolumLoaderMutex` + `mStagingMutex` and may `Reset()`
  (alloc) on the audio thread -> lock-free SPSC queue, `Reset()` on loader thread.
- `_PrepareBuffers` may `resize` main I/O buffers mid-stream -> pre-reserve in
  `OnReset` + assert in `ProcessBlock`.
- `_VolumSelectIR` does a sync WAV load on the UI callback -> async loader path.
- `AmpSettingsEqual` builds two JSON trees per knob move for the dirty flag ->
  field-wise/hashed compare.
- Every param change marks settings dirty + `OnIdle` always saves -> dirty only
  VoLum-owned params, coalesce saves.

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
| 1.1 | Settings 4-way duplication -> composed codec | P1 | Phase 2 |
| 1.2 | Exhaustive settings round-trip test | P1 | Phase 1 |
| 2 | Shared `DrawVoLumSelection` + source guard | P1 | Phase 2 |
| 3.1 | Decompose `NeuralAmpModeler.cpp` + `VoLumCustomUi.h` | P1 | Phase 3 |
| 3.2 | Dedup scrollbar/glyph/PRE-menu | P1 | Phase 3 |
| 3.3 | R4 content-bridge rename/collapse | P1 | Phase 4 |
| 4 | Q3 custom-content correctness | P1/P2 | Phase 5 |
| 5 | P2 RT/perf hardening | P0/P1 | Phase 6 |
| 6 | Q1 pins + Q4 harness | P1 | Phase 1 |
| 7 | Rules/docs SSOT + selection + checklist + file-map | P1 | Phase 7 |
