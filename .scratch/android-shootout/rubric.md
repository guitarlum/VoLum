# android-shootout — review rubric

Used by the cold Opus reviewer (the gate) and the GPT-5.6 second reviewer
(advisory) for each candidate, and with criteria 1 and 6 only for ticket 02.

## Before scoring

1. Read `c:\Users\SteffenDangmann\.agents\skills\frontend-design\SKILL.md` and
   `.scratch/android-shootout/spec.md` ("VoLum identity").
2. Read the desktop references: every `docs/user-guide-*.png` you need for the
   screen being compared, and the motion sheets in
   `.scratch/android-shootout/reference/`.
3. Read the candidate evidence for the **exact commit under review**:
   `.scratch/android-shootout/evidence/<x>/<sha>/` — `verify-candidate.json`,
   every capture at font scale 1.0 and 1.3, every motion contact sheet, the
   coverage report. Do not read the worker's summary or report.
4. If `verify-candidate.json` is missing, not `"pass": true`, or names another
   commit, stop: the verdict is `defects` with "mechanical gate not green".

## Criteria (score 1–5 each; pass = every score ≥ 4 and no blocker)

1. **VoLum identity.** Next to the desktop screenshots and motion sheets, it reads
   as the same product: Poiret One / Josefin Sans / Michroma used as the desktop
   does, raisin-black + amber/gold + brass + teal + cyan grid, the shared
   selection styles, gold brackets, PRE crosshatch, AMP circuit board, the shared
   moving art. Generic Material components, system fonts, or invented palettes
   are blockers.
2. **Feature discoverability.** Every feature id in the coverage report is not
   just operable but findable: a guitarist who knows the desktop finds it without
   hunting. Hidden-behind-long-press-only for a primary control is a defect.
3. **Visual craft.** Hierarchy, alignment, spacing rhythm, density. No clipping,
   overlap, truncation or wrapped labels at font scale 1.0 or 1.3. Value readouts
   legible. Empty, loading and error states designed, not default.
4. **Touch ergonomics.** Targets ≥ 48 dp (mechanical) and sensibly larger for
   live controls. Knobs and sliders work with a fingertip (drag model, fine
   adjust, value feedback, no accidental changes while scrolling). Primary live
   controls (bypass, tuner, scene/sound switching) reachable without precision.
5. **Navigation.** At most 3 taps from home to any feature. Current rig, sound,
   bypass and input state readable at a glance. Back behaviour predictable.
6. **Motion and feedback.** Press states on every interactive element,
   transitions that explain where things went, art motion matching the desktop's
   timing and easing (compare the sheets). No dead taps, no jarring jumps.
7. **Live-use readiness.** Scene/sound switching, tuner, metronome, MIDI learn,
   microphone permission denied, no USB interface, interface unplugged, model or
   file load failure, pack import conflict — each is designed and recoverable.

## Verdict file

Write `.scratch/android-shootout/verdicts/<x>.md` (`<x>` = `a`, `b`, `c` or
`design`), replacing any previous content:

```markdown
commit: <full sha under review>
verdict: pass | defects
reviewer: claude-opus-5-5-high
round: <n>
scores: identity=<n> discoverability=<n> craft=<n> touch=<n> navigation=<n> motion=<n> live=<n>

## Blockers
- <screen id or feature id>: <what is wrong, what good looks like>

## Defects
- ...

## Second reviewer findings considered
- <finding>: fixed-required | rejected because <reason>
```

Append the same block (without replacing) to `verdicts/<x>-history.md`.
`verdict: pass` is only legal when every score is ≥ 4, there are no blockers, and
every second-reviewer blocker is either required in the next round or rejected
with a written reason.
