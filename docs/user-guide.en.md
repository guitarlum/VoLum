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

1. **Connect your guitar.** In the standalone app, open Settings (the gear, top right), stay on **SIGNAL** and click **Audio & MIDI devices...**. Pick your audio interface, one mono input for the guitar, and your outputs. In a DAW, put VoLum on a track that records your guitar.
2. **Pick an amp** in the left list, then a channel and a cab.
3. **Add pedals and effects.** Click **PRE** for pedals in front of the amp, **POST** for chorus, delay, reverb and tremolo.
4. **Save the tone.** Press **Ctrl+S**, type a name and press Enter.
5. **Play it.** Press **P** to switch to PLAY. Five Sounds are ready on programs 0 to 4. Click one, or step through them with the arrow keys or a MIDI foot controller.

## The BUILD Screen

![VoLum BUILD screen](user-guide-main.png)

BUILD is where you make a tone.

1. **Amp list:** the 15 bundled amps, then your own amps under **CUSTOM**.
2. **Amp panel:** the amp you are editing. With Dual Amp on, it splits into **MAIN** and **SUPPORT**.
3. **Channel and cab row.**
4. **Knob row:** the knobs of the amp, pedal or effect you selected.
5. **PRE | AMP | POST strip:** opens one section at a time (or press `1`, `2`, `3`).
6. **Toolbar (top right):** PLAY/BUILD switch, tuner, metronome and Settings.

The preset name sits in the middle of the header, with `<` and `>` to step through presets.

**Knobs:** drag or use the mouse wheel. Double-click resets a knob. Pedal and effect knobs stay editable while the pedal is off.

### Each Amp Remembers Its Rig

VoLum remembers cab, channel, knobs, PRE pedals, POST effects and Dual Amp separately for every amp. Switch back to an amp and everything is where you left it.

To keep the same pedals or effects while you try different amps, click the **lock** in the PRE or POST header. A **Store** arrow appears when the locked pedals differ from what this amp has saved; click it to save them to this amp. Unlock to return to the amp's own pedals. Switching to PLAY keeps what you hear: VoLum stores the locked section to the current amp and unlocks.

## Amps And Cabs

Pick an amp in the left list, or press `Up` / `Down`. A rough guide:

- **Clean, blues and boutique:** Sebago Texas Flood (a Dumble Steel String Singer-style pedal platform), THC Sunset, Bad Cat Mini Cat.
- **Vintage and classic-rock crunch:** Orange ORS100, Orange OD120, Marshall JMP 2203, Marshall 2204.
- **Modern and high gain:** Soldano SLO100, Diezel Herbert, Marshall JVM, H&K TriAmp, Fryette Deliverance, Lichtlaerm Prometheus, Brunetti XL 2.
- **Do-it-all:** Ampete One pairs an American and a British voice in one amp.

**Channels:** every amp has two to six channels, one per captured gain setting.

