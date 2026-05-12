# VoLum Agent Notes

Keep this file as a small routing index. Detailed guidance lives in scoped Cursor rules and skills so agents load only what the task needs.

## Scope

- Main product code: `NeuralAmpModeler/`.
- Vendored/submodule code: `iPlug2/`, `NeuralAmpModelerCore/`, `eigen/`. Avoid edits unless task explicitly targets them.
- `AudioDSPTools/` is a submodule, but VoLum actively depends on DSP there.
- Top-level `rigs/` is the dev source of bundled amp profiles. Shipped artifacts rename it to `VoLumRigs/`.

## Start Here

- UI/layout work: `.cursor/rules/volum-ui.mdc` and skill `volum-ui-change`.
- Params, presets, state migration: `.cursor/rules/volum-state-params.mdc` and skill `volum-param-state-change`.
- CI, installers, releases, artifacts: `.cursor/rules/volum-release-packaging.mdc` and skill `release-manager`.
- Build/CI failures: skill `native-build-debugger`.
- Submodule or vendored code questions: `.cursor/rules/volum-submodules.mdc`.

## Fast Commands

- Windows tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- macOS tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh`
- macOS sanitizer tests: `bash NeuralAmpModeler/scripts/run-tests-mac.sh --sanitize`
- Windows app smoke check: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- Windows portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Windows installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- macOS release-equivalent package: `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`
- Format: `bash format.bash`

## Test Map

- DSP helpers/effects: doctests in `NeuralAmpModeler/tests/` (`test_process_io.cpp`, `test_delay_reverb_dsp.cpp`, `test_volum_pre_effects.cpp`, `test_tone_stack.cpp`, tuner/metronome tests).
- Main signal-chain decisions: `VoLumProcessingPlan.h` plus `test_volum_processing_plan.cpp`.
- Params/keyboard/state: `test_eparam_order.cpp`, `test_keyboard_steps.cpp`, `test_volum_chunk_version.cpp`, `test_volum_chunk_codec.cpp`.
- User settings JSON: `test_volum_user_settings_io.cpp`.
- Main amp rigs: `test_nam_rigs.cpp`; new `.nam` files under `rigs/` must load AND survive one `process()` block there (finite + bounded output).
- PRE NAM captures: `VoLumPrePedalCaptures.h`, `test_volum_pre_pedal_captures.cpp`, and the PRE section of `test_nam_rigs.cpp`; new files under `rigs/PrePedals/` must discover, load, and package.
- UI/layout: `test_volum_ui_regressions.cpp`, especially pure layout checks before source-string locks.
- Master safety / NaN containment: `test_volum_master_safety.cpp` (final-bus tanh + NaN-to-0 contract) and `test_volum_nan_guard.cpp` (the `VoLumNanGuard.h` in-place scrub used after every NAM `process()`).
- Bypass identity: `test_volum_bypass_identity.cpp` asserts the chain is identity when every optional stage is in its bypass configuration.
- Packaging/installers/plugin validation: `verify-packaging-win.ps1`, `verify-packaging-mac.sh`, `verify-installer-win.ps1`, `validate-vst3-win.ps1`, `validate-vst3-mac.sh`, and `.github/workflows/ci.yml`.

## Upstream Sync (Tracking NAM Player + NAM Core)

VoLum is an independent project but deliberately stays close enough to its two
upstream sources that future fixes and features can be cherry-picked rather
than reinvented. Two upstreams, two cadences:

1. **NeuralAmpModelerCore** (`NeuralAmpModelerCore/`, MIT) - the NAM model
   runtime (wavenet / lstm / convnet / ResamplingContainer). Submodule. New
   upstream tags should be pulled and tested.
   - Sync: `git submodule update --remote NeuralAmpModelerCore`, then run
     `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` + `test_nam_rigs.cpp`
     (it loads every bundled `.nam` and runs one process block). If any rig
     fails to load or emits non-finite samples, pin the submodule back and
     open an issue upstream.
   - The bundled main amps live in `rigs/` and PRE captures in
     `rigs/PrePedals/`; both must keep loading across submodule upgrades.

2. **NeuralAmpModelerPlugin** (the upstream iPlug2 plugin shell VoLum started from).
   Available as the `upstream` remote pointing at
   `https://github.com/sdatkinson/NeuralAmpModelerPlugin.git` (push disabled).
   - Sync: `git fetch upstream main`, then
     `git log --oneline VoLum-last-upstream-merge..upstream/main -- NeuralAmpModeler` to
     see new upstream commits. Cherry-pick or merge selectively into a
     `chore/upstream-sync-YYYY-MM-DD` branch; never blanket-merge.
   - Tag the merge point in changelog so the next sync knows where to start.

