# Update notifier (must-ship, not a headline)

Locked map: `.scratch/release-1.3.0/map.md`. Contract: `backlog/F14-update-notifier.md` **except** the seen/badge rule below.

## Outcome

Notify-only. Standalone + plugin. Static GitHub Pages `appcast.json` on `release: published`. Badge on gear; Settings row / Check now / toggle; one-shot footer. No download, no in-place update, no identifiers.

## Delta vs F14 (locked)

An available version is **seen** when the player uses the **update row** or **Check now**. Opening Settings only **shows** the reminder; it does **not** clear the badge. (F14's "opened Settings once" would fire every time someone set MIDI channel.)

## S1 — appcast (can land first, nothing user-visible)

- Workflow `.github/workflows/publish-appcast.yml` on `release: published`.
- URL: `https://guitarlum.github.io/VoLum/appcast.json`
- Schema per F14 (`schema`, `stable.version/published/url/notes/downloads`, `message` reserved).
- GitHub Pages on `gh-pages` is a one-time repo setting — implement the workflow and a `docs/appcast` or `gh-pages` publish step; document the manual Pages enable in the ticket comments if you cannot flip it from here.

## S2 — client core

Files as F14: `VoLumUpdateCheck.h` (pure, no I/O), `VoLumUpdateState.h` sidecar `volum-update-state.json`, `VoLumHttpGet.h` / `.cpp` (WinHTTP) / `.mm` (NSURLSession), `VoLumUpdateCheck.inc.cpp`. Threading: detached thread captures `shared_ptr` by value, never `this`. Process-wide once-guard + 24 h machine throttle. Failures silent.

## S3 — UI

Accent dot on the settings control in **PLAY and BUILD**. Settings: update row above the version line, auto-check toggle, Check now. Footer one-shot below load-error priority. Badge clears only on update-row click or Check now.

## Tests (gate)

`test_volum_update_check.cpp`: version compare, manifest parse, truncated/empty/wrong-type rejected without throw, unknown keys ignored, throttle boundary, badge lifecycle (**Settings open does not clear**; row/Check now does). No test touches the network. Sidecar round-trip beside `test_volum_settings_atomic_write.cpp`. Register in CMake + vcxproj.

Prove revert-fail. Windows suite. `run-app-win.ps1` for badge/row. Offline launch: no badge, no stall, no error.

## Docs

Changelog. EN/DE: toggle + privacy (plain GET, no query string, no identifiers).

## Out of scope

Auto-update, code signing, telemetry, beta channel.