**Cabs:** **No Cab** (the amp alone), **G12**, **G65**, **V30**, or **Custom IR** for your own cabinet (see [Custom IRs](#custom-irs)). `S` / `Shift+S` step through No Cab, G12, G65 and V30 (pick Custom IR with its own button).

**Knobs:** **INPUT**, **GATE** (noise gate), **BASS**, **MID**, **TREBLE** and **OUTPUT**. **OUTPUT** fully down (`−∞`) mutes the amp.

All bundled amps, cabs and PRE pedals are NAM A2 captures. If your computer struggles, try **LITE** in [Settings](#signal-tab).

## PRE Pedals

![VoLum PRE section](user-guide-pre.png)

PRE pedals sit in front of the amp: **PITCH**, **COMP**, **NAM 1**, **NAM 2**. Click **PRE** (or press `1`), click a pedal card, switch it on with its LED (or `Space`; `B` in a plug-in), and set it up in the knob row.

### Pitch

![VoLum Pitch pedal in Transpose mode](user-guide-pitch-transpose.png)

Pick **TRANSPOSE** or **OCTAVER** (the default).

- **TRANSPOSE** shifts your whole signal for drop tunings. **SEMI** goes from −12 to +7 semitones (starts at −2), **MIX** blends in your dry tone, **LEVEL** sets the output. **INSTANT** (default) is for single notes and leads, with the lowest latency; **POLY** is for chords, with a little more latency.
- **OCTAVER** adds octaves and follows chords. **OCT DN**, **OCT UP** and **DRY** set the levels of octave down, octave up and your own note; **LEVEL** sets the output. **VINTAGE** is gritty and darker, **MODERN** stays clean.

![VoLum Pitch pedal in Octaver mode](user-guide-pitch-octaver.png)

### Compressor

**COMP** evens out your picking. **INPUT** sets how hard it compresses, **ATTACK** and **RELEASE** how fast it reacts and lets go, **OUTPUT** the level.

### NAM Pedals

![VoLum PRE pedal capture menu](user-guide-pre-pedal.png)

**NAM 1** and **NAM 2** each hold a captured drive, boost or fuzz pedal. Click an empty card to pick a capture; on a filled card, click once to select it and again to change it. Your own pedals appear under **CUSTOM**.

Good starting points: Nuke, Bender, Myth or Mash for clean amps, Revival Drive at the edge of breakup, Klon, TS, TS+ or Fatbee for mid and high gain.

The knobs are **GAIN**, **BASS**, **MID**, **MID Hz**, **TREBLE** and **LEVEL**.

## POST Effects

![VoLum POST section](user-guide-post.png)

POST effects sit after the amp: **CHORUS**, **DELAY**, **REVERB**, **TREM**. Click **POST** (or press `3`), click an effect card, switch it on with its LED (or `Space`; `B` in a plug-in), then pick a mode and set the knobs. Each mode remembers its own knob settings.

### Chorus

![VoLum Chorus card](user-guide-chorus.png)

Chorus runs right after the amp and cab, before Delay, Reverb and Tremolo.

- **CLASSIC:** a Juno-60 style stereo sweep.
- **WARPED:** tape wow and flutter; at **MIX** 100% it becomes a vibrato.
- **CLEAR:** a wide Dimension-style chorus that stays in tune in mono.
- **ENSEMBLE** (default): an 80s rack chorus spread across left, centre and right.

The knobs are **RATE**, **DEPTH**, **TONE**, **WIDTH** (0% is mono) and **MIX**.

### Delay

Pick **DIGITAL**, **ANALOG** or **REVERSE**. The knobs are **TIME**, **FEEDBACK**, **MIX**, **TONE**, plus **GRIT**, **WEAR** or **BLOOM** for the mode's character.

- **PING-PONG** (Digital and Analog) bounces the repeats between left and right. They sound a little quieter than the plain delay.
- **TEMPO SYNC** turns **TIME** into a note **DIVISION**: 1/2, 1/4, 1/4., 1/4T, 1/8, 1/8., 1/8T, 1/16.

### Reverb

Pick **HALL**, **PLATE** or **OKTAVERB**. The knobs are **MIX**, **DECAY**, **TONE** and **PRE-DLY**. **OKTAVERB** adds a pitch-shifted shimmer (**HALO**, **SHIMMER** or **BLOOM**) and an **INTENSITY** knob.

### Tremolo

![VoLum Tremolo card](user-guide-tremolo.png)

Tremolo runs last, so it pulses everything, reverb included.

- **OPTICAL** (default): a choppy throb.
- **BIAS:** a smooth, even pulse.
- **HARMONIC:** lows and highs pulse in turn; **X-OVER** sets where they split.

The knobs are **RATE**, **DEPTH**, **SHAPE** (smooth to square) and **MIX**. **TEMPO SYNC** works like the delay's.

### Tempo

Delay and Tremolo share one tempo. In a DAW they follow the song; in the standalone app they follow the metronome BPM, even with the click off.

## Dual Amp

![VoLum Dual Amp](user-guide-dual-amp.png)

Dual Amp plays two amps at once: **MAIN** and **SUPPORT**.

1. Open the AMP section (`2`).
2. Click the **Dual Amp** button ("Switch to Dual Amp"), or press `Space` (`B` in a plug-in).
3. Click the SUPPORT side (**Choose support amp**) and pick the second amp. **(none)** empties it again.
4. Click MAIN or SUPPORT (or press `Tab`) to choose which amp you edit.

Each side has its own channel, cab or IR, knobs and a small **PAN** knob. When you first switch Dual Amp on, MAIN goes left and SUPPORT right.

SUPPORT has a **Ø** (polarity) button, on by default. If a pair sounds thin or hollow, toggle it.

## Presets

![VoLum preset menu](user-guide-presets.png)

A preset stores the whole rig of one amp. Each amp has its own list.

### Factory Presets

Every bundled amp comes with one or two read-only Factory presets, 21 in all. You can put any of them on PLAY without saving first.

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

Press **Ctrl+S**:

- On a Factory preset or an unnamed rig, the name box suggests **New Preset**, and **Save** creates a new User preset. Factory presets are never overwritten.
- On one of your User presets, the button reads **Update** and overwrites it. Type a different name and it becomes **Save**, which creates a new preset.
- A User preset may share a Factory preset's name; it is listed under **USER**. If a name is taken twice, VoLum adds a number, for example "SLO Lead 2".

`Enter` saves; `Esc` or **Cancel** closes the box without saving.

### The Preset Menu

Click the preset name in the header:

- **Default (factory settings)** resets the amp.
- **FACTORY** and **USER** list the presets.
- **Overwrite "name"** saves into the User preset you are on, after asking.
- **Save current as new...** opens the name box.
- **Manage presets...** lists your User presets with buttons to overwrite, rename and delete.

**(unsaved)** after the name means you changed something since loading the preset.

If you delete a preset that is on PLAY, its switches show **Invalid slot** until you give them another Sound.

## PLAY

PLAY is your stage view: your Sounds, eight stomp switches and the amp art. Switch with `P` or the PLAY/BUILD switch; switching never changes what you hear.

A **Sound** is an amp with one of its presets. Each Sound sits on a program number from 0 to 127, the numbers a MIDI foot controller sends.

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

Many foot controllers count from 1, so on those they are presets 1 to 5. This happens only once; a board you have changed or cleared is never filled again.

### Play Your Sounds

![VoLum PLAY board](user-guide-play.png)

- **Recall a Sound:** click its row, use the arrow keys, or send MIDI.
- **LIVE** marks the Sound you recalled last.
- **Stomps:** turn Pitch, Comp, NAM 1, NAM 2, Chorus, Delay, Reverb and Tremolo on and off. Click them or press `1` to `8`. Right-click a stomp to edit that pedal in BUILD.
- **Tweak and save:** change a Sound and press **Ctrl+S**. If the LIVE switch still holds the Sound you started from, your saved version takes its place, so a changed Factory Sound becomes your own copy.

### Add, Replace And Arrange Sounds

![VoLum PLAY Add Sound picker](user-guide-play-picker.png)

- **+ Add this sound** puts what you hear on the next free program number (when all 128 are taken, it opens the picker instead). If it has no name yet, the name box opens first.
- **+ Add Sound** opens the picker: choose a **PROGRAM** number, then a Factory or User preset.
- **Replace:** double-click a row. **Clear:** click its `×`. **Reorder:** drag a row onto another to swap, or between rows to move it.

### Moving Art

The amp art moves while you play, more the harder you play, and stands still when you stop. To keep it still and save a little CPU, turn off **Animate art in PLAY** in [Settings](#signal-tab).

## MIDI Foot Controllers

A foot controller recalls your PLAY Sounds:

- **Program Change N** recalls the Sound on program N.
- **CC 102 with value N** does the same. Use it if your DAW does not pass Program Change on to plug-ins.

A program number with no Sound is ignored; the current tone keeps playing.

### Connect The Controller

- **Standalone:** pick the controller's MIDI input in **Settings > SIGNAL > Audio & MIDI devices...**.
- **DAW:** route the controller's MIDI to the VoLum track. If your host does not pass Program Change on, send the Recall CC instead.

### The MIDI Tab

![VoLum Settings, MIDI tab](user-guide-settings-midi.png)

**What this VoLum listens to:**

- **All channels** (default) suits one guitarist with one board.
- **One channel** (`CH 1` to `CH 16`) lets two VoLums share one MIDI cable.
- **Recall CC** sets the CC number that recalls Sounds (`0` to `119`, default `102`). Any of these, CC 0 included, can be the Recall CC.
- **VST3:** the plugin receives MIDI on channel 1 only, so send recall messages on MIDI channel 1 (AU and standalone accept the channel set here).

**What each program number plays** shows your Sounds as 16 banks of 8 switches, the same list as PLAY. Change banks with the arrows, the mouse wheel or `PageUp` / `PageDown`. Click a switch to choose its Sound, drag it to move it, or hover and click `×` to clear it.

VoLum does not switch banks with Bank Select (CC 0/32) messages, and does not use MIDI notes, pitch bend, MIDI Learn or MIDI output.

## Tuner And Metronome

![VoLum tuner](user-guide-tuner.png)

Open the tuner with its toolbar button or `T`. Your guitar is muted while it is open. Close it with `Esc` or a click outside.

![VoLum metronome](user-guide-metronome.png)

Open the metronome with its toolbar button or `M`. Switch it on, set the tempo (30 to 300 BPM) and volume, and choose `1/4`, `2/4`, `3/4`, `4/4` or `6/8`.

## Your Own Amps, IRs And Pedals

VoLum loads your own NAM captures and impulse responses. Imports are copied into VoLum's library, so they keep working if you move the originals. The standalone app and every plug-in share this library.

### Custom Amps

![VoLum custom amp builder](user-guide-custom-amp.png)

1. Click **+** in the **CUSTOM** part of the amp list.
2. Name the amp and click **+ Add .nam files**.
3. Give each file a cab slot and a channel. Files named like `V30-MyAmp-2.nam` fill this in for you: cab first (`AMP`, `DI` or `DIRECT` mean no cab), channel number last.
4. Click **Save amp**.

Use the pen and bin icons in the amp list to edit or delete a custom amp.

### Custom IRs

![VoLum custom IR menu](user-guide-custom-ir.png)

A custom IR replaces the cab, so the current channel needs a No Cab capture. Every bundled amp has one; on a custom amp, use a channel with a file that starts with `AMP`, `DI` or `DIRECT`. On other channels the Custom IR button is greyed out.

1. Click **Custom IR** in the cab row.
2. Click **Manage custom IRs...**, then **+ Import IR (.wav)**.
3. Pick the IR from the **Custom IR** menu.

VoLum checks that an import is a readable WAV before copying it into the library.

To shape an IR, click the **gear** on its row in **Manage custom IRs**: **Level**, **Low cut** and **High cut**.

### Custom Pedals

![VoLum custom pedal menu](user-guide-custom-pedal.png)

In the NAM pedal menu, click **Manage custom pedals...**, then **+ Import pedal (.nam)**. Your pedals appear under **CUSTOM**.

### Deleting Content

If you delete something that is playing, VoLum moves on: a deleted amp falls back to a bundled one, a deleted pedal leaves its slot empty, and a deleted IR returns to a stock cab: cab 1 on a bundled amp, the current channel's stock cab on a custom amp, or one on another channel if the current channel has none (No Cab only if the amp has no stock cab). VoLum asks first.

## Back Up And Share With Packs

![VoLum Pack export](user-guide-pack-export.png)

A Pack is one `.volumpack` file with your custom amps, IRs, pedals and presets. Use it to back up your library, move it to another computer, or share part of it. Open **Settings > SYSTEM** and click **Export Pack...** or **Import Pack...**.

### Export

- **Everything:** your whole library and your PLAY program list. From the standalone app it also carries your machine settings.
- **Sounds:** the presets you tick.
- **A whole amp:** a custom amp with all its presets.

**ALSO INCLUDING** lists what your choice needs, like an IR or a pedal. It always goes along, so nothing arrives broken.

### Import

![VoLum Pack import preview](user-guide-pack-import.png)

The preview lists everything in the Pack. Untick what you do not want. Each row says what will happen: **Add**, **Replace**, **Keep mine**, **Reloads** (playing now) or **Skip**.

VoLum knows an item by its identity, not its name. If the Pack holds the very same item you already have (for example, your own amp from a backup), the merge mode decides which version wins. A different item that only shares a name with one of yours is added beside it, and you keep both.

An **Everything** Pack offers three merge modes:

- **Overwrite:** the Pack's version wins. Your other items stay.
- **Add:** your version wins. Only new items are added.
- **Reset:** the Pack replaces your library; anything not in it is deleted. Only available while every item is ticked.

A **Sounds** or **A whole amp** Pack always merges like Overwrite and never deletes anything.

**PLAY list and machine settings:** in the standalone app, an Everything Pack restores them (last amp, scenes, Lite, calibration, Output mode, MIDI slots) only if you tick **Also restore machine settings**, which starts unticked. A plug-in has no such box: there an Everything Pack always replaces your PLAY list.

Before importing, VoLum keeps your previous library as `volum-content.json.packbak`. A damaged Pack changes nothing.

## Settings

Open Settings with the gear or `H`; close it the same way, with `Esc`, or by clicking outside.

### SIGNAL Tab

![VoLum Settings, SIGNAL tab](user-guide-settings-signal.png)

- **Input calibration:** enter your interface's input level in dBu and switch on **Calibrate input**. This only works with captures that store their capture level; the bundled amps do not, so the card says "This model has no capture level".
- **Output mode:** **Raw**, **Normalized** (default, similar loudness for all amps) or **Calibrated**. Calibrated needs a capture that stores its output level, so it stays off for the bundled amps. The standalone app keeps your choice for the next launch; in a DAW the project stores it.
- **Performance:** **FULL** (default) or **LITE**, which uses less CPU at slightly lower quality. **Animate art in PLAY** switches the moving art on or off.
- **Audio & MIDI devices...** (standalone only) opens the device window.

### MIDI Tab

See [MIDI Foot Controllers](#the-midi-tab).

### SYSTEM Tab

![VoLum Settings, SYSTEM tab](user-guide-settings-system.png)

- **Keyboard shortcuts** and **Model information**.
- **Back up your library:** **Export Pack...** and **Import Pack...** (see [Packs](#back-up-and-share-with-packs)).
- **About:** version, **Read the manual**, and the update check.

### Update Checks And Privacy

VoLum checks for a new release at most once a day. A gold dot on the gear means one is available; Settings links to the release page. VoLum never downloads or installs anything itself, and the check sends no tracking data. Turn it off with **Check for updates automatically**.

### Audio & MIDI Devices (Standalone)

Open the window with **Audio & MIDI devices...** on the SIGNAL tab (or `Ctrl+,` on Windows, **VoLum > Preferences...** on macOS). Choose the driver, the input and output devices, the sample rate, the buffer size and the MIDI input. Use one mono input for the guitar. A smaller buffer means less delay but more CPU load.

## Using VoLum In A DAW

- VoLum runs as a VST3, and as an AU on macOS (Logic Pro and GarageBand need the AU).
- A new plug-in starts with the amp settings from your standalone app. After that, the DAW project keeps its own settings.
- Your custom library and PLAY program list are shared by every VoLum on the computer.
- In a plug-in, `B` switches the selected pedal on and off, so `Space` stays free for play/stop.
- VoLum reports its delay to the DAW, which compensates for it.

## Keyboard Shortcuts

Keys work when no text box is open.

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
| `Tab` / `Shift+Tab` | Next / previous pedal; in Dual Amp, MAIN / SUPPORT |
| `S` / `Shift+S` | Next / previous cab |
| `Enter` | Select a knob |
| `Space` (standalone), `B` (plug-in) | Switch the selected pedal on or off; in AMP, Dual Amp |

**Selected knob**

| Key | Action |
| --- | --- |
| `Up` / `Down` | Turn the knob (`Shift` for small steps) |
| `Left` / `Right` | Select the previous / next knob |
| `Enter` | Type an exact value |
| `Delete` / `Backspace` | Reset to default |
| `Esc` | Deselect the knob |

**PLAY**

| Key | Action |
| --- | --- |
| `Up` / `Down`, `Left` / `Right` | Previous / next Sound |
| `1` to `8` | Toggle the eight stomps |

## Troubleshooting

- **"Output safety active - lower output or wet mix" in the footer:** turn down the amp **OUTPUT**, Delay **MIX** or Reverb **MIX**.
- **No sound with the tuner open:** that is on purpose; close the tuner.
- **Dual Amp sounds thin:** toggle **Ø** on the SUPPORT side.
- **Foot controller ignored in a DAW:** send the Recall CC (102) instead of Program Change, and check the channel on the MIDI tab.
- **Interface unplugged at start:** VoLum opens without audio. Connect the interface and restart VoLum.
- **Windows: no 48 kHz on your interface:** some interfaces (for example the UA-2X2) offer no 48 kHz with DirectSound. Use ASIO instead, with the vendor's driver or ASIO4ALL.
- **Windows: VoLum says another copy is already running:** end that copy in Task Manager and start VoLum again.
- **macOS: an AU from an older VoLum misbehaves after updating:** remove it from the track and insert it again.
- **macOS: the plug-in does not show up after installing from a zip:** see the [README](../README.md#important-security-notice).

## Where VoLum Keeps Your Files

| What | Windows | macOS |
| --- | --- | --- |
| Settings | `%LOCALAPPDATA%\VoLum\volum-settings.json` | `~/Library/Application Support/VoLum/volum-settings.json` |
| Custom library | `%LOCALAPPDATA%\VoLum\content` | `~/Library/Application Support/VoLum/content` |
| Diagnostic log | `%LOCALAPPDATA%\VoLum\volum.log` | `~/Library/Application Support/VoLum/volum.log` |

## Report A Bug Or Request A Feature

Open an [issue on GitHub](https://github.com/guitarlum/VoLum/issues/new/choose). Use **Bug report** for crashes or wrong behaviour and **Feature request** for ideas. Attach `volum.log` (see [Where VoLum Keeps Your Files](#where-volum-keeps-your-files)) if you can.
