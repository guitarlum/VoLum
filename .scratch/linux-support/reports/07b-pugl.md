# 07b pugl editor

Status: issues. Release validator and pluginval pass on the merge. The sanitizer REAPER smoke now opens the editor and stops in shared NanoVG `stb_image`, outside the adapter and pugl.

## Heads

- Superproject `feature/linux-07b-pugl`: `ea161dc28b62988921a1ddedc131110976b5cbeb`.
- iPlug2 `volum/linux-07b`: `9362b06b212f4e2102af9d83aa78ee45c728fc44` (`--no-ff` merge of `volum/linux` `56a660461`). Not `volum/linux-07a`.
- CI on that head: [run 40094896390](https://github.com/guitarlum/VoLum/actions/runs/40094896390).
- Earlier [run 40089826109](https://github.com/guitarlum/VoLum/actions/runs/40089826109) on `4f68b34e`: Windows succeeded. macOS was cancelled (concurrency `cancel-in-progress` when the next dispatch on this ref started). That run's Windows job had already finished green.

## What this candidate is

Vendored pugl (X11 + GL only) plus `IGraphicsLinux`. The host `IRunLoop` calls `puglUpdate(world, 0)` from the X connection fd and a 16 ms timer. There is no pugl blocking loop, no render thread, and no `XInitThreads`.

pugl is upstream `6cba2b564a7a767cefbd9259c87c0284b0e2acb8` (2026-10-17). Local changes are in `iPlug2/Dependencies/IGraphics/pugl/VENDORED.md`: the `XInitThreads` call is removed, and GLX 1.2 `glXChooseVisual` is used only when `glXChooseFBConfig` returns nothing.

## Rubric

### 1. Gates

Release build on the box at `ea161dc2`, Xvfb `1920x1080x24`.

| Gate | Result |
| --- | --- |
| Steinberg validator | `Result: 47 tests passed, 0 tests failed` (`validator_exit:0`) |
| pluginval 10 in-process | `SUCCESS` (`pluginval_exit:0`). No `IPlugLinux X error`. |
| REAPER release, 2×20 | `PASS` on the pre-merge editor. No `IPlugLinux X error`. |
| REAPER `VOLUM_SANITIZE`, 2×20 | Stopped. UBSan in shared `stb_image.h:1578`. No `IPlugLinux X error`. No AddressSanitizer report. |
| Windows CI `40089826109` | success |
| macOS CI `40089826109` | cancelled (concurrency) |
| Windows + macOS CI `40094896390` | success on `ea161dc2` (Windows, macOS, both installer upgrades, macOS standalone e2e) |

`VOLUM_SANITIZE=ON` rebuilt `build-linux-asan/VoLum.vst3` after the `NChanDelay` merge. REAPER found the plug-in and opened an editor. UBSan then aborted. The hit is `(-1 << magbits)` in vendored NanoVG, reached from layout bitmap load. It is not in `IGraphicsLinux` and not in pugl. Not patched here.

```
runtime error: left shift of negative value -1
#0 stbi__process_marker.cold  iPlug2/Dependencies/IGraphics/NanoVG/src/stb_image.h:1578
#1 stbi__jpeg_load
#2 stbi__load_flip
#3 stbi_load_from_file
#4 stbi_load
#5 nvgCreateImage
#6 iplug::igraphics::IGraphicsNanoVG::LoadAPIBitmap
#7 iplug::igraphics::IGraphicsNanoVG::LoadBitmap
#8 NeuralAmpModeler::_BuildVoLumLayout
#9 iplug::igraphics::IGraphicsLinux::OpenWindow
#10 iplug::igraphics::IGEditorDelegate::OpenWindow
#11 IPlugVST3View::attached
#12 REAPER
```

Smoke progress before the abort: `STEP fx VST3: VoLum (Lum)` then `STEP inserted 2`. No `PASS` line.

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
