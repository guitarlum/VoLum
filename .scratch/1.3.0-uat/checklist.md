# 1.3.0 UAT: what is left for you (about 50 min)

Machines covered everything else overnight (see `.scratch/1.3.0-hunt/loop.md`): golden
renders, 1,300+ unit tests plus AddressSanitizer, the Windows end-to-end scenarios with real
WinMM MIDI, REAPER VST3 on Windows (Program Change and CC 102, save/reopen, two instances,
restore guard), audio loopback and driver switching on the UA-2X2, and on the macOS CI
runner: universal binaries, strict codesign, Rosetta auval, and REAPER AU + VST3 with
Program Change, CC 102 and project reload. What no machine could do: a real Mac with the HX,
Gatekeeper, an elevated Windows installer, and two repo/product decisions.

Tick `[x]` when it works. Anything odd: one line under Findings, and keep going.

## Release blockers (5 min)

- [ ] **Enable GitHub Pages (Q-07).** Repo Settings > Pages > Source: "GitHub Actions".
  Without it the appcast deploy fails and every 1.3.0 user sees "Could not check for
  updates".
- [ ] **Skim the defaults in `.scratch/1.3.0-hunt/questions.md` (Q-01..Q-18).** Every one
  shipped with the default named there; only reply on the ones you disagree with.

## Mac pass (about 35 min)

Build: in Safari on the MacBook open
[CI run 38013121147](https://github.com/guitarlum/VoLum/actions/runs/38013121147) (final
`dev`), download the `VoLum-mac` artifact, unzip it, and use the `...-macos-installer.dmg`.
Hardware: HX Stomp XL over one USB cable as audio interface and MIDI. On the HX, Global
Settings > MIDI/Tempo: MIDI Over USB On, PC Send On, channel 1.

- [ ] **1. Install over 1.2.x with Gatekeeper.** Run the DMG. If macOS blocks it, System
  Settings > Privacy & Security > Open Anyway. Write down the exact wording of each step
  (Findings). Expect: standalone, VST3 and AU installed; your 1.2.x presets and custom
  content still there.
- [ ] **2. Audio device and persistence.** VoLum > Preferences... (also Settings > SIGNAL >
  Audio & MIDI devices...). Pick the HX for input, output and MIDI. Play at buffer 64 and
  128, listen for crackle. Turn a knob, Cmd+Q right away, reopen. Expect: device, buffer and
  knob kept.
- [ ] **3. Apple silicon listening.** Tuner agrees with the HX tuner. Octaver on a high-gain
  amp, frets 12-19 on the high E with bends: no wobble. Transpose INSTANT -2: clean. Dual
  Amp in PLAY (Tab): both lanes audible, no click when toggling.
- [ ] **4. Retina look.** Glance at BUILD, PLAY (art moves while you play, P toggles),
  Settings SIGNAL/MIDI/SYSTEM, the MIDI footswitch board, a Dual Amp lane. Expect: crisp
  text, nothing clipped. Screenshot anything odd.
- [ ] **5. Standalone MIDI with the HX.** Settings > MIDI: assign 3 Sounds to programs 0-2.
  Turn the HX preset knob through 01A/01B/01C: VoLum follows, the LIVE lamp moves. "One
  channel" = 2: VoLum ignores the HX; set it back to All. Pull the USB cable while running,
  replug, reselect the device: no crash. Quit: the app exits.
- [ ] **6. AU in Reaper with the HX (quick).** VoLum (AU) on a track with HX audio in and HX
  MIDI as track input. HX presets switch the Sound. Save, quit Reaper, reopen: same Sound.
- [ ] **7. Small Mac-only paths.** Settings > SYSTEM > Check now gives a clear answer (needs
  Pages from the blocker above). About > "Read the manual" opens the browser.
- [ ] **8. Pack from Windows to Mac.** Copy `presetSounds.volumpack` to the Mac, Settings >
  SYSTEM > Import Pack. Expect: all presets, with names and art.

## Windows pass: installer upgrade (about 10 min)

The overnight lane could not elevate, so the real installer was never run over an installed
version. This laptop still has 1.2.1 installed (backup:
`.scratch/1.3.0-hunt/l6/backup-initial-121/`).

- [ ] **1. Upgrade install.** Download `VoLum-windows` from the CI run above, run
  `VoLum-Setup.exe` (accept UAC). Expect: no error, `C:\Program Files\VoLum\VoLum.exe` and
  the VST3 report 1.3.0, one "VoLum" entry in Apps (no duplicate 1.2.1 entry).
- [ ] **2. Your normal library.** Launch from the Start menu. Your custom amps, presets and
  settings are intact; PLAY is not re-filled (your test builds already created a MIDI map).
- [ ] **3. Reaper VST3 project from 1.2.x.** Open an existing project that uses VoLum: same
  Sound and knobs as before.
- [ ] **4. (Optional) Uninstall and reinstall.** Apps > VoLum > Uninstall: program and VST3
  gone, your library in `%LOCALAPPDATA%\VoLum` stays. Reinstall to keep using it.

## Findings

- Gatekeeper wording (Mac step 1):
-
