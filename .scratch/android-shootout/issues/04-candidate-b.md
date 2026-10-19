# 04 Candidate B — Stage Floor

Status: blocked
Blocked by: 01 (start), 02 (review only counts once `feature/android-design` is merged in)
Branch: `feature/android-ui-b` off `feature/android-shootout`
Worker: gpt-5.6-sol-high (with `/frontend-design` and `android-native-dev`)

## Direction

Performance first. Home is PLAY: the sounds as big tiles with their moving art,
bypass, tuner and scene switching hit from arm's length on a phone clipped to a
mic stand. BUILD is a pedalboard: PRE pedals, the amp and POST effects laid out
as a board you tap into; editing opens large, fingertip-sized controls. Every
desktop feature is still reachable within 3 taps.

## Scope

May write: `NeuralAmpModeler/projects/android/ui-b/**`,
`NeuralAmpModeler/projects/android/app/src/b/**`. Reports to
`.scratch/android-shootout/reports/04-b-r<round>.md`.
Must not write: `core/`, `design/`, `harness/`, other candidates, `engine/`.
Needs something in `:core` or `:design`? Write it in the report under
"Requests"; the coordinator routes it.

## Acceptance (every round)

- `pwsh NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate b` exit 0 for the branch head.
- Cold Opus review per `rubric.md` → `verdicts/b.md` `verdict: pass` for that commit.
- Branch contains the accepted `feature/android-design` head.
- Pushed to GitHub.

## Verifier checklist

`rubric.md` in full, plus: uses `:design` for every identity element,
`featureId` on every operable control, live controls ≥ 64 dp, no scrolling on
fixed control surfaces (only catalogs scroll).
