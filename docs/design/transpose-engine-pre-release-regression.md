# VoLum Transpose-Engine — Pre-Release Relentless Regression Pass

Branch: `feature/transpose-engine`. Pass date: 2026-06-30.
Entry HEAD: `bf4bc0f` (after the upstream tremolo/POLY agent finished, branch CI
green). Pass HEAD: `59e5e84`.

Goal: exhaust automated regression coverage (laptop + REAPER + unit tests +
GitHub CI) of every new branch feature and its edge cases, so only a thin
manual smoke remains on macOS and Windows before release.

Tags: **VERIFIED** (already covered, re-confirmed), **ADDED** (new test this
pass), **FALLBACK** (automation blocked — moved to manual smoke, logged).

Baseline before the pass: `run-tests-win.ps1` green — 479 cases /
1,948,995 assertions. After the pass: **489 cases / 1,951,699 assertions, 0
failed**; source parity 40 tests.

---

## 0. What actually landed on the branch (reconciled `c1182a4..HEAD`)

- `4afed68` Tremolo refinement + per-mode memory + Delay tempo sync.
- `bf4bc0f` PRE Pitch **POLY** (polyphonic/chord) transpose character — a third
  character beyond DROP / INSTANT (scope addition vs the original plan).
- **Params:** `kDelaySync`, `kDelayDivision` appended at the end
  (`kNumParams` 91 → 93); all prior indices stable.
- **Serialization:** NO chunk-version bump. New state rides the additive,
  self-describing id-tail (`VoLumChunkIdTail.h`, schema 5) and additive
  `volum-settings.json` keys. This is the correct additive path; the original
  plan's "chunk-version bump" assumption was wrong and the serialization tests
  were aimed at old/partial-tail tolerance instead of a version migration.

The upstream agent (`cb5b091c`) already shipped strong coverage of its own
deltas: POLY polyphony / single-note accuracy / no-drift / latency ladder,
tremolo shape-morph + depth-floor mapping + sync→ms, and id-tail / JSON /
dirty-compare round-trips for the tremolo-mode, pitch-mode, and delay-sync
fields. This pass therefore targeted the *gaps* around those deltas, not
duplicates.

---

## 1. Coverage by feature area

### Presets — VERIFIED + ADDED
`test_volum_custom_content.cpp` / `test_volum_content_store.cpp` /
`test_volum_content_crud_edge.cpp` already cover capture / recall / overwrite /
rename / delete, AddPreset de-dup, owner-key isolation (factory vs custom amp),
case-insensitive name uniqueness, `ClampName` UTF-8 safety, reopen dirty
baseline from preset content, and recalled-snapshot registry round-trip. The
id-tail back-compat tests added below protect the chunk presets ride on.
**ADDED:** a **non-circular** preset/scene persistence pin — the existing
preset round-trips all assert fidelity via `AmpSettingsEqual`, which is defined
as `AmpSettingsToJson(a)==AmpSettingsToJson(b)` and therefore cannot detect a
field dropped from that codec (it vanishes from both sides equally). The new
test round-trips a fully-populated snapshot through the actual preset path
(`AmpSettingsToJson`→`AmpSettingsFromJson`) and compares the **decoded struct
fields directly** for every 1.2.0 effect field (PRE pitch incl. POLY character +
per-mode snapshots, POST tremolo sync + per-mode, delay sync/division) and the
BYO id refs (`activeIrId`, `supportActiveIrId`, `supportCustomId`, slot/channel),
so a preset silently losing new effect/BYO state on save/reload now fails loudly.

### PRE/POST lock — VERIFIED + ADDED
`test_volum_pre_post_lock.cpp` covers switch-while-locked dirty, reload-on-B /
switch-back-clears-dirty, live-snapshot JSON round-trip without touching slots,
snapshot omitted when lock off, and old-format detection. cb5b091c extended the
dirty compare + copy blocks for the new per-mode + delay-sync fields. The locked
delay / tremolo / pitch snapshots also round-trip through the chunk id-tail.
**ADDED:** source-locks pinning the PRE-pitch and POST-tremolo per-mode
re-entrancy guards (below).

### BYO amp / IR / pedal — VERIFIED
Content store / CRUD / `test_volum_ir_file_guard.cpp` cover cab+channel
persistence, per-lane IR isolation (`perAmpIrId` vs `perAmpSupportIrId`), DIRECT
(cab-less) detection, 3-char cab name clamp, monotonic non-reused pedal indices
and PRE-capture pool exhaustion. id-tail back-compat (below) protects the string
refs.

### A2 Lite — VERIFIED + ADDED
`test_golden_rigs.cpp` covers the A2 fast-path activation, lazy Lite-submodel
activation, and that the Lite slice (channels_3) differs from Full (channels_8)
and is ~3.5× cheaper; `test_volum_user_settings_io.cpp` covers machine-global
`liteMode` round-trip + missing→Full default.
**ADDED:** strict **negative** detector cases — empty/unrelated config, generic
two-layer-array WaveNet, post-stack head present, and missing `head_scale` are
all rejected, so the fast path can never hijack an ordinary model. (All bundled
rigs are now A2 containers, so the negatives are synthesized.)

### Tremolo — VERIFIED + ADDED
DSP (cubic shape morph, audible depth floor 0.40, sync→Hz/ms) covered by
cb5b091c.
**ADDED:** out-of-range division + degenerate-BPM clamp pins for
`VoLumTremoloSyncRateHz` / `VoLumTremoloSyncMs` (no `kQuarterMultiplier` OOB);
`VoLumTremoloDivisionName` defined for every division + safe default; source-lock
that the per-mode mode-switch save/restore is wrapped by
`mVolumTremoloRestoreInProgress`; source-lock that `kTremoloDepth` routes through
`VoLumTremoloDepthKnobToInternal` in the process block.

