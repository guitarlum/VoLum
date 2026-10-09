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
