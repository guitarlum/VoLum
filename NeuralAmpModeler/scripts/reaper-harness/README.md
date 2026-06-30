# VoLum REAPER render harness

Headless smoke that loads VoLum as a track FX in a real REAPER host, runs a test
tone through it, and reads the track's post-FX output directly via an audio
accessor (no render dialog, no temp WAVs). Asserts the plugin renders
finite / bounded / non-silent audio across a few scenarios and survives a project
save/reload round-trip.

## Run

```powershell
pwsh NeuralAmpModeler/scripts/reaper/run-reaper-harness.ps1
```

Optional: `-Reaper <path-to-reaper.exe>` (default `C:\REAPER\reaper.exe`),
`-TimeoutSec <n>`.

## What it checks

- VoLum instantiates in REAPER (VST3).
- Output is finite (no NaN/Inf), bounded (`peak < 8.0`), and non-silent.
- Tremolo / Pitch scenarios stay finite + bounded.
- Save project -> reopen (`noprompt:`) -> re-render: RMS matches within tolerance
  (real-host state round-trip; complements CI `pluginval --strictness-level 10`).

## Notes / limits

- REAPER instantiates whatever `VoLum.vst3` it has **scanned** (typically
  `%COMMONPROGRAMFILES%\VST3`). For a true HEAD regression smoke, install the
  freshly built VST3 there first, otherwise the harness exercises the previously
  installed build.
- Parameters are resolved **by name** (not EParam index), so scenario toggles
  keep working across parameter-order changes; a name that does not exist on the
  loaded build is skipped (logged, non-fatal).
- The harness installs a temporary `C:\REAPER\Scripts\__startup.lua` that runs
  `volum-harness.lua` only while a sentinel file exists, and restores any prior
  `__startup.lua` on exit.
- GUI-only rig state (which `.nam` amp / IR / preset is loaded) is not driven
  here; that is covered by the doctest suite (chunk/settings round-trips) and the
  manual mac+win smoke checklist.
- REAPER must be **cleanly shut down** before a run. The harness force-kills
  REAPER at the end (ReaScript has no native clean-quit without SWS), so the
  *next* launch can hit REAPER's "was not shut down properly" recovery modal,
  which blocks the startup script (symptom: timeout, no `harness.log`). If that
  happens, launch REAPER once manually and quit it (File > Quit) to clear the
  flag, then re-run. Install SWS and wire a clean-quit action to make this fully
  unattended. The automated host guarantee in CI is `pluginval` (state
  round-trip + automation at strictness 10); this harness is a local real-host
  audio-sanity smoke on top of that.
