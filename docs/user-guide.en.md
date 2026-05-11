# VoLum User Guide

**Languages:** English | [Deutsch](user-guide.de.md)

This guide explains the VoLum interface after installation. For download and setup steps, see the [main README](../README.md).

## Main View

VoLum main view

1. **Amp browser:** choose one of the bundled amp models from the left sidebar.
2. **Amp panel:** shows the currently selected amp. In Dual Amp mode it splits into MAIN and SUPPORT lanes.
3. **Speaker and channel controls:** choose the cab/speaker flavor and the gain-stage/channel for the focused amp.
4. **Knob row:** edit the currently focused amp, PRE pedal, or POST effect.
5. **PRE | AMP | POST strip:** click a section to expand it. Only one section is expanded at a time.
6. **Toolbar:** tuner, metronome, and settings live in the top-right area.

Most amp controls are saved per amp. When you return to an amp, VoLum restores its speaker, channel, knobs, PRE pedals, and Dual Amp setup.

## Amp Notes

VoLum includes a mix of vintage, modern, and boutique amp captures. These notes are short orientation hints, not strict rules.

- **Orange ORS100 1972:** old picture-panel Orange voice with no master volume. Expect big, clean-to-loud power-amp character rather than modern preamp gain.
- **Orange OD120 1975:** the Overdrive branch of the classic Orange circuit. It adds a master-volume gain structure, so it can push into more gain than the earlier picture-panel ORS100.
- **Marshall JMP 2203 1976:** early master-volume Marshall and a transition-era ancestor of the JCM800 2203 sound.
- **Marshall 2204 1982:** vertical-input 50W JCM800-era Marshall voice.
- **Marshall JVM 210H:** modded to include the OD1 voice from the JVM 410.
- **Lichtlaerm Prometheus:** KT88/EL34 power section with lots of headroom.
- **Diezel Herbert Mk1:** late Mk1-era Herbert, close to the Mk2 feature set but with the older Mk1 sound.
- **Soldano SLO100:** 2021-style SLO with the Deep/Depth control version.
- **Sebago Texas Flood:** high-end 100W Steel String Singer-style amp with premium build options and transformer choices.
- **THC Sunset:** boutique German Trainwreck-inspired amp.

## Tuner And Metronome

VoLum tuner overlay

Open the tuner from the top-right toolbar. While the tuner is open, VoLum mutes the output so you can tune silently. Click outside the tuner or press `Esc` to close it.

VoLum metronome controls

Open the metronome from the top-right toolbar. You can enable or disable it, set BPM with the `+` and `-` controls or by typing a value, adjust volume, and choose `1/4`, `2/4`, `3/4`, `4/4`, or `6/8`.

## Dual Amp

VoLum Dual Amp view

Dual Amp lets you combine the main amp with a support amp.

1. Click the split-panel Dual Amp button in the amp panel.
2. Click the SUPPORT side to choose a second amp.
3. Click MAIN or SUPPORT to focus that lane. The speaker, channel, and knob row follow the focused lane.
4. Use the lane pan knobs to place the two amps in the stereo field.

When Dual Amp is active, the output meter shows separate left and right bars. MAIN and SUPPORT settings are stored with the current main amp, so each amp can have its own paired setup.

VoLum compensates different NAM resampler latencies between MAIN and SUPPORT before the lanes are panned and summed. The `Ø` symbol in the upper-right of the SUPPORT amp flips its polarity against MAIN and is on by default for new Dual Amp setups; this can improve centered mono stacks for some capture pairs. If a centered dual-amp stack still sounds phasey, check that both lanes use the intended speaker/channel file and compare gate, EQ, IR, and output settings.

## PRE Section

VoLum PRE section

The PRE section runs before the amp. It contains a compressor and two assignable NAM pedal slots. Click PRE in the triptych strip to expand it, then click a card to focus its controls in the knob row.

Click a NAM pedal card while it is focused to open the capture chooser. Pedal captures are grouped by pedal type and sorted by gain inside each group.

Recommended starting points:

- **Clean or low-gain amps:** Nuke, Bender, Myth, and Mash.
- **Clean to low-gain amps:** Revival Drive.
- **Mid- to high-gain amps:** Minotaur Klon, TS, and Fatbee.

