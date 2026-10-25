# VoLum 1.4.0 beta — Linux

Draft for the owner to publish. This is the Linux part of the 1.4.0-beta notes, in the shape of the 1.3.x notes. Windows and macOS sound and behaviour stay as in 1.3.1; this draft does not replace those notes.

![VoLum PLAY in Dual Amp: amp art moving with the guitar](https://raw.githubusercontent.com/guitarlum/VoLum/v1.3.1/docs/volum-dual-amp.gif)

**Native Linux, as a beta.** A VST3 with its editor, and a standalone app, for 64-bit PCs.

### New

- **Linux VST3.** The editor draws on X11, on the host's UI thread. A user install puts it in `~/.vst3/VoLum.vst3` (`Contents/x86_64-linux/VoLum.so`).
- **Linux standalone.** ALSA, JACK and PulseAudio in **Audio & MIDI devices...**. JACK appears when `libjack.so.0` loads, PulseAudio when `libpulse-simple.so.0` loads. On a PipeWire system, pick JACK or PulseAudio: VoLum uses that layer and has no PipeWire driver of its own.
- **ALSA MIDI.** The standalone recalls PLAY Sounds from an ALSA sequencer port: Program Change, the Recall CC (102), an empty program, a burst, and One channel.
- **Your library** lives in `$XDG_DATA_HOME/VoLum`, or `~/.local/share/VoLum` when that variable is unset, empty or relative. The same folder on Windows and macOS is unchanged.
- **Open and Save** ask the desktop file portal, then zenity, then kdialog. The update check loads libcurl when it is installed and stays off when it is missing.

### Install

Download `VoLum-<version>-linux-x86_64.tar.gz`. The name has no leading `v`.

```bash
tar -xzf VoLum-<version>-linux-x86_64.tar.gz
cd VoLum-<version>-linux-x86_64
./install.sh
```

The archive contains `VoLum`, `resources/`, `VoLum.vst3/`, `VoLumRigs/`, `install.sh`, a desktop file, an icon, `LICENSE` and `THIRD_PARTY_LICENSES.md`.

- `./install.sh` installs for the current user: `~/.vst3/VoLum.vst3`, `~/.local/bin/VoLum`, `$XDG_DATA_HOME/VoLum` (default `~/.local/share/VoLum`) and a menu entry.
- `./install.sh --system` installs under `/usr/lib/vst3`, `/usr/local/bin`, `/usr/share/VoLum` and `/usr/share/applications`. Run it as root. The script does not elevate.
- `./install.sh --uninstall` removes the program, the VST3, the bundled rigs and the menu entry. The library, settings and log stay; a data folder left empty is removed. Add `--system` to remove the system install.

GTK 3 is required for the standalone only. When it is missing, `install.sh` still installs the VST3 and prints the package name (`libgtk-3-0` on Ubuntu 22.04, `libgtk-3-0t64` on Ubuntu 24.04, `gtk3` on Fedora).

Details are in the user guide, section **Linux** (`docs/user-guide.en.md`).

### Tested

- **Packages.** The same tarball was installed, checked and removed in Ubuntu 22.04, Ubuntu 24.04 and Fedora containers (the Fedora image's packages were Fedora 44). Each ran the Steinberg VST3 validator (47 passed, 0 failed), opened the standalone, quit cleanly, and uninstalled both the user and the `--system` install. An Ubuntu 24.04 container without GTK 3 still installed the VST3 and named `libgtk-3-0t64`.
- **Hosts.** REAPER loaded the VST3, opened the editor, rendered audio that matched the reference, and ran two instances. Carla ran it in single-client JACK mode. pluginval passed at strictness 10, including the editor tests. The Steinberg validator passed 47 tests.
- **Standalone audio.** JACK at 48000 Hz and 256 frames, with a clean minute after warm-up on the CI runner. PulseAudio is a listed driver when its library loads.
- **MIDI.** Program Change, CC 102, an empty program, a burst and One channel passed on Ubuntu 22.04's generic kernel, which has an ALSA sequencer.
- **Editor.** Compared with the Windows GL2 window at display scales 1 and 2 (main view, Settings SIGNAL and SYSTEM, the custom amp builder, Pack export, the tuner).

Built on Ubuntu 22.04 (glibc 2.35).

### Known limitations

- This is a beta.
- X11, or XWayland on a Wayland desktop. Native Wayland is outside this beta. The standalone quits when there is no X display.
- Latency on particular hardware is not certified.
- VST3 and standalone only. AU, CLAP, LV2 and an ARM build are outside this beta.
- Standalone MIDI is the ALSA sequencer. A kernel without it lists no inputs. JACK MIDI does not reach the standalone.
- The device window is a plain system dialog, not the dark Settings skin.
- Open and Save need a desktop file portal, zenity or kdialog.
- The update check stays off when libcurl is not installed.

### Get it

- **Linux:** `VoLum-<version>-linux-x86_64.tar.gz`
- **Windows and macOS:** the 1.3.1 packages. Sound, presets and the library on those systems stay as they are.

**Full Changelog**: https://github.com/guitarlum/VoLum/compare/v1.3.1...v1.4.0
