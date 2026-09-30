# 15 Owner's factory presets baked into factory-presets.json

Status: claimed
Blocked by: owner's .volumpack (Sounds export)
Implementer: coordinator

Owner (2026-09-23 23:58): current factory presets are empty placeholders; he will dial one
sound per amp, export them as a Sounds Pack, and wants them baked in as the defaults.
Locked: ONE factory preset per amp, shown with the owner's name (not "Ready"); ids stay
`factory:<amp>:v1` so PLAY slots / MIDI maps keep resolving.

## Done
- Name support: `FactoryPreset::name` read from `rigs/factory-presets.json` ("name", trimmed,
  48-byte UTF-8-safe cap, empty/missing = "Ready"), used by the preset bar, preset menu,
  PLAY Sound choices and ResolveSound. Test "A factory preset shows the name its snapshot file
  gives it" (RED without the read). Suite 1091 green. Committed locally, not pushed (app build
  not yet re-verified).

## Remaining
- Owner exports the Pack; extract each preset's settings into `rigs/factory-presets.json`
  under `factory:<amp>:v1` with its name. Refuse / flag presets that reference non-bundled
  content (custom IR, custom pedal, custom amp).
- Test: every shipped factory preset parses to non-default settings and only references bundled
  captures (loop over rigs/factory-presets.json).
- App build, e2e, push, CI.

## Comments
