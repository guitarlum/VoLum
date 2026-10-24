# 07b pugl editor

Status: issues. The release editor gates pass. The sanitizer build aborts in a pre-existing constructor path before REAPER can open the editor.

## Heads

- Superproject `feature/linux-07b-pugl` code: `4f68b34ec6e9c144cc1818ff344ae329a606635f`.
- iPlug2 `volum/linux-07b`: `db0dc6b8e24b3868b73aff98f4b2e7da4cf947f8` (from `8eb3b8968`, not `volum/linux` and not `volum/linux-07a`).
- CI: [run 40089826109](https://github.com/guitarlum/VoLum/actions/runs/40089826109) on that code head. Windows tests job succeeded. Formatting succeeded. macOS was still running when this report was written.

## What this candidate is

Vendored pugl (X11 + GL only) plus `IGraphicsLinux`. The host `IRunLoop` calls `puglUpdate(world, 0)` from the X connection fd and a 16 ms timer. There is no pugl blocking loop, no render thread, and no `XInitThreads`.

pugl is upstream `6cba2b564a7a767cefbd9259c87c0284b0e2acb8` (2026-10-17). Local changes are in `iPlug2/Dependencies/IGraphics/pugl/VENDORED.md`: the `XInitThreads` call is removed, and GLX 1.2 `glXChooseVisual` is used only when `glXChooseFBConfig` returns nothing.

## Rubric

### 1. Gates

Release build on the box, Xvfb `1920x1080x24`.

Steinberg validator:

```
Result: 47 tests passed, 0 tests failed
```

pluginval `--strictness-level 10 --validate-in-process` reached `SUCCESS`, including the editor tests. After the handler was limited to this editor's `Display`, that log has no `IPlugLinux X error` line.

REAPER 7.82, throwaway `HOME`, `__startup.lua`: two instances (`VST3: VoLum (Lum)`), each editor opened and closed 20 times, then `PASS`. No `IPlugLinux X error` in that log.

`VOLUM_SANITIZE=ON` produced `build-linux-asan/VoLum.vst3`. REAPER's scan did not list it (`FAIL VoLum is not installed; scanned 249`). Loading that `.so` in-process under UBSan aborts first:

```
runtime error: null pointer passed as argument 1, which is declared to never be null
iplug::IPlugProcessor::InitLatencyDelay()
NChanDelay.h:36
```

That runs from the VST3 constructor, before `OpenWindow`. It is not an editor-path ASan hit. The release REAPER smoke is the one that completed.

Screenshots of the real UI: `reports/07b-scale1.png` (900x600) and `reports/07b-scale2.png` (1800x1200), via `setContentScaleFactor`. `import` was already installed. No extra apt packages.

### 2. Threading

- UI thread only. `IPlugLinuxAssertUiThread` records the first caller and Debug builds assert later callers (`IPlugLinuxRunLoop.cpp`, `TouchUiThread`). VST3 `onFDIsSet` / `onTimer` call it (`IPlugVST3_View.h`).
- No `XInitThreads`. World is `puglNewWorld(PUGL_MODULE, 0)` (`IGraphicsLinux.cpp` around the `PUGL_MODULE` comment). The vendored call site is the comment in `pugl/src/x11.c` line 178.
- GL is made current with `puglEnterContext` before realize setup, before `Draw`, and in `DrawResize` (`IGraphicsLinux.cpp`, the `puglEnterContext` calls).
- `removed()` calls `PrepareToClose()` before unregistering the fd and timer, then `CloseWindow()` (`IPlugVST3_View.h` `DetachLinuxRunLoop`). `PrepareToClose` clears the open flag so a late host callback does not touch X or GL.

### 3. Size

Adapter and the Linux integration around it:

| File | Lines |
| --- | --- |
| `IGraphicsLinux.cpp` | 859 |
| `IGraphicsLinux.h` | 79 |
| `IPlugLinuxRunLoop.cpp` | 416 |
| `IPlugLinuxRunLoop.h` | 107 |
| `IPlugVST3_Linux.cpp` | 349 |
| `IPlugVST3_Linux.h` | 43 |

Adapter (`IGraphicsLinux.*`): 938 lines. Run loop plus the VST3 adapter: 915. Vendored pugl directory: 5807 lines across 16 files (`x11.c` is 2445). Upstream pugl is the bulk. The adapter does not contain a second event loop.

### 4. HiDPI

`IPlugView::setContentScaleFactor` stores the host factor and calls `SetScreenScale` once the window exists (`IPlugVST3_View.h`). If the host has not set one, `OpenWindow` reads `Xft.dpi` from `XResourceManagerString` and otherwise `GDK_SCALE` (`IGraphicsLinux.cpp`, `ScaleFromXftOrGdk`). Scale 2 in the screenshot is `setContentScaleFactor(2)` from the throwaway host, and the child is 1800x1200 for a 900x600 editor.

### 5. Input

pugl button, motion, scroll, key, and text events map onto `OnMouseDown` / `OnMouseDrag` / `OnMouseUp` / `OnMouseDblClick` / `OnMouseWheel` / `OnKeyDown` / `OnKeyUp`. Double click is a second press inside 0.4 s and 5 px. Key codes use pugl's Unicode and `PUGL_KEY_*` values, mapped to iPlug `IKeyPress` the way PR #1336 mapped keysyms. Printable keys wait for `PUGL_TEXT` so the shifted character is the one inserted. Cursors go through `puglSetCursor`. Text entry is VoLum's `ITextEntryControl` (drawn by IGraphics). `CreatePlatformTextEntry` logs and does not open a native widget. Dialogs and the clipboard return false and log; ticket 09 owns them. `canResize` stays false because `PLUG_HOST_RESIZE` is 0. `PlatformResize` sizes the child to `WindowWidth * screen scale`.

### 6. Carrying risk

pugl `6cba2b56` is current (commit date 2026-10-17). Two local patches, both written down in `VENDORED.md`. The class is still `IGraphicsLinux`, matching this fork and PR #1336's platform name. The PR's render thread and `XInitThreads` design are not in this tree. The GLX 1.2 fallback exists because ticket 05 found FBConfig missing on some llvmpipe servers; when FBConfig works, pugl's upstream path is unchanged.

## Tests

Linux `bash NeuralAmpModeler/scripts/run-tests-linux.sh` on `4f68b34e`:

```
[doctest] test cases:     1483 |     1483 passed | 0 failed | 1 skipped
[doctest] Status: SUCCESS!
```

Windows `run-tests-win.ps1` on the same Windows-visible sources (the later commits only touch Linux `.cpp` and the Linux CMake file):

```
[doctest] test cases:     1470 |     1470 passed | 0 failed | 1 skipped
[doctest] Status: SUCCESS!
```

Exit code 0. The new file is in `tests/CMakeLists.txt` and `NeuralAmpModeler-Tests.vcxproj` because the parity check requires both. On Windows and macOS the file compiles to nothing.

Red, with `IPlugLinuxReleaseRunLoop` changed to unregister every binding (not committed; the source was restored):

```
test_iplug_linux_runloop.cpp:86: ERROR: CHECK( bFires == 1 ) is NOT correct!
  values: CHECK( 0 == 1 )
```

## Apt packages

None added for the passing gates. `import` and `gdb` were already on the box.
