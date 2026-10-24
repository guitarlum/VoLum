# 07 IGraphicsLinux + VST3 X11 view + IRunLoop (arena synthesis)

Status: **PASS** on the final code head `eed5a7b`: box gates, laptop tests and CI are all green. The ticket is not marked resolved: the coordinator merges `feature/linux-07-graphics` into `feature/linux-support` after the cold verifier.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject | `feature/linux-07-graphics` | code head `eed5a7b727f36b1e43083b9bef6652a2598b6677`; this report is a later, report-only commit |
| iPlug2 | `volum/linux-07` | `72593f1683be9baf2bc287af9d854e829750a152` |
| AudioDSPTools | from `feature/linux-support` | `3f28127e3125adb8befbdb56e26e986aaf9c0e55` |

- **iPlug2 `volum/linux-07`:**
  - `fb7b8f2c3` (07a), then a merge of `origin/volum/linux` `56a66046` (the NChanDelay zero-latency fix), giving merge `626e0e612`.
  - Then one graft commit, `72593f168`.
- **Superproject `feature/linux-07-graphics`:**
  - Created from `origin/feature/linux-07a-xlib`.
  - Merged `origin/feature/linux-support` (`03a5a03c`): ticket 04's `Linux tests` CI job, the archive g++-12 reference identity, and AudioDSPTools `3f28127`. The iPlug2 conflict was resolved to `volum/linux-07`.
  - Then the graft commits `30321f2` and `eed5a7b`.
- No rebase and no force-push. `volum/linux`, `volum/linux-07a` and `volum/linux-07b` were not changed.

## Why A is the base

The cross-family judge (Grok) and the coordinator scored the six ticket criteria: **A 28, B 20; base A.** Candidate A is our own Xlib+GLX core with the run-loop registry. Candidate B is vendored pugl X11 plus an adapter.

Rejected from B, with the reason:

| B item | Why it is not taken |
| --- | --- |
| Vendored pugl | About 5.8k vendored lines to carry for what A does in about 1.9k of our own, with no new dependency. |
| A process-wide `XSetErrorHandler` that is never restored | Installing a handler is process-wide. A's handler chains errors on the host's connections to the previous handler, and restores it when the last editor closes, unless someone stacked another handler on top. |
| No `GetPlatformWindowScale()` override | At HiDPI the host is told logical sizes while the window is physical. A returns `GetScreenScale()`, as `IGraphicsWin` does. |
| `attached()` returns success without a timer | The editor would never draw. A refuses a host frame without `IRunLoop`. |
| B's `WDL/wdlendian.h` | A's version keeps the non-Linux branch byte-for-byte the pre-ticket-05 line that shipped in 1.3.1. |
| B's factory host-context hook in `IPlug_include_in_plug_src.h` | A's component-context loop, with the `Steinberg::Timer` fallback for `setHostContext`, covers the editor-closed case without touching that shared header. No gap was found. |

## Grafts and fixes on top of A

1. **The NChanDelay suppression is gone** (coordinator; the fix is in iPlug2 `volum/linux` `56a66046`).
   - `.scratch/linux-support/arena-07/a/ubsan.supp` now has one line, `shift-base:stb_image.h`.
   - The sanitizer gates are the regression gate for the fix: across every log of the final run, 0 files mention NChanDelay and 0 lines say `runtime error`.
