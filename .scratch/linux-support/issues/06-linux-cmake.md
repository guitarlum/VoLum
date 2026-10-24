Status: resolved
Blocked by: 05

# 06 Linux CMake for the VST3 and the app

## Scope

May write:
- new `NeuralAmpModeler/projects/linux/**` (CMake)
- `NeuralAmpModeler/scripts/build-linux.sh`
- `NeuralAmpModeler/config.h`, only with `OS_LINUX` guards

## Work

- **Targets:**
  - `VoLum-vst3`: `VoLum.vst3/Contents/x86_64-linux/VoLum.so` plus
    `Contents/Resources` with fonts, images and `VoLumRigs`.
  - `VoLum-app`: the standalone.
- **Sources:** reuse the source lists of the tests CMake and the `.vcxproj`
  files: NAMCore, AudioDSPTools, the product `.cpp` files, the iPlug2 VST3 and
  APP sources.
- **Flags and linking:**
  - `IGRAPHICS_NANOVG;IGRAPHICS_GL2;NAM_ENABLE_A2_FAST`.
  - Static libstdc++ and libgcc; RtAudio defines `__LINUX_ALSA__ __UNIX_JACK__ __LINUX_PULSE__`.
  - JACK and Pulse loaded at runtime (weak symbols or `dlopen` shim).
- **Resources:** embedded the way the Windows `.rc` embeds them, or loaded from
  `Contents/Resources`. Pick one and document it in the CMake.
- It builds with the ticket 05 stubs before 07 exists. The editor may be absent
  (`IGraphicsLinux` stub returns no window).

## Acceptance

- `build-linux.sh Release` produces both targets on the box and on `ubuntu-22.04` CI.
- Steinberg `validator VoLum.vst3` passes with no editor, covering processing and
  state. `readelf -d VoLum.so` shows no `libjack`, `libpulse` or `libstdc++` NEEDED.
- No change to `.sln`, `.vcxproj` or the Xcode projects.