The EQ controls in VoLum are not a display of the physical amp or pedal settings used during profiling. For the intended captured sound, you do not need to touch the amp EQ or pedal EQ. Use those EQ controls only when you want extra tone shaping after the profile.

PRE settings are local to the current amp. This makes it possible to keep different compressor, pedal, and pedal-EQ choices per amp.

## POST Section

VoLum POST section

The POST section runs after the amp. It contains Delay and Reverb cards. Click POST in the triptych strip to expand it, then click a card to focus its controls in the knob row.

- **Delay:** Digital, Analog, and Reverse modes with Time, Feedback, Mix, Tone, and a mode-specific fifth knob (`Grit`, `Wear`, or `Bloom`). Ping-Pong alternates delay repeats between left and right (works from mono too); unavailable in Reverse. Factory defaults use **320 ms** Time for **Digital** and **Analog** (Reverse stays on its own longer default).
- **Reverb:** Hall, Plate, and Oktaverb modes with Mix, Decay, Tone, and Pre-Delay. Oktaverb adds Intensity and a `HALO / SHIMMER / BLOOM` selector. Factory snapshots use **20%** Mix for Hall and Plate; each Oktaverb sub-mode starts at **30%** Mix with Intensity at **65%** (Halo), **70%** (Shimmer), and **75%** (Bloom); Decay/Tone/Pre-Delay stay at their per-voice defaults. Halo runs both octave-up and octave-down voices in the feedback loop at once for a dual pitch-vector wash that keeps real body, Shimmer builds pure octave-up feedback for a lush climbing tail, and Bloom fades the wet signal in slowly for pad-like swells. Each Oktaverb sub-mode remembers its own Mix/Decay/Tone/Pre-Delay/Intensity values and is level-managed to avoid clipping at its defaults.

The small LED on each card shows whether the effect is active. The bottom label shows the current mode or preset summary. POST settings (delay + reverb knob positions, modes, and active toggles) persist per main amp - switching amps swaps the POST scene the same way it swaps PRE. When you first visit an amp with no saved POST scene yet, it starts from the factory POST defaults; from then on, that amp remembers its own POST settings. Double-click any POST knob to restore that knob's default.

Switching Delay mode, Ping-Pong, Reverb mode, or Oktaverb sub-mode clears the effect's internal tail so repeats and ambience from the previous mode do not leak into the new one.

The Reverb Mix knob is a true wet/dry equal-power crossfade: at 30-40% the reverb is musically present without dominating, at 100% the dry signal is meaningfully reduced. Mix changes are smoothed, so fast edits or automation stay clean instead of zippering. The Delay Mix knob is an additive blend (delay repeats sit on top of the dry signal), and Reverse Delay matches Digital and Analog so engaging Reverse no longer drops perceived volume.

## Output Safety

VoLum runs an always-on output safety stage at the very end of the chain (after Delay and Reverb). It does nothing on normal material, but if you stack a hot rig with heavy POST effects and crank the Output knob, peaks above roughly +3 dBFS are smoothly rolled toward a +6 dBFS ceiling so you cannot accidentally send a runaway signal to your speakers. When the stage catches a peak, the OUT meter turns red and the footer shows `Output safety active - lower output or wet mix` for a few seconds. If you see that warning often, lower Output, Reverb Mix, or Delay Mix - the safety stage is a backstop, not a creative limiter, and it should rarely engage during normal play. The plugin still relies on your audio interface gain and monitor level for absolute SPL safety.

## Keyboard Controls

- With no knob selected: `Up` / `Down` changes amp, `Left` / `Right` changes channel in AMP view.
- Click a knob to select it for keyboard control.
- Selected knob: `Up` / `Down` adjusts the value, `Left` / `Right` selects another knob.
- Hold `Shift` for finer changes.
- Press `Enter` for exact numeric entry.
- Press `Delete` or `Backspace` to reset the selected knob.
- Press `Esc` to leave knob keyboard mode.

## Settings

VoLum stores user settings automatically:

- **Windows:** `%LOCALAPPDATA%\VoLum\volum-settings.json`
- **macOS:** `~/Library/Application Support/VoLum/volum-settings.json`

The standalone app and VST3 plugin use the same VoLum settings location on each system.