2. **Sanitizer builds link the shared runtime** (from candidate B's `projects/linux/CMakeLists.txt`).
   - `-static-libstdc++ -static-libgcc` now sit in `if(NOT VOLUM_SANITIZE)` at both link sites (`VoLum-vst3`, `VoLum-app`). The ASan `.so` now has `libstdc++.so.6` and `libgcc_s.so.1` NEEDED.
   - The Release `.so` NEEDED set is unchanged.
   - **`LD_PRELOAD=libasan.so` stays, and the reason was measured** on the box (`ASAN_PRELOAD=none`):
     - REAPER could not insert the plug-in;
     - pluginval exited 1 with `ASan runtime does not come first in initial library list; you should either link runtime to your application or manually preload it with LD_PRELOAD`.
     - An ASan-instrumented `.so` in an uninstrumented host needs the preload. The CMake comment now says exactly that.
   - **The extra `libstdc++.so.6` preload that 07a needed for pluginval is no longer needed** and is removed. With `libasan` alone, pluginval-ASan gives `SUCCESS`, 5/5 editor opens.
3. **UI-thread checks at the IGraphicsLinux entry points** (judge review).
   - The calls are in `OpenWindow` (`IGraphicsLinux.cpp:124`), `CloseWindow` (`:173`), `ProcessX11Events` (`:371`, which is the X event dispatch) and `OnDisplayTimer` (`:386`).
   - They use the registry's own check through the new `IPlugLinuxCheckUIThread()` (`IPlugLinuxRunLoop.h`): a Debug assert, plus an always-on count and `… called off the UI thread` log. No new lock.
   - Every gate counts those lines: 0 in the final run.
4. **`DeactivateGLContext` depth always balances** (latent bug the judge found).
   - In 07a, when a re-entrant `CloseWindow` destroyed the context inside an open scope, `DeactivateGLContext` returned before decrementing the depth.
   - The nesting now lives in `IGraphicsLinuxGLNesting.h` (60 lines, no X or GL). `Enter(hasContext)` and `Leave()` always pair, and `Leave()` reports whether the saved context must be restored. `IGraphicsLinux.cpp:341-364` calls them unconditionally.
   - `DestroyGLWindow` no longer zeroes a depth counter behind the scope's back.
   - `OnDisplayTimer` now also skips `glXSwapBuffers`/`XFlush` when a control closed the editor while drawing.
   - Two new doctests cover it. It is reachable without X because the class has no X dependency; see Doctests.
5. **`WDL/wdlendian.h` is kept as in A.** Against `8eb3b8968` its diff is `+6 −3`, only the Linux narrowing; the `#elif __FLOAT_WORD_ORDER__ != __BYTE_ORDER__` branch for every other compiler is the 1.3.1 line.
6. **Not ported:** pugl, and B's `IPlug_include_in_plug_src.h` hook (above).

Platform layer after the grafts (`wc -l`):

| File | Lines |
| --- | ---: |
| `IGraphicsLinux.cpp` | 718 |
| `IGraphicsLinux.h` | 127 |
| `IGraphicsLinuxGLNesting.h` | 60 |
| `IGraphicsLinuxKeys.cpp` | 197 |
| `IGraphicsLinuxKeys.h` | 34 |
| `IPlugLinuxRunLoop.cpp` | 357 |
| `IPlugLinuxRunLoop.h` | 167 |
| `IPlugVST3_LinuxRunLoop.h` | 308 |
| **Total** | **1968** |

## Gate evidence (box, code head `eed5a7b`)

- **Box:** WSL2 Ubuntu 26.04, GCC 15.2, Mesa llvmpipe, `xvfb-run -a -s "-screen 0 1920x1080x24"`. Scale 2 uses a 3840x2160 screen so that 1800x1200 fits.
- **Command:** `box.ps1 -Branch feature/linux-07-graphics -Sha eed5a7b…` (localai guard on, one build lock) running `bash .scratch/linux-support/arena-07/a/run-gates.sh build validator pluginval reaper input shots xft-shot asan-build asan-reaper asan-pluginval tests`.
- **Exit code:** `gates_rc=0`, taken from the script itself and not through `tee`.
- **Output:** `~/volum-linux/out/07a/eed5a7b/`.
- **Box hygiene:** before every job, `ps -eo pid,etime,args | grep -E 'volum-07|run-gates'` showed no stale job of mine.

```
=== validator ===       Result: 47 tests passed, 0 tests failed              validator exit 0
=== pluginval ===       pluginval exit 0, editor opens/closes: 5/5   SUCCESS  0 X-error/off-thread/sanitizer lines
=== reaper ===          reaper cycles scale=1 asan=0 cycles=20 instances=2: PASS
                          steps: 22, editor opens/closes: 40/40, X errors: 0, off-thread calls: 0, sanitizer report lines: 0
=== input ===           reaper input: PASS   steps: 15, opens/closes 1/1, X errors 0, off-thread 0, sanitizer 0
=== shots ===           reaper shot scale=1: PASS   reaper shot scale=2: PASS   (each 1/1, 0/0/0)
=== xft-shot ===        reaper shot (Xft.dpi 192): PASS   (1/1, 0/0/0)
=== asan-build ===      NEEDED libasan.so.8, libubsan.so.1 (+ libstdc++.so.6, libgcc_s.so.1)
=== asan-reaper ===     reaper cycles scale=1 asan=1 cycles=20 instances=2: PASS
                          steps: 22, editor opens/closes: 40/40, X errors: 0, off-thread calls: 0, sanitizer report lines: 0
=== asan-pluginval ===  pluginval exit 0, editor opens/closes: 5/5   SUCCESS  0 X-error/off-thread/sanitizer lines
=== tests ===           [doctest] test cases: 1486 | 1486 passed | 0 failed | 1 skipped
                        [doctest] assertions: 30251342 | 30251342 passed | 0 failed
                        run-tests-linux.sh exit 0
GATES PASSED: build validator pluginval reaper input shots xft-shot asan-build asan-reaper asan-pluginval tests
```

The only suppression active was `shift-base:stb_image.h`. A sweep over every log under `out/07a/eed5a7b/`:

```
NChanDelay lines: 0   runtime error lines: 0   off-thread lines: 0   X error lines: 0
```

**Input** (`reaper-input.*/result.txt`, via xdotool):

| Action | Result |
| --- | --- |
| Click sound 02 | 214589 px change |
| Down arrow | 202123 px change |
| Wheel over the list | 15265 px change |
| Double click | no crash, no X error |
| Corner-resizer drag | 900x600 to 1020x680 editor; REAPER's window follows (1020x708) |
| Space forwarded to the host | sent; play state stays 0, because headless REAPER cannot enter play with its audio device closed. Unproven, as in 07a |

**HiDPI trace:**

| Run | Trace | FX window |
| --- | --- | --- |
| Scale 1 | `open … 900x600 px, screen scale 1.00 from default` | 900x628 |
| `GDK_SCALE=2` | `open … 1800x1200 px, screen scale 2.00 from GDK_SCALE` | 1800x1257 |
| `Xft.dpi: 192` | `open … 1800x1200 px, screen scale 2.00 from Xft.dpi` | 900x628 |

In the `Xft.dpi` run, REAPER's `setContentScaleFactor(1)` then wins.

**Screenshots** (refreshed from this run): `reports/07-scale1.png` (900x628) and `reports/07-scale2.png` (1800x1257).

## Doctests

`NeuralAmpModeler/tests/test_iplug_linux_runloop.cpp` now has 7 cases (5 from 07a, plus 2 for the GL nesting); all pass in the box suite above. `redproof.sh` on `eed5a7b` reverts each fix and runs the matching case:

```
### single process-wide loop (ticket 05)      :124 CHECK( idleB == 1 )              values: CHECK( 0 == 1 )
### no reference count                         :150 CHECK( frame.fds.size() == 1 )   values: CHECK( 0 == 1 )
### pending timers never attach                :181 REQUIRE( loop.timers.size() == 1 ) values: REQUIRE( 0 == 1 )
### no UI-thread check                         :198 CHECK_FALSE( otherIsUIThread )   values: CHECK_FALSE( true )
### poll loop keeps unregistered timers        :275 CHECK( self.calls == 1 )         values: CHECK( 18 == 1 )
### GL nesting skips the decrement with nothing to restore (07a)
                                               :224 CHECK( nesting.Depth() == 0 )    values: CHECK( 1 == 0 )
### restored                                   test cases: 7 | 7 passed | 0 failed
```

The GL-nesting red proof puts 07a's early return back into the class. 07a returned before decrementing when there was no context to give back; the reverted `Leave()` returns before decrementing when there is nothing to restore.

The doctest covers the class. That `IGraphicsLinux` calls `Leave()` on every `DeactivateGLContext` is code (`IGraphicsLinux.cpp:354-364`), backed by the REAPER and pluginval open/close gates (40/40 and 5/5, release and ASan).

## Laptop and CI

- `pwsh -NoProfile -File NeuralAmpModeler\scripts\run-tests-win.ps1` on `eed5a7b`: exit 0. doctest 1470 passed, 0 failed, 1 skipped; 30255183 assertions.
- <a id="ci"></a>CI: [run 40104891332](https://github.com/guitarlum/VoLum/actions/runs/40104891332) on `eed5a7b` (the code head). **Every job succeeded:**
  - Formatting
  - Windows (tests + installer + portable zip + plugin validation)
  - **Linux tests**
  - macOS (tests + full distribution + plugin validation)
  - NAMCore regression tests
  - Coverage
  - Windows installer upgrade
  - macOS installer upgrade
  - macOS standalone e2e
- The commit that adds this report changes only `.scratch/linux-support/reports/` files. It is report-only and needs no second CI run.

## Not done, or left as is

- Forwarding unhandled keys to the host is still unproven under headless REAPER (above).
- Dialogs, clipboard and URLs are ticket 09 (logging stubs). The standalone window loop is ticket 10.
- No changelog or user-doc line; ticket 15 owns them.
