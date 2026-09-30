# 01 Test build parity + UI/perf baselines

Status: resolved
Blocked by: none
Report: C1 (build-01, build-02) in `.scratch/perf-sweep/report.md`; evidence `.scratch/perf-sweep/findings/build-config.md`

## Why
The Windows test binary is compiled without `/GL` / `/LTCG` while the shipped app and VST3 have both, so the
goldens and the realtime-budget test check a differently compiled binary. The budget test also runs without
the FTZ/DAZ mode `ProcessBlock` sets. Every later ticket leans on these tests.

## Paths
- New `NeuralAmpModeler/config/VoLum-release-opt.props` (Release|x64 ClCompile: `WholeProgramOptimization`
  on, `IntrinsicFunctions`, `FavorSizeOrSpeed=Speed`, `FunctionLevelLinking`; Link: `OptimizeReferences`,
  `EnableCOMDATFolding`, LTCG).
- `NeuralAmpModeler/projects/NeuralAmpModeler-Tests.vcxproj`, `-app.vcxproj`, `-vst3.vcxproj` (import it;
  delete the now-duplicated values in app/vst3 only if the effective tlog flags stay identical).
- `NeuralAmpModeler/tests/test_volum_realtime_budget.cpp` (FTZ/DAZ around `ProcessTimed`, saving and
  restoring MXCSR so later test cases are unaffected; x86 only).
- Guard: extend `test_build_script_guards.cpp` (or the nearest build-guard test) to assert the Tests project
  imports the shared props.

## Also in this ticket: baselines (no product change)
Under the build lock, from the worktree's Release app, capture and store under
`C:\dev\VoLum\.scratch\perf-safe-wins\baseline\`:
- PNGs via `.ui-sandbox-launch.ps1 -Reseed` + `ui-drive.ps1 -Locked` for: BUILD idle, PLAY (canvas
  `10,10;743,22`), PLAY with `VOLUM_ART_ANIM_DEBUG=13:0:7.3`, tuner open (`T`), BUILD with Settings closed after
  a knob hover. Name them so ticket 11-15 can diff 1:1.
- `VOLUM_FRAME_PERF=0` frame/region counts for BUILD idle and PLAY silent (10 s each), and
  `.scratch/perf-sweep/cpu-sample.ps1` rows for BUILD idle and PLAY (copy the script's approach; it expects
  the main checkout path, so run an adapted copy against the worktree exe).
- `NeuralAmpModeler-Tests.exe --test-case=*realtime* -s` medians (3 runs).

## Acceptance
- Tests tlog shows `/GL` and the link shows `/LTCG`; app/vst3 tlog flags unchanged.
- Full suite green **without** any golden change. If a golden or hash moves under `/GL`, STOP and escalate:
  that means today's tests do not represent the shipped sound.
- Baselines written and listed in the ledger row.

## Proof
Build-guard assertion shown red against the old Tests.vcxproj, green after.

## Verifier conditions
MXCSR restored at the end of the budget test; no golden file or tolerance touched; tlog evidence attached.

## Result (2026-09-26, commit 0106d37b)
- New `NeuralAmpModeler/config/VoLum-release-opt.props` sets the ClCompile/Link metadata directly (the
  `WholeProgramOptimization` *property* is read inside `Microsoft.Cpp.props`, so a later sheet cannot rely on
  it). Imported in the Release|x64 property-sheet group of Tests, app and VST3; app/vst3 lost their duplicated
  Optimization/FunctionLevelLinking/IntrinsicFunctions/FavorSizeOrSpeed/COMDAT/OptimizeReferences lines.
- Tests tlog (`a2_fast.cpp`): before `/O2`, no `/GL /Gy /Oi /Ot`; after `/GL /Gm- /GS /Gy /MT /O2 /Oi /Ot`,
  identical to app/vst3. Link: before no `/LTCG /OPT`; after `/LTCG:incremental /OPT:ICF /OPT:REF`.
- App/VST3 unchanged: app rebuild "All outputs are up-to-date", CL and link tlogs byte-identical to before.
  VST3 compile flags and 14 defines identical to its old tlog (only `/MP`, which tlogs never record). Evaluated
  MSBuild metadata, HEAD vs now, identical for app and vst3 (`evidence/01/evaluated-meta.txt`). The VST3 did
  not link locally: `iPlug2/Dependencies/IPlug/VST3_SDK` holds only README.md since 2026-09-23 (environment,
  not this change).
- Budget test: `ScopedDenormalsOff` (save MXCSR, `disable_denormals()`, restore) around the timed chain; new
  case "The budget chain runs with denormals flushed and hands back the caller's FP mode".
- Proofs: guard red against the old Tests.vcxproj (`REQUIRE( import != npos )`, `evidence/01/red-guard.txt`),
  green after. MXCSR restore removed: new case red (`40928 == 8096`, i.e. 0x8040 leaked,
  `evidence/01/red-mxcsr.txt`), green after revert.
- Full suite `run-tests-win.ps1`: 1130/1130, every golden and hash unchanged under `/GL`. ASan build
  (`-Asan -Filter "denormals flushed"`) green; MSVC falls back from incremental LTCG itself.
- Budget medians: absolute times drift 20-40% within the session on this hybrid laptop. Pinned P-core
  interleaved A/B (`evidence/01/ab/`): `/GL` vs `/O2` and FTZ on vs off within +/-10-15% noise, `/GL` leaning a
  few % slower. Baselines and recipes: `baseline/README.md`.
- No changelog line: internal build/test change, nothing a player notices.
