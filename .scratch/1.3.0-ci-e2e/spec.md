# 1.3.0 CI end-to-end: installers and the Mac standalone

## Why

The owner has no Mac until next week and wants 1.3.0 out without waiting. Release
confidence for macOS and for the real installers comes from GitHub runners instead.
macOS runners are free for this public repo, the runner is Apple silicon with a
logged-in desktop session, and Windows runners run elevated. Nothing here needs money.

Already covered elsewhere (do not rebuild): unit + golden tests on macOS CI, strict
codesign, auval under Rosetta, REAPER AU + VST3 recall on the macOS CI runner, the Windows
standalone e2e (`NeuralAmpModeler/scripts/e2e-standalone-win.ps1`) with real WinMM MIDI
through loopMIDI, REAPER VST3 on Windows.

Not coverable by any machine (stays on the owner's Mac pass next week): the HX Stomp over
USB on macOS, listening by ear, the Gatekeeper dialog as a user sees it, Retina sharpness.

## What ships

Three new jobs in `.github/workflows/ci.yml`, blocking like the others, so
`merge-verified-branch.ps1` (which requires a green `ci.yml` run) gates on them. Exact job
names (the predicate looks them up):

- `Windows installer upgrade`
- `macOS installer upgrade`
- `macOS standalone e2e`

Each job consumes the installer/DMG built by the existing Windows/macOS jobs in the same run
(upload/download artifacts, `needs:`), never rebuilds them.

Each job ends its log with `ALL PASS <n> checks` on success, and runs a **negative control**
first: the same checker invoked with one deliberately wrong expectation (e.g. expected
version `9.9.9`) must fail, and the job prints `NEGATIVE CONTROL OK`. That is how these
tests prove they can go red (the repo rule: a test that cannot fail is worse than none).

Scripts (one owner file per job, no logic inline in YAML beyond calling them):

- `NeuralAmpModeler/scripts/installer-upgrade-win.ps1`
- `NeuralAmpModeler/scripts/installer-upgrade-mac.sh`
- `NeuralAmpModeler/scripts/e2e-standalone-mac.sh` (python helpers allowed next to it)

Every script **refuses to run unless `GITHUB_ACTIONS=true`** (or an explicit
`-IKnowThisIsAThrowawayVm` / `--throwaway-vm` flag). They install into Program Files /
/Applications and write a user library; on the owner's laptop that would overwrite the
installed VoLum and the real `%LOCALAPPDATA%\VoLum` library, which must never change.

## Decisions (locked)

- Old versions to upgrade from: `v1.2.3` (required) and `v1.2.2` (second leg, same job).
  `v1.2.1` has no GitHub release. Download with `gh release download`.
- The seeded 1.2.x library comes from repo fixtures (the ones `e2e-standalone-win.ps1
  -Scenario upgrade` uses), never from a developer machine.
- Gatekeeper on CI is recorded, not asserted beyond "the verdict was captured": the build is
  ad-hoc signed and not notarized (`makedist-mac.sh`), so `spctl` rejects a quarantined copy,
  same as 1.2.x. The job writes the exact `spctl` output to the job summary for the release
  notes. A screenshot of the Finder dialog is a stretch goal, not a predicate line.
- Screenshots from the Mac standalone are uploaded as an artifact and must be non-blank
  (pixel variance check). Whether they look right is the verifier's job, not the predicate.
- Pack import through the native open dialog is out of scope on macOS (Pack logic is unit
  tested on macOS CI).
- No product (C++) changes are planned. A product bug found on the way: crash, hang, stuck
  UI or data loss gets a `feature/*` fix with a red-then-green test, cold verify, CI, merge.
  Anything else goes to `.scratch/1.3.0-hunt/findings.md` as F-113+ for 1.3.1.

## Facts the worker needs

- Windows installer: Inno Setup, `NeuralAmpModeler/installer/VoLum.iss` (`AppVersion=1.3.0`,
  `DefaultDirName={autopf}\VoLum`, VST3 into `{cf64}\VST3\VoLum.vst3\`). Silent flags
  `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LOG=<file>`; uninstall via
  `unins000.exe /VERYSILENT`. Uninstall entry `HKLM\...\Uninstall\<AppId>_is1`: read AppId
  from the .iss and check 1.2.3 used the same one (one Apps entry after upgrade).
- Release assets: `VoLum-v1.2.3-windows-setup.exe`, `VoLum-v1.2.3-macos-installer.dmg`
  (pkg inside, built with pkgbuild in `makedist-mac.sh`), same pattern for 1.2.2.
- macOS library: `$HOME/Library/Application Support/VoLum` (`VoLumPaths.h`), so a
  standalone launched directly (`/Applications/VoLum.app/Contents/MacOS/VoLum`, not `open`)
  with `HOME=<tempdir>` is sandboxed. Logs go to `volum.log` in that folder.
- Windows e2e already accepts `-Exe` and `-SeedFrom`: run its `fresh` and `upgrade`
  scenarios against `C:\Program Files\VoLum\VoLum.exe` after the upgrade.
- Mac MIDI: a virtual CoreMIDI source from python (`pip install python-rtmidi`,
  `MidiOut().open_virtual_port("VoLum Loop")`); the standalone's RtMidi lists virtual
  sources. How the standalone persists its MIDI input choice on macOS: read
  `IPlugAPP_host` / the Windows e2e's `Write-SandboxAudioConfig` and mirror it.
- Audio on the macOS runner: copy whatever the existing macOS REAPER CI steps do (read the
  macOS job in `ci.yml`); BlackHole via brew is acceptable if nothing exists.
- Fresh-install expectations (mirror `Test-Fresh` in the Windows e2e): `volumUiMode` play,
  `buildTipSeen` false, `volumActivePresetId` `factory:12:v1`, log line
  `recall slot=0 amp=factory:12 preset=factory:12:v1`, PLAY pre-filled with the five
  Factory Sounds (`$script:PrefillMapKey`).
- MIDI expectations (mirror `Test-Midi`): Program Change 9 recalls program 9, CC 102
  value 42 recalls program 42, an empty program is ignored, a burst ends on the last
  program, "One channel" = 2 ignores channel 1, `lastPlaySlot` saved on quit.

## Out of scope

Notarization, buying a Mac, cloud Macs, Retina checks, REAPER on Mac (already in CI),
Linux, changing `publish-appcast.yml`.
