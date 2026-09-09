# VoLum Screenshot Recipes

Reproducible recipe for regenerating every `docs/user-guide-*.png`. The standalone
app is the source of truth for state, so we ship a deterministic **seed** (a known
`volum-settings.json` + dual-amp sidecar + custom-content library) and drive the
UI with the small capture harness under `NeuralAmpModeler/scripts/`.

The screenshots are shared: both `user-guide.en.md` and `user-guide.de.md`
reference the same PNGs, so one capture pass covers both languages.

For sidebar overflow (scrollbar, custom-row glyphs) that the seed library does
not reach, launch a **debug** build with `VOLUM_SEED_CUSTOM_AMPS=N`. That calls
`volum::custom::AddCustomAmp` in `VoLumCustomContentApi.h` into a temp sandbox
and never touches the real content store. The `+` builder overlay is not
scriptable with `win-click.ps1`; do not try to complete it from the harness.

To review the update badge, About pill, and footer reminder without a real
release, launch with `VOLUM_FAKE_UPDATE=1`. That injects an in-memory 2.0.0
manifest and does not write `volum-update-state.json`.

Canvas clicks and captures go through `scripts/ui-drive.ps1` (client pixels, one
process per shot). Do not chain `win-click.ps1` then `capture-volum-canvas.ps1`:
foreground is lost between processes and PrintWindow returns a blank canvas.
`win-click.ps1` stays window-relative for ad-hoc probes.

Pack Open/Save dialogs are separate `#32770` windows. Default `ui-drive.ps1`
ForceFront cancels iPlug `PromptForFile` and the import overlay shows **No Pack
opened.** Write the seed Pack first, then
`ui-drive.ps1 -Clicks "718,346" -PackOpen <seed.volumpack>`. That pastes via the
clipboard; do not SendKeys an 8.3 path (`~` is ALT).

## 0. Prerequisites

- Build the standalone (Release x64): `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
  (or build target `NeuralAmpModeler-app`). Exe lands at
  `NeuralAmpModeler/build-win/app/x64/Release/VoLum.exe`.
- Harness scripts (Windows PowerShell 5.x):
  - `scripts/ui-drive.ps1 -Clicks "cx,cy" -Out <png>` - canvas click + capture.
    `-PackOpen <file>` completes the native Open dialog without ForceFront.
  - `scripts/capture-volum-canvas.ps1 -OutPath <png>` - crops the client canvas
    to match the docs framing (~900x600 at the default window size).
  - `scripts/win-key.ps1 -Keys "<SendKeys>"` - sends keys (`1`/`2`/`3`, `{UP}`,
    `{DOWN}`, `{LEFT}`, `{RIGHT}`, `{TAB}`, `{ENTER}`, `{ESC}`, `t`, `m`).
  - `scripts/win-click.ps1 -X <x> -Y <y>` - clicks a window-relative pixel.
  - `scripts/win-screenshot.ps1` - full-window capture + a dark-pixel sanity check.

> WARNING: the seed OVERWRITES your personal VoLum library. Back it up first:
> ```powershell
> Copy-Item "$env:LOCALAPPDATA\VoLum" "$env:LOCALAPPDATA\VoLum-backup" -Recurse -Force
> ```
> Restore it when done by copying the backup back over `$env:LOCALAPPDATA\VoLum`.

## 1. Install the seed

Copy the seed over the live library, then launch:

```powershell
$dst = "$env:LOCALAPPDATA\VoLum"        # macOS: ~/Library/Application Support/VoLum
Copy-Item docs/screenshot-seed/volum-settings.json          "$dst\" -Force
Copy-Item docs/screenshot-seed/volum-dual-amp-settings.json "$dst\" -Force
Copy-Item docs/screenshot-seed/content "$dst\" -Recurse -Force
```

To write the Pack the import shot opens, without the OS Save As dialog:

```powershell
$env:VOLUM_WRITE_SEED_PACK = "$env:TEMP\volum-docs\seed.volumpack"
pwsh NeuralAmpModeler/scripts/run-tests-win.ps1 -Filter "Screenshot-seed library WritePacks"
```

Do not commit the `.volumpack`.

The seed pre-dials the feature amps to sensible "some effects on, never all"
scenes (both NAM slots assigned — empty `+` is a fail) and seeds the
bring-your-own library:

- `THC Sunset` (lastAmpIdx 14, User preset **Sunset Crunch**): NAM1 Klon + NAM2
  Halcyon TS engaged, Hall reverb on, Pitch/Comp off. BUILD hero, PRE, Pitch,
  Presets, Tuner, Metronome.
- `Soldano SLO100` (13): Compressor + Klon + Halcyon TS engaged, pitch off.
  PLAY empty / picker / board.
- `Orange ORS100 1972` (11): both NAM assigned but bypassed; Digital delay +
  Hall reverb on, tremolo off. POST / Chorus / Tremolo.
- `Marshall 2204 1982` (7): dual-amp on with `Marshall JMP 2203` as SUPPORT;
  both NAM assigned but bypassed.
- Custom library: amp "Monomyth Skeleton Key" (DIRECT + V30 on ch1, G12 on ch2),
  pedal "5000$ Klon", IR "Marshall 4x12 / Royer", SLO100 User bank plus THC
  **Sunset Crunch**.

## 2. Geometry

At the default launch size the window is ~916x659 and the captured canvas is
900x600. `win-click.ps1` wants window-relative pixels; canvas point (cx,cy) maps
to roughly window (cx+8, cy+51). The 1.3.0 capture loop clicks **canvas**
coordinates: header toggle `(743, 22)`, gear `(869, 22)`, Settings tabs SIGNAL /
MIDI / SYSTEM `(300, 113)` / `(450, 113)` / `(600, 113)`. Sections switch with
`1` PRE / `2` AMP / `3` POST; amps switch with `{ESC}` then `{UP}`/`{DOWN}`.

`win-key.ps1` only lands while VoLum is the foreground window, and merely
launching it is not enough: click into the window once (`win-click.ps1`) before
the first key, or the whole key sequence goes nowhere and the capture silently
shows the wrong state. A capture that comes back uniformly light grey is the same
symptom - the GL surface was never composited because the window was not in
front. A **locked workstation** produces exactly that, for every shot, with no
other symptom: `PrintWindow` hands back DWM's last composited surface, and while
LogonUI owns the desktop there is none. There is no way round it from a script,
so check `Get-Process LogonUI` before blaming the recipe, and unlock first.

The Settings tab strip is sized to content and centred, so tab x positions move
when the number of tabs changes. It is three tabs wide since 1.3.0: window x
308 / 458 / 608 for SIGNAL / MIDI / SYSTEM at y 164.

## 3. Per-shot recipe

Launch fresh (`Start-Process ...\VoLum.exe`; wait ~6s) before each group. All
capture with `capture-volum-canvas.ps1 -OutPath docs/user-guide-<name>.png`.

| PNG | Amp / how to reach | State delta from seed | Transient step |
| --- | --- | --- | --- |
| `user-guide-play-empty.png` | Soldano SLO100 (`lastAmpIdx` 13, empty `midiSoundMap`) | no PLAY assignments | canvas: empty click `(10,10)` then toggle `(743, 22)` into PLAY. Fail the shot if **+** is not **+ Add this sound** or if PLAY\|BUILD words are in the header |
| `user-guide-play-picker.png` | Soldano, after one User Sound is LIVE | map slot 0 to Crunch Rhythm | from the empty board, **+ Add this sound** `(450, 324)` once to put LIVE on the rail, then rail **+ Add Sound** `(803, 301)`. Picker: PROGRAM next-free, User heading `(450, 265)` expanded |
| `user-guide-play.png` | Soldano SLO100 | slots 0–2 = Crunch Rhythm / Lead Boost / Clean Verb, slot 0 LIVE | canvas toggle `(743, 22)` if you are in BUILD. Fail if **+** is not **+ Add Sound** or if a safety banner is visible |
| `user-guide-main.png` | THC Sunset (seed lastAmpIdx 14, AMP view, **Sunset Crunch**) | none | click **THC Sunset** in the browser (93,565) if the custom amp is focused. Compact pill left of tuner. Fail if NAM 2 is an empty `+` or the preset bar is not Sunset Crunch |
| `user-guide-settings-signal.png` | any | none | canvas gear `(869, 22)`; Settings opens on the tab it was left on, so click **SIGNAL** `(300, 113)` |
| `user-guide-settings-midi.png` | any | seed a few `midiSoundMap` entries, one of them pointing at a preset id that does not exist (`preset_gone_forever`), so the list shows assigned rows and a red **Invalid slot** | from Settings, click **MIDI** `(450, 113)` |
| `user-guide-settings-system.png` | any | none | from Settings, click **SYSTEM** `(600, 113)`. Both **Back up your library** help lines stay inside the card |
| `user-guide-pre.png` | THC Sunset | `preCompActive=true` (hero keeps Comp off) | `1` then `{RIGHT}{RIGHT}` (focus Klon). Fail if either NAM slot is empty |
| `user-guide-pre-pedal.png` | THC Sunset | none | from PRE/Klon focused, click (471,251) to open the capture chooser |
| `user-guide-pitch-transpose.png` | THC Sunset | `prePitchActive=true, prePitchMode=0, prePitchSemitones=-2, prePitchTransChar=2`; Comp off; both NAM still assigned | `1` then `{LEFT}` (focus PITCH) |
| `user-guide-pitch-octaver.png` | THC Sunset | `prePitchActive=true, prePitchMode=1, prePitchOctDown=0.8, prePitchVoicing=1`; Comp off; both NAM still assigned | `1` then `{LEFT}` |
| `user-guide-presets.png` | THC Sunset | none (Sunset Crunch bank) | click preset bar (546,76). Menu must show Default, Ready, Sunset Crunch, Manage |
| `user-guide-post.png` | ORS100 (lastAmpIdx 11) | none | `{ESC}` then `{UP}` to 11, `3` (POST; Delay focused). Fail if either NAM slot is an empty `+` |
| `user-guide-chorus.png` | ORS100 | `postChorusActive=true` | `3` then `{LEFT}` (focus CHORUS) |
| `user-guide-tremolo.png` | ORS100 | `postDelayActive=false, postTremoloActive=true, postTremoloMode=1` | `3` then `{RIGHT}{RIGHT}` (focus TREM) |
| `user-guide-dual-amp.png` | Marshall 2204 (lastAmpIdx 7) | none (dual on in sidecar) | `2` (AMP). Fail if either NAM slot is empty |
| `user-guide-custom-amp.png` | Monomyth (`{ESC}` then 8x `{DOWN}` from Marshall 2204) | none | click pen icon (125,625) to open builder |
| `user-guide-custom-ir.png` | Monomyth (ch1) | none | click "Custom IR" cab (674,111) |
| `user-guide-custom-pedal.png` | any | none | `1`, click NAM1 (471,251) twice, click "Manage custom pedals..." (494,628) |
| `user-guide-tuner.png` | THC Sunset | none | key `t`. Background is the filled BUILD hero |
| `user-guide-metronome.png` | THC Sunset | none | key `m`. Background is the filled BUILD hero |
| `user-guide-pack-export.png` | any (Soldano map as in PLAY board) | none | canvas gear `(869, 22)`, **SYSTEM** `(600, 113)`, **Export Pack...** `(525, 346)`, scope **Sounds** `(250, 173)`, tick first Sound `(198, 228)`. Fail if the companion band is missing or still says `Also including:` |
| `user-guide-pack-import.png` | any | `$env:VOLUM_WRITE_SEED_PACK="$env:TEMP\volum-docs\seed.volumpack"` then `run-tests-win.ps1 -Filter "Screenshot-seed library WritePacks"`; do not commit the Pack | canvas gear / SYSTEM, then `ui-drive.ps1 -Clicks "718,346" -PackOpen $env:TEMP\volum-docs\seed.volumpack`. Fail on **No Pack opened.** |

State deltas are edits to the focused amp's block in `volum-settings.json` between
launches (close the app, edit the JSON with the same key names shown above, then
relaunch). Everything else is reachable from the seed with the transient step.

## 4. Verify + restore

- Eyeball each PNG: input/output should read `0.0 dB` (never `-20`/`-inf`), and a
  reasonable subset of effects should be lit (never all off, never all on).
- Layout audit (second pass, not the shooter): fail the shot if any label crosses
  a card or pill edge, sits on a hairline, or wraps out of its frame. About:
  checkbox + **Check now** stay inside the About card. Pack: scope subtitle and
  also-including text stay inside their pills. PLAY: art sits above the name
  banner; the destination toggle is a compact pill immediately left of tuner /
  metronome / gear.
- Restore your real library from the backup created in step 0.