### Pitch / Octave — VERIFIED + ADDED
POLY DSP (chord-voice survival, single-note accuracy across range, no-drift,
latency ladder Instant<Drop<Poly) and per-mode chunk round-trip covered by
cb5b091c.
**ADDED:** source-lock that the PRE-pitch per-mode switch is wrapped by
`mVolumPreRestoreInProgress`; id-tail back-compat for a pitch block with a
short/missing `modes` array (seeds ship defaults, no OOB).

### Delay tempo-sync — VERIFIED + ADDED
Per-amp + locked delay-sync chunk / JSON / dirty round-trips covered by
cb5b091c.
**ADDED:** source-lock that synced delay time is derived via
`VoLumTremoloSyncMs(postBpm, kDelayDivision)` and clamped to 10–2000 ms, that
the Sync toggle refreshes layout to swap the Time knob for the division stepper
(`mVolumDelayDivStep`), plus the division clamp pins above.

### Serialization — VERIFIED + ADDED
Existing: no-tail (pre-1.2.0) probes empty, malformed-JSON rejected,
length-past-chunk rejected, empty round-trip, schema-1 (no `supIr`) reads empty.
**ADDED:** older tail WITHOUT `pitch`/`trem`/`dly` keys loads every effect tail
absent (bypassed defaults) including the locked-effect snapshots; effect block
with a missing/short `modes` array seeds per-mode ship defaults without OOB
(the partial-upgrade path).

---

## 2. REAPER harness — built + verified, with one caveat (FALLBACK on full headless)

`NeuralAmpModeler/scripts/reaper-harness/` (`run-reaper-harness.ps1` +
`volum-harness.lua` + README) loads VoLum as a track FX on a synthesized tone,
reads the track's post-FX output via an audio accessor (no render dialog, no
temp WAVs), and asserts finite / bounded / non-silent audio plus a project
save/reload (`noprompt:`) round-trip. Params are resolved **by name** so it
survives EParam-order drift. Verified passing (7/7 checks) against the scanned
VST3.

Caveats / FALLBACK, logged:
- REAPER instantiates whatever `VoLum.vst3` it has **scanned**
  (`%COMMONPROGRAMFILES%\VST3`). On this machine that path is non-writable
  without admin, so the harness exercises the *previously installed* build, not
  necessarily HEAD. For a true HEAD smoke, install the freshly built VST3 first.
- REAPER has no SWS-free clean-quit from ReaScript, so after the harness
  force-kills it, the *next* launch can hit REAPER's "not shut down properly"
  recovery modal (symptom: timeout, no `harness.log`). Re-run after a manual
  REAPER launch/quit, or install SWS for a clean-quit action.
- The automated host guarantee in CI is `pluginval --strictness-level 10`
  (parameter fuzz, in-host state save/restore round-trip, automation, multi
  sample-rate/block) on the real binary, both Windows + macOS. The REAPER
  harness is a local real-host audio-sanity smoke on top of that; GUI-only rig
  state (which `.nam`/IR/preset is loaded) is covered by the doctest
  chunk/settings round-trips.

---

## 3. CI

Local Windows tests green at every commit. Full CI matrix (Windows + macOS
tests, ASan/UBSan, pluginval, packaging) dispatched on each push via
`gh workflow run ci.yml --ref feature/transpose-engine`; entry HEAD `bf4bc0f`
was green, and the pass HEAD `59e5e84` run was dispatched for final
confirmation. Verify the latest run is green before promoting.

---

## 4. Manual smoke checklist (macOS + Windows) — ONLY what automation cannot verify

Automation cannot judge *audible* quality, GUI rendering, or real-DAW host
quirks. Keep the smoke to these:

1. **Audible POLY chords.** PRE Pitch → Transpose → POLY, +5 semitones, play a
   triad and a power chord: every voice should shift and stay intact (DROP /
   INSTANT will garble the chord — confirm the contrast). Listen for latency
   feel (~49 ms) being acceptable.
2. **Tremolo feel.** Sweep DEPTH from min → max: even the minimum knob should
   audibly throb (40% floor). Sweep SHAPE 0→1: gentle sine → soft square, no
   hard chop in the first third. Check OPTICAL / BIAS / HARMONIC sound distinct.
3. **Tempo sync, both effects.** Engage Delay TEMPO SYNC and Tremolo SYNC in a
   real DAW at 90 / 120 / 150 BPM: the Time/Rate knob is replaced by the
   DIVISION stepper, repeats/throb lock to the grid, and changing host tempo
   tracks live. Confirm the Time/Rate knob is not editable while synced.
4. **Per-mode memory by ear.** Set distinct knobs per Tremolo mode and per Pitch
   mode (Transpose vs Octaver), switch away and back: the knobs should recall.
5. **Preset + lock round-trip in a real DAW.** Build a rig (custom amp/IR/pedal
   + the new effects), save a preset, lock PRE and POST, save the DAW project,
   reopen it (and restart the plugin): rig, preset selection, locks, and all new
   effect/mode state should restore identically. Repeat once on each OS.
6. **A2 Lite by ear/CPU.** Toggle A2 Lite on an Ampete (A2) amp: lower CPU, tone
   still musical; confirm a non-A2 context is unaffected.
7. **UI highlight + layout.** Spot-check the new POLY pill and DIVISION steppers
   draw their active/selected state and lay out correctly at a couple of window
   sizes (no clipping, scrollbars where lists overflow).

Everything else (param persistence, chunk/JSON round-trips, DSP finiteness,
clamps, back-compat, A2 detection, host validation) is covered by the automated
suite + pluginval and does not need manual repetition.
