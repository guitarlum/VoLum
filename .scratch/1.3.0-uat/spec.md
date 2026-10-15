# 1.3.0 UAT hardening

Locked grilling. Do not reopen product calls. Do not add tickets next to
`.scratch/release-1.3.0/map.md`.

## Outcome

One overlay/scroll stack, one save-vs-assign model, PLAY/BUILD as one sounding
rig, and the PLAY chrome ticket 12 already locked (destination toggle, art above
the banner, IN+idle illumination).

## Save vs assign

- Ctrl+S writes the live rig into a Sound. Never moves PLAY / MIDI numbers.
- Factory or Default: in-app name popup → new User preset → that row selected
  and clean.
- Dirty User: overwrite in place, no popup.
- Default (no recalled snapshot) can dirty against factory-default settings and
  can Save As / Ctrl+S.
- + while the live rig is not on the PLAY list (dirty, or a saved User preset
  with no number): **Add this sound** → next free PC (Save As first if still
  Factory/Default).
- + while clean and assigned: **Add Sound** (library picker).
- Assignment is Add / reassign / clear (clicks). No row-swap or picker-to-row
  drag.

## PLAY / BUILD

- Flip never recalls.
- PRE/POST lock drops on entering PLAY and stays off.
- Right-click a PLAY stomp → BUILD, that card selected, knobs up.
- One header toggle; idle shows the other mode.
- T / M / H and Ctrl+S work in PLAY.

## Lists / layout / art

- Factory / User: only one section → start open; both exist → start collapsed;
  then session memory; do not collapse after a pick.
- Art sits above the name banner. IN-meter brightness when playing, slow idle
  pulse when quiet, frame corona only. Cache the PLAY fractal.
- `volum::ui` overlay tag SSOT. `volum::scroll` owns every list thumb.

## Tests

Prove-revert-fail for overlay z-order, scroll drag, About/Pack in-bounds,
Default dirty + Ctrl+S, Add this sound, lock drop, PLAY keys, picker groups,
right-click stomp, illumination dim vs loud.
