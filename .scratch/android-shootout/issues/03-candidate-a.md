# 03 Candidate A — Signal Flow

Status: blocked
Blocked by: 01 (start), 02 (review only counts once `feature/android-design` is merged in)
Branch: `feature/android-ui-a` off `feature/android-shootout`
Worker: claude-opus-5-5-high (with `/frontend-design` and `android-native-dev`)

## Direction

Desktop-faithful BUILD: the PRE / AMP / POST chain strip is always visible, the
selected section or block opens a focus editor below it, the knob row replaces
in place like the desktop. PLAY is a second surface with the hero art, the
sounds list and the pedal tiles. The phone in landscape is a small desktop done
right for fingers, not a shrunken desktop.

## Scope

May write: `NeuralAmpModeler/projects/android/ui-a/**`,
`NeuralAmpModeler/projects/android/app/src/a/**`. Reports to
`.scratch/android-shootout/reports/03-a-r<round>.md`. Only the reviewer writes
`verdicts/`.
Must not write: `core/`, `design/`, `harness/`, other candidates, `engine/`.
Needs something in `:core` or `:design`? Write it in the report under
"Requests"; the coordinator routes it. Do not work around it locally with a
copy of the identity.

## Acceptance (every round)

- `pwsh NeuralAmpModeler/scripts/android-box/verify-candidate.ps1 -Candidate a` exit 0 for the branch head.
- Cold Opus review per `rubric.md` → `verdicts/a.md` `verdict: pass` for that commit.
- Branch contains the accepted `feature/android-design` head.
- Pushed to GitHub.

## Verifier checklist

`rubric.md` in full, plus: uses `:design` components for every identity element
(no local colours, fonts, selection drawing or art), `featureId` on every
operable control, no scrolling on fixed control surfaces (only catalogs scroll).
