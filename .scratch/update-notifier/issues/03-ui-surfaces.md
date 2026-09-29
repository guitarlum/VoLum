# Update notifier UI (S3)

Status: resolved
Blocked by: 02

## Goal

Gear badge, Settings row, toggle, Check now, one-shot footer. Badge does **not** clear on opening Settings.

## Do this

Kick check from `OnUIOpen()`; consume in `OnIdle()`. Process-wide once-guard. Badge on the settings opener in BUILD and PLAY (if PLAY exists, keep a settings control). Footer priority below load errors.

Launch `run-app-win.ps1`. Offline: no badge, no stall, no error. Optional: point `appcast` URL at a local file in a debug-only override for visual check — do not ship a hidden always-on override.

## Tests

Badge-lifecycle cases in `test_volum_update_check.cpp` (Settings-open vs row vs Check now). Source pin: settings opener does not call `MarkSeen`.

## Done when

Standalone judged. Revert-fail proven. Docs in ticket 04.
