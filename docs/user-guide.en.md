# VoLum User Guide

**Languages:** English | [Deutsch](user-guide.de.md)

VoLum is a guitar amp collection for the stage, the studio and the practice desk. You shape a tone in **BUILD** and play your Sounds in **PLAY**. This guide covers VoLum 1.3. For downloads and installation, see the [README](../README.md).

## Contents

- [Quick Start](#quick-start)
- [The BUILD Screen](#the-build-screen)
- [Amps And Cabs](#amps-and-cabs)
- [PRE Pedals](#pre-pedals)
- [POST Effects](#post-effects)
- [Dual Amp](#dual-amp)
- [Presets](#presets)
- [PLAY](#play)
- [MIDI Foot Controllers](#midi-foot-controllers)
- [Tuner And Metronome](#tuner-and-metronome)
- [Your Own Amps, IRs And Pedals](#your-own-amps-irs-and-pedals)
- [Back Up And Share With Packs](#back-up-and-share-with-packs)
- [Settings](#settings)
- [Using VoLum In A DAW](#using-volum-in-a-daw)
- [Keyboard Shortcuts](#keyboard-shortcuts)
- [Troubleshooting](#troubleshooting)
- [Where VoLum Keeps Your Files](#where-volum-keeps-your-files)
- [Report A Bug Or Request A Feature](#report-a-bug-or-request-a-feature)

## Quick Start

1. **Connect your guitar.** In the standalone app, open Settings (the gear, top right), stay on **SIGNAL** and click **Audio & MIDI devices...**. Pick your audio interface, one mono input for the guitar, and your outputs. In a DAW, put VoLum on a track that records your guitar input.
2. **Pick an amp** in the left list, then a channel and a cab.
3. **Add pedals and effects.** Click **PRE** for pedals in front of the amp, **POST** for chorus, delay, reverb and tremolo.
4. **Save the tone.** Press **Ctrl+S**, type a name and press Enter.
5. **Play it.** Press **P** to switch to PLAY. Five Sounds are ready on programs 0 to 4. Click one to play it, or step through them with the arrow keys or a MIDI foot controller.

The bundled amps were captured with the interface input around +4 dBu. Set your interface to a similar level for the closest match.

## The BUILD Screen

![VoLum BUILD screen](user-guide-main.png)

BUILD is where you make a tone.

1. **Amp list:** the 15 bundled amps, then your own amps under **CUSTOM**.
2. **Amp panel:** the amp you are editing. With Dual Amp on, it splits into **MAIN** and **SUPPORT**.
3. **Channel and cab row:** the channel stepper and the cab buttons.
4. **Knob row:** the knobs of the amp, pedal or effect you selected.
5. **PRE | AMP | POST strip:** opens one section at a time. Keys `1`, `2` and `3` do the same.
6. **Toolbar (top right):** the PLAY/BUILD switch, tuner, metronome and Settings gear.

The preset name sits in the middle of the header, with `<` and `>` to step through presets.

The PLAY/BUILD switch always shows where a click takes you: a ring of stomps in BUILD, faders in PLAY. Hover it to read the destination. `P` does the same from the keyboard.

**Knobs:** drag a knob or use the mouse wheel. Double-click resets it. Click a knob to select it, then use the arrow keys (see [Keyboard Shortcuts](#keyboard-shortcuts)). Pedal and effect knobs stay editable while the pedal is off, so you can set it up before you switch it on.

### Each Amp Remembers Its Rig

VoLum remembers your cab, channel, knobs, PRE pedals, POST effects and Dual Amp setup separately for every amp. Switch back to an amp and everything is where you left it.

To keep the same pedals or effects while you try different amps, click the **lock** in the PRE or POST header:

- While locked, PRE (or POST) stays as it is when you switch amps.
- When the locked pedals differ from what this amp has stored, a **Store** arrow appears. Click it to save them to this amp only.
- Click the lock again to unlock. The amp returns to its own stored pedals. Unsaved changes in the locked section are dropped.
- Switching to PLAY keeps what you hear: VoLum stores the locked section to the current amp and turns the lock off.

## Amps And Cabs

Pick an amp in the left list, or press `Up` / `Down` when no knob is selected. A rough guide:

- **Clean, blues and boutique:** Sebago Texas Flood (a Dumble Steel String Singer-style pedal platform), THC Sunset, Bad Cat Mini Cat.
- **Vintage and classic-rock crunch:** Orange ORS100, Orange OD120, Marshall JMP 2203, Marshall 2204.
- **Modern and high gain:** Soldano SLO100, Diezel Herbert, Marshall JVM, H&K TriAmp, Fryette Deliverance, Lichtlaerm Prometheus, Brunetti XL 2.
- **Do-it-all:** Ampete One pairs an American and a British voice in one amp.

**Channels:** every amp has two to six channels, one per captured gain setting. Step through them with the channel arrows, or `Left` / `Right` in the AMP section.

**Cabs:** pick **No Cab** (the amp alone), **G12**, **G65**, **V30**, or **Custom IR** to load your own cabinet impulse response (see [Custom IRs](#custom-irs)). `S` steps to the next cab, `Shift+S` back.

**Knobs:** **INPUT**, **GATE** (noise gate threshold), **BASS**, **MID**, **TREBLE** and **OUTPUT**. The EQ knobs are extra tone shaping; they do not have to match the knob settings used when the amp was captured. **OUTPUT** is unity at `0.0 dB`; turned fully down it reads `-∞ dB` and mutes the amp.

Every bundled amp, cab and PRE pedal capture is a NAM Architecture 2 (A2) profile. If your computer struggles, try **LITE** in [Settings](#signal-tab).

## PRE Pedals

![VoLum PRE section](user-guide-pre.png)

PRE pedals sit in front of the amp, in this order: **PITCH**, **COMP**, **NAM 1**, **NAM 2**.

1. Click **PRE** (or press `1`).
2. Click a pedal card to select it.
3. Switch it on with the LED on the card, or press `Space` (`B` in a plug-in).
4. Set it up in the knob row.

### Pitch

![VoLum Pitch pedal in Transpose mode](user-guide-pitch-transpose.png)

Pick the mode with **TRANSPOSE** / **OCTAVER**. A new rig starts on **OCTAVER**.

- **TRANSPOSE** shifts your whole signal for drop tunings and capo-style shifts. **SEMI** sets the interval from −12 to +7 semitones (it starts at −2). **MIX** blends in your dry tone and **LEVEL** sets the output. Choose the engine with **INSTANT** / **POLY**:
  - **INSTANT** (default) is for single notes and leads: lowest latency (about 8.6 ms) and the tightest attack.
  - **POLY** is for chords and riffs: every note of the chord is shifted, at about 14 ms latency.
- **OCTAVER** adds octaves and follows chords. **OCT DN** and **OCT UP** set the octave-down and octave-up levels, **DRY** your original note, and **LEVEL** the output. **VINTAGE** adds grit and a darker tone, **MODERN** stays clean. It starts with OCT DN at 80%, OCT UP off, DRY at 100% and MODERN, and tracks single notes up to the 24th fret of the high E.

![VoLum Pitch pedal in Octaver mode](user-guide-pitch-octaver.png)

Both modes hold their tuning through long sustain and work down to the low F# of an 8-string. In a DAW, VoLum reports the pitch delay to the host, so the track stays in time.

### Compressor

**COMP** evens out your picking. **INPUT** sets how hard it compresses, **ATTACK** and **RELEASE** how fast it reacts and lets go, and **OUTPUT** the level. **OUTPUT** fully down (`-∞ dB`) mutes it.

### NAM Pedals

![VoLum PRE pedal capture menu](user-guide-pre-pedal.png)

**NAM 1** and **NAM 2** each hold a captured drive, boost or fuzz pedal. Both start empty. Click an empty card to open the capture menu; on a card that already holds a pedal, click once to select it and again to open the menu. Captures are grouped (Klon, TS / Boost, Distortion, Fuzz) and sorted from less to more gain. Your own pedals appear under **CUSTOM** at the bottom.

Good starting points:

- Clean or low-gain amps: Nuke, Bender, Myth, Mash.
- Edge-of-breakup amps: Revival Drive.
- Mid- and high-gain amps: Klon, TS, TS+, Fatbee.

The knobs are **GAIN**, **BASS**, **MID**, **MID Hz**, **TREBLE** and **LEVEL**. **LEVEL** fully down (`-∞ dB`) mutes the pedal.

## POST Effects

![VoLum POST section](user-guide-post.png)

POST effects sit after the amp, in this order: **CHORUS**, **DELAY**, **REVERB**, **TREM**.

1. Click **POST** (or press `3`).
2. Click an effect card to select it.
3. Switch it on with the LED on the card, or press `Space` (`B` in a plug-in).
4. Pick a mode at the top of the knob row and set the knobs.

Each effect remembers its knob settings per mode, so you can try another mode and come back without losing anything. Changing mode never drags the old echoes or reverb tail into the new one.

### Chorus

![VoLum Chorus card](user-guide-chorus.png)

Chorus runs before Delay and Reverb, so it colours the amp and not the echoes. Pick a voice:

- **CLASSIC:** a Juno-60 style stereo sweep.
- **WARPED:** tape wow and flutter. At **MIX** 100% it becomes a vibrato.
- **CLEAR:** a wide Dimension-style chorus that stays in tune in mono.
- **ENSEMBLE** (default): an 80s rack chorus with three voices across left, centre and right.

The knobs are **RATE**, **DEPTH**, **TONE** (darkens the chorus only, not your dry tone), **WIDTH** (0% is mono-safe) and **MIX**. Every voice starts at **MIX** 50%. At **MIX** 0% the chorus is fully out of the signal, so you can leave it on and fade it in. Chorus has no tempo sync.

### Delay

Pick **DIGITAL**, **ANALOG** or **REVERSE**. The knobs are **TIME**, **FEEDBACK**, **MIX**, **TONE**, and a character knob per mode: **GRIT** (Digital), **WEAR** (Analog) or **BLOOM** (Reverse).

- **PING-PONG** (Digital and Analog) bounces the repeats right, left, right. They sit about 3 to 7 dB quieter than the plain delay, because each repeat plays on one side only. It works with Dual Amp however the amps are panned.
- **TEMPO SYNC** locks the repeats to the tempo. **TIME** becomes a **DIVISION** stepper: 1/2, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T, 1/16.

### Reverb

Pick **HALL**, **PLATE** or **OKTAVERB**. The knobs are **MIX**, **DECAY**, **TONE** and **PRE-DLY** (the gap before the reverb starts, 10 ms by default). **OKTAVERB** adds pitch-shifted shimmer with three voices, **HALO**, **SHIMMER** and **BLOOM**, and an **INTENSITY** knob.

### Tremolo

![VoLum Tremolo card](user-guide-tremolo.png)

Tremolo runs last, so it pulses the whole sound including the reverb. Pick a voice:

- **OPTICAL** (default): a choppy, photocell-style throb.
- **BIAS:** a smooth, even sine pulse.
- **HARMONIC:** splits the sound into lows and highs that pulse in turn. The extra **X-OVER** knob sets the split frequency.

The knobs are **RATE**, **DEPTH**, **SHAPE** (from smooth sine to hard square) and **MIX**. **TEMPO SYNC** turns **RATE** into a **DIVISION** stepper, like the delay.

### Tempo

Delay and Tremolo share one tempo. In a DAW they follow the song tempo. In the standalone app they follow the metronome BPM, even while the click is off.

## Dual Amp

![VoLum Dual Amp](user-guide-dual-amp.png)

Dual Amp plays two amps at once: **MAIN** and **SUPPORT**.

1. Open the AMP section (`2`).
2. Click the split-panel **Dual Amp** button ("Switch to Dual Amp"). `Space` (`B` in a plug-in) does the same.
3. Click the SUPPORT side (**Choose support amp**) and pick the second amp from the list. **(none)** at the top empties the lane again.
4. Click MAIN or SUPPORT (or press `Tab`) to choose which amp the cab row and knob row edit.

Each lane has its own channel, cab or custom IR, knobs, and a small **PAN** knob in its title strip. When you first switch Dual Amp on, MAIN goes hard left and SUPPORT hard right.

The SUPPORT lane has a **Ø** (polarity) button, on by default. Some amp pairs sound fuller with it on, others with it off. If the pair sounds thin or hollow, toggle **Ø**.

The SUPPORT **OUTPUT** knob fully down (`-∞ dB`) mutes that lane. VoLum lines up the timing of both amps before it mixes them.

## Presets

![VoLum preset menu](user-guide-presets.png)

A preset stores the whole rig of one amp: cab, channel, knobs, PRE pedals, POST effects and Dual Amp setup. Each amp has its own list.

### Factory Presets

Every bundled amp comes with one or two Factory presets, 21 in all. They are read-only and use only content that ships with VoLum. You can put any of them on PLAY without saving first.

| Amp | Factory presets |
| --- | --- |
| Ampete One | Ampete Rhythm, Ampete Lead |
| Bad Cat Mini Cat | BadCat Crunch |
| Brunetti XL 2 | American Lead |
| Diezel Herbert Mk1 | Thicc Rhythm, Sanitarium |
| Fryette Deliv. 120 | Dry Rhythm |
| H&K TriAmp Mk2 | HiFi Heavy, HiFi Crunch |
| Lichtlaerm Prom. | Modern Rhythm, Modern Lead |
| Marshall 2204 | JCM800 Crunch |
| Marshall JMP 2203 | Crack the Skye |
| Marshall JVM | Modern British |
| Orange OD120 | Stoner |
| Orange ORS100 | An old Soul |
| Sebago Texas Fl. | The bestest Clean |
| Soldano SLO100 | SLO Lead, SLO Crunch |
| THC Sunset | Sunset Crunch, Sunset Clean |

### Save A Preset

Press **Ctrl+S** in any BUILD section. A name box opens:

- On a Factory preset, Default or an unnamed rig, it suggests **New Preset**. **Save** creates a new User preset. Factory presets are never overwritten.
- On one of your User presets, it suggests that preset's name. The button reads **Update**, and saving overwrites it. Type another name and the button changes to **Save**, which creates a new preset.
- A User preset may share a Factory preset's name; it is listed under **USER**. If the name is taken a second time, VoLum adds a number, for example "SLO Lead 2".

`Enter` saves. **Cancel**, `Esc` or a click outside the box closes it without saving. Name boxes edit like any text box: `Ctrl+Backspace` deletes a word, `Ctrl+Left` / `Ctrl+Right` jump by word (add `Shift` to select), double-click selects a word, and `Ctrl+Z` / `Ctrl+Y` undo and redo.

### The Preset Menu

Click the preset name in the header to open the menu:

- **Default (factory settings)** resets the amp to its shipped settings.
- **FACTORY** and **USER** list the presets. Click a heading to open or close it.
- **Overwrite "name"** saves the current rig into the User preset you are on, after asking.
- **Save current as new...** opens the name box.
- **Manage presets...** opens a panel with all User presets of this amp. **+ Save current as new** adds one. Each row has icons to overwrite, rename and delete. Double-click a row to load it.

The `<` / `>` arrows step through the amp's Factory presets, then its User presets. **(unsaved)** after the name means you changed something since you loaded the preset. It goes away when the rig matches again.

Saving in BUILD never changes which Sound sits on a PLAY switch. If you delete a preset that is on PLAY, the confirmation names those switches (for example "On PLAY 03 and 12: they will read Invalid."). They keep their program numbers and show **Invalid slot** until you give them another Sound.

## PLAY

PLAY is your stage view: your Sounds on a list, eight stomp switches, and the amp art. Switch with `P` or the PLAY/BUILD switch. Switching never changes what you hear.

A **Sound** is an amp with one of its presets. Each Sound sits on a program number from 0 to 127, the same numbers a MIDI foot controller sends.

### The Five Starting Sounds

![VoLum PLAY with its five starting Sounds](user-guide-play-start.png)

The first time VoLum 1.3 opens your library, PLAY gets five Factory Sounds:

| Program | Sound | Amp |
| --- | --- | --- |
| 0 | The bestest Clean | Sebago Texas Fl. |
| 1 | SLO Crunch | Soldano SLO100 |
| 2 | Modern Rhythm | Lichtlaerm Prom. |
| 3 | Crack the Skye | Marshall JMP 2203 |
| 4 | Ampete Lead | Ampete One |

Many foot controllers count from 1, so there these are presets 1 to 5. This happens only once. A board you have changed or cleared is never filled again.

### Play Your Sounds

![VoLum PLAY board](user-guide-play.png)

- **Recall a Sound:** click its row, press `Up` / `Down` (or `Left` / `Right`), or send MIDI. The arrow keys skip empty and broken entries and wrap around at the ends.
- **LIVE** marks the Sound you recalled last. **(unsaved)** means you changed it since.
- **Stomps:** the eight switches turn Pitch, Comp, NAM 1, NAM 2, Chorus, Delay, Reverb and Tremolo on and off. Click them or press `1` to `8`. They change nothing else. An empty NAM slot does nothing. Right-click a stomp to edit that pedal in BUILD.
- **Tweak and save:** change a Sound in BUILD or with the stomps, then press **Ctrl+S**. If the LIVE switch still holds the Sound you started from, your saved version takes its place. That way a changed Factory Sound becomes your own copy on the same switch. Ctrl+S never picks a new program number.

### Add, Replace And Arrange Sounds

![VoLum PLAY Add Sound picker](user-guide-play-picker.png)

The **+** button at the end of the list changes with what you are playing:

- **+ Add this sound** puts what you hear on the next free program number. A User Sound or an unchanged Factory preset goes straight on. If the rig has no name yet, or you changed a Factory preset, the name box opens first; **Save** creates the User Sound and adds it in one step. The Factory Sound keeps its own row.
- **+ Add Sound** appears when what you hear is already on the list. It opens the picker: choose a **PROGRAM** number (the next free one by default; a taken number shows what it replaces), then a Factory or User preset.

To replace a Sound, double-click its row or use its assign button. To clear a row, click its small `×`. Drag a row onto another row to swap them, between two rows to slide the Sounds along, or onto the dashed **Add** plate to move it to the end. If every program number is taken, **+** opens the picker on program 0.

If you clear every row, PLAY shows **No Sounds assigned** with a **+** button in the middle. The amp you were playing keeps sounding.

### Moving Art

The amp art lights up and moves while you play and returns to its still picture when you stop. The harder you play, the more it moves. Every bundled amp has its own motion; custom amps slowly turn and breathe. To keep the art still and save a little CPU, turn off **Animate art in PLAY** in [Settings](#signal-tab).

## MIDI Foot Controllers

A foot controller recalls your PLAY Sounds:

- **Program Change N** recalls the Sound on program N.
- **CC 102 with value N** does the same. Use it if your DAW does not pass Program Change on to plug-ins.

Program numbers run from 0 to 127. A number with no Sound, or with a deleted Sound, is ignored and the current tone keeps playing.

### Connect The Controller

- **Standalone:** pick the controller's MIDI input in **Settings > SIGNAL > Audio & MIDI devices...**.
- **DAW:** route the controller's MIDI to the VoLum track. The standalone app and the AU always receive Program Change. Some VST3 hosts do not pass it on; then send the Recall CC instead.

### The MIDI Tab

![VoLum Settings, MIDI tab](user-guide-settings-midi.png)

**What this VoLum listens to:**

- **All channels** (the default; MIDI calls this Omni) is right for one guitarist with one board.
- **One channel** with a channel from `CH 1` to `CH 16` lets two VoLums share one MIDI cable.
- **Recall CC** sets the CC number that recalls Sounds: `0` to `119`, default `102`.

**What each program number plays** shows your Sounds like a foot controller: 16 banks of 8 switches. Bank 1 holds programs 0 to 7, bank 2 holds 8 to 15, and so on up to bank 16 (120 to 127). It is the same list as PLAY; a change here shows up there and the other way round.

- Change banks with the arrows next to **BANK**, the mouse wheel, `PageUp` / `PageDown`, or the 16 dots (a lit dot means the bank has Sounds).
- Click a switch to choose its Sound. Hover a switch and click `×` to clear it.
- Drag a switch onto another to swap them, or onto an empty switch to move it. While dragging, hover an arrow or a dot to change bank.
- The switch of the LIVE Sound is lit **LIVE**. A switch whose Sound was deleted is red.

The PLAY list is shared by every VoLum on your computer. The listen setting and Recall CC belong to each plug-in instance; a new instance starts on All channels and CC 102.

VoLum does not use MIDI notes, pitch bend, Bank Select (CC 0/32), MIDI Learn or MIDI output. CC 120 to 127 cannot be the Recall CC, because hosts reserve them.

On macOS, MIDI input makes the AU an "aumf" (music effect) instead of an "aufx" (effect). If an AU instance from an older VoLum misbehaves after updating, remove it and insert it again.

## Tuner And Metronome

![VoLum tuner](user-guide-tuner.png)

Open the tuner with its toolbar button or `T`. While it is open, your guitar is muted so you can tune silently; the metronome keeps clicking. Close it with `Esc` or a click outside.

![VoLum metronome](user-guide-metronome.png)

Open the metronome with its toolbar button or `M`. Switch it on, set the tempo with `+` / `-` or type a value (30 to 300 BPM, default 120), set the volume, and choose `1/4`, `2/4`, `3/4`, `4/4` or `6/8`.

## Your Own Amps, IRs And Pedals

VoLum loads your own NAM captures and impulse responses. Imports are copied into VoLum's own library, so they keep working if you move or delete the originals. The standalone app and every plug-in share this library.

### Custom Amps

![VoLum custom amp builder](user-guide-custom-amp.png)

1. Click **+** in the **CUSTOM** part of the amp list ("Create a custom amp").
2. Name the amp and click **+ Add .nam files**.
3. Give each file a cab slot and a channel. Files named like `V30-MyAmp-2.nam` fill this in for you: the first part is the cab (`AMP`, `DI` or `DIRECT` for no cab, `G12`, `G65`, `V30`), the last number is the channel.
4. Click **Save amp**.

Each file is one captured channel-and-cab combination; VoLum switches between them. Both NAM A1 and A2 captures work. If any file cannot be read, nothing is saved and the builder names the file.

Custom amps play like bundled ones, also as the Dual Amp SUPPORT. Use the pen and bin icons in the amp list to edit or delete one. If a file goes missing later, the footer says so and VoLum keeps the last capture that worked.

### Custom IRs

![VoLum custom IR menu](user-guide-custom-ir.png)

A custom IR is your own speaker cabinet. It replaces the cab, so it needs an amp-only (No Cab) capture: every bundled amp has one, and custom amps have one if you added a `DIRECT` file. Where there is none, **Custom IR** and **No Cab** are greyed out; hover them to see why.

1. Click **Custom IR** in the cab row.
2. In the menu, click **Manage custom IRs...**, then **+ Import IR (.wav)**.
3. Pick the IR from the **Custom IR** menu.

Each Dual Amp lane has its own custom IR. VoLum sets imported IRs to a similar loudness as the stock cabs and refuses very large files (a whole song picked by mistake).

To fine-tune an IR, click the **gear** on its row in **Manage custom IRs**. **Level** (±24 dB), **Low cut** (20 to 800 Hz) and **High cut** (1 to 20 kHz) step with **+** / **−**, or click a value and type it, for example `2.5k` or `-3 dB`. `0` or `off` switches a cut off. These settings travel with the IR wherever you use it. A gold gear marks a shaped IR.

### Custom Pedals

![VoLum custom pedal menu](user-guide-custom-pedal.png)

In the NAM pedal capture menu, click **Manage custom pedals...**, then **+ Import pedal (.nam)**. Imported pedals appear under **CUSTOM** in the same menu.

### Deleting Content

If you delete something that is playing, VoLum moves on in the same step: a deleted amp falls back to a bundled one, a deleted pedal leaves its slot empty, a deleted IR returns to the amp's own cab. The confirmation tells you what will happen first. Another open VoLum keeps playing until it next needs the deleted item.

## Back Up And Share With Packs

![VoLum Pack export](user-guide-pack-export.png)

A Pack is one `.volumpack` file with your custom amps, IRs, pedals and presets. Use it to back up your library, move it to another computer, or share part of it. Open **Settings > SYSTEM** and use **Export Pack...** or **Import Pack...** on the **Back up your library** card.

### Export

Choose what goes in:

- **Everything:** your whole library. From the standalone app it also carries your machine settings and MIDI program list.
- **Sounds:** tick presets. Your PLAY Sounds are listed first, in program order.
- **A whole amp:** a custom amp with every preset saved on it.

The **ALSO INCLUDING** band lists what your choice needs, like an IR or a pedal. It always travels along, so nothing arrives broken. A preset on a bundled amp brings its custom IR and pedal, but no amp, because the other computer has the bundled amp already.

### Import

![VoLum Pack import preview](user-guide-pack-import.png)

The preview lists every item in the Pack, all ticked. Untick what you do not want; anything a ticked preset needs stays ticked. Each row says what will happen: **Add**, **Replace**, **Keep mine**, **Reloads** (it is playing now) or **Skip**. An item with the same name as one of yours is added beside it, and both are kept.

An **Everything** Pack offers three ways to merge:

- **Overwrite:** the Pack wins where both have an item. Your other items stay.
- **Add:** your version wins. Only new items are added.
- **Reset:** the Pack replaces your library. Items not in the Pack are deleted. Only offered while every item is ticked.

A **Sounds** or **A whole amp** Pack always merges like Overwrite and never offers Reset, so a Pack from a friend cannot wipe your library.

In the standalone app, an Everything Pack also offers **Also restore machine settings** (last amp, per-amp settings, Lite, input calibration and MIDI program list).

Your previous library is kept as `volum-content.json.pre-import.bak`. A damaged Pack changes nothing and says **This Pack is damaged.** A Pack from a newer VoLum is refused.

## Settings

Open Settings with the gear or `H`. Close it with the gear, the X, `H`, `Esc`, or a click outside the panel. Settings has three tabs and reopens on the one you used last.

### SIGNAL Tab

![VoLum Settings, SIGNAL tab](user-guide-settings-signal.png)

- **Input calibration:** enter your interface's input level in dBu and switch on **Calibrate input**. VoLum then drives the amp as hard as during the capture. This needs a capture that records its capture level. The bundled amps do not, so for them the card reads "This model has no capture level".
- **Output mode:** **Raw** leaves the level alone, **Normalized** (default) brings all amps to a similar loudness, **Calibrated** matches your input calibration (only for captures that carry an output level).
- **Performance:** **FULL** (default) or **LITE**. LITE runs a smaller version of the A2 captures on every amp and NAM pedal lane, for less CPU at slightly lower quality. It does not affect the Pitch pedal or older non-A2 captures. **Animate art in PLAY** (default ON) switches the moving art on or off; the sound is not affected.
- **Audio & MIDI devices...** (standalone only) opens the audio and MIDI device window. In a plug-in, use your DAW's audio settings.

LITE and Animate art are saved for your computer, not per project. Input calibration is the default for every new VoLum on this computer.

### MIDI Tab

See [MIDI Foot Controllers](#the-midi-tab).

### SYSTEM Tab

![VoLum Settings, SYSTEM tab](user-guide-settings-system.png)

- **Keyboard shortcuts:** a short list of keys.
- **Model information:** details of the loaded capture.
- **Back up your library:** **Export Pack...** and **Import Pack...** (see [Packs](#back-up-and-share-with-packs)).
- **About:** version, a **Read the manual** link to this guide, **Check for updates automatically** and **Check now**.

### Update Checks And Privacy

VoLum checks for a new release at most once a day. A gold dot on the gear means one is available; open Settings to read it and follow the link to the release page. VoLum never downloads or installs anything itself.

The check is a plain request for `https://guitarlum.github.io/VoLum/appcast.json`, with no tracking and no identifier. Turn it off with **Check for updates automatically**.

### Audio & MIDI Devices (Standalone)

Open the window with **Audio & MIDI devices...** on the SIGNAL tab. On Windows, `Ctrl+,` also works; on macOS, **VoLum > Preferences...**.

- Choose the driver, the input and output devices (they can differ), the sample rate, the buffer size and the MIDI input.
- Use one mono input channel for the guitar.
- Buffer sizes are 48, 64, 96, 128, 256, 512, 1024, 2048, 4096 and 8192 samples. If the driver grants a different size, VoLum shows and keeps what is really running.
- The sample rate shown is the one the driver really runs. If your interface refuses a rate, VoLum tells you which one it uses instead.

The **Latency** line shows what you hear when the driver reports its own delay (ASIO does). Otherwise it shows VoLum's part plus the buffer, and says the real figure is higher. VoLum itself adds no delay unless a capture runs at a different sample rate than your interface (about 1.4 ms at 44.1 kHz) or the Pitch pedal is on.

## Using VoLum In A DAW

- VoLum runs as a VST3 plug-in, and as an AU on macOS. Logic Pro and GarageBand need the AU.
- **Your standalone tones are the starting point.** A new plug-in instance starts with the per-amp settings you made in the standalone app. From then on, the DAW project stores that instance's settings. Plug-ins never write per-amp settings back, so two tracks cannot overwrite each other's tones.
- **Per computer:** LITE and Animate art are not stored in projects, so a project made on a fast computer still runs LITE on a slow one. Input calibration changed in any VoLum becomes the default for new instances; a saved project keeps its own.
- **Per instance:** BUILD or PLAY, the MIDI listen setting and the Recall CC. A new instance starts in BUILD, on All channels and CC 102.
- **Shared:** the custom library and the PLAY program list. A project stores references to your custom amps, IRs, pedals and presets and finds them again as long as they are still in your library.
- In a plug-in, `B` switches the selected pedal on and off, so `Space` stays free for your DAW's play/stop.
- VoLum reports its delay to the DAW, which compensates for it automatically.

## Keyboard Shortcuts

Keys work when no text box is open. While Settings, a Pack window or another panel is open, keys do not change the amp or PLAY behind it.

**Everywhere**

| Key | Action |
| --- | --- |
| `P` | Switch between BUILD and PLAY |
| `T` / `M` / `H` | Tuner / metronome / Settings |
| `Ctrl+S` | Save a preset or Sound |
| `Esc` | Close the open panel or menu |

**BUILD**

| Key | Action |
| --- | --- |
| `1` / `2` / `3` | Open PRE / AMP / POST |
| `Up` / `Down` | Previous / next amp |
| `Left` / `Right` | AMP: previous / next channel. PRE and POST: previous / next pedal |
| `Tab` / `Shift+Tab` | Next / previous pedal; in Dual Amp, switch between MAIN and SUPPORT |
| `S` / `Shift+S` | Next / previous cab (AMP section) |
| `Enter` | Select a knob of the selected pedal or amp |
| `Space` (standalone), `B` (plug-in) | Switch the selected pedal on or off; in the AMP section, Dual Amp |

**Selected knob**

| Key | Action |
| --- | --- |
| `Up` / `Down` | Turn the knob (`Shift` for small steps) |
| `Left` / `Right` | Select the previous / next knob |
| `Enter` | Type an exact value |
| `Delete` / `Backspace` | Reset to default |
| `Esc` | Deselect the knob |

When typing a value, a comma works as a decimal point and you may add the unit (`dB`, `%`, `ms`, `Hz`, `s`, `st`). Values outside the range snap to the nearest end.

**PLAY**

| Key | Action |
| --- | --- |
| `Up` / `Down`, `Left` / `Right` | Previous / next Sound |
| `1` to `8` | Toggle the eight stomps, left to right |

**Settings MIDI tab:** `PageUp` / `PageDown` change bank. `Esc` first closes an open Sound picker, then Settings.

Full screen-reader support is not available yet.

## Troubleshooting

- **"Output safety active - lower output or wet mix" in the footer, red OUT meter:** VoLum's output limiter caught runaway peaks. Lower the amp **OUTPUT**, Delay **MIX** or Reverb **MIX**.
- **No sound with the tuner open:** that is on purpose; close the tuner.
- **Dual Amp sounds thin:** toggle **Ø** on the SUPPORT lane.
- **Foot controller ignored in a DAW:** send the Recall CC (102) instead of Program Change, and check the channel on the MIDI tab.
- **The driver you picked has no device** (for example ASIO without an ASIO interface): VoLum shows an error and goes back to the last working setup.
- **Interface unplugged at start:** VoLum opens without audio and keeps your settings. Connect the interface and restart VoLum.
- **Windows: VoLum says another copy is already running:** that copy really is still open. End it in Task Manager and start VoLum again.
- **macOS: the plug-in does not show up after installing from the zip:** see the [README](../README.md#important-security-notice).

## Where VoLum Keeps Your Files

| What | Windows | macOS |
| --- | --- | --- |
| Settings | `%LOCALAPPDATA%\VoLum\volum-settings.json` | `~/Library/Application Support/VoLum/volum-settings.json` |
| Custom library | `%LOCALAPPDATA%\VoLum\content` | `~/Library/Application Support/VoLum/content` |
| Diagnostic log | `%LOCALAPPDATA%\VoLum\volum.log` | `~/Library/Application Support/VoLum/volum.log` |

If VoLum cannot read the library, it keeps the damaged file as `volum-content.json.bak` and tells you the next time a window opens. The log is small, trims itself, and records startup, the audio setup, every amp and IR load, and any errors.

## Report A Bug Or Request A Feature

Open an [issue on GitHub](https://github.com/guitarlum/VoLum/issues/new/choose). Use **Bug report** for crashes or wrong behaviour and **Feature request** for ideas. Attach `volum.log` (see [Where VoLum Keeps Your Files](#where-volum-keeps-your-files)) if you can.
