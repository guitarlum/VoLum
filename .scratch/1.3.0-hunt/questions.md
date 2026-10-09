# Questions for the owner

UX judgment calls and product decisions found overnight. No behavior was changed for these.

### Q-01 Ctrl+S on a Factory preset opens "New Preset"
- Source: owner, 08/10 22:18
- Today: Factory presets are read-only; Ctrl+S saves a new User preset (name defaults to
  "New Preset"; the Factory name is allowed and lands in the USER group).
- Owner said: leave as is for now.

### Q-02 Metronome settings are not saved (F-13)
- BPM, volume and time signature reset to 120 / 4/4 / 50% on project reload and on
  standalone relaunch. Saving them needs a state-format addition, so it was left for
  post-1.3.0. Ship 1.3.0 like 1.2.x (same behavior), or add it now?

### Q-03 Window size and the last Settings tab are not remembered (F-14, F-15)
- Standalone opens at the default size; Settings always opens on SIGNAL. Both match
  1.2.x. Want either remembered?

### Q-04 PLAY row clicks discard unsaved edits without a prompt
- Clicking (or double-clicking, before the picker opens) a PLAY row recalls that Sound
  and drops unsaved knob edits. "+ Add this sound" on a modified User preset opens the
  dialog with Update, so Enter overwrites it. Intended?

### Q-05 Typing an existing User name while on a Factory preset silently stores "Name 2"
- No feedback in the save dialog that the name was changed.

### Q-06 "Read the manual" always opens the English guide
- `config.h` VOLUM_MANUAL_URL points at `docs/user-guide.en.md`; a German guide exists
  (`user-guide.de.md`) but the app has no language setting to pick it. Keep English-only,
  or open the German guide when the OS language is German? Also: the link follows `main`,
  not the installed version's tag.

### Q-07 RELEASE BLOCKER: enable GitHub Pages before publishing 1.3.0 (F-64)
- The 1.3.0 update check reads `https://guitarlum.github.io/VoLum/appcast.json`, written by
  `publish-appcast.yml` on release publish. Pages is not enabled (has_pages=false), so that
  deploy fails and every 1.3.0 user sees "Could not check for updates". Repo Settings >
  Pages > Source: "GitHub Actions". Not changed overnight (repo setting).

### Q-08 Pack export/import close without any success message
- A successful export and a cancelled one look the same; import closes with no summary.
  Add a short confirmation line (e.g. "Saved 12 presets to X.volumpack")?

### Q-09 Window can be resized past the screen (F-66 clamps it); want maximize enabled?

### Q-10 Right-click acts like a left click everywhere in BUILD
- Loads amps, switches cabs, opens the preset menu. A stray right-click changes the sound.
  Ignore right-clicks (or give them a context menu)?

### Q-11 BUILD preset < / > and the preset menu drop unsaved edits without asking (see Q-04 for PLAY)

### Q-12 Small BUILD UX calls (UI tester B)
- A fresh IR always shows a gold gear (auto-normalise trim counts as "shaped").
- Footer shows the internal custom-amp filename (`amp_46393043_2__V30-MONO-1.nam`) and no
  sign of an IR on SUPPORT.
- Custom amp cab row shows "--" placeholders for missing cabs.
- PRE pitch card reads PITCH / OCT / "+0 st" depending on the amp.
- Save with an empty name looks enabled but does nothing.
- LITE caption "Smaller A2 slice, lower CPU." stays visible while FULL is selected.

### Q-13 Pack import preview: Overwrite names only the incoming item; Reset lists no deletions
- Overwrite says "Replace Custom amp 'sadfsfd'" without naming the local "Monomyth Skeleton
  Key" it replaces; Reset deleted "Sunset Crunch" with no row saying so. Show both?

### Q-14 PLAY UX calls (UI tester C)
- Custom-amp art is static and teal on both lanes (factory art moves; MAIN is gold).
- T and M don't close what they opened (H does toggle Settings).
- Picker: adding to off-screen program 127 gives no feedback / no scroll; typed 200 ignored
  rather than clamped; no search or keyboard navigation.
- Banner falls back to the amp name after clearing LIVE although the preset is loaded.
- Factory "Ampete Lead" (04) and user "Ampete Lead" (12) look identical on the board.
- Tab does nothing (no hint) while SUPPORT is empty; PAN has no value readout; no tap tempo.
- CPU (one core): PLAY single 97% (art on) / 93% (off), PLAY Dual 119% / 103%, BUILD Dual 72%.
  F-61 covers part of this; is PLAY's cost acceptable for 1.3.0?
