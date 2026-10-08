# 1.3.0 UAT: a Mac pass now, a Windows pass on the release build

Machines already cover the sound of every amp, pedal and effect (golden renders), MIDI
decoding, Pack logic, 1.2.x state and library migration, VST3 validation, the installers and
the Windows end-to-end scenarios. These two passes cover only what no machine has tested: the
Mac, real MIDI from the HX Stomp XL, AU and Reaper, and Gatekeeper.

Tick `[x]` when it works. Anything odd: one line under Findings, and keep going.

## Mac pass, now (about 35 min)

Build: open [CI run 37244323156](https://github.com/guitarlum/VoLum/actions/runs/37244323156)
in Safari on the MacBook. Download the `VoLum-mac` artifact, unzip it, and use the
`...-macos-installer.dmg`. Hardware: HX Stomp XL over one USB cable as both the audio interface
and MIDI. On the HX, Global Settings > MIDI/Tempo: MIDI Over USB On, PC Send On, channel 1.

- [ ] **1. Install over 1.2.2 with Gatekeeper.** Run the installer DMG. If macOS blocks it, use
  System Settings > Privacy & Security > Open Anyway. Write down the exact wording of each step
  for the release notes (under Findings). Expect: standalone, VST3 and AU all installed, and
  your 1.2.2 presets and custom content still there.
- [ ] **2. Audio device and persistence.** Open VoLum > Preferences... (and also try Settings >
  SIGNAL > Audio & MIDI devices...). Pick the HX for input, output and MIDI input. Play at
  buffer 64 and 128 and listen for crackle. Turn a knob, quit right away with Cmd+Q, then
  reopen. Expect: the device, buffer and knob were all kept.
- [ ] **3. Apple silicon listening.** The tuner agrees with the HX's own tuner. Octaver on a
  high-gain amp, frets 12-19 on the high E with bends: no wobble. Transpose INSTANT -2: clean.
  (Apple silicon runs its own fast math paths, which were fixed in this release.)
- [ ] **4. Retina look.** Glance at BUILD, PLAY (art moves while you play, P toggles), Settings
  SIGNAL/MIDI/SYSTEM, the MIDI footswitch board, and a Dual Amp lane. Expect: crisp text,
  nothing clipped. Screenshot anything odd.
- [ ] **5. Standalone MIDI.** In Settings > MIDI, assign 3 Sounds to programs 0-2. Turn the HX
  preset knob through 01A/01B/01C: VoLum follows, and with Settings open the LIVE lamp moves.
  Set "One channel" to 2: VoLum ignores the HX. Set it back to All. Pull the USB cable while
  running, plug it back in and reselect the device: no crash. Quit: the app actually exits.
- [ ] **6. AU in Reaper.** Rescan plug-ins, then insert VoLum (AU) on a track with HX audio in
  and HX MIDI as the track input. HX presets switch the Sound. Insert a second VoLum on another
  track with a different amp, then remove one. Save the project, quit Reaper and reopen it.
  Expect: same Sounds and knobs, no crash.
- [ ] **7. Small Mac-only paths.** Settings > SYSTEM > Check now gives a clear answer. About >
  "Read the manual" opens the browser.
- [ ] **8. Pack from Windows to Mac.** Copy `presetSounds.volumpack` to the Mac and import it
  (Settings > SYSTEM > Import Pack). Expect: all 21 presets, with their names and art.

## Windows pass, on the release build (about 15 min)

Setup: the HX is plugged in with the Line 6 driver installed. Close the VoLum standalone
before opening Reaper, because Windows lets only one app open a MIDI port at a time.

- [ ] **1. First run, with your real library untouched.** Run
  `$env:LOCALAPPDATA="$env:TEMP\volum-firstrun"; & "C:\Program Files\VoLum\VoLum.exe"`.
  Expect: PLAY already shows the 5 Sounds in order (The bestest Clean, SLO Crunch, Modern
  Rhythm, Crack the Skye, Ampete Lead). Pick the HX MIDI input, then walk the HX preset knob
  through 01A to 02B. Each program recalls the right amp and sounds as you dialed it. Also open
  the Factory menu on an amp with two presets (for example Soldano): both appear.
- [ ] **2. Reaper VST3.** VoLum on a track with HX MIDI input. Try a Program Change, then a
  footswitch command sending CC 102 with value = program number. Write down which of the two
  arrived (under Findings), for the user guide. Save the project, reopen it: same state.
- [ ] **3. Blind switch-on (in the first-run window from step 1).** Pick Default on any amp:
  Pitch on gives an octave down, switching it to Transpose gives -2, and Chorus on gives
  ENSEMBLE.
- [ ] **4. Saving in BUILD leaves PLAY alone.** In your normal library, recall a Sound that is
  on a switch, tweak it, Ctrl+S it under a new name, then Manage > New on another amp. Back in
  PLAY: no switch changed. Then in PLAY, tweak a Factory Sound and press Ctrl+S: that switch
  now plays your copy.
- [ ] **5. Your normal library.** Launch normally. Your library and settings are intact. PLAY
  is not re-filled; that is expected, because your test builds already created a MIDI map.

## Findings

- Gatekeeper wording (Mac step 1):
- Reaper VST3, Program Change or CC 102 (Windows step 2):
-
