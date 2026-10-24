# 06 Linux CMake for the VST3 and the app

Status: pass, pending the single CI dispatch of this commit. The box (GCC 15.2), Ubuntu 22.04 (g++-12 12.3, glibc 2.35) and Windows tests are green. The review merge is in.

## Heads

- Superproject: `feature/linux-06-cmake` at the commit that contains this report. Code before the report was `d4c1ac0befe54f83cc8897ebef65ee72a4d47d2f`.
- iPlug2 `volum/linux`: `8eb3b8968f92195abf5bf025b4961b64c7767e95`.
- AudioDSPTools `volum/linux`: `f2cfd018fb0dc009fff0e41845a83e96ae6eaab5` (from the merge of `origin/feature/linux-support`).
- `git diff origin/feature/linux-support -- NeuralAmpModeler/config.h NeuralAmpModeler/VoLumPaths.h` is empty.
- Pre-review CI [run 40075268411](https://github.com/guitarlum/VoLum/actions/runs/40075268411) is green on `35239678615a680af52ab0f2fe0299496ed6a78c` and does not contain this review. One new dispatch covers this commit. It is not cancelled.

## Targets

`NeuralAmpModeler/projects/linux/CMakeLists.txt`, driven by `NeuralAmpModeler/scripts/build-linux.sh Release`.

- `VoLum-vst3`: `build-linux/VoLum.vst3/Contents/x86_64-linux/VoLum.so` plus `Contents/Resources`.
- `VoLum-app`: `build-linux/standalone/VoLum`.
- `validator`: optional (`VOLUM_BUILD_VALIDATOR`, default ON). Compiled from the vendored SDK's `public.sdk/samples/vst-hosting/validator` and the hosting/base sources its CMakeLists names. The SDK's own CMake requires 3.25, so this file lists the sources instead of `add_subdirectory` of the SDK. That is what lets Ubuntu 22.04's CMake 3.22 configure it.

Module entry is the SDK's `public.sdk/source/main/linuxmain.cpp` (`ModuleEntry` / `ModuleExit`). `InitModule` / `DeinitModule` stay the ones iPlug already provides. `dllmain.cpp` is not compiled.

`BUILD_DIR` overrides the build directory (default `build-linux`) so a second compiler can keep its own tree.

`build-linux.sh` exports `CC=gcc-12` and `CXX=g++-12` when both exist and neither variable is already set. The box has no `g++-12` and keeps the default GCC 15. An explicit `CC`/`CXX` wins.

## Flags and linking

Defines: `IGRAPHICS_NANOVG`, `IGRAPHICS_GL2`, `NAM_ENABLE_A2_FAST`, `GRAYED_ALPHA=0.5f`, `IPLUG_EDITOR=1`, `IPLUG_DSP=1`, `NOMINMAX`, plus `VST3_API` or `APP_API`.

RtAudio is compiled with `__LINUX_ALSA__`, `__UNIX_JACK__`, `__LINUX_PULSE__`. RtMidi is compiled with `__LINUX_ALSA__` only.

Link line: `-static-libstdc++ -static-libgcc` and `${CMAKE_DL_LIBS}`. ALSA (`-lasound`, app only), X11, GL, fontconfig and freetype are linked with `--no-as-needed` so they are `DT_NEEDED` even where the leaf only dlopens fontconfig. libatomic is linked because GCC 15 emits `__atomic_*` for the 16-byte atomics in the tuner. libcurl is not linked.

There is no `-fpermissive`. AudioDSPTools `f2cfd018` writes the alias as `dsp::LanczosResampler<...>`, which is what GCC 12 needed. The Ubuntu 22.04 rebuild with g++-12 produced no other GCC 12 error.

## Resource layout

Fonts and images are files on disk, not a Windows `.rc`. Ticket 05's `LocateResource` looks fonts up as type `ttf` and images by extension.

- VST3: `Contents/Resources/ttf|png|jpg|svg/` and `Contents/Resources/VoLumRigs/`. The `.so` is in `Contents/x86_64-linux/`, so `../Resources/<type>/<file>` is the hit.
- App: `standalone/resources/ttf|png|jpg|svg/` and `standalone/VoLumRigs/` next to the binary.

`JosefinSans-Bold.ttf` is copied from `resources/fonts-win` because that is the file `JOSEFINSANS_BOLD_HEAVY_FN` names.

Paths are ticket 03's. This branch no longer has `LinuxDataDir`. Rigs use `LinuxRigsCandidates(LinuxModuleFile(), LinuxVoLumDataDir())`: beside the module, then `../Resources/VoLumRigs`, then the XDG data dir, then `/usr/share/VoLum/VoLumRigs`. That is the layout the copy step writes. Nothing in the resource layout needed a second implementation.

`config.h` is the single `#define PLUG_HAS_UI 1`. `parse_config.py` reads `PLUG_HAS_UI` as 1. The temporary no-editor behaviour is iPlug2 `IPLUG_LINUX_EDITOR_STUB` (defined on the Linux VST3 target). `createView` returns null, `isPlatformTypeSupported` returns `kResultFalse`, and `attached` returns `kNotImplemented`. Ticket 07 deletes the macro. `IGraphicsLinux.cpp` is still the link stub.

`VoLumHttpGetLinux.cpp` is in the DSP object list. `VoLumHttpGet.cpp` does not define `VolumHttpGetString` on Linux. The loader is `dlopen` of libcurl, so the `.so` has no libcurl `NEEDED`.

`IPlugLogger.h` `CurrentTime()` now passes the remaining bytes of its 32-byte buffer to `snprintf`. That change is on every platform. The formatted text is the same; the old length told fortify the destination was still 32 bytes while the pointer was already past the start. GCC killed the process in the plug-in constructor.

## JACK and Pulse

`NeuralAmpModeler/projects/linux/VoLumJackPulseDlopen.cpp` defines the JACK and Pulse symbols RtAudio calls and `dlopen`s `libjack.so.0` and `libpulse-simple.so.0` (then `libpulse.so.0` for `pa_strerror`). A missing library makes that API fail at the call. They are not on the link line.

The standalone preferences combo lists ALSA, JACK, PulseAudio in that order (`kDeviceAlsa` 0, `kDeviceJack` 1, `kDevicePulse` 2). The message loop is headless SWELL (no GDK). `swell_oswindow_maximize` is a no-op in `VoLumSwellHeadless.cpp` because the real function lives in the GDK file ticket 07 owns.

## Validator (box, GCC 15.2)

`build-linux.sh Release` on `d4c1ac0b` (GCC 15, no `g++-12` on the box):

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

No `libjack`, `libpulse`, `libstdc++`, or `libcurl`. The app adds `libasound.so.2` and nothing else from that set.

## Ubuntu 22.04

`docker run --rm --name volum-linux06-u2204-review` on a fresh `ubuntu:22.04`. No other container was stopped. The image installs `g++-12` (12.3.0). `CMAKE_CXX_COMPILER` in `build-linux-2204-gcc12/CMakeCache.txt` is `/usr/bin/g++-12`. The script selects that compiler when it is installed. The binary is still glibc 2.35. `-fpermissive` is not in the cache and not on the command line.

GCC 12 still rejects `std::fmodf` and `std::expf` (they are not in namespace `std` in that libstdc++). The Linux branch calls `::fmodf` and `::expf`. No further GCC 12 error appeared.

```
gcc12_build_ok
docker_build_exit:0
```

`readelf -d` of that `VoLum.so` matches the box list above (no jack, pulse, libstdc++, or libcurl).

## Windows tests

`run-tests-win.ps1` exited 0 on the review head. doctest: 1470 passed, 0 failed, 1 skipped; 30255183 assertions.

## Apt packages

On the box, `sudo apt-get install -y gdb` (to backtrace the fortify abort). The image already had the X11, GL, ALSA, JACK, Pulse, fontconfig and freetype headers.

Inside the throwaway Ubuntu 22.04 container: `build-essential`, `g++-12`, `cmake`, `git`, `pkg-config`, `libasound2-dev`, `libx11-dev`, `libgl1-mesa-dev`, `libfontconfig1-dev`, `libfreetype6-dev`, `libjack-jackd2-dev`, `libpulse-dev`.

## CI

The pre-review head `35239678` is [run 40075268411](https://github.com/guitarlum/VoLum/actions/runs/40075268411) (Windows and macOS success). This review changes product and iPlug sources, so that run does not cover it. One `ci-watch.ps1 -Ref feature/linux-06-cmake -Dispatch` runs on the commit that contains this report.
