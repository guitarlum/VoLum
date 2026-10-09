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
- [x] L1 code hunters (all bug classes reported back, findings triaged)
- [x] L2 UI testers, real input (all surfaces reported back, findings triaged; A b1787d8d, B 1c820716, C a9f7d217)
- [ ] L3 monkey script written, run >= 2 h total, findings triaged
- [ ] L4 REAPER VST3 harness extended (PC vs CC 102, save/reopen, two instances)
- [x] L5 audio loopback glitch scan + driver switching
- [ ] L6 installer upgrade over installed 1.2.1
- [ ] L7 macOS CI evidence (REAPER AU, standalone screenshot, universal check, Rosetta auval)
- [x] L8 docs rewrite (EN/DE guides + READMEs), checked against the app (merged 07bf039b; later behavior fixes may need doc touch-ups)
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
| 09/10 03:58 | Docs: 3922e7c7 third verifier 600cd42e FAIL (IR per channel); coordinator fix 0a8f9cb8, re-verified PASS. Merged feature/hunt-docs into dev (07bf039b) + changelog, pushed bdf6a048 (also pushes the .scratch loop commits). L8 [x]. F-36, F-57, F-68 fixed. | no |
| 09/10 04:15 | hunt-17 appcast notes 851dca17 + drain 3f2414ea (coordinator, verifier nit) cold-verified PASS (claude-sonnet f95c9591); merged into dev fc843df1 (CI-only change, no C++). Changelog conflicts: keep both lines, newest branch line on top. dev CI 37873537386 queued (bdf6a048 run cancelled by concurrency). | no |
| 09/10 04:22 | Mac evidence phase 2 fb03dc43 (run 37873627110): TCC grants + sheet clicker got REAPER running; AU render/PC1/CC102/reload PASS; VST3 render/CC102/reload PASS, VST3 PC 1 FAIL (reproducible) -> F-72. hunt-19 VST3 PC (wt19, opus 0b7b707e) spawned as 5th build (headline feature). Mac REAPER checks leave the owner checklist except a re-run after hunt-19. | no |
| 09/10 04:58 | UI tester B 1c820716 done: no crash/hang/data loss; F-73..F-83, Q-10..Q-12. New units: hunt-20 IR robustness (F-73..F-76), hunt-21 iPlug2 text entry (F-77), hunt-18 grows (F-78..F-82); F-83 docs batch. Desktop: UI tester C (PLAY/Dual/tuner/metronome, Factory + copy of real library) a9f7d217. Build workers 01/02/03/05 still working (uncommitted changes in their worktrees), hunt-19 VST3 PC running. | no |
| 09/10 05:23 | hunt-03 3906dbfb (gpt 6e59de48): F-08, F-40 fixed, red 2 / green, ASan 1225/1225, golden unchanged; full suite 1259/1260 (realtime budget flake under load). Cold verifier a6f0d11d (opus). Build slot -> hunt-04 unserialize F-10, F-11, F-39 (wt04, gpt 591a9f62). F-84 flakes, F-85 guard script worktree bug. | no |
| 09/10 05:27 | hunt-02 29ca2278 + iPlug2 4a9af5d0 (gpt b0b7ac3a): F-02..F-07 fixed, red/green each, suite 1263 + ASan 1228 green; worker dispatched CI 37878538860 (pre-policy brief). Cold verifier 0436a709 (opus). Coordinator must run e2e midi -RequireMidi on the integration build. F-86 UB in IPlugAPP_dialog. Build slot -> hunt-11 scene migration F-09, F-47, F-48 (wt11, opus 021addf6). | no |
| 09/10 05:35 | hunt-01 c3b9de71 (opus 8f8d1ee9): F-01 fixed (routing committed headless; also preset recall + session restore of custom amps), structural regression red/green, ASan green, suite 1258/1259 (realtime-budget load flake). Worker dispatched CI 37879465244 (pre-policy). Cold verifier 35c13878 (gpt). Build slot -> hunt-14 pack robustness F-49,50,51,54,55,71 (wt14, gpt 7fbe6ee5). F-85 guard scripts fixed by coordinator 22ef8082, verifier c3b04ca3. | no |
| 09/10 05:40 | hunt-19 2696f8dd + iPlug2 410255270 (opus 0b7b707e): F-72 root cause = REAPER delivers VST3 PC only via IUnitInfo program list (getUnitByBus + kIsProgramChange param), rounding secondary; 7/8 new tests red on old code; suite 1265 green; VST3 built; COM probe OK. F-87 VST3 MIDI channel 1 only. Verifier 20ac1d0a (gpt). Build slot -> settings unit F-12 + F-63 (wt12, opus 60ec32a7; hunt-16 folded in). | no |
| 09/10 05:43 | hunt-02 verifier 0436a709 (opus) FAIL: F-07 probe failure persists channel 1/1; OK after Apply+revert skips audio restart (regression); clang-format red on new test (worker CI 37878538860 Formatting failed). Should-fix F-05 fallback persist path, legacy MIDI name ambiguity, port reopen on OK; F-86 UB confirmed. Resumed b0b7ac3a (no CI dispatch). 6 builds running briefly (incremental rework). | no |
| 09/10 05:47 | hunt-03 verifier a6f0d11d FAIL (F-08 swap on audio thread races idle-tick save; tests source-scraping; no ASan/timing); F-40 approved. Resumed 6e59de48 with pending-bitmask + main-thread swap design. F-85 verified PASS (c3b04ca3), merged dev 362bbb13, wtguards removed. F-84 is pre-existing (pack test shared temp dir) -> hunt-22. | no |
| 09/10 05:52 | hunt-01 verifier 35c13878 (gpt) PASS; comment nit fixed 0a4cb0e7. Merge into dev once CI 37879465244 is green (e2e standalone runs in the next desktop window on dev). Integration note for hunt-04: defer the whole custom selection/routing transaction to the main thread, not gated on the editor, not duplicated in OnUIOpen. | no |
| 09/10 05:57 | hunt-19 verifier 20ac1d0a (gpt) FAIL: program param changes from project reload become live recalls (Cubase dummy PC on reload); structure/IDs/decode approved; F-87 confirmed pre-existing. Resumed 0b7b707e for reload guard + behavioural reload test. | no |
| 09/10 06:10 | UI tester C a9f7d217 done (real library untouched): F-88 (DATA LOSS: Pack import with restore machine settings saves old live sound over restored amp -> hunt-23 top), F-89 sidebar stale custom amp after import, F-90 Tab reloads custom SUPPORT capture, F-91 footer internal filename, F-92 custom channel No Cab stick; Q-13, Q-14 (PLAY CPU 97-119% of a core). L2 UI testers A/B/C complete. Desktop: monkey runner 724038e9 (~100 min). Then: e2e on dev, REAPER phase 2 after hunt-19 rework, L5 audio, L6 installer, L9 screenshots/GIF. | no |
| 09/10 06:16 | hunt-01 CI 37879465244 green (all jobs) -> merged dev 97f858af; .gitattributes changelog merge=union e858a96d (stops per-merge changelog conflicts). F-01 fixed. e2e standalone on dev pending next desktop window. | no |
| 09/10 06:32 | hunt-05 84f184c3 (opus a24a992c): F-37 ResetExclusion try-lock, F-38, F-25, F-23 fixed; red/green per pin + ASan race test; ASan 1237/1237, Release 1271/1272 (budget flake). Cold verifier f37a0a68 (gpt). F-93, F-94 logged. Build slot -> hunt-23 F-88 Pack restore overwrite (wt23, opus 64822050). | no |
| 09/10 07:26 | hunt-19 rework 993e49a3 + iPlug2 1e0d6b85e: kCanAutomate dropped, VST3ProgramRestoreGuard (0.5 s, stopped transport), 8/15 red -> 15/15 green; re-verify 20ac1d0a; CI 37888368095 dispatched on the branch; mac evidence phase 3 (a6e3835a) waits for it and runs VST3 PC + reload checks. Windows REAPER phase 2 after the monkey run. | no |
| 09/10 07:28 | hunt-05 verifier f37a0a68 (gpt) FAIL: _UpdateLatency reads live DSP pointers after unlock; latency ignores mVolumSupportSelected (F-93); no timing evidence. Exclusion, F-38, F-25, F-23, golden approved. Resumed a24a992c. | no |
| 09/10 07:30 | hunt-19 re-verify 20ac1d0a (gpt) FAIL: kCanAutomate drop spec-correct; guard reuses stale kPlaying when ProcessContext is absent, Arm/EndBlock race (needs generation/CAS), armed forever on zero-frame/suspended processing, wrapper-level tests missing. Resumed 0b7b707e. Mac phase 3 a6e3835a keeps running on CI 37888368095: its PC-without-kCanAutomate result still holds, its guard result will be superseded. | no |
| 09/10 07:45 | hunt-03 rework 08c6e5cf (gpt 6e59de48): F-08 mode changes queued atomically, applied on main thread before idle save / serialize; F-40 + latency safety; 5/5 behavioral tests; ASan 1226/1226; Release 1260 + realtime-budget flake; timing table supplied. Re-verify a6f0d11d (opus), asked to check overlap with hunt-05 latency work. CI 37890075013 dispatched. No new build worker: 8 builds already running on a loaded machine. | no |
| 09/10 07:50 | Monkey runner 724038e9 done: ~100 min, 3,287+ events (smoke, keyboard, factory 15', mouse, overlay 15', PLAY 15'): no crash, hang, dump, log error or content corruption; peak 347 MB. Harness fixes 764c51db, a59b27dc on feature/hunt-monkey. F-95 startup refusal after a stress run (likely single-instance guard vs killed/foreign process). L3 still needs ~20 min more for 2 h. Desktop -> dev standalone rebuild + e2e all + midi (_dev-e2e.log). | no |
| 09/10 08:01 | dev e858a96d standalone rebuilt; e2e all 451 PASS (2 SKIP: seed has no custom amp scene), e2e midi -RequireMidi 20/20 PASS (_dev-e2e.log). Desktop -> L5 audio lane 27d02eef (loopback glitch scan with output on OUT 2, driver/buffer/rate switching, rate-switch stress ASIO4ALL 128/48000, F-95 watch, CPU vs Q-14). | no |
| 09/10 08:15 | hunt-03 re-verify a6f0d11d (opus) FAIL: Oktaverb sub-mode still loses its snapshot (save updates remembered sub-mode), stale Oktaverb requests, tests never run plugin code. Approved: lock-free mask, other modes, _UpdateLatency under mStagingMutex (merges clean with hunt-05), golden/state unchanged, 1261/1261. Resumed 6e59de48 (helper the plugin calls + runtime red). CI 37890075013 result moot until rework. Note: run-tests-win.ps1 stops early under Windows PowerShell 5 (guard git stderr); use pwsh. | no |
| 09/10 08:47 | Mac evidence phase 3 a6e3835a PASS (source CI 37888368095 green, evidence 37894739681, feature/hunt-mac-ci fc2b9fcf): AU + VST3 render, PC1, CC102=2, reload all PASS; VST3 PC delivered without kCanAutomate; reopen appends no `[midi] recall`. L7 stays open for one re-run on the reworked hunt-19 guard. | no |
| 09/10 08:57 | hunt-11 265856f1 (opus 021addf6): F-09, F-47, F-48 fixed (legacy customScenes written back unchanged, copy-not-move, unreadable IR stays uncalibrated); 4 tests red 26/58 -> green; suite 1260/1261 + flake; no format/golden change claimed. Verifier bb23893d (gpt, asked to judge whether writing customScenes back counts as a format change). CI 37896275378. F-96 stale e2e scene checks. L6 must include a real 1.2.1 library upgrade (custom amps + old project, quit without saving, reopen). Writer saw LNK1000 under memory pressure: no new build workers until the count drops. | no |
| 09/10 09:00 | hunt-02 rework ec6dfc54 + iPlug2 f94f4eb7 (gpt b0b7ac3a): F-07 probe keeps saved routing, Apply->revert restarts audio, F-05 fallback never persisted, MIDI namever=2 + legacy migration + IDOK reconnect, F-86 UB, format clean; F 2/6 -> 6/6, Prefs 0/1 -> 1/1; full 1263/1264 + flake; ASan 1229/1229. Re-verify 0436a709 (opus, incl. 1.2.1 downgrade with namever=2). CI 37896570821. Desktop todo after L5: e2e midi -RequireMidi on the hunt-02 build. | no |
| 09/10 09:10 | L5 audio lane 27d02eef done (under build load): loopback 20.35 ms RT ASIO4ALL 48k/128 clean; 445 s DirectSound glitch scan; 25 driver/buffer/rate applies OK, persistence 7/7; rate-switch stress 4/4 PASS; F-95 0/12 after normal close (harness). F-97 Dual Amp toggle click 6/6 (-> hunt-24), F-98 small toggle steps (low conf), F-99 DirectSound no 48k + misleading notice, F-100 DirectSound phase jumps under load. CPU @128 ASIO: Soldano 68%/91% BUILD/PLAY, 2204 29/52, 2204+Sunset Dual 48/65. Tooling (glitchscan + loopback-safe -SeedIni, window polling) formatted + merged dev eeb55579. L5 ticked. Desktop -> monkey top-up (keyboard + mouse 15' each). | no |
