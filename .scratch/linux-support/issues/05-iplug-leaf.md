Status: resolved
Blocked by: 01

# 05 iPlug2 Linux leaf pieces (compile-level)

## Scope

May write:
- `iPlug2/**` on branch `volum/linux`, as new files or `OS_LINUX` sections only
- `NeuralAmpModeler/iplug2-patches/**` if the patch scripts need to know about new files

## Work

- Fix the `IGraphicsLinux.h` syntax.
- `IPlugPaths.cpp` Linux, ported from PR #1336.
- `IPlugPlatform` and `IPlug_include_in_plug_hdr` Linux sections.
- NanoVG GL2 on Linux: drop the `#error`, add the glad GLX loader.
- A Linux `Timer_impl` behind an interface that ticket 07 binds to the host
  `IRunLoop` or the app loop.
- Fontconfig/FreeType font lookup as a fallback to the embedded TTFs.

## Acceptance

- A Linux compile-only translation-unit check builds every new file with
  `-Wall -Werror` on GCC and Clang. Use a small CMake target in
  `NeuralAmpModeler/scripts/linux/` or a doctest that includes the headers.
- Windows: `run-tests-win.ps1` exits 0. macOS: the CI macOS job is green.
- `git -C iPlug2 diff b98aacb44 --stat` shows Windows and macOS files changed
  only inside `OS_LINUX` blocks.
