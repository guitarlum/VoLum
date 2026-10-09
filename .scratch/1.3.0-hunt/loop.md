# 1.3.0-hunt

## Goal

Overnight, owner AFK: find and fix as many 1.3.0 bugs as machines can find (UI with real
input, monkey, code hunters, REAPER, audio loopback, installer upgrade, macOS CI), merge
guarded fixes and bit-identical refactors into `dev`, rewrite the docs, reshoot every
screenshot, add the Dual Amp GIF to the READMEs, and leave the owner only a short Mac
checklist. No main, no tag, no release PR, no version bump. Spec: `spec.md`.

## Predicate

- `pwsh .scratch/1.3.0-hunt/verify.ps1` exit 0 (every lane `[x]` below, every finding has
  a disposition, docs/GIF/screenshot/checklist structural checks, run-tests-win, -Asan,
  standalone build, e2e all `-RequireMidi`, REAPER harness, rate-switch stress on the
  UA-2X2 through ASIO4ALL, latest dev CI green on origin/dev)

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort 1.3.0-hunt`.
Do not trust chat memory. Continue until every Predicate line passes. Owner said: do not
stop until everything is done. Escalate (write to questions.md and continue other lanes)
on product calls, golden/state-format changes, irreversible git.

## Lanes

- [x] L0 stuck-click fix committed and pushed (red without fix proven)
- [ ] L1 code hunters (all bug classes reported back, findings triaged)
- [ ] L2 UI testers, real input (all surfaces reported back, findings triaged)
- [ ] L3 monkey script written, run >= 2 h total, findings triaged
- [ ] L4 REAPER VST3 harness extended (PC vs CC 102, save/reopen, two instances)
- [ ] L5 audio loopback glitch scan + driver switching
- [ ] L6 installer upgrade over installed 1.2.1
- [ ] L7 macOS CI evidence (REAPER AU, standalone screenshot, universal check, Rosetta auval)
- [ ] L8 docs rewrite (EN/DE guides + READMEs), checked against the app
- [ ] L9 all screenshots reshot + Dual Amp GIF on README top + release-notes draft
- [ ] L10 opportunities: safe ones merged, others on feature/post-1.3.0-opt
- [ ] L11 handoff: UAT checklist trimmed, laptop settings restored, morning summary

## Standing orders (owner, 2026-10-09 02:33)

- Full authority over the laptop. Owner asleep; does the Mac pass tomorrow.
- Git: HTTPS remote URL only, never edit git config, no force-push. Bugs on
  `feature/hunt-NN`, safe refactors on `feature/opt-NN`, others on `feature/post-1.3.0-opt`.
  Each merged into dev only after: regression test red without the fix, cold verifier
  from a different model family, CI green (dispatch with ci-watch.ps1 -Dispatch).
- Guarded: golden render or preset/state-format changes -> report only. UX judgment calls
  -> questions.md, no behavior change.
- Serial: one MSBuild writer and one real-input desktop driver at a time. Worktrees:
  never junction submodules; `git submodule update --init` inside the worktree.
- Hardware: UA-2X2 only (L out -> In 1 loopback, direct monitor off; WASAPI "Speakers
  (UA-2X2)"/"Line (UA-2X2)", ASIO via ASIO4ALL; the Valeton ASIO driver does not open
  it). No HX on Windows. loopMIDI port `VoLum Loop`. REAPER portable `C:\REAPER\reaper.exe`.
- Docs: short and concise (documentation-writer skill). GIF: PLAY in Dual Amp, THC Sunset
  MAIN left, Soldano SLO100 SUPPORT right, synthetic DI, `docs/volum-dual-amp.gif`.

## Laptop settings changed overnight (restore at the end)

- HKCU\Control Panel\Desktop DelayLockInterval: was 900, now 4294967295
- HKCU\Control Panel\Desktop ScreenSaveActive: was 1, now 0
- powercfg monitor-timeout-ac: was 10 min (0x258), now 0; standby-timeout-ac was 0, now 0
- powercfg LIDACTION AC: was 1 (restored to 1 last time), now 0

## Verifier

- hash: (none yet)
- verdict: (none yet)

## Children

(none)

## Ledger

| When | What | Predicate |
| --- | --- | --- |
| 09/10 02:35 | Loop created. Loopback UA-2X2 WASAPI OK (5/5 bursts, 62.7 ms, noise floor 0). VoLum Loop port present. Laptop AFK settings applied. | no |
| 09/10 02:45 | L0: stuck-click test red without fix (2 checks), full suite 1257 green, pushed 5e4c00d7 (dev CI running). Standalone built 02:41. Spawned: 8 code hunters (opus/gpt/grok), docs rewrite (wtdocs, opus), mac evidence CI (wtmacci, gpt), UI tester Settings/devices/Pack (real input, opus), monkey script writer (wtmonkey, gpt), REAPER harness phase 1 (wtreaper, opus). | no |
| 09/10 02:55 | MIDI hunter f38d7832: F-01..F-07. Note: Windows standalone has DirectSound + ASIO only (no WASAPI backend). Workers: hunt-01 headless custom recall (wt01, opus 8f8d1ee9), hunt-02 iPlug2 APP host F-02..F-07 (wt02, gpt b0b7ac3a; iPlug2 fork branch volum/hunt-02-app-host). | no |
| 09/10 03:10 | State, DSP, preset, thread hunters done: F-08..F-40 triaged (reported: F-13,16,21,26; questions Q-02..Q-05). Monkey script pushed 7878385a (feature/hunt-monkey). Workers: hunt-03 params path F-08+F-40 (wt03, gpt 6e59de48), hunt-05 reset/audio F-37,38,25,23 (wt05, opus a24a992c). Queue (worktrees being prepared): 04 unserialize F-10,11,39; 06 AudioDSPTools F-17,18,24; 07 tuner/chorus F-19,20,22; 08 selection/idle F-27,30; 09 pack/repair F-28,31,34; 10 small model F-29,32,33,35; 11 scene migration F-09; 12 settings race F-12. CI policy: workers don't dispatch; coordinator merges verified branches into an integration branch, runs local suite+e2e, one full CI per batch, then dev. Max 4 concurrent build workers. | no |
| 09/10 03:02 (wall clock) | Docs rewrite 8cfb4543 (feature/hunt-docs) done; resumed e7f43381 for F-36, F-57, Gatekeeper wording, +4 dBu claim. vendor-denylist.txt copied into all existing worktrees (re-copy for wt06+). REAPER harness phase 1 92569363 (feature/hunt-reaper, unpushed, not run): 5 MIDI/roundtrip/2-instance/offline-vs-realtime scenarios, -Scenario, -Sandbox. Phase 2 (run+fix) waits for the desktop. F-62 suspected (host param view after MIDI recall). | no |
| 09/10 03:09 | Docs f66ea3bf: cold verifier 610360a5 (gpt) FAIL: Pack same-name rule (identity is by ID) and README POST Chorus placement wrong; nits German headings, length 547 lines. Resumed writer e7f43381 to fix and trim; re-verify with a fresh cold verifier after. | no |
| 09/10 03:35 | Docs bc8dcd59 (474 lines, German headings, Pack identity + Chorus fixed); fresh cold verifier af5cde08 (grok) running. UI tester A b1787d8d done: no crash/hang, stuck-click fix holds; F-63..F-71, Q-07 (RELEASE BLOCKER: enable GitHub Pages; update check is new in 1.3.0, has_pages=false), Q-08, Q-09. New queue: hunt-16 output mode persist (F-63), hunt-17 appcast notes/message (F-64), hunt-18 small UI (F-66, F-67, F-70). Desktop: UI tester B (BUILD/presets/pedals/IR/shortcuts) 1c820716 started. | no |
| 09/10 03:36 | Mac evidence a638e4bb (feature/hunt-mac-ci), run 37869916350 green on dev 5e4c00d7 artifact: universal binaries, installer + strict codesign, Rosetta AU auval, standalone lifecycle PASS; Gatekeeper rejects ad-hoc (expected); REAPER AU/VST3 SKIP (runner mic/audio dialogs). Resumed a6e3835a for phase 2 (TCC pre-grant, reaper.ini, BlackHole). Worktrees wt13..wt18 being created. | no |
| 09/10 03:42 | Docs bc8dcd59: cold verifier af5cde08 (grok) FAIL: standalone Pack import needs "Also restore machine settings" (unticked) for PLAY list; custom IR also with AMP/DI files. Resumed writer with these + nits + UI-tester doc items (F-67 CC0, F-68 tab per session, F-65 DirectSound line, F-70 recipe IR name). Third cold verifier (gpt) after. | no |
