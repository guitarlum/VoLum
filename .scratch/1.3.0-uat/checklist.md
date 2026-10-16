# 1.3.0 UAT: what only you can check (about 55 min)

Machines cover the rest on every build: golden renders, 1,470 unit tests (plus
AddressSanitizer on macOS), the Windows end-to-end scenarios (484 checks, including the
custom IR / pedal import), both installer upgrade jobs, the macOS standalone e2e, and
pluginval on both platforms. What no machine can do: a real Mac with the HX, Gatekeeper, an
elevated Windows installer over your install, REAPER with your projects, and your ears.

Use the release assets of the **re-cut** v1.3.0 draft (16/10, after the import fix). Older
assets and CI artifacts predate it.

Tick `[x]` when it works. Anything odd: one line under Findings, and keep going.

## Release blockers

- [x] **Enable GitHub Pages (Q-07).** Done.
- [x] **Defaults in `.scratch/1.3.0-hunt/questions.md` (Q-01..Q-18).** Decided 11/10: all
  ship as-is.

## Ears (about 5 min, either machine)

Bug-sweep items no test can close: listen once.

- [ ] **Oktaverb at Mix 0** is the dry signal: no octave bleeding through.
- [ ] **Pitch and Compressor reset**: switch each off and on mid-note; no burst, no stuck
  tail.
- [ ] **Chorus at full mix** on each voice (CLASSIC, WARPED, CLEAR, ENSEMBLE): no zipper or
  click when you move Mix or Rate.

## Mac pass (about 30 min)

Asset: `VoLum-v1.3.0-macos-installer.dmg` from the v1.3.0 draft (open it from the Releases
page signed in). Hardware: HX Stomp XL over one USB cable as audio interface and MIDI; on
the HX, Global Settings > MIDI/Tempo: MIDI Over USB On, PC Send On, channel 1.

- [ ] **1. Install over 1.2.x with Gatekeeper.** If macOS blocks it, System Settings >
  Privacy & Security > Open Anyway. Write down the exact wording (Findings). Expect:
  standalone, VST3 and AU installed; your 1.2.x presets and custom content still there.
- [ ] **2. Audio device and persistence.** Settings > SIGNAL > Audio & MIDI devices...: the
  HX for input, output and MIDI. Play at buffer 64 and 128, listen for crackle. Turn a knob,
  Cmd+Q right away, reopen. Expect: device, buffer and knob kept.
- [ ] **3. Apple silicon listening.** Tuner agrees with the HX tuner. Octaver on a high-gain
  amp, frets 12-19 on the high E with bends: no wobble. Transpose INSTANT -2: clean. Dual
  Amp in PLAY: both lanes audible, and **Tab** (switching which lane you edit) does not
  click. Known and not a finding: switching Dual Amp itself on or off clicks once (F-97,
  1.3.1).
- [ ] **4. Retina look and the Metal change.** Glance at BUILD, PLAY (art moves while you
  play), Settings SIGNAL/MIDI/SYSTEM, the MIDI footswitch board. Expect: crisp, nothing
  clipped. Then minimise, switch Spaces and back, and close/reopen the AU editor a few
  times: it redraws every time, never freezes or goes blank.
- [ ] **5. Standalone MIDI with the HX.** Settings > MIDI: assign 3 Sounds to programs 0-2.
  HX preset knob through 01A/01B/01C: VoLum follows. Pull the USB cable while running,
  replug, reselect the device: no crash. Quit: the app exits.
- [ ] **6. AU in REAPER (quick).** HX presets switch the Sound. Save, quit, reopen: same
  Sound.

## Windows pass (about 15 min)

This laptop still has 1.2.1 installed (backup: `.scratch/1.3.0-hunt/l6/backup-initial-121/`).

- [ ] **1. Upgrade install.** Run `VoLum-v1.3.0-windows-setup.exe` from the draft (accept
  UAC). Expect: no error, VoLum.exe and the VST3 report 1.3.0, one "VoLum" in Apps.
- [ ] **2. Your library.** Launch from the Start menu: custom amps, presets and settings
  intact; PLAY not re-filled.
- [ ] **3. Your long-named IRs (the tester's report).** Manage custom IRs > + Import IR:
  pick the IRs that differ only by their trailing number, all at once. Expect: every one is
  added, the list tells them apart by their ends, and picking one shows a cab-row chip that
  differs from its neighbour's. Importing the same files again says they are already in your
  library.
- [ ] **4. REAPER VST3 project from 1.2.x.** Same Sound and knobs as before.
- [ ] **5. Portable next to the install.** Unzip `VoLum-v1.3.0-windows-portable.zip` to its
  own folder and run it while 1.3.0 is installed. PRE pedal menu: the real factory pedals,
  no placeholders; a pedal you switch on is audible.

## Findings

- Gatekeeper wording (Mac step 1):
-
