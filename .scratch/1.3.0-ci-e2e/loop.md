# 1.3.0-ci-e2e

## Goal

Every dev CI run proves the Windows and macOS installers upgrade 1.2.x cleanly and the macOS
standalone passes fresh / upgrade / MIDI / screenshot end-to-end checks, so 1.3.0 can ship
without a Mac in hand (spec: `.scratch/1.3.0-ci-e2e/spec.md`).

## Predicate

- `pwsh .scratch/1.3.0-ci-e2e/verify.ps1` exit 0
- A cold verifier from a different model family accepted each ticket's diff (## Verifier).

Bounds: tickets 01-03 only; cut-off Monday 12/10/2026 08:00. At the cut-off, stop, append a
ledger row and leave a short morning summary in `.scratch/1.3.0-ci-e2e/morning.md`.

Safety: the real `%LOCALAPPDATA%\VoLum` stays at its baseline (verify.ps1 checks it). Never
run the new installer/e2e scripts on this laptop: they install into Program Files and
refuse outside GitHub Actions. Never run `run-app-win.ps1 -RealLibrary`. Never touch `main`,
tags, releases or the version number. Never change git config. No force-push.

## Wake prompt

First tool call: `pwsh NeuralAmpModeler/scripts/conductor-status.ps1 -Effort 1.3.0-ci-e2e`.
Do not trust chat memory. Continue until every Predicate line passes.
Escalate and stop on new product, new sound / golden retune, irreversible git, or a real dead end.

## Verifier

- hash: e3b0c44298fc1c14
- verdict: accepted
- ticket 01: `feature/ci-e2e-01` @ `47b89fae`, CI 38180953415. Cold verifier GPT accepted.
- ticket 02: `feature/ci-e2e-02` @ `0c6f6b9a`, CI 38198542144 green. Job `macOS installer upgrade` logged `NEGATIVE CONTROL OK` and `ALL PASS 107 checks`. spctl recorded (DMG, pkg, app rejected, exit 3), not asserted. Cold verifier GPT first said the seed should be a repo fixture, then accepted: there is no checked-in fixture, and the 1.2.x app writes the library on the runner. 712-line script waived. Audio Error on a 1.2.x settings.ini is F-113, not a script defect.
- ticket 03: `feature/ci-e2e-03` @ `09ca2eb9`, CI 38209565384 green. Job `macOS standalone e2e` logged `NEGATIVE CONTROL OK` and `ALL PASS 86 checks`, and uploaded `mac-e2e-shots`. Cold verifier GPT accepted. The IR-count fix measures the file inside the content folder. 909-line script waived.
- final records: cold verifier accepted the ticket statuses, run ids, log markers, F-113 and morning summary before the housekeeping commit; the recorded hash is the clean post-commit worktree hash.

## Children

(none)

## Ledger

| When | What changed | Predicate moved |
| --- | --- | --- |
| 11/10 22:45 | Loop written. Owner enabled GitHub Pages (source: GitHub Actions); the github-pages environment only admits branch `main`, so a tag rule `v*` is needed (owner asked to add it). Local dev is 1 commit ahead of origin (`22b65a02`, hunt ledger, .scratch only); the first merge pushes it. Real library baseline 2026-10-09 20:24:21.703. | no |
| 11/10 22:55 | Owner approved: tag rule `v*` added to the github-pages environment (now `main` branch + `v*` tag), so the release appcast deploy is admitted. | no |
| 11/10 23:10 | Claimed ticket 01. Worktree `C:\dev\VoLum-ci-e2e-01` on `feature/ci-e2e-01` (`-NoSubmodules`; this ticket does not build). Local dev is still 3 scratch commits ahead of origin; those get pushed immediately before the first merge so `merge-verified-branch.ps1` sees local==origin. Spawned one local worker. | no |
| 12/10 01:40 | Ticket 01 green on 47b89fae (run 38180953415, NEGATIVE CONTROL OK, ALL PASS 93). Cold verifier accepted. Merging into dev. | partial |
| 12/10 01:45 | Merged feature/ci-e2e-01 into dev as c3f1e3e8 and pushed. The push started dev CI; not dispatched again. Claimed ticket 02. | partial |
| 12/10 06:15 | Ticket 02 green on 0c6f6b9a (run 38198542144, NEGATIVE CONTROL OK, ALL PASS 107, spctl recorded). Cold verifier accepted after withdrawing the fixture defect. F-113 filed for the 1.2.x settings.ini audio failure. Two pluginval failures on 38192562105 were a runner GPU flake; the same head passed on 38198542144. Merging into dev. | partial |
| 12/10 06:40 | Merged feature/ci-e2e-02 into dev as fc1db78f and pushed. The push started dev CI; not dispatched again. Claimed ticket 03. | partial |
| 12/10 07:50 | Cut-off. verify.ps1 exit 1 (no macOS standalone e2e job on origin/dev, ticket 03 still claimed). Run 38205604479 failed 1 of 84 checks in the e2e script. Fix 09ca2eb9 dispatched as 38209565384 and left running. Not verified, not merged. morning.md written. Real library baseline unchanged. | no |
| 12/10 08:50 | Owner extended the loop by two hours. Run 38209565384 on 09ca2eb9 is green (NEGATIVE CONTROL OK, ALL PASS 86, mac-e2e-shots). Cold verifier accepted. Merging into dev. | partial |
| 12/10 09:05 | Merged feature/ci-e2e-03 into dev as b6ea72f7 and pushed. The push started dev CI. Waiting on it with ci-watch, not a second dispatch. | partial |
| 12/10 09:47 | Dev CI 38214773536 on b6ea72f7 is green, including both installer jobs and macOS standalone e2e. verify.ps1 exit 0. | yes |
| 13/10 10:25 | Final conductor records cold-reviewed and accepted. Committing ticket statuses, loop, morning summary and F-113; merged ci-e2e worktrees removed with the safe worktree script. | yes |
