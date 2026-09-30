# 1.3.0 UAT: one pass on Windows, one on macOS

Tick `W` / `M` when it works. Anything odd: one line under Findings, keep going.

Builds: CI artifacts `VoLum-win` / `VoLum-mac` from the latest green `dev` run
(`gh run download <run id> -n VoLum-win -D <dir>`, same for `VoLum-mac`).
Also try: type a space at the end of a preset name - the caret moves at once.

| # | Do this | Expect | W | M |
|---|---------|--------|---|---|
| 1 | Fresh install (or upgrade over 1.2.3), open standalone | Audio devices listed; your amps, presets and settings still there | | |
| 2 | Octaver defaults, high-gain amp, play 12th-19th fret on high E, bends + vibrato | No wobble or bitcrush; octave follows the note | | |
| 3 | Transpose INSTANT -2 and -12: single notes, then a power chord | Clean pitch; chord may smear (use POLY), no clicks | | |
| 4 | Pick each amp's factory preset (SKIP until your Pack is baked; this build still has "Ready") | Your baked sound loads, shows your name | | |
| 5 | Switch amp / channel / cab while playing | No click, no dropout; stepping back to a channel is instant | | |
| 6 | P to PLAY: stomps, rail, art moves while you play; P back to BUILD | Highlight on the sound you picked; nothing stuck | | |
| 7 | Tuner open in BUILD and PLAY, then close | Reads notes; closes on click outside and Esc | | |
| 8 | Stop playing for 10 s, then hit a note | First note is there at full level (no fade-in, no gap) | | |
| 9 | Export a Sounds Pack, import it on the other machine | Sounds, names and art arrive | | |
| 10 | MIDI footswitch / PC: recall a sound, bypass a stomp | Right sound, right stomp | | |
| 11 | Dual Amp on, pan both lanes | Both amps heard, pan follows | | |
| 12 | Batch A listen: Oktaverb Mix 0 = fully dry; Chorus Mix 100; Pitch + Comp after reset | Mix 0 is dry, full mix is not thin, reset sounds like a fresh pedal | | |
| 13 | Quit, reopen; DAW: save session, reopen | Everything as you left it; meters move, CPU calm when idle | | |

## Findings

-
