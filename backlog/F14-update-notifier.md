# F14 — In-app update notifier

Plan a subtle, opt-out update notification for standalone and plugin builds. Design is **settled** (resolved in a grilling session); this ticket exists to schedule and implement it, not to re-open the design.

## Problem

Release discovery is entirely push-based and depends on the maintainer posting to Reddit or YouTube. Most VoLum users are guitar players with no GitHub account who do not watch the repo, so a meaningful share of the install base runs an old build indefinitely and has no way to find out otherwise. Bug fixes only reach the people who happen to see a post.

## Scope

**In:** an in-app notification that a newer version exists, with a link out to the download page.

**Out, deliberately:** downloading anything; in-place update / file swapping / relaunch (the Sparkle/WinSparkle model); telemetry, analytics or unique identifiers; a beta/prerelease channel.

The auto-update exclusion is not primarily about effort. Windows `signtool` is commented out in `scripts/makedist-win.bat` and macOS builds run `CODESIGN=0` in `makedist-mac.sh` with `TODO` credentials. An app that silently downloads an unsigned binary and asks the user to run it trains exactly the wrong instinct, and the SmartScreen/Gatekeeper warnings would make it feel *less* trustworthy than today's "go to the website" flow. **Auto-update is blocked on code signing, not on this ticket.** If signing ever lands, the manifest designed below already carries the per-platform installer URLs needed to build on top of it.

## Locked decisions

- **Notify only.** Clicking through opens the browser.
- **Static JSON manifest** on GitHub Pages, not the GitHub Releases API. Avoids the 60-request/hour unauthenticated IP limit, is ~500 bytes instead of ~80 KB, and we own the schema so fields can be added later.
- **Published on the `release: published` event**, never during the build. `release-native.yml` creates a *draft* that is published by hand afterwards, so writing the manifest at build time would announce a version whose page still 404s for everyone. An event-triggered workflow structurally cannot do this, and it also covers releases created or edited manually.
- **Standalone and plugin both check**, throttled to one request per machine per 24 h, with a process-wide once-guard so eight plugin instances in one session produce at most one request.
- **Subtle surfacing, nothing to dismiss.** The explicit anti-goal is Reaper's startup modal, which interrupts before the user has done anything and teaches people to reflexively close it.
- **On by default, zero identifiers.** A plain GET of a static file with no query string. GitHub Pages logs are not visible to us, so appending `?version=&os=` would leak information without producing usable data. One toggle in Settings, disclosed in both user guides.
- **State in a new sidecar,** `volum-update-state.json`, not in `volum-settings.json`. Plugin instances deliberately never rewrite the main settings file (multi-instance conflict), so throttle state living there would never persist from a plugin and every DAW launch would re-check. A dedicated file can be written atomically from all formats, cannot version-conflict the main settings schema, and a lost race is harmless — it just means one extra check tomorrow.
- **Seen means done.** Opening Settings once records the version as seen and clears the badge permanently; only a strictly newer version raises it again.
- **The destination URL comes from the manifest,** not hardcoded, so it can be repointed from the GitHub release page to a friendlier download page later without shipping a new build.

## Manifest

Served at `https://guitarlum.github.io/VoLum/appcast.json`:

```json
{
  "schema": 1,
  "stable": {
    "version": "1.3.0",
    "published": "2026-08-14",
    "url": "https://github.com/guitarlum/VoLum/releases/tag/v1.3.0",
    "notes": "One-line plain-language summary.",
    "downloads": {
      "win": ".../VoLum-v1.3.0-windows-setup.exe",
      "mac": ".../VoLum-v1.3.0-macos-installer.dmg"
    }
  },
  "message": null
}
```

The first client reads only `stable.version`, `stable.notes` and `stable.url`. `downloads` and `message` are **reserved and deliberately unused** — they exist so a later client can consume them while today's client ignores them, and so old clients degrade gracefully rather than choking on unknown keys. `message` is the hook for non-release announcements ("1.3.0 has a known issue at 96 kHz", "new rig pack available") if that is ever wanted.

## Architecture

New files:

- `VoLumUpdateCheck.h` — pure logic, no I/O. Parses a version string into a triple, compares against `PLUG_VERSION_STR` from `config.h`, parses the manifest with the vendored nlohmann json, decides throttle and badge state. Kept free of I/O specifically so it is fully testable.
- `VoLumUpdateState.h` — sidecar read/write reusing `WriteJsonAtomically()` from `VoLumSettingsFileIO.h`. Fields: `lastCheckUtc`, `lastSeenVersion`, `latestKnownVersion`, `latestKnownUrl`, `autoCheck`.
- `VoLumHttpGet.h` — one function, `bool VolumHttpGetString(const char* url, std::string& out, int timeoutMs)`.
- `VoLumHttpGet.cpp` — WinHTTP. Ships with Windows, so no new dependency; use `#pragma comment(lib, "winhttp.lib")` rather than editing `AdditionalDependencies` across six configuration blocks in each of the app and vst3 vcxproj files.
- `VoLumHttpGet.mm` — NSURLSession. Plain HTTPS needs no ATS exception.
- `VoLumUpdateCheck.inc.cpp` — orchestration, tail-included from `NeuralAmpModeler.cpp` like the other `.inc.cpp` siblings.
- `tests/test_volum_update_check.cpp`
- `.github/workflows/publish-appcast.yml`