### Upstream-Equivalent vs VoLum-Only Files

Future syncs should focus diffs on the upstream-equivalent files; VoLum-only
files almost never need upstream input.

- **Upstream-equivalent** (still tracks `NeuralAmpModelerPlugin` shape):
  - `NeuralAmpModeler.cpp` (kept at this path on purpose; the four heavy VoLum
    sub-systems were extracted into `VoLumLoader.inc.cpp` /
    `VoLumSettings.inc.cpp` / `Unserialization.cpp` precisely so this file
    shrinks toward upstream-equivalent content).
  - `NeuralAmpModeler.h`
  - `NeuralAmpModelerControls.h`
  - `ToneStack.cpp`, `ToneStack.h`
  - `config.h`
  - `Colors.h`
  - `choc_DisableAllWarnings.h`, `choc_ReenableAllWarnings.h`
  - `Unserialization.cpp` (file path matches upstream; VoLum-only fields are
    additive inside it)
  - `projects/*.vcxproj*`, `projects/NeuralAmpModeler-macOS.xcodeproj/`,
    `projects/NeuralAmpModeler-iOS.xcodeproj/` (touch these only when adding
    new source files; if upstream changes its project layout, mirror).

- **VoLum-only** (never expect upstream changes here):
  - Every `VoLum*.h` and `VoLum*.cpp` / `VoLum*.inc.cpp` in `NeuralAmpModeler/`.
  - `rigs/`, `docs/`, `format.bash`, `.cursor/`, `AGENTS.md`,
    `THIRD_PARTY_LICENSES.md`, `installer/changelog.txt`,
    `installer/license.rtf`.
  - Everything under `AudioDSPTools/` lives on the `guitarlum/AudioDSPTools`
    fork (branch: `effect-staging`); upstream there is
    `sdatkinson/AudioDSPTools` and follows the same selective-cherry-pick
    rule.

### Rules when extracting / refactoring

- **Do not move** `NeuralAmpModeler.cpp`, `NeuralAmpModeler.h`,
  `Unserialization.cpp`, `ToneStack.{h,cpp}`, `NeuralAmpModelerControls.h`,
  or `config.h` out of `NeuralAmpModeler/`. Their paths must match upstream so
  cherry-picks apply without manual fixup.
- **VoLum-only code added to upstream-equivalent files** must be visually
  separable (clear `// VoLum:` comment fence, `#if VOLUM_AMPETE_PRODUCT`
  guard, or extraction into a `VoLum*.inc.cpp` tail-include). The
  `#if VOLUM_AMPETE_PRODUCT` block in `NeuralAmpModeler.cpp` is the canonical
  pattern.
- **New VoLum source files always** start with the `VoLum` prefix.

## UI / Source File Map

After the 1.0 hygiene split, the plugin source is sliced as follows. New code
goes into the most specific file; do not dump everything back into the big
umbrellas.

Plugin .cpp translation unit (compiled as `NeuralAmpModeler.cpp` on every
platform - the four `inc.cpp` files are tail-included; not separate TUs):

- `NeuralAmpModeler.cpp` - constructor, UI attachment, `ProcessBlock`,
  `OnReset`, `OnIdle`, `OnParamChange` / `OnParamChangeUI`, `OnMessage`, IO
  helpers, knob selection / keyboard / layout, tuner & metronome toggles,
  PRE-capture refresh, dual-amp focus, DSP helpers.
- `VoLumLoader.inc.cpp` - async loader thread + queueing
  (`_VolumStartLoader`, `_VolumStopLoader`, `_VolumQueueMainModelLoad`,
  `_VolumDrainLoaderResults`, `_VolumLoaderThreadMain`, request dispatch).
- `VoLumSettings.inc.cpp` - per-amp settings persistence
  (`_VolumSaveCurrentToSettings`, `_VolumRestoreFromSettings`,
  delay/reverb/Oktaverb mode snapshots,
  `_VolumSaveSettingsToFile`/`_VolumLoadSettingsFromFile`).
