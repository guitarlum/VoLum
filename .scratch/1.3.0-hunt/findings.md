# Findings

Format: see `spec.md`. Every entry needs a final Disposition other than `open`.

### F-00 Audio & MIDI devices dialog keeps the click captured
- Source: owner, manual test 08/10 21:52
- Severity: stuck-ui
- Repro: Settings > SIGNAL > Audio & MIDI devices..., close the dialog, click anywhere
- Expected / Actual: click acts normally / every click reopens the device dialog
- Disposition: open
