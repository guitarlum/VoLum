# Chorus docs, changelog, screenshots

Status: resolved
Blocked by: 03

## Goal

User-visible finish for Chorus.

## Do this

- Changelog: one line for the new POST Chorus (modes + knobs + place in the chain).
- `docs/user-guide.en.md` and `docs/user-guide.de.md` in sync: POST section names four cards and the four modes; note default WARPED and bypassed.
- Refresh `docs/user-guide-post.png` (and a chorus-focused shot if the POST recipe needs it) via `docs/screenshot-recipes.md`. Update the recipe table if POST click-targets moved.
- Mark tickets 01–04 resolved. No `ready-for-agent` left in this directory.

## Done when

EN/DE match. Screenshots match the shipping UI. Windows tests still green.

## Result

- Changelog: one 1.3.0 line for the POST Chorus - the four voices, the shared
  knob row, its place ahead of Delay, MIX 0 as a bit-perfect bypass, no tempo
  sync, ships bypassed on WARPED, old presets and sessions unaffected.
- `docs/user-guide.en.md` and `docs/user-guide.de.md` both name four POST cards
  and the four modes, note the WARPED default and the bypassed-on-arrival state,
  and describe RATE / DEPTH / TONE / WIDTH / MIX. Same anchors and same image
  references in both.
- `docs/user-guide-post.png` re-shot (four cards) and `docs/user-guide-chorus.png`
  added, both from the seeded ORS100 scene per `docs/screenshot-recipes.md`; the
  recipe table gained the `user-guide-chorus.png` row. The personal library was
  backed up and restored around the capture pass.
- Windows suite green after the docs pass: 750 cases, 7,689,176 assertions.
