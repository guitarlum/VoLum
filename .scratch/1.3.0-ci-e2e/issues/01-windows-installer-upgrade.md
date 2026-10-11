# 01 Windows installer upgrade job

Status: ready-for-agent

## Scope

Write: `NeuralAmpModeler/scripts/installer-upgrade-win.ps1`, the `Windows installer upgrade`
job in `.github/workflows/ci.yml`, a changelog line. Must not touch product C++.

## Acceptance

- Refuses to run unless `GITHUB_ACTIONS=true` or `-IKnowThisIsAThrowawayVm`.
- Negative control first (wrong expected version must fail) -> `NEGATIVE CONTROL OK`.
- Upgrade from `v1.2.3` and from `v1.2.2`. A runner cannot reset to a clean VM mid-job, so
  use two matrix legs (one per old version) or uninstall cleanly between them; the worker
  picks and documents it. Per old version:
  - Silent install of the old setup; exe and VST3 report the old version.
  - Seed a 1.2.x library from repo fixtures; record a hash of every file.
  - Silent install of this run's 1.3.0 setup over it: exit code 0; `VoLum.exe` and the VST3
    binary report 1.3.0; exactly one VoLum uninstall entry; every bundled rig file the 1.3.0
    installer ships is present; the library hash is unchanged by the installer itself.
  - `e2e-standalone-win.ps1 -Exe "C:\Program Files\VoLum\VoLum.exe"` scenarios `fresh` and
    `upgrade` pass (sandboxed LOCALAPPDATA, as the script already does).
  - Running the 1.3.0 setup a second time (repair) succeeds and changes nothing above.
  - Silent uninstall: program folder and VST3 gone, uninstall entry gone, library untouched.
- Log ends `ALL PASS <n> checks`; any failure fails the job.
