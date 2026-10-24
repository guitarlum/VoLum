# 07a Xlib+GLX IGraphicsLinux (arena candidate a)

Status: **PASS** on the box and on the laptop; CI: see [CI](#ci). The ticket stays open for the arena judges.

VoLum's real editor opens in REAPER for Linux and in pluginval, at scale 1 and 2, from our own Xlib+GLX platform layer. Every X and GL call runs on the host UI thread from the host's `IRunLoop`. `IPLUG_LINUX_EDITOR_STUB` and the `IGraphicsLinux.cpp` stub body are gone.

## Heads

| | Branch | Code head |
| --- | --- | --- |
| Superproject | `feature/linux-07a-xlib` | `f88fdb768133eb29d55fde39b294be518459525d` (this report is a later, report-only commit) |
| iPlug2 | `volum/linux-07a` (off `8eb3b8968`, fast-forward only) | `fb7b8f2c39d6ef272cc4fd4c6eb52b1f0b04fc39` |

`volum/linux` and `volum/linux-07b` were not touched. No force-push.

## Rubric 1: gates

All on the box (WSL2 Ubuntu 26.04, GCC 15.2, Mesa llvmpipe), under `xvfb-run -a -s "-screen 0 1920x1080x24"`, every host with a throwaway `HOME`/`XDG_*` and a throwaway REAPER config. Gate scripts: `.scratch/linux-support/arena-07/a/run-gates.sh` (steps), `reaper-smoke.sh` + `reaper-smoke.lua`, `redproof.sh`, `ubsan.supp`. Output: `~/volum-linux/out/07a/f88fdb7/`.

`IGRAPHICS_LINUX_TRACE=1` is set for every host run, so the logs count real editor opens and closes. Every gate also fails on any `IGraphicsLinux: X error`, any `off the UI thread`, and any `ERROR: AddressSanitizer:` or `runtime error:` line.

| Gate | Build | Result |
| --- | --- | --- |
| Steinberg validator | Release | `Result: 47 tests passed, 0 tests failed`, exit 0 |
| pluginval 1.0.4 `--strictness-level 10 --validate-in-process` | Release | `SUCCESS`, exit 0, editor opens/closes 5/5, 0 bad lines |
| REAPER 7.82, 2 instances, 20 open/close cycles each | Release | PASS, editor opens/closes 40/40, 0 X errors, 0 off-thread calls |
| REAPER, same script | `VOLUM_SANITIZE=ON` (ASan+UBSan, RelWithDebInfo) | PASS, 40/40, 0 X errors, 0 sanitizer report lines |
| pluginval strictness 10 | `VOLUM_SANITIZE=ON` | `SUCCESS`, exit 0, 5/5, 0 sanitizer report lines |
| REAPER scripted input (xdotool) | Release | PASS, see rubric 5 |
| Screenshots scale 1, scale 2 (`GDK_SCALE`), scale 2 (`Xft.dpi`) | Release | PASS, see rubric 4 |

pluginval summary (`pluginval.log`):

```
Strictness level: 10
pluginval / Scan for plugins ... / Open plugin (cold) / Open plugin (warm) / Plugin info /
Plugin programs / Editor / Open editor whilst processing / Audio processing /
Non-releasing audio processing / Plugin state / Plugin state restoration / Automation /
Editor Automation / Automatable Parameters / Parameters / Background thread state /
Parameter thread safety / auval / vst3 validator / Basic bus / Listing available buses /
Enabling all buses / Disabling non-main busses / Restoring default layout / Fuzz parameters
SUCCESS
IGraphicsLinux trace: 5 x "editor ... open in ...", 5 x "editor ... closed"
```

The REAPER Lua (`reaper-smoke.lua`) inserts VoLum on two tracks. Each cycle opens A, opens B, closes one while the other stays open, checks with `TrackFX_GetOpen` that the other is still open, then closes it. Odd cycles close A first, even cycles close B first. At the end it deletes both tracks, so the instances are destroyed while REAPER's loop still runs. It saves, then quits through File: Quit.

```
reaper cycles scale=1 asan=0 cycles=20 instances=2: PASS
  steps: 22, editor opens/closes: 40/40, X errors: 0, off-thread calls: 0, sanitizer report lines: 0
reaper cycles scale=1 asan=1 cycles=20 instances=2: PASS
  steps: 22, editor opens/closes: 40/40, X errors: 0, off-thread calls: 0, sanitizer report lines: 0
```

How the sanitizer runs are set up:

- The plug-in links `libasan.so.8` and `libubsan.so.1` (`readelf -d`). The hosts are not instrumented, so they run with `LD_PRELOAD=libasan.so` and `detect_leaks=0`.
- **pluginval needs the system `libstdc++` preloaded as well.** pluginval links libstdc++ statically. With only libasan preloaded, libGLX dlopens Mesa, and Mesa's `libLLVM` crashes in its static initialisers (`pthread_once` with a null function) inside `glXChooseFBConfig`, before any VoLum GL call. With both libraries preloaded the run is clean.
- **REAPER's helper child processes inherit `LD_PRELOAD`.** They fail to map ASan's shadow (`ReserveShadowMemoryRange failed`), never load VoLum, and are not counted.
- **UBSan has two suppressions.** Both findings are pre-existing and not Linux-specific; see [Found along the way](#found-along-the-way).
  - `nonnull-attribute:NChanDelay.h`: `memset(nullptr, 0, 0)` from `InitLatencyDelay`.
  - `shift-base:stb_image.h`: NanoVG's vendored JPEG decoder left-shifts negative values while it loads VoLum's `.jpg` art.
  - Without them, pluginval-ASan logged 11 `stb_image.h` "left shift of negative value" lines. That is the proof that the gate catches UBSan output; the 11 lines were in the `e6d31b1` run.

## Rubric 2: threading

- **No `XInitThreads`.**
  - `rg XInitThreads iPlug2/IGraphics iPlug2/IPlug` finds only three comments that say it is not used.
  - No thread is created anywhere in the platform layer or the run loop.
- **Everything is driven from the host loop.**
  - VST3 `attached()` queries `Linux::IRunLoop` from the `IPlugFrame` and pushes it before `OpenWindow`. It refuses a host without one. `IPlugVST3_View.h:139-153`.
  - `OpenWindow` binds the X connection fd and a 16 ms draw/idle timer. `IGraphicsLinux.cpp:162-163`.
  - Those bindings register on the top loop, which is that frame loop. Through `IPlugVST3RunLoop` they become `registerEventHandler` and `registerTimer`. `IPlugVST3_LinuxRunLoop.h:77,98`.
  - `removed()` calls `CloseWindow` first, then pops the frame loop. `IPlugVST3_View.h:161-167`.
  - `CloseWindow` unbinds the timer and the fd before it touches anything else. `IGraphicsLinux.cpp:172-175`.
- **GL is current before every draw.**
  - `OnDisplayTimer` calls `ActivateGLContext` before `IsDirty`, `Draw` and `glXSwapBuffers`, then `DeactivateGLContext`. `IGraphicsLinux.cpp:381-404`.
  - `ActivateGLContext` saves the thread's current context and always calls `glXMakeCurrent(mDisplay, mPlugWnd, mGLContext)`. `DeactivateGLContext` gives back whatever the host or another editor had current. `IGraphicsLinux.cpp:340-363`.
  - The same scope wraps everything else that can touch GL:
    - X event dispatch, because input handlers load bitmaps (`:371`);
    - `DrawResize` (`:333`);
    - NanoVG's own texture loads, through the `ActivateGLContext`/`DeactivateGLContext` overrides;
    - `OnViewDestroyed` in `CloseWindow` (`:181-185`).
  - Two editors in one process never draw into each other's context.
- **Nothing touches X or GL after `removed()`.**
  - `CloseWindow` unbinds both callbacks, destroys the NanoVG state with the context current, and destroys the GLX context, window and colormap. It then `XSync`s while our error handler is still installed and closes this editor's own X connection. `IGraphicsLinux.cpp:266-286`.
  - Every other entry point checks `mPlugWnd`/`mDisplay` first: `PlatformResize`, cursors, `GetMouseLocation`, and `SetScreenScale` from a late `setContentScaleFactor`.
  - `IGEditorDelegate::CloseWindow` then deletes the `IGraphics`.
- **The child window's visual and colormap match the GLX FBConfig.**
  - `glXChooseFBConfig` (RGB8, depth 24, stencil 8, double buffer, preferring the parent's depth), then `glXGetVisualFromFBConfig`, then `glXCreateNewContext` from that FBConfig. `XCreateColormap` and `XCreateWindow` use that same visual and depth. `IGraphicsLinux.cpp:191-264`.
  - Ticket 05's GLX 1.2 `CreateGLContext` is only the fallback. Every run logged `GLX FBConfig, visual 0x1c9, depth 24`.
- **The X error handler is non-fatal and logs.**
  - `OnXError` logs `IGraphicsLinux: X error <text> (request m.n, resource 0x…)` and counts it. `IGraphicsLinux.cpp:72-83`.
  - `XSetErrorHandler` is process-wide, so ours is installed while any editor has a connection open. Errors on the host's own connections are chained to the previous handler. The old handler comes back only if nobody stacked another on top. `:84-100`.
  - `CreateGLWindow` `XSync`s and fails cleanly if creating the window raised an error. `:255-262`.
- **Editor-closed timers.**
  - `initialize()` pushes the component context's `IRunLoop`. If there is none, it uses `Steinberg::Timer`, which the SDK binds to the `IRunLoop` from `IPluginFactory3::setHostContext`. `IPlugVST3.cpp:56-64`.
  - `terminate()` pops it (`:78`). It is an `IPlugLinuxPushedRunLoop` member, so it is also popped before `IPlugAPIBase` destroys its timer. `IPlugVST3.h:177`.
- **The single-loop limitation is gone.** Loops form a stack with reference counts. Popping a loop moves only the bindings it carried to the new top. `IPlugLinuxRunLoop.cpp:134-180`.
- **UI-thread-only binding is checked, not locked.**
  - The first registry call latches the UI thread. Any other thread asserts in Debug, and is counted and logged (`IPlugLinuxRunLoop: <fn> called off the UI thread`) in every build. `IPlugLinuxRunLoop.cpp:62-77`.
  - Every gate above greps for that line: 0 in pluginval, including its "Background thread state" and "Parameter thread safety" tests, and 0 in REAPER.
- **Re-entrancy.** A handler can be unregistered from inside its own callback:
  - the VST3 adapter's `onTimer`/`onFDIsSet` hold an `IPtr` to themselves (`IPlugVST3_LinuxRunLoop.h:172,197`);
  - the factory-timer adapter releases the SDK timer on a later call (`:236,254`);
  - the poll loop looks every ready source up again before it dispatches it (`IPlugLinuxRunLoop.cpp:317-353`).

## Rubric 3: code size

`wc -l`, after clang-format (iPlug2 `.clang-format`, clang-format 21.1.8):

| File | Lines | Role |
| --- | ---: | --- |
| `IGraphics/Platforms/IGraphicsLinux.cpp` | 712 | window, GLX, events, cursors, scale, text-entry/menu fallback, ticket-09 stubs |
| `IGraphics/Platforms/IGraphicsLinux.h` | 126 | no Xlib in the header (XIDs as `unsigned long`), so no macro leak into plug-in code |
| `IGraphics/Platforms/IGraphicsLinuxKeys.cpp` | 197 | keysym to `kVK_*`, keysym to UTF-8 |
| `IGraphics/Platforms/IGraphicsLinuxKeys.h` | 34 | |
| `IPlug/Linux/IPlugLinuxRunLoop.cpp` | 355 | loop stack, bindings, UI-thread check, poll+timerfd loop |
| `IPlug/Linux/IPlugLinuxRunLoop.h` | 160 | |
| `IPlug/VST3/IPlugVST3_LinuxRunLoop.h` | 308 | Steinberg `IRunLoop` adapter, `Steinberg::Timer` adapter |
| **Platform layer total** | **1892** | |
| `IPlugVST3.cpp` / `.h` / `IPlugVST3_View.h` | +59 / −22 | `OS_LINUX` sections and the stub removal |

Ticket 05's leaf files are reused unchanged: `IGraphicsLinuxGL.*` (160) and `IGraphicsLinuxFont.*` (271). For scale, PR #1336's `IGraphicsLinux.cpp` alone is 1835 lines, with GTK dialogs and the render thread.

## Rubric 4: HiDPI

- **Precedence:**
  1. `IPlugView::setContentScaleFactor`. The view stores it and applies it right after `OpenWindow`, also when it arrived before `attached()`. `IPlugVST3_View.h:152-153,172-180`.
  2. `Xft.dpi` from `XResourceManagerString` divided by 96. `IGraphicsLinux.cpp:288-307`.
  3. `GDK_SCALE`. `:308-316`.
  4. Then 1. The result is rounded to 0.25 and clamped to [1, 4].
- **Physical pixels:**
  - `GetPlatformWindowScale()` returns the screen scale, as on Windows, so the host is told physical pixels.
  - The X window is `WindowWidth() * scale` and the FBO is `WindowWidth() * GetScreenScale()`.
  - Mouse coordinates are divided by `GetTotalScale()`.
- **Evidence** (`IGRAPHICS_LINUX_TRACE` lines from the box):

```
GDK_SCALE=2 (REAPER also passes 2):
  editor 0x400004 open in 0x2002d8, 1800x1200 px, screen scale 2.00 from GDK_SCALE
  editor 0x400004 resized to 1800x1200 px, screen scale 2.00      <- host setContentScaleFactor(2)
  FX window geometry 1800x1257
Xft.dpi: 192 via xrdb, no GDK_SCALE:
  editor 0x400004 open in 0x2002da, 1800x1200 px, screen scale 2.00 from Xft.dpi
  editor 0x400004 resized to 900x600 px, screen scale 1.00        <- host setContentScaleFactor(1) wins
default:
  editor 0x400004 open in 0x2002da, 900x600 px, screen scale 1.00 from default
```

- **Screenshots:** `reports/07a-scale1.png` is 900x628, the 900x600 editor plus REAPER's 28 px FX header. `reports/07a-scale2.png` is 1800x1257, with the editor at 1800x1200 and the text and art rendered at 2x.
  - The scale-2 run uses a 3840x2160 Xvfb screen, because 1800x1200 does not fit 1920x1080.
  - Both are `import -window <FX window>` on the box and show VoLum's real UI.

## Rubric 5: input

| Input | Where | Evidence on the box (`reaper-smoke.sh --mode input`, xdotool) |
| --- | --- | --- |
| Left/right press and release, modifiers (Shift/Ctrl/Alt) | `IGraphicsLinux.cpp:419-456` | Clicking "02" selects Modern Rhythm; 213471 pixels change |
| Drag (implicit X grab), deltas, cursor lock with warp | `:457-489` | Corner-resizer drag, below |
| Wheel (buttons 4/5; 6/7 ignored) | `:424-430` | Wheel over the sound list; 21070 pixels change |
| Double click (time and distance, replaces the second down as `WM_LBUTTONDBLCLK` does) | `:434-446` | Double click on the art: no crash, no X error |
| Keys: `KeysymToVK` (PR #1336 table plus keypad, F13-F24, locks, Super, ISO_Left_Tab), UTF-8 text from Latin-1 and Unicode keysyms, auto-repeat release dropped | `IGraphicsLinuxKeys.cpp`, `IGraphicsLinux.cpp:503-531` | Down arrow moves to "03 Crack the Skye"; 200364 pixels change |
| Unhandled keys forwarded to the host's embed window, as `IGraphicsWin` forwards them to the root window | `IGraphicsLinux.cpp:520-529` | Space was sent; REAPER's play state stayed 0 (headless REAPER with its audio device closed). Delivery unproven, no error |
| Keyboard focus taken on click (`XSetInputFocus`) | `:431-432` | The key steps above reached VoLum |
| Cursors: all 14 `ECursor` shapes, cached font cursors; hide/lock with a blank pixmap cursor | `:539-626` | `SIZENWSE` on the corner resizer |
| Mouse enter/leave (`OnSetCursor`, `OnMouseOut` outside grabs) | `:490-497` | |
| Text entry fallback drawn by IGraphics (`ITextEntryControl`); popup fallback (`IPopupMenuControl`) | `:628-645` | VoLum attaches `ITextEntryControl` itself |
| UI-driven resize: corner resizer, then `PlatformResize`, then `XResizeWindow` plus host `resizeView` | `:320-331` | Editor 900x600 to 960x640 to 1020x680; REAPER's FX window follows (1020x708) |
| Host-driven resize | `IPlugVST3_View.h` `canResize`/`onSize` (unchanged) | VoLum sets `PLUG_HOST_RESIZE 0`, so `canResize` is false. This fork's `OnParentWindowResize` is a no-op on every platform |

Input screenshots stay on the box (`out/07a/f88fdb7/reaper-input.*/input-*.png`) and are not committed.

## Rubric 6: carrying risk

- **New dependencies: none.**
  - `readelf -d VoLum.so` NEEDED is the same set as in ticket 06: `libX11`, `libGL`, `libfontconfig`, `libfreetype`, `libatomic`, `libm`, `libc`.
  - Xrm is part of libX11 and GLX is part of libGL. Nothing is vendored. The only new compiled file is `IGraphicsLinuxKeys.cpp`.
- **Names aligned with PR #1336:** `IGraphicsLinux`, `ProcessX11Events()`, `OnDisplayTimer()`, `KeysymToVK`, `mDisplay`, `mPlugWnd`, `mGLContext`, `GetPlatformWindowScale() == GetScreenScale()`, `GetPlatformAPIStr() == "x11"`, the ticket 05 `CreateGLContext` and fonts.
- **Deliberately different from PR #1336:**
  - No `XInitThreads`, no `pthread` timer, no `SetHostDriven`/`GetX11Fd`. The run-loop registry does that job for every host.
  - GLX 1.3 FBConfig first, with the PR's GLX 1.2 path as fallback.
  - No GTK; dialogs and clipboard are ticket 09.
  - X headers kept out of `IGraphicsLinux.h`, instead of undefining macros in it.
- **Interface for tickets 09 and 10:** `IPlugLinuxPollRunLoop` (poll + timerfd) is ready for the app main. `IPlugLinuxPushedRunLoop` is the RAII push/pop the app can use too.

## Doctests

`NeuralAmpModeler/tests/test_iplug_linux_runloop.cpp` contains five cases.

- It compiles to nothing off Linux.
- It is listed in `tests/CMakeLists.txt`. It is also listed in `NeuralAmpModeler-Tests.vcxproj`: the parity check (`check-test-source-parity.ps1` and the copy in `run-tests-linux.sh`) fails without it, and the brief allows that one line.

Box: `bash NeuralAmpModeler/scripts/run-tests-linux.sh` (built first with `-j 10` in the same configuration; the script's own build was then a no-op):

```
Test source parity OK (102 tests / both descriptors agree).
[doctest] test cases:     1484 |     1484 passed | 0 failed | 1 skipped
[doctest] assertions: 30251385 | 30251385 passed | 0 failed |
[doctest] Status: SUCCESS!
```

Red proof (`redproof.sh`, run on `f88fdb7`): each fix is reverted in `IPlugLinuxRunLoop.cpp`, the suite is rebuilt and the case is run.

```
### single process-wide loop (ticket 05)   -- popping a loop detaches every binding
test_iplug_linux_runloop.cpp:122: ERROR: CHECK( idleB == 1 ) is NOT correct!  values: CHECK( 0 == 1 )
### no reference count
test_iplug_linux_runloop.cpp:148: ERROR: CHECK( frame.fds.size() == 1 ) is NOT correct!  values: CHECK( 0 == 1 )
### pending timers never attach
test_iplug_linux_runloop.cpp:179: FATAL ERROR: REQUIRE( loop.timers.size() == 1 ) is NOT correct!  values: REQUIRE( 0 == 1 )
### no UI-thread check
test_iplug_linux_runloop.cpp:196: ERROR: CHECK_FALSE( otherIsUIThread ) is NOT correct!  values: CHECK_FALSE( true )
### poll loop keeps unregistered timers
test_iplug_linux_runloop.cpp:246: ERROR: CHECK( self.calls == 1 ) is NOT correct!  values: CHECK( 18 == 1 )
### restored
[doctest] test cases:  5 |  5 passed | 0 failed | 1480 skipped
```

The first case is the two-instance case from the ticket 05 review. B's idle timer and B's draw timer keep firing after A's editor closes. With ticket 05's single loop, B gets 0 ticks.

## Laptop and CI

- `pwsh -NoProfile -File NeuralAmpModeler\scripts\run-tests-win.ps1`: exit 0. doctest 1470 passed, 0 failed, 1 skipped; 30255183 assertions. The Windows-visible code is identical on the final head: the only later change was in `IGraphicsLinux.cpp`/`.h`, which Windows does not compile.
- CI: see the next section.

### CI

- [Run 40092730330](https://github.com/guitarlum/VoLum/actions/runs/40092730330) on `e6d31b1`: Formatting, Windows (tests + installer + portable zip + plugin validation), macOS (tests + full distribution + plugin validation), NAMCore, Coverage and the Windows installer upgrade succeeded. The macOS standalone e2e and the macOS installer upgrade were still queued for a macOS runner at the time of writing.
- The final code head `f88fdb7` only changes Linux-only files (`IGraphicsLinux.cpp`/`.h`) from `e6d31b1`. It got its own dispatch after that run finished: CI_FINAL.

## Found along the way

- **Ticket 06 ODR bug, fixed here** (`projects/linux/CMakeLists.txt`). The IGraphics/IPlug object library was compiled once without `VST3_API` and linked into the VST3. `IControl` derives from `Steinberg::FObject` only under `VST3_API`, so VoLum's controls and `IGraphics.cpp` disagreed on the vtable.
  - The first real editor open crashed in `IGraphics::AttachControl`, where `OnAttached()` landed on `IControl::OnDropMultiple`. ASan/UBSan pinned it: `member call on null pointer ... IControl.h:147`.
  - The objects are now built per API: `volum_gfx_VST3_API` and `volum_gfx_APP_API`. The standalone had the same latent bug the other way round.
- **Pre-existing UBSan findings, not Linux-specific, suppressed in the sanitizer gates only:**
  - `IPlug/Extras/NChanDelay.h:36` does `memset(nullptr, 0, 0)` when the latency is 0;
  - NanoVG's vendored `stb_image.h` left-shifts negative values in the JPEG decoder.
  - Both are outside ticket 07's write scope.
- **`WDL/wdlendian.h`:** the float-endian check is narrowed to Linux. Non-Linux compilers see the original `#if __FLOAT_WORD_ORDER__ != __BYTE_ORDER__` again.
- **REAPER and Xvfb under automation:**
  - REAPER asks "Save unsaved project?" on File: Quit for an untitled project. The script saves, reopens from that file, saves in place and then quits.
  - Xvfb resets when its only client exits, which drops `RESOURCE_MANAGER`. The Xft.dpi run uses `-noreset`.

## Known gaps (later tickets)

- Dialogs, clipboard, `OpenURL` and message boxes log `IGraphicsLinux: ... not implemented` and return false, empty or `kNoResult` (ticket 09). A file prompt reports cancel; it never echoes the default path back.
- The standalone app still has no window loop. `IPlugLinuxPollRunLoop` is ready, and a parent of `nullptr` creates a top-level window, but `WM_DELETE_WINDOW` and the app main are ticket 10.
- No XEmbed protocol messages, and `ConfigureNotify` from the host is ignored. Neither REAPER nor pluginval needs them.
- Forwarding unhandled keys to the host is implemented but not observed working under headless REAPER (play state 0).
- No changelog or user-doc line: the brief's scope does not include them, and ticket 15 owns the docs.

## Checklist

- **Windows and macOS code paths unchanged outside the stub removal.**
  - `git diff 8eb3b8968 -- IPlug/VST3` shows only `OS_LINUX` sections, the removed stub branches, and `createView` back to its `b98aacb44` body.
  - `IGraphicsLinux*` and `IPlugLinuxRunLoop*` are Linux-only files.
  - `wdlendian.h` is narrowed as above.
  - CI's Windows and macOS jobs are green.
- **clang-format:**
  - new and changed iPlug2 C++ is formatted with iPlug2's `.clang-format`;
  - the test file is formatted with the root `.clang-format`;
  - the existing upstream-style VST3 files were edited by hand in their own style, not reformatted;
  - CI Formatting passed.
- **LF and exec bits:** `git ls-files --eol` shows `i/lf w/lf` for every gate script, and the three `.sh` files are mode 100755. On `f88fdb7`, before they were committed, `run-tests-linux.sh` reported `Shell exec bits OK (35 tracked .sh files)`.
- **No hard-coded paths** outside the scratch scripts. The only one is the REAPER/pluginval location under `$HOME/volum-linux/tools`, which can be overridden with `REAPER=`/`PLUGINVAL=`.
- **Packages:** none installed. Already on the box: `xvfb 2:21.1.22-1ubuntu1.2`, `imagemagick 8:7.1.2.18`, `xdotool 1:3.20160805.1`, `x11-xserver-utils 7.7+11build1` (xrdb), `gdb`, clang-format 21.1.8.
- **Outside the brief's write list:** none. The `.vcxproj` line is the parity-list entry the brief allows.
