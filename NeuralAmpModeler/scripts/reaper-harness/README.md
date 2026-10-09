# VoLum REAPER render harness

Headless smoke that loads VoLum as a track FX in a real REAPER host, renders a
test tone **through** the plugin using "apply track FX to items as a new take",
and measures that take. Asserts the plugin renders finite / bounded / non-silent
audio, that enabling an effect actually changes the audio, and that a project
save/reload round-trip reproduces the same render.

## Run

```powershell
pwsh NeuralAmpModeler/scripts/reaper-harness/run-reaper-harness.ps1
pwsh NeuralAmpModeler/scripts/reaper-harness/run-reaper-harness.ps1 -Scenario all -Sandbox
```

Optional: `-Scenario <name[,name]|all>` (default `core`), `-Sandbox`,
`-Reaper <path-to-reaper.exe>` (default `C:\REAPER\reaper.exe`),
`-TimeoutSec <n>` (default: a budget per selected scenario), `-InstalledVst3`.

`-Sandbox` launches REAPER with `LOCALAPPDATA` pointed at an empty folder in the
work directory. VoLum reads its library, settings and `volum.log` from there, so
it starts on the default rig with the five pre-filled Sounds on programs 0-4, and
your real library is neither read nor changed. Use it for every MIDI scenario:
without it the harness cannot check which Sound a program should recall.

By default the runner points REAPER's VST3 scan path at
`NeuralAmpModeler/build-win` for the duration of the run, so the harness measures
the bundle you just built and `"VoLum"` can only resolve to one binary. Updating
`%COMMONPROGRAMFILES%\VST3` needs an elevated build, so without this the run would
silently exercise whatever was installed last. `reaper.ini` and the plugin scan
cache are restored afterwards. Pass `-InstalledVst3` to test the installed build
instead.

## What it checks

`core` (the default):

- VoLum instantiates in REAPER (VST3).
- **The harness is measuring VoLum at all.** The `bypassed` scenario must come
  back as the input tone bit-for-bit, and the `default` scenario must not match
  it. See the note below - this pair exists because the harness spent its first
  life measuring its own input file.
- Output is finite (no NaN/Inf), bounded (`peak < 8.0`), and non-silent.
- Tremolo and PRE Pitch stay finite and bounded **and change the audio**. A
  "finite" check on a render identical to the default one proves nothing.
- Save project -> reopen (`noprompt:`) -> re-render: RMS matches within tolerance
  (real-host state round-trip; complements CI `pluginval --strictness-level 10`).

The other scenarios print `PASS` / `FAIL` / `SKIP` / `WARN` lines with their
numbers, in `harness.log` and in the runner output. `WARN` is advisory (a host
quirk or a product finding) and does not fail the run. `FACT` lines record what
REAPER actually delivers, for the user guide:

- `midi-pc`: Program Change 0, 1, 3, a burst (4 then 1), the same program twice,
  and an empty program (100). Facts: `pc_reaches_volum_vst3`, `pc_path`,
  `pc_slot_exact`, `pc0_on_fresh_instance`, `pc_repeat_same_program_recalls`.
- `midi-cc102`: the recall CC with values 0, 2, 4, a burst, a repeat, and CC 103
  (must be ignored). Facts named `cc102_*` like the ones above.
- `project-roundtrip-after-recall`: recall slot 1, turn Bass, save, recall slot 3
  (so the machine's last-used Sound differs), reopen: the render and Bass must
  match what was saved.
- `two-instances`: slot 1 on track A, slot 3 on track B; B's recall must leave A
  alone, A must sound the same after B's VoLum is deleted, and A must still recall.
- `offline-vs-realtime`: apply-FX render against a realtime recording of the same
  track's output (send to a capture track, transport record), compared on total
  energy. `SKIP` when REAPER's audio engine is not running.

A recall passes only when `volum.log` has `[midi] recall slot=N` for exactly the
slot that was sent (with `-Sandbox`, also the expected amp and preset), and the
render and the saved FX state both changed. MIDI goes in as a MIDI item on
VoLum's track rendered with the same apply-FX action, which needs no audio
device. If that produces no `[midi]` line and the audio engine runs, the harness
retries through REAPER's virtual MIDI keyboard into the armed track, and the
`*_path` fact says which one worked (`item`, `live` or `none`).

VoLum recalls in `OnIdle`, which runs on a Win32 timer on REAPER's main thread,
so the harness is a coroutine on `reaper.defer` and never busy-waits.

## Why apply-FX instead of a track audio accessor

The original harness read a track audio accessor, on the assumption that it
returns the track's post-FX output. It does not - it serves the track's source
audio. Every scenario therefore reported the *input tone's* peak and RMS,
identical to six decimal places whether VoLum was loaded, fully bypassed, or
driven with tremolo at stock depth, and the numbers matched a direct measurement
of `input.wav` exactly. Six checks were printing PASS while measuring nothing.

`Item: Apply track/take FX to items (stereo output)` renders the item through the
track's FX chain offline into a new take, which a take accessor can then read.
The rendered take is deleted after each scenario so renders never stack, and the
project's media path is pointed at the work directory so REAPER does not leave
`input render NNN.wav` files in the user's Documents folder.

## Notes / limits

- The first render after instantiation is discarded: the amp capture loads on a
  worker thread and is staged by the audio callback, so an immediate render
  catches a partly-staged rig (visibly - its peak differs by ~50%).
- NaN detection is best-effort. Apply-FX writes the take in REAPER's configured
  apply-FX format, which may be fixed point and would clamp a non-finite sample
  rather than preserve it. The hard guarantee for that is CI's `pluginval` at
  strictness 10.
- Parameters are resolved by exact name first, then by substring, so scenario
  toggles keep working across parameter-order changes; a name that does not exist
  on the loaded build is skipped (logged, non-fatal).
- The harness installs a temporary `C:\REAPER\Scripts\__startup.lua` that runs
  `volum-harness.lua` only while a sentinel file exists, and restores any prior
  `__startup.lua` on exit.
- REAPER is force-killed at the end (ReaScript has no native clean-quit without
  SWS). That leaves the round-trip project marked as `faultyproject=` in
  `reaper.ini`, and on the next launch REAPER blocks every startup script behind a
  "load it anyway?" modal - which used to make every second run time out with no
  `harness.log`, looking exactly like a plugin hang. The runner now strips that
  marker (and any reopen-on-launch reference to its work directory) before and
  after each run, so consecutive runs work unattended.
- Without `-Sandbox` the plugin restores the library's last-used rig, so `core`
  renders whatever the standalone left behind. Rig persistence itself is covered
  by `e2e-standalone-win.ps1` and the doctest chunk/settings round-trips.
- VoLum applies a recalled Sound to its own parameters but does not tell the
  VST3 controller, so `TrackFX_GetParam` can keep reading the old knobs after a
  MIDI recall. The `host parameter view follows the recall` check reports that as
  `WARN`; the saved FX state and the audio are what the PASS checks rely on.
