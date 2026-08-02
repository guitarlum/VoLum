# Q4 — Screenshot harness: the last three gaps

## Already landed (changelog `06/29/2026`)

Most of this ticket is done. Kept open only for the three items in "Remaining"
below; the original text follows for context.

- **Deterministic seeding:** `VOLUM_SEED_CUSTOM_AMPS=N` (`NeuralAmpModeler.cpp`
  ~475-511), `#ifndef NDEBUG`, seeded into a temp sandbox so it never touches the
  real content store.
- **Scrollbar geometry test:** `VoLumAmpListScroll.h` extracted, with four cases in
  `test_volum_amp_list_scroll.cpp` covering scrollable state, `RowRightX`, thumb
  size/position and drag-to-offset mapping.
- **Runbook:** `docs/screenshot-recipes.md` plus `docs/screenshot-seed/`.
- **`volum-ui.mdc`:** documents the seeding recipe (~68-72).

## Remaining

1. The **name-area shrink-to-fit** geometry is still unpinned — it was in scope
   item 2 below but no helper or test covers it.
2. `AGENTS.md` "Fast Commands" still does not mention `win-screenshot.ps1` /
   `win-click.ps1`, so the harness stays undiscoverable from the routing index.
3. Cross-link `AddCustomAmp` (now `VoLumCustomContentApi.h`, not
   `VoLumCustomContentMock.h`) as the seam the harness should use, and state that
   the `+` builder overlay is not scriptable.

Note the harness's practical ceiling, learned the hard way during the 1.2.1 audit:
`PrintWindow` cannot composite VoLum's GL surface while the desktop is locked, so
the capture comes back as a blank white client area. Any overnight or unattended
screenshot sweep needs an unlocked session.

---

# Original ticket — deterministic custom-amp seeding + a real scrollbar test

Make the local self-verification harness able to reach UI states that only appear
once the amp library overflows (i.e. with custom amps present), and add a proper
test for the sidebar scrollbar. Priority: low-to-medium. Nothing is broken; this
removes a verification blind spot found while fixing the amp-list scrollbar
(`feature/post-tremolo`): the scrollbar only renders when `ContentHeight >
mRECT.H`, which needs custom amps, and there is currently no scripted way to
create one — the `+` opens the free-form builder overlay, which synthetic
`win-click.ps1` cannot complete, so the scrollbar drag + label-gutter behavior
could not be screenshotted and had to be confirmed by hand.

## Problem

- `NeuralAmpModeler/scripts/win-screenshot.ps1` + `win-click.ps1` can drive
  overlays, but cannot populate the CUSTOM section. The builder needs multi-field
  input the click harness can't drive.
- Custom amps are the only trigger for: the sidebar scrollbar (drag + track-jump
  + gutter), the CUSTOM header populated state, custom-row hover/edit/delete
  glyphs, and `ScrollToRevealCustom`. None of these have a deterministic capture
  path today.
- The new scrollbar behavior in `VoLumAmpList.h` (drag via
  `OnMouseDown`/`OnMouseDrag`/`OnMouseUp`, `RowRight()` gutter, name auto-shrink)
  is only pinned by a brittle source-string lock in
  `test_volum_ui_regressions.cpp`, not by a behavioral/geometry test.

## Scope

1. Deterministic custom-amp seeding for local capture. Pick one:
   - A debug-only standalone affordance (env var like `VOLUM_SEED_CUSTOM_AMPS=N`,
     or a hidden menu/keybind) that calls the existing
     `volum::custom::AddCustomAmp(name, art)` convenience overload N times on
     startup, so `win-screenshot.ps1` can capture the overflow/scrollbar state.
     Must be gated so it never ships in release builds and never writes the user's
     real content store.
   - OR a tiny scripted builder-completion path the click harness can invoke.
   Prefer the first (smaller, no overlay automation).
2. A real scrollbar test (not just a source-string lock). Extract the pure
   scrollbar/gutter geometry from `VoLumAmpListControl` into a free
   helper (mirroring the `VoLumTriptychLayout.h` split) so a doctest can assert:
   thumb size/position vs. scroll offset, `RowRight()` reserves the gutter only
   when scrollable, track-click maps a y to the expected offset, and the name
   area shrinks to fit. Put the helper + test under the existing UI-regression
   coverage.
3. Screenshot-refresh runbook. Document the end-to-end "how to refresh sidebar /
   custom-amp / scrollbar screenshots" steps (seed customs → screenshot → reset)
   so the next agent doesn't rediscover the blocker.

## AGENTS.md review (explicit ask)

Review `AGENTS.md` and `.cursor/rules/volum-ui.mdc` for screenshot/verification
guidance and close the gaps this story exposes:
- `AGENTS.md` "Fast Commands" lists the app smoke check but not the
  `win-screenshot.ps1` / `win-click.ps1` capture/drive harness — add a short
  pointer (or a `.cursor/rules` note) so it is discoverable.
- `volum-ui.mdc` "Verification" says "inspect the app" but gives no recipe for
  states behind runtime-grown lists (custom amps). Add the seeding recipe / link
  to the runbook from (3).
- Cross-link `AddCustomAmp` (`VoLumCustomContentMock.h`) as the seam tests/harness
  should use, and note that the `+` builder overlay is not scriptable.

## Acceptance

- A documented, repeatable command/flow produces a screenshot of the sidebar with
  custom amps and a visible, correctly-spaced scrollbar.
- The debug seeding path is compiled out (or hard-disabled) in release builds and
  leaves the user's persisted custom content untouched.
- A doctest covers the scrollbar/gutter geometry and fails if the gutter or
  drag-to-offset mapping regresses; `run-tests-win.ps1` green; source-parity
  descriptor count updated.
- `AGENTS.md` / `volum-ui.mdc` updated per the review above.

Work on a dedicated feature branch off the latest `dev`, named
`feature/screenshot-harness-custom-amp-seed`. Do not commit to `dev` or `main`
directly. Merge back into `dev` only after acceptance criteria are met. Never
promote to `main` outside of a release.
