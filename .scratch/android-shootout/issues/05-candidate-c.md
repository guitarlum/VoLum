# 05 Candidate C — Studio Instrument

Status: blocked
Blocked by: 01 (start), 02 (review only counts once `feature/android-design` is merged in)
Branch: `feature/android-ui-c` off `feature/android-shootout`
Worker: grok-4.7-high (with `/frontend-design` and `android-native-dev`)

## Direction

The hero art is the instrument. It fills the screen; PRE, AMP and POST blocks
float over it as overlays that glow with the signal. Tapping a block raises a
contextual bottom sheet with its controls; the art stays visible above it.
Metering is rich and alive (input, output, gate activity, tuner as an overlay on
the art). It should feel like holding a premium hardware modeler.

## Scope

May write: `NeuralAmpModeler/projects/android/ui-c/**`,
`NeuralAmpModeler/projects/android/app/src/c/**`. Reports to
`.scratch/android-shootout/reports/05-c-r<round>.md`.
Must not write: `core/`, `design/`, `harness/`, other candidates, `engine/`.
Needs something in `:core` or `:design`? Write it in the report under
"Requests"; the coordinator routes it.

## Acceptance (every round)

- `pwsh NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate c` exit 0 for the branch head.
- Cold Opus review per `rubric.md` → `verdicts/c.md` `verdict: pass` for that commit.
- Branch contains the accepted `feature/android-design` head.
- Pushed to GitHub.

## Verifier checklist

`rubric.md` in full, plus: uses `:design` for every identity element,
`featureId` on every operable control, overlays never hide a value the user is
editing, bottom sheets never cover the control they were opened from.
