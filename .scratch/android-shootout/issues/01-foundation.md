# 01 Foundation

Status: ready-for-agent
Branch: `feature/android-shootout` (worktree `C:\dev\VoLum-android-shootout`, already exists)
Worker: claude-opus-5-5-high
Blocks: 02, 03, 04, 05, 06

## Goal

Create the shared Android project, the feature inventory, the fake engine, the
test harness and the box tooling, so the design-system worker and the three
candidate workers can start in parallel on disjoint paths.

## Scope

May write: `NeuralAmpModeler/projects/android/**` except `design/`, `ui-*/`,
`engine/`; `NeuralAmpModeler/scripts/android-box/**`;
`.scratch/android-shootout/{features.md,reference/**,evidence/**,reports/01-foundation.md}`.
Must not write: `NeuralAmpModeler/*.h|*.cpp`, submodules, `.github/`, docs.

## Deliverables

1. `.scratch/android-shootout/features.md`: every user-facing control, mode,
   dialog, list, setting, state and recovery path, one row each:
   `id | desktop source (user-guide section + code symbol) | Android mapping | screen id | interaction (tap/toggle/drag/select/text/file) | N/A reason`.
   Sources: every section of `docs/user-guide.en.md`, the parameter enum in
   `NeuralAmpModeler/NeuralAmpModeler.h`, the settings tabs (SIGNAL, MIDI,
   SYSTEM), presets, PLAY, packs, custom content, tuner, metronome, dual amp,
   every PRE/POST effect mode and its mode-conditional controls.
2. Gradle project `NeuralAmpModeler/projects/android/` (AGP + Kotlin + Compose
   BOM, current stable versions that work with JDK 17, compileSdk/targetSdk 35,
   minSdk 29, NDK from `~/volum-android/ndk-version.txt` on the box):
   `:core`, `:harness`, `:app` (flavors `a`/`b`/`c`), stub `:design`, stub
   `:ui-a`/`:ui-b`/`:ui-c` (each renders a placeholder for every screen id), and
   an empty `engine/` placeholder. See `spec.md` "Project shape".
   - `:core`: `FeatureId` registry generated from or checked against
     `features.md` (a unit test fails when they diverge), domain state, reducer,
     deterministic fake engine, screen-id and fixture registry. Unit tests per
     feature id.
   - `:harness`: `Modifier.featureId(id)` semantics key; a capture activity that
     renders `--es screen <id> --es fixture <id>`; instrumented tests run per
     flavor: (a) coverage — every non-N/A feature id is found on its screen,
     operated through semantics only, and changes reducer state as specified;
     (b) reachability — every screen id is reached from home by clicks only,
     at most 3 taps; (c) touch targets ≥ 48 dp for every featureId node. The
     tests must fail on the stub candidates.
3. Box tooling `NeuralAmpModeler/scripts/android-box.ps1` (laptop side) and
   `NeuralAmpModeler/scripts/android-box/*.sh` (box side), steps:
   `Sync` (push branch to the box bare repo, check out a box worktree
   `~/volum-android/wt/<branch-slug>`), `Build` (flavor, under a build lock),
   `Test` (unit + instrumented, under the emu lock; boots the AVD if not running
   and keeps it running), `Capture` (every screen id × fixture at font scale 1.0
   and 1.3), `Motion` (`screenrecord` of scripted interactions per screen →
   ffmpeg frame contact sheet, `~/hubris/bin/ffmpeg` exists), `Fetch` (scp
   results to `.scratch/android-shootout/evidence/<x>/<sha>/`). Follow the
   ssh traps in `spec.md`.
4. `NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate a|b|c|design`:
   runs Sync → Build → Test → Capture → Motion → Fetch for the branch head and
   writes `evidence/<x>/<sha>/verify-candidate.json`
   (`{"commit","pass","build","unit","coverage":{"total","passed","missing":[]},"reachability","touch","crash_anr","captures","motion"}`).
   Exit 0 only when every gate passes. Logcat FATAL/ANR during the run fails it.
5. Desktop motion references in `.scratch/android-shootout/reference/`:
   build and launch the desktop standalone with
   `pwsh NeuralAmpModeler/scripts/run-app-win.ps1` from `C:\dev\VoLum` (sandbox
   library), capture repeated frames with `win-screenshot.ps1` per
   `docs/screenshot-recipes.md` (section 3c for PLAY art), and assemble contact
   sheets: BUILD hero art idle, PLAY art at energy 0/0.5/1, a pedal toggle, a
   section switch. Close the app afterwards.

## Acceptance

- `verify-candidate.ps1 -Candidate a` runs end to end on the box and exits
  non-zero **only** because the stub UI misses features (coverage/reachability),
  with build, unit tests, captures and motion sheets produced.
- `:core` unit tests pass; the features.md ↔ registry test passes.
- `features.md` covers every user-guide section (a checklist at its end maps
  each `##`/`###` heading of `docs/user-guide.en.md` to feature ids).
- Reference sheets exist for the four desktop motions.
- Branch pushed to GitHub (`git push https://github.com/guitarlum/VoLum.git feature/android-shootout`).

## Verifier checklist

- Coverage test cannot pass by calling the reducer directly (semantics only).
- No feature listed N/A without a reason; no user-guide heading unmapped.
- Box scripts strip CRs, use locks, never touch `~/localai-platform`.
- No phase-narrating comments in scripts or tests.
