# 05 iPlug2 Linux leaf

Status: pass. Box compile check, Windows tests, and CI (macOS and Windows jobs) are green.

## Heads

- Superproject `feature/linux-05-iplug-leaf` code that CI built: `532ed7aa865b48c3fcf97df00a68776b0da1c14d`. This report is the commit on top of that.
- iPlug2 `volum/linux`: `068ac6a340ee833c19a9521739e3cabac6c4a0e9` (fast-forward from `b98aacb44`, no rebase, no force-push).
- CI: [run 40061394173](https://github.com/guitarlum/VoLum/actions/runs/40061394173) on `532ed7aa865b48c3fcf97df00a68776b0da1c14d`.

## What was ported from iPlug2#1336

Fetched as `pr1336` (`03d3c1af`). Not merged and not rebased onto upstream.

Paths (`IPlug/IPlugPaths.cpp`, `OS_LINUX` only), copied from the PR:

- `AppSupportPath` — `XDG_CONFIG_HOME`, else `~/.config`; system path `/etc`.
- `DesktopPath` — `~/Desktop`.
- `VST3PresetsPath` — `/usr/share/vst3/presets/...` or `~/.vst3/presets/...`.
- `LocateResource` — absolute path, then `dladdr` of this module (`resources/<type>/`, `../Resources/`, flat `resources/`), then `/proc/self/exe`.

The PR does not define the other symbols the header already declares. This leaf adds them on the same XDG root and `dladdr` directory so later tickets can link: `HostPath` (`/proc/self/exe`), `PluginPath` (`dladdr`), `BundleResourcePath` (`../Resources/` for VST3), `UserHomePath`, `INIPath`, `WebViewCachePath` (`XDG_CACHE_HOME` or `~/.cache`). VoLum's own `$XDG_DATA_HOME/VoLum` layout stays in VoLum, not in these iPlug helpers.

NanoVG stays GL2, same as Windows:

- `IGraphicsNanoVG.cpp`: the `OS_LINUX` `#error` is now `NANOVG_GL2_IMPLEMENTATION` / `NANOVG_GL3_IMPLEMENTATION`. A `__linux__` include of `<glad/glad.h>` sits above `IGraphicsNanoVG.h` so `GLuint` exists. Windows and macOS branches are unchanged.
- `IGraphics_select.h` and `IGraphics_include_in_plug_hdr.h`: `#elif defined OS_LINUX` includes `<glad/glad.h>`. The old `#else` (`<OpenGL/gl.h>`) is still there.

GLX loader (`IGraphics/Platforms/IGraphicsLinuxGL.h` / `.cpp`), the GLX 1.2 sequence from `IGraphicsLinux::CreateGLContext` in the PR:

- Attribute list `GLX_RGBA`, double buffer, 8/8/8, depth 24, then `glXChooseVisual` + `glXCreateContext`.
- `DestroyGLContext`, `ActivateGLContext`, `DeactivateGLContext`.
- `LoadGL()` is `gladLoadGL()` after the caller has made the context current.
- Include order is fixed: X11, glad, `GL/glx.h`, then the header that undefs X11 `Bool`. No `XInitThreads`. No render thread.

Fontconfig (`IGraphics/Platforms/IGraphicsLinuxFont.h` / `.cpp`), the PR's lookup, not its window class:

- `LinuxFileFont` and `LinuxMemFont` read TTF bytes. NanoVG/FreeType (or stb) consumes those bytes later; this leaf does not call FreeType itself, matching the PR.
- `LoadFontconfig()` `dlopen`s `libfontconfig.so.1` and resolves `FcPatternCreate`, `FcPatternAddString`, `FcPatternAddInteger`, `FcConfigSubstitute`, `FcDefaultSubstitute`, `FcFontMatch`, `FcPatternDestroy`, `FcPatternGetString`.
- `LoadPlatformFontFromFile` / `FromName` / `FromMemory` are the bodies of the three `IGraphicsLinux::LoadPlatformFont` overloads. Named lookup is the fallback when the face is not an embedded TTF.
- `IGraphicsPrivate.h`: `OS_LINUX` sets `FONT_DESCRIPTOR_TYPE` to `WDL_String*`, as in the PR.

Plug header (`IPlug_include_in_plug_hdr.h`, `OS_LINUX`): `EXPORT`, `BUNDLE_ID ""`, `APP_GROUP_ID ""`.

`IPlugPlatform.h`: the existing `__linux__` branch now wraps `#define OS_LINUX` in `#ifndef`, as in the PR. Windows and macOS branches are unchanged.

`IGraphicsLinux.h` is the Linux platform header (it is only included from the `OS_LINUX` branch). Syntax fix: `virtual ~IGraphicsLinux();` and the missing class semicolon. Override signatures now match this fork's `IGraphics` (`HideMouseCursor(bool, bool)`, `PromptForFile` completion handler, `PromptForColor` handler, `CreatePlatformPopupMenu(IPopupMenu&, const IRECT, bool&)`). `ShowMouseCursor()` was not a base method, so it is gone. The class stays a declaration; the window, events, and draw loop are ticket 07. `IGraphicsLinux.cpp` is still the stub.

Not ported, on purpose: the PR's render thread, `XInitThreads`, pthread `Timer`, GTK dialogs, keysym table, and the rest of `IGraphicsLinux.cpp`.

## Run loop

`IPlug/Linux/IPlugLinuxRunLoop.h`. All X11/GL work stays on the host UI thread. The header does not start a thread and does not call `XInitThreads`.

`IPlugLinuxRunLoop` is the small interface ticket 07 implements twice (VST3 `IRunLoop` adapter, and the standalone app loop):

- `RegisterFd` / `UnregisterFd` — readable fd, for the X connection.
- `RegisterTimer` / `UnregisterTimer` — periodic callback. `UnregisterTimer` must be safe from inside that callback.

`IPlugLinuxSetRunLoop` installs the process-wide loop. `Timer::Create` on `OS_LINUX` binds through `IPlugLinuxBindTimer`. If no loop is installed yet, the timer is remembered and attached when one is set. `Timer_impl` has no thread and no `pthread`.

## Compile check

`NeuralAmpModeler/scripts/linux/compile-check/` (`run.sh` is LF, mode `100755`). Object library, `-Wall -Wextra -Werror`, GCC and Clang, `-j 14`.

Vendored headers are `-isystem`. `IPlugPaths.cpp` still sees sibling headers, so that one file suppresses `-Wno-unused-function` and `-Wno-unused-variable` (the warnings are `MidiNoteName` and the string tables, not the new Linux paths). `IGraphicsNanoVG.cpp` suppresses unused-parameter, GCC sequence-point / maybe-uninitialized, and Clang's unused private field. Those are the pre-existing drawing TU. New files (`IPlugLinuxRunLoop`, `IGraphicsLinuxFont`, `IGraphicsLinuxGL`) are compiled with the warnings left on.

Box (`lum@192.168.178.91`), sha `532ed7aa865b48c3fcf97df00a68776b0da1c14d`:

```
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
[100%] Built target volum_linux_leaf
Ubuntu clang version 21.1.8 (6ubuntu1)
[100%] Built target volum_linux_leaf
compile-check: gcc and clang ok
```

Exit code 0. No apt packages were required to compile. `clang-format` and `clang-format-21` were installed (`sudo apt-get install -y`) only to format the new C++ files with iPlug2's `.clang-format` (Clang 21.1.8).

## Diff against `b98aacb44`

`git -C iPlug2 diff b98aacb44 --stat`: 17 files, +916 / -17. Pre-existing files:

| File | Where the diff sits |
| --- | --- |
| `IPlug/IPlugPaths.cpp` | New `#elif OS_LINUX` include and the Linux function block. Windows/web bodies untouched. |
| `IPlug/IPlugTimer.h`, `IPlugTimer.cpp` | New `#elif OS_LINUX` timer. Mac/Win/web bodies untouched. |
| `IPlug/IPlugPlatform.h` | Inside the existing `__linux__` / `__linux` branch (`#ifndef OS_LINUX`). |
| `IPlug/IPlug_include_in_plug_hdr.h` | Replaces the `OS_LINUX` `//TODO:`. |
| `IGraphics/IGraphics_select.h`, `IGraphics_include_in_plug_hdr.h` | Added `#elif defined OS_LINUX` before the unchanged `#else`. |
| `IGraphics/Drawing/IGraphicsNanoVG.cpp` | `__linux__` glad include, and the `OS_LINUX` branch that was `#error NOT IMPLEMENTED`. |
| `IGraphics/IGraphicsPrivate.h` | Added `#elif defined OS_LINUX` for `FONT_DESCRIPTOR_TYPE`. |
| `IGraphics/Platforms/IGraphicsLinux.h` | The file is the Linux class. Destructor, semicolon, and `override` signatures so it parses against this fork. |
| `WDL/wdlendian.h` | Float-endian check no longer fires when `__FLOAT_WORD_ORDER__` is absent on `__linux__` (Clang). The existing `__linux__` bswap block uses `<byteswap.h>` / `bswap_16`. GCC 15 still takes the builtin path. Windows (`_WIN32`) and macOS (`__APPLE__`) branches are untouched. |

New files: `IPlug/Linux/IPlugLinuxRunLoop.h`, `.cpp`, `IGraphicsLinuxFont.h`, `.cpp`, `IGraphicsLinuxGL.h`, `.cpp`.

No render thread. No `XInitThreads` call (`IPlugLinuxRunLoop.h` only names it to say the interface does not use it).

## Results

- Box compile check: **pass** (exit 0, GCC 15.2.0 and Clang 21.1.8) on `532ed7aa865b48c3fcf97df00a68776b0da1c14d`.
- Windows `run-tests-win.ps1`: **exit 0**. doctest 1470 passed, 0 failed, 1 skipped; 30254726 assertions passed. The worktree's ignored `VST3_SDK` tree was empty (fresh submodule clone); the headers were copied from the main checkout's local SDK, which is gitignored. That copy is not part of the commit.
- CI run [40061394173](https://github.com/guitarlum/VoLum/actions/runs/40061394173): **success** on `532ed7aa865b48c3fcf97df00a68776b0da1c14d`. `Windows (tests + installer + portable zip + plugin validation)` completed success. `macOS (tests + full distribution + plugin validation)` completed success. Formatting, NAMCore, both installer upgrades, and macOS standalone e2e also succeeded.