Platform-native HTTP was chosen over libcurl, cpp-httplib+OpenSSL, and iPlug2's `IWebView` because this code ships inside a VST3/AU loaded into someone else's process. WinHTTP and NSURLSession add no dependency, no bundled CA certificate store and no binary weight, and inherit OS certificate validation and system proxy settings for free.

Existing files touched:

- `VoLumPaths.h` — add `VolumUpdateStateFilePath()` beside `VolumUserSettingsFilePath()` (line ~201).
- `NeuralAmpModeler.cpp` — kick the check from `OnUIOpen()` (~1133), consume the latched result in `OnIdle()` (~782).
- `VoLumLayoutBuild.inc.cpp` — badge dot on the settings opener (the control at ~1063 that calls `HideAnimated(false)` on `kCtrlTagSettingsBox`).
- `NeuralAmpModelerControls.h` — update row above the existing version line in `AboutControl` (~1322), plus the auto-check toggle and a "Check now" button.
- `VoLumCoreControls.h` — `VoLumFooterControl` (519) already carries status text; the one-shot update line sits at a priority *below* load errors.
- Build files: app and vst3 vcxproj, the Xcode project (APP, VST3, AU targets), and both `tests/CMakeLists.txt` and `NeuralAmpModeler-Tests.vcxproj` — the latter two together, or `check-test-source-parity.ps1` fails.
- `installer/changelog.txt`, `docs/user-guide.en.md`, `docs/user-guide.de.md`.

### Threading

The hazard is a detached thread outliving a plugin instance. Follow the `VoLumLoader.inc.cpp` pattern, simplified: `OnUIOpen()` checks a process-wide once-flag, then the 24 h throttle, then spawns a detached thread that captures a `std::shared_ptr<UpdateResult>` **by value and never touches `this`**, so instance destruction during a request is safe. `OnIdle()` reads the slot and sets the badge plus the one-shot footer line.

Every failure path — offline, DNS failure, timeout, malformed JSON, a sandboxed AU host with no network access — is silent and leaves the badge unset. The check only runs when a UI exists, so headless/offline render usage never makes a request.

## UX

A small accent dot on the settings button, persisting until Settings is opened. Inside Settings, above the existing `Version 1.2.1 x64 VST3` line, an "Update available: 1.3.0 — What's new" row that stays as a passive reminder. Once per new version, a single footer line at startup that yields immediately to the rig name or any load error. A "Check for updates automatically" toggle and a "Check now" button in Settings.

## Slicing

- **S1 — manifest and publishing** (~2-3 h). The `release: published` workflow, the gh-pages branch, and a one-time manual step to enable GitHub Pages on it. Ships safely on its own with no client change and nothing user-visible, so it can land early and independently.
- **S2 — client core** (~1 day). HTTP on both platforms, version/manifest logic, the sidecar, tests. The fiddly part is build wiring, especially adding the `.mm` to three Xcode targets.
- **S3 — UI surfaces** (~0.5-1 day). Badge, Settings row, toggle, footer line.
- **S4 — docs** (~1-2 h). Changelog, both user guides including the privacy disclosure.

Roughly **2-3 focused days**. If it needs trimming, the honest cut list is the "Check now" button, the one-shot footer line and the `message` field — perhaps half a day combined. S1 through S3 are close to indivisible; there is no useful half-version.

## Why it should be early rather than late

This feature has a **one-release lag before it produces any value**. Nobody running 1.2.1 or earlier will ever be notified, because the notifier does not exist in their build. Whichever release ships it still needs the usual Reddit/YouTube push to seed it, and the benefit only starts with the release *after* that. Ship it in 1.3.0 and 1.3.1 is the first release that finds its own audience; slip it to 1.4.0 and that becomes 1.4.1. That is the main argument for prioritising it over a more visible feature despite it being invisible to existing users on the day it ships.

## Risks

- **Xcode project editing** for the `.mm` across three targets is the most likely source of build pain; universal (arm64 + x86_64) builds must be verified.
- **GitHub Pages must be enabled** on the gh-pages branch as a one-time manual step; forgetting it means the manifest 404s and the feature silently does nothing (it at least fails safe).
- **A malformed manifest** must never break the app. Covered by tests, but it is the one input that comes from outside the binary and it is parsed on a background thread inside a host process — worth a `review-security` pass.
- **AU in sandboxed hosts** may have no network access. Must fail silently.
- The manifest is a live broadcast channel into users' DAWs. Only the maintainer can write it, but a mistake reaches everyone within 24 h.

## Acceptance criteria

- Version compare, manifest parsing, throttle boundary and badge lifecycle are covered by `test_volum_update_check.cpp`, including truncated/empty/wrong-type manifests rejected without throwing and unknown keys ignored. No test touches the network — `VolumHttpGetString` stays out of the test target entirely, which is why the fetch and the logic live in separate headers.
- A sidecar round-trip case sits alongside `test_volum_settings_atomic_write.cpp`.
- Windows tests green (`scripts/run-tests-win.ps1`); standalone launches and the badge plus Settings row are judged by eye (`scripts/run-app-win.ps1`).
- Offline launch shows no badge, no stall and no error.
- EN/DE guides document the toggle and the privacy behavior; changelog line added.

Implement on `feature/update-notifier` off the latest `dev`; merge back into `dev` once acceptance criteria are met. Never promote to `main` outside of a release. Recommended skills for the implementation chat: `volum-ui-change`, `volum-param-state-change`, `release-manager`; `review-security` afterwards.