- `Unserialization.cpp` - chunk-version migration / state restore.

UI control headers (per-section split; `VoLumTriptych.h` and
`VoLumCoreControls.h` are umbrellas that `#include` the pieces):

- `VoLumTriptych.h` - `VoLumTriptychControl` (PRE/AMP/POST triptych) +
  `VoLumChainConnectorControl`. Umbrella for the rest.
- `VoLumTriptychMotifs.h` - `DrawEffectMotif` (COMP / PRE-NAM / DELAY / REVERB
  fractal motifs used by pedal cards and Quiet slots).
- `VoLumTriptychMenus.h` - `VoLumPreCaptureMenuControl`,
  `VoLumSupportAmpMenuControl`, and the `VoLumPreCaptureMenuItem` row struct.
- `VoLumPedalCardControl.h` - focused pedal-card control with cached art
  layer + preset-name footer.
- `VoLumCoreControls.h` - sidebar (background, logo, amp list), speaker row,
  knob row, meters, dividers/footers, channel stepper, keyboard hints, exact
  entry, param-value display, settings overlay frame.
- `VoLumHero.h` - `VoLumHeroImageControl` (procedural fractal hero +
  AMP title strip + Dual Amp chip + lane PAN dots) and
  `VoLumSupportPolarityControl`.
- `VoLumKeyboardModel.h` - shared keyboard target rings and per-parameter
 step sizes used by runtime controls and tests.
- `VoLumTunerMetronomeOverlay.h` - `VoLumTunerControl`,
  `VoLumMetronomeButtonControl`, `VoLumMetronomeControl`.

Source-string regression locks in `test_volum_ui_regressions.cpp` use a
`ReadPluginSource()` helper that reads `NeuralAmpModeler.cpp` plus the three
tail-included `inc.cpp` siblings - moving a function between those files
does not break a lock unless the string itself changes.

Promoting the `.inc.cpp` siblings to real separate translation units (real
`<ClCompile>` entries in every `.vcxproj` and matching `PBXFileReference` /
`PBXBuildFile` entries in the macOS / iOS Xcode projects) is tracked as a 1.1
hygiene follow-up; it requires verification on macOS which we cannot run from
this environment.

## DSP / RT Invariants

These are locked by doctest; when changing the audio chain, keep them green or update the test and changelog together.

- POST effects (`mDelay`, `mReverb`) are `Reset()` on every active -> inactive edge so re-engaging never replays a stale tail. Tracking state: `mPostDelayWasActive` / `mPostReverbWasActive` in `NeuralAmpModeler.h`.
- `Reverb::SetParams` calls `Reset()` whenever `mode` or (for Oktaverb) `subMode` changes, matching `Delay::SetParams`.
- Reverb Mix is one-pole smoothed (~10 Hz) to kill zipper noise during automation; `mMixSmoothed` snaps to target on sample-rate change and on Reset.
- NAM model output is scrubbed via `volum::ScrubNonFiniteInPlace` after every `mModel->process` / `mSupportModel->process`; on non-finite, both POST effects are also `Reset()` so no NaN can lodge in their state.
- `volum::SoftSafetyClip` maps NaN / +/-Inf to 0 (changed from "passes NaN through" in 1.0). Final-bus contract.
- Legacy `_StageModel` / `_StageIR` writes are serialized against the audio-thread move in `_ApplyDSPStaging` via `mStagingMutex`. VoLum worker-queue drain still runs lock-free on the audio thread.
- Dual-amp scratch buffers (`mDualMainLaneBuffer`, `mDualSupportLaneBuffer`, `mDualMainAlignedBuffer`, `mDualSupportAlignedBuffer`) are pre-reserved in `OnReset` so `ProcessBlock` never allocates on the audio thread.

## Non-Negotiables

- Write/update focused tests for confirmed feature or bugfix work.
- User-facing UI or feature changes must update `docs/user-guide.en.md` and `docs/user-guide.de.md` together; refresh stable `docs/user-guide-*.png` screenshots when the visible UI changes.
- Append one dated line to `NeuralAmpModeler/installer/changelog.txt` for user-visible behavior.
- Do not reorder `EParams` or rename stable parameter names without state migration and tests.
- Keep unrelated local dirt, especially expected `iPlug2` ASIO patch dirt, out of commits.