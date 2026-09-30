# VoLum v1.3.0 - PLAY

Draft for the GitHub release. Check asset names with `gh release view v1.3.0` before publishing.

## What's new

- **PLAY mode.** A stage surface next to BUILD (press **P**): eight stomp switches, a rail of your Sounds, and amp art that moves while you play. All 15 factory amps have their own motion.
- **MIDI recall of complete Sounds.** Program Change (and a Recall CC for hosts that never send PC) recalls an amp with one of its presets. Settings -> MIDI shows the 128 programs as a foot controller: 16 banks of 8.
- **Chorus.** A fourth POST pedal ahead of Delay with four voices: CLASSIC (Juno-style sweep), WARPED (tape wow and flutter), CLEAR (wide, mono-clean) and ENSEMBLE (80s tri-stereo rack).
- **Packs.** Export and import your custom amps, IRs, pedals and presets as one `.volumpack` file: everything, just Sounds, or a whole amp.
- **Factory presets.** Every factory amp ships with a dialled-in sound. (ONLY IF the owner's Pack is baked before release; otherwise drop this line.)
- **Settings in three tabs** (SIGNAL, MIDI, SYSTEM), and an optional once-a-day update reminder.

## Fixed

- The Octaver no longer wobbles on high notes into a high-gain amp: pitch tracking now covers the whole neck, up to the 24th fret of the high E. DROP and INSTANT Transpose follow high notes too.
- Typing a space at the end of a name moves the caret right away.
- Lower CPU and memory use across the board: less work with the editor closed, in PLAY, with the tuner open, and when several instances load the same amp. The sound is unchanged apart from the Octaver fix.

## Downloads

- Windows: `VoLum-v1.3.0-windows-setup.exe` (installer) or `VoLum-v1.3.0-windows-portable.zip` (VST3 + standalone, keep `VoLumRigs` next to them).
- macOS: `VoLum-v1.3.0-macos-installer.dmg` (standalone, VST3 and AU), or the separate standalone, VST3 and AU downloads.

Your presets, custom content and settings carry over from 1.2.x.

**Full Changelog**: https://github.com/guitarlum/VoLum/compare/v1.2.3...v1.3.0
