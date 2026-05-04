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

- **Delay:** Tape, Digital, Ping Pong, and Reverse modes with Time, Feedback, and Mix.
- **Reverb:** Hall, Plate, and Oktaverb modes with Mix, Decay, Tone, Pre-Delay, and Shimmer.

The small LED on each card shows whether the effect is active. The bottom label shows the current mode or preset summary. POST effects are shared/global rather than per-amp, so they behave like the final effects placed after the amp section.

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