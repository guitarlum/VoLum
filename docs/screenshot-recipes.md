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

The Pack file dialogs are separate top-level windows, so `win-key.ps1` (which
activates VoLum's main window first) sends its keys to the wrong place. Type into
them with `[System.Windows.Forms.SendKeys]::SendWait()` against whatever is in the
foreground instead, and give the dialog ~2s to appear.

## 0. Prerequisites

- Build the standalone (Release x64): `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
  (or build target `NeuralAmpModeler-app`). Exe lands at
  `NeuralAmpModeler/build-win/app/x64/Release/VoLum.exe`.
- Harness scripts (Windows PowerShell 5.x):
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

The seed pre-dials the five feature amps to sensible "some effects on, never all"
scenes and seeds the bring-your-own library:

- `THC Sunset` (lastAmpIdx 14): Klon PRE pedal + Hall reverb - the hero/main shot.
- `Soldano SLO100` (13): Compressor + Klon + Halcyon TS engaged, pitch off.
- `Orange ORS100 1972` (11): Digital delay + Hall reverb on, tremolo off.
- `Marshall 2204 1982` (7): dual-amp on with `Marshall JMP 2203` as SUPPORT.
- Custom library: amp "Monomyth Skeleton Key" (DIRECT + V30 on ch1, G12 on ch2),
  pedal "5000$ Klon", IR "Marshall 4x12 / Royer", SLO100 preset bank.

## 2. Geometry

At the default launch size the window is ~916x659 and the captured canvas is
900x600. Click coords below are window-relative (what `win-click.ps1` expects);
canvas point (cx,cy) maps to roughly window (cx+8, cy+51). Sections switch with
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
| `user-guide-play.png` | THC Sunset (seed lastAmpIdx 14) | start with no `midiSoundMap` entries | click the destination toggle (735,78) if the header shows the stomp-ring (you are in BUILD; one click enters PLAY). Canvas bounds are `mainR-218..mainR-128`, `T+12..T+42`. Then click the empty-state **+ Add Sound** (458,371), then the first Factory row (458,209); repeat from the rail Add row (810,212) and (810,282) for two more Factory rows, then click rail row 01 (810,226) to recall it (LIVE) |
| `user-guide-main.png` | THC Sunset (seed lastAmpIdx 14, AMP view) | none | click **THC Sunset** in the browser (93,565) - the seed reopens on the custom amp |
| `user-guide-settings-signal.png` | any | none | click the gear (870,80); Settings opens on the tab it was left on, so click **SIGNAL** (308,164) |
| `user-guide-settings-midi.png` | any | seed a few `midiSoundMap` entries, one of them pointing at a preset id that does not exist, so the list shows both an assigned and a red missing row | from Settings, click **MIDI** (458,164) |
| `user-guide-settings-system.png` | any | none | from Settings, click **SYSTEM** (608,164) |
| `user-guide-pre.png` | SLO100 (`{ESC}{UP}` to 13) | none | `1` then `{RIGHT}` (focus Klon) |
| `user-guide-pre-pedal.png` | SLO100 | none | from PRE/Klon focused, click (471,251) to open the capture chooser |
| `user-guide-pitch-transpose.png` | SLO100 | `prePitchActive=true, prePitchMode=0, prePitchSemitones=-2, prePitchTransChar=2`; comp+NAM off | `1` then `{LEFT}` (focus PITCH) |
| `user-guide-pitch-octaver.png` | SLO100 | `prePitchActive=true, prePitchMode=1, prePitchOctDown=0.8, prePitchVoicing=1`; comp+NAM off | `1` then `{LEFT}` |
| `user-guide-presets.png` | SLO100 | none (bank seeded) | click preset bar (546,76) |
| `user-guide-post.png` | ORS100 (lastAmpIdx 11) | none | `3` (POST; Delay focused) |
| `user-guide-chorus.png` | ORS100 | `postChorusActive=true` | `3` then `{LEFT}` (focus CHORUS) |
| `user-guide-tremolo.png` | ORS100 | `postDelayActive=false, postTremoloActive=true, postTremoloMode=1` | `3` then `{RIGHT}{RIGHT}` (focus TREM) |
| `user-guide-dual-amp.png` | Marshall 2204 (lastAmpIdx 7) | none (dual on in sidecar) | `2` (AMP) |
| `user-guide-custom-amp.png` | Monomyth (`{ESC}` then 8x `{DOWN}` from Marshall 2204) | none | click pen icon (125,625) to open builder |
| `user-guide-custom-ir.png` | Monomyth (ch1) | none | click "Custom IR" cab (674,111) |
| `user-guide-custom-pedal.png` | any | none | `1`, click NAM1 (471,251) twice, click "Manage custom pedals..." (494,628) |
| `user-guide-tuner.png` | any | none | key `t` |
| `user-guide-metronome.png` | any | none | key `m` |
| `user-guide-pack-import.png` | any | none | click gear (870,80), **SYSTEM** (608,164), then **Export Pack...** (533,397) in the Content library card, **Export...** (536,521), type a `.volumpack` path into Save As + `{ENTER}`, then **Import Pack...** (726,397) and the same path + `{ENTER}` |

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
  banner; the destination toggle is left of tuner / metronome / gear and larger
  than those circles.
- Restore your real library from the backup created in step 0.
