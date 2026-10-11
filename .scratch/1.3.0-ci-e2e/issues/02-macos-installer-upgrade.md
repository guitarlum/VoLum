# 02 macOS installer upgrade job

Status: ready-for-agent

## Scope

Write: `NeuralAmpModeler/scripts/installer-upgrade-mac.sh`, the `macOS installer upgrade` job
in `.github/workflows/ci.yml`, a changelog line. Must not touch product C++.

## Acceptance

- Refuses to run unless `GITHUB_ACTIONS=true` or `--throwaway-vm`.
- Negative control first -> `NEGATIVE CONTROL OK`.
- Install `v1.2.3` from its installer DMG (`hdiutil attach`, `sudo installer -pkg ... -target /`):
  app, VST3 and AU report 1.2.3 (`CFBundleShortVersionString`).
- Seed a 1.2.x library under a temp `HOME` from repo fixtures; hash it.
- Install this run's 1.3.0 installer DMG over it: app, VST3 and AU report 1.3.0; bundled rigs
  present; `pkgutil --pkgs` receipts updated; library hash unchanged by the installer;
  `auval -v` finds the 1.3.0 component.
- Launch the installed standalone once against the seeded library (temp `HOME`), quit
  gracefully (AppleScript quit or SIGTERM-free path the app handles), process exits, no crash
  report under `~/Library/Logs/DiagnosticReports` for VoLum.
- Gatekeeper: a quarantined copy of the DMG/pkg/app (`xattr -w com.apple.quarantine ...`)
  run through `spctl --assess` (install and exec types); the verbatim output goes to
  `$GITHUB_STEP_SUMMARY`. Recorded, not asserted (ad-hoc build).
- Log ends `ALL PASS <n> checks`.
