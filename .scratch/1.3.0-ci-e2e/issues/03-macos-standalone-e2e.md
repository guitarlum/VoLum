# 03 macOS standalone e2e job

Status: resolved

## Scope

Write: `NeuralAmpModeler/scripts/e2e-standalone-mac.sh` (+ python helpers beside it), the
`macOS standalone e2e` job in `.github/workflows/ci.yml`, a changelog line,
`docs/screenshot-recipes.md` / AGENTS.md Fast Commands entry. Must not touch product C++
unless a crash/hang/data-loss bug is found (then: separate `feature/*` fix per spec.md).

## Acceptance

Every launch uses a temp `HOME`; the binary is run directly so the env applies.

- Negative control first -> `NEGATIVE CONTROL OK`.
- **fresh**: empty library -> window appears, settings written with `volumUiMode` play,
  `buildTipSeen` false, `volumActivePresetId` `factory:12:v1`; `volum.log` has
  `recall slot=0 amp=factory:12 preset=factory:12:v1`; content store has the five Factory
  Sounds on programs 0-4; graceful quit exits the process.
- **upgrade**: 1.2.x fixture library -> launches, migrates as the Windows `upgrade` scenario
  expects, graceful quit.
- **midi**: virtual CoreMIDI source `VoLum Loop` selected as the standalone's MIDI input
  (persisted the way the Mac standalone stores it). Program Change 9 -> program 9;
  CC 102 value 42 -> program 42; empty program ignored; burst 3,100,9,42,101 ends on 101;
  "One channel" = 2 ignores channel-1 messages and accepts channel 2; `lastPlaySlot` and
  `volumActivePresetId` saved on quit. Assert via `volum.log` `recall ...` lines and the saved
  settings, as `Test-Midi` does on Windows.
- **shots**: screenshots of PLAY (with the "Tweak your sound" note), BUILD (P key), Settings
  SIGNAL / MIDI / SYSTEM, uploaded as artifact `mac-e2e-shots`; each image passes a
  non-blank check (pixel variance). A missing Screen Recording permission on the runner is a
  SKIP with a reason, not a silent pass.
- Log ends `ALL PASS <n> checks`; flaky GUI steps may retry once, logged as a retry.
