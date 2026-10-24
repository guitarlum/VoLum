Status: claimed
Blocked by: 05, 06

# 07 IGraphicsLinux core + VST3 X11 view + IRunLoop (ARENA)

## Scope

May write:
- `iPlug2/IGraphics/Platforms/IGraphicsLinux.*`
- vendored `iPlug2/Dependencies/IGraphics/pugl/**` (candidate b only)
- `iPlug2/IPlug/VST3/**`, inside `OS_LINUX` sections
- `NeuralAmpModeler/projects/linux/**`

## Arena

Two runners, each on its own branch:
- (a) `feature/linux-07a-xlib`: own Xlib+GLX core.
- (b) `feature/linux-07b-pugl`: vendored pugl X11 (ISC) plus an adapter.

Same brief, same gates. Cross-family judges score these criteria:
1. Gates pass: validator, pluginval 10, REAPER editor open/close ×20, two instances.
2. Threading correctness: everything on the host UI thread, no `XInitThreads`,
   GL made current per draw, no work after `removed()`.
3. Code size and readability of the platform layer.
4. HiDPI: `setContentScaleFactor`, then `Xft.dpi`/`GDK_SCALE` as fallbacks.
5. Input completeness: mouse, wheel, keys and text entry, cursors, resize.
6. Risk of carrying it: an unmaintained dependency, and how far it drifts from
   PR #1336 naming.

The winner is the base. Graft the best parts of the loser, then run a final
verify on `feature/linux-07-graphics`.

## Work (both candidates)

- **Window:** a child window inside the host's `X11EmbedWindowID`, with a visual
  and colormap that match the GLX FBConfig.
- **Input:** event pump, mouse/keys/cursor (keysyms ported from #1336), resize, scale.
- **VST3:** `IPlugVST3_View` accepts `kPlatformTypeX11EmbedWindowID`.
  - It registers the X fd and a 16 ms timer with `IRunLoop` from the `IPlugFrame`.
  - Editor-closed timers come from the `IRunLoop` obtained through `setHostContext`.
- **Standalone:** the loop provider is a simple poll/timerfd loop. Ticket 10 wires
  it into the app main.

## Carried from the ticket 05 review (coordinator, 2026-10-24)

- **One loop per process today:** `IPlug/Linux/IPlugLinuxRunLoop.cpp` keeps a
  single process-wide `IPlugLinuxRunLoop*`, and every timer re-binds when it
  changes.
  - With two VoLum editors in one host, closing one must not detach the other
    editor's timers. Use reference counts or a per-editor loop stack, and add a
    test with two instances where one closes.
- **No lock:** the binding table has no lock. Either prove that every
  `Timer::Create`/`Stop` runs on the UI thread in VST3 and APP (assert it in
  Debug), or add a mutex. Never call into the host loop while holding it.
- **`WDL/wdlendian.h`:** its float-endian check now also errors on non-Linux
  compilers that lack `__FLOAT_WORD_ORDER__`. Windows and macOS CI are green,
  but narrow it to Linux if you touch that file.

## Acceptance

- Under `xvfb-run -a` on the box or a cloud VM:
  - pluginval at strictness 10 passes, including the editor tests;
  - REAPER Lua smoke opens and closes the editor 20 times with two instances;
  - no X errors and no ASan reports in a sanitizer build.
- One screenshot of the editor at scale 1 and one at scale 2, in `reports/`.
- Windows `run-tests-win.ps1` exits 0, and the macOS CI job is green.
