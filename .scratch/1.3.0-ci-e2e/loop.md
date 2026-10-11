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

- hash:
- verdict:

## Children

(none)

## Ledger

| When | What changed | Predicate moved |
| --- | --- | --- |
| 11/10 22:45 | Loop written. Owner enabled GitHub Pages (source: GitHub Actions); the github-pages environment only admits branch `main`, so a tag rule `v*` is needed (owner asked to add it). Local dev is 1 commit ahead of origin (`22b65a02`, hunt ledger, .scratch only); the first merge pushes it. Real library baseline 2026-10-09 20:24:21.703. | no |
