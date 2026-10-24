# 06 Linux CMake for the VST3 and the app

Status: pass on the box (GCC 15.2) and on Ubuntu 22.04 (GCC 12.3, glibc 2.35). CI is the run below; Windows tests exited 0.

## Heads

- Superproject code under this report: `feature/linux-06-cmake` `35239678615a680af52ab0f2fe0299496ed6a78c`.
- iPlug2 `volum/linux`: `863f2850686392a12b8195501ac2f4c535d7a86b`.
- CI: [run 40075268411](https://github.com/guitarlum/VoLum/actions/runs/40075268411) dispatched on `35239678615a680af52ab0f2fe0299496ed6a78c`. An earlier run failed in `parse_config.py` because the `PLUG_HAS_UI` lines were indented; that script slices from column 0. The defines stay in column 0 (`0` on Linux, `1` otherwise, last value `1`) inside a `clang-format off` block so the formatter does not indent them.

## Targets

`NeuralAmpModeler/projects/linux/CMakeLists.txt`, driven by `NeuralAmpModeler/scripts/build-linux.sh Release`.

- `VoLum-vst3`: `build-linux/VoLum.vst3/Contents/x86_64-linux/VoLum.so` plus `Contents/Resources`.
- `VoLum-app`: `build-linux/standalone/VoLum`.
- `validator`: optional (`VOLUM_BUILD_VALIDATOR`, default ON). Compiled from the vendored SDK's `public.sdk/samples/vst-hosting/validator` and the hosting/base sources its CMakeLists names. The SDK's own CMake requires 3.25, so this file lists the sources instead of `add_subdirectory` of the SDK. That is what lets Ubuntu 22.04's CMake 3.22 configure it.

Module entry is the SDK's `public.sdk/source/main/linuxmain.cpp` (`ModuleEntry` / `ModuleExit`). `InitModule` / `DeinitModule` stay the ones iPlug already provides. `dllmain.cpp` is not compiled.

`BUILD_DIR` overrides the build directory (default `build-linux`) so a second compiler can keep its own tree.

## Flags and linking

Defines: `IGRAPHICS_NANOVG`, `IGRAPHICS_GL2`, `NAM_ENABLE_A2_FAST`, `GRAYED_ALPHA=0.5f`, `IPLUG_EDITOR=1`, `IPLUG_DSP=1`, `NOMINMAX`, plus `VST3_API` or `APP_API`.

RtAudio is compiled with `__LINUX_ALSA__`, `__UNIX_JACK__`, `__LINUX_PULSE__`. RtMidi is compiled with `__LINUX_ALSA__` only.

Link line: `-static-libstdc++ -static-libgcc`. ALSA (`-lasound`, app only), X11, GL, fontconfig and freetype are linked with `--no-as-needed` so they are `DT_NEEDED` even where the leaf only dlopens fontconfig. libatomic is linked because GCC 15 emits `__atomic_*` for the 16-byte atomics in the tuner.

GCC older than 13 also gets `-fpermissive`. Ubuntu 22.04's GCC 12 rejects a using-declaration in AudioDSPTools that changes the meaning of `LanczosResampler`. GCC 13 made that a warning. The box GCC 15 does not get the flag.

## Resource layout

Fonts and images are files on disk, not a Windows `.rc`. Ticket 05's `LocateResource` looks fonts up as type `ttf` and images by extension.

- VST3: `Contents/Resources/ttf|png|jpg|svg/` and `Contents/Resources/VoLumRigs/`. The `.so` is in `Contents/x86_64-linux/`, so `../Resources/<type>/<file>` is the hit.
- App: `standalone/resources/ttf|png|jpg|svg/` and `standalone/VoLumRigs/` next to the binary.

`JosefinSans-Bold.ttf` is copied from `resources/fonts-win` because that is the file `JOSEFINSANS_BOLD_HEAVY_FN` names. `VoLumPaths.h` gained a `__linux__` rig search (next to the module, then `../Resources/VoLumRigs`, then `$XDG_DATA_HOME/VoLum/VoLumRigs`, then `/usr/share/VoLum/VoLumRigs`) and the same XDG data dir for settings, content and the log.

`config.h` sets `PLUG_HAS_UI` to 0 on `__linux__`, so `createView` returns null and the validator covers processing and state with no editor. `IGraphicsLinux.cpp` is still the link stub: `OpenWindow` returns null. Ticket 07 replaces that file.

## JACK and Pulse

`NeuralAmpModeler/projects/linux/VoLumJackPulseDlopen.cpp` defines the JACK and Pulse symbols RtAudio calls and `dlopen`s `libjack.so.0` and `libpulse-simple.so.0` (then `libpulse.so.0` for `pa_strerror`). A missing library makes that API fail at the call. They are not on the link line.

The standalone preferences combo lists ALSA, JACK, PulseAudio in that order (`kDeviceAlsa` 0, `kDeviceJack` 1, `kDevicePulse` 2). The message loop is headless SWELL (no GDK). `swell_oswindow_maximize` is a no-op in `VoLumSwellHeadless.cpp` because the real function lives in the GDK file ticket 07 owns.

## Validator (box, GCC 15.2)

`build-linux.sh Release` on `117866ff` (same C++ as the final head except the later clang-format of the stub and the GCC-12-only `-fpermissive`, which GCC 15 does not use):

```
build_exit:0
help_exit:0
bare_exit:0
Result: 47 tests passed, 0 tests failed
validator_exit:0
```

`--help` prints `VoLum standalone (Linux).` and returns 0. With no arguments the process constructs the plug-in and returns 0. The first bare run aborted in `CurrentTime()`: `snprintf` was told the destination was 32 bytes while pointing at the tail of that array. GCC fortify killed it, and the validator died in the same constructor. The length is now the bytes that remain.

`readelf -d` of `VoLum.vst3/Contents/x86_64-linux/VoLum.so`:

```
 0x0000000000000001 (NEEDED)             Shared library: [libX11.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libGL.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libfontconfig.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libfreetype.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libatomic.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libm.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x0000000000000001 (NEEDED)             Shared library: [ld-linux-x86-64.so.2]
```

No `libjack`, `libpulse`, or `libstdc++`. The app adds `libasound.so.2` and nothing else from that set.

## Ubuntu 22.04

`docker run --rm --name volum-linux06-u2204-gcc12d` on a fresh `ubuntu:22.04`. No other container was stopped. The stock `g++` is GCC 11, which cannot compile `std::atomic<std::shared_ptr<...>>` in NAMCore. The image installs `g++-12` (12.3.0) and the build sets `CC`/`CXX` to it. The binary is still glibc 2.35.

GCC 12 also rejects `std::fmodf` and `std::expf` (they are not in namespace `std` in that libstdc++). The Linux branch calls `::fmodf` and `::expf`.

`build-linux-2204-gcc12` produced `VoLum-vst3`, `VoLum-app` and `validator`. On the box, that validator against that bundle:

```
Result: 47 tests passed, 0 tests failed
validator_exit:0
help_exit:0
```

`readelf -d` of the 22.04 `VoLum.so` is the same NEEDED list as the box build (no jack, pulse, or libstdc++).

## Windows tests

`run-tests-win.ps1` exited 0 on `117866ff` (every Windows-compiled change, including the `CurrentTime` fix and the `__linux__` math branch). doctest: 1470 passed, 0 failed, 1 skipped; 30254727 assertions. The two commits after that only add the GCC 12 `-fpermissive` flag and clang-format of `IGraphicsLinux.cpp`, which the Windows projects do not compile.

## Apt packages

On the box, `sudo apt-get install -y gdb` (to backtrace the fortify abort). The image already had the X11, GL, ALSA, JACK, Pulse, fontconfig and freetype headers.

Inside the throwaway Ubuntu 22.04 container: `build-essential`, `g++-12`, `cmake`, `git`, `pkg-config`, `libasound2-dev`, `libx11-dev`, `libgl1-mesa-dev`, `libfontconfig1-dev`, `libfreetype6-dev`, `libjack-jackd2-dev`, `libpulse-dev`.

## CI

[Run 40075268411](https://github.com/guitarlum/VoLum/actions/runs/40075268411) on `35239678615a680af52ab0f2fe0299496ed6a78c`. Watching for Windows and macOS green.
