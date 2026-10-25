# 09 File dialogs, clipboard, OpenURL, message boxes

Status: **PASS** on code head `884bf5ad`. The ticket Status line is unchanged.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject code | `feature/linux-09-dialogs` | `884bf5ad9e559c178c0ba8a05c90b740006b58bf` |
| iPlug2 | `volum/linux-09` | `81096efce99a3b6df11fcb714ed0451cc06c0166` |
| This report | a later commit on the same branch | see `git log -1` |

`volum/linux-09` is `volum/linux` `72593f1` plus the dialog commits. It is pushed to `https://github.com/guitarlum/iPlug2.git`. The fix round did not change it. Not merged into `volum/linux`, `feature/linux-support`, or `dev`.

## Design

File pickers and message boxes are child processes. Nothing links GTK. The host UI thread never `waitpid`s on them.

**Chain.** `PromptForFile`, `PromptForOpenFiles` (multi-select) and `PromptForDirectory` try, in order:

1. xdg-desktop-portal `FileChooser.OpenFile` or `SaveFile` via `gdbus call`, then `gdbus monitor` until `Request.Response`.
2. `zenity --file-selection`.
3. `kdialog`.

A missing binary, a failed spawn, or a portal response other than success or cancel falls through. Portal `uint32 1` and a zenity/kdialog non-zero exit are a user cancel and stop the chain. Paths are argv elements (`posix_spawn`), including spaces, quotes and non-ASCII. `POSIX_SPAWN_CLOEXEC_DEFAULT` drops the host's file descriptors on exec. A test override is prepended to `PATH` so a fake tool can still run `sleep`; `Which()` only searches the override, so the real `zenity` is not used.

**Delivery.** The child's stdout is a non-blocking pipe bound with `IPlugLinuxBindFd`. A 50 ms `IPlugLinuxBindTimer` also `waitpid(WNOHANG)`s, so a host that only reports `POLLIN` still notices an empty-stdout exit (`POLLHUP`). The completion runs on the UI thread. `CancelOwner` on editor close drops completions and reaps the child.

**Message boxes.** `zenity`, then `kdialog`. `ShowMessageBox` returns `kNoResult` immediately. The button is delivered to the completion handler. VoLum's notices ignore the return value and clear the pending text before the call, so the box does not re-enter `OnIdle`.

**Open and reveal.** `xdg-open`. Reveal of a file opens its parent directory. The process is reaped from the run loop.

**Clipboard.** X11 `CLIPBOARD`. A selection this window owns is returned from the cache. Otherwise `XConvertSelection` plus `poll` on the X connection, deadline `kWaitMs` (100). Other events stay queued. `SelectionRequest` is answered from `ProcessX11Events`.

**Call sites.** Linux-only `#if defined(OS_LINUX)` forks. A cross-platform completion handler would switch the macOS `NSSavePanel` to `beginSheetModalForWindow`. Pack export and pack open share one writer and one opener with the synchronous path. Manage import and Builder "add .nam" stash the paths and re-enter the same function; that re-entry does not copy the import or builder body.

New units: `IGraphicsLinuxDialogs.cpp`, `IGraphicsLinuxClipboard.cpp`. `IGraphicsLinux.cpp` only forwards. The blank line with trailing spaces in `IPlugVST3.cpp` `createView` is gone.

## Coordinator fix round

1. **Windows and macOS dialog sequence.** A stripper treats `#if defined(OS_LINUX)` as false, drops that branch and the guard lines, keeps the `#else` body, and compares the result with `git show 43ecf015:<file>` after newline normalization.

   - `VoLumCustomOverlay.h`: match, 2130 lines. Manage import and Builder add both compile to the original macOS comment, one `ReleaseMouseCapture()`, and `PromptForFiles`.
   - `VoLumPackOverlay.h`: match, 936 lines. `OpenPickedPack` sits in the Linux branch.
   - Linux arms, read in source: StartImport, Builder `kAddFile`, pack export and pack open each call `ReleaseMouseCapture()` once, inside the arm that opens the dialog. Nothing releases the mouse before the `#if`.

   `VoLumPackActions.inc.cpp` is not a byte match of `43ecf015`. With `OS_LINUX` undefined the dialog lines are still the original comment, one release, and `PromptForFile` without a completion handler. The write and open bodies moved into `writeChosen` and `openChosen`. The synchronous path calls those helpers. The log strings are the same text.

2. **Shared helpers.** `writeChosen` appends `.volumpack`, writes the pack, and logs `export failed` or `export wrote`. `openChosen` opens the pack and logs `opened`, `skipped on open`, or `open refused`. Both the synchronous return and the Linux completion call the same helper. The overlay re-entry only stashes paths and calls `StartImport` or `HandleBuilderAction` again.

3. **Async coverage.** Carried to ticket 11. No new Xvfb check in this round. The harness hooks (`VOLUM_PACK_SAVE_PATH`, `VOLUM_PACK_OPEN_PATH`, `VOLUM_IMPORT_OPEN_PATHS`) return before `PromptForFile` / `PromptForOpenFiles`, so they never run the completion. The scenario list is in `.scratch/linux-support/issues/11-e2e-linux.md` under "Carried from ticket 09": pack export, pack open, Manage import, Builder add. Each must be shown red with the completion not invoked. Ticket 11 owns them.

4. **Changelog.** The existing 1.4.0 line stays. Ticket 15 rewrites those lines.

`run-tests-win.ps1` on this branch takes the named mutex `Local\VoLum-run-tests-win-build` from `feature/linux-support` `bf5ce757`, so MSBuild does not run in two worktrees at once.

## Clipboard wait bound

The no-owner doctest used to pass on `elapsed < 250`, on a sanitizer `return`, and on an Xvfb socket miss after only `CHECK(kWaitMs == 100)`. A 200 ms wait passed, and a green Linux job did not show that the wait ran.

The GCC case now checks `elapsed >= kWaitMs - 10` and `elapsed < kWaitMs + 50`. The 10 ms is the truncation under the deadline. The 50 ms is one `poll` wakeup past `kWaitMs`, not a second wait: `2 * kWaitMs` is 200, and 200 is not `< 150`. The test prints the elapsed milliseconds. `run-tests-linux.sh` sets `VOLUM_REQUIRE_X=1` and runs the GCC binary under `xvfb-run`. A missing display then fails. Without the variable, the same miss prints a `MESSAGE` and returns. The sanitizer build still skips, and that skip prints a `MESSAGE`. The GCC job does not.

iPlug2 was not changed. `kWaitMs` is still 100.

| Check | Result |
| --- | --- |
| Box GCC | 1496 passed, 0 failed, 1 skipped. Printed `clipboard no-owner wait measured 99 ms (lower 90, upper 150)` |
| Box Clang ASan/UBSan | 1461 passed, 0 failed, 36 skipped. Message: sanitizer build skips the live wait |
| Red, no display | `VOLUM_REQUIRE_X=1` and `DISPLAY` unset. `FATAL ERROR: VOLUM_REQUIRE_X=1 and no X display; the clipboard wait was not measured`. `RED_DISPLAY_RC=1` |
| Red, two waits | Deadline set to `kWaitMs * 2`, then restored. `CHECK( 199 < 150 )` failed. Printed `measured 199 ms`. `RED_DOUBLE_RC=1`. `git diff --exit-code -- iPlug2` was empty afterwards |
| CI | [40143126884](https://github.com/guitarlum/VoLum/actions/runs/40143126884) on `884bf5ad`: **success**. Every `ci.yml` job, including macOS tests, macOS installer upgrade, and macOS standalone e2e. GCC log: `VOLUM_REQUIRE_X: 1` and `clipboard no-owner wait measured 99 ms (lower 90, upper 150)` |

An earlier dispatch on `7547e7f2`, [40141141655](https://github.com/guitarlum/VoLum/actions/runs/40141141655), failed formatting on the `printf` wrap only. `884bf5ad` is that wrap. The wait logic is the same.

## Evidence (fix round, `516cf043`)

Box: WSL2, GCC and Clang, one build lock, localai guard passed. Worktree `516cf043`, iPlug2 `81096efce`.

| Check | Result |
| --- | --- |
| GCC doctest | 1496 passed, 0 failed, 1 skipped. `GCC_RC=0` |
| Clang ASan/UBSan doctest | 1461 passed, 0 failed, 36 skipped. `CLANG_RC=0` |
| Windows tests | 1470 passed, 0 failed, 1 skipped. exe exit 0. Serial mutex held for the build |
| CI | [40123212253](https://github.com/guitarlum/VoLum/actions/runs/40123212253) on `516cf043`: **success**. Every `ci.yml` job completed success, including macOS tests, macOS installer upgrade, and macOS standalone e2e. None skipped |

## Evidence (dialog engine, `912018a9`)

REAPER smoke and the red proofs below are from the head before this fix round. That head's CI was [40117911722](https://github.com/guitarlum/VoLum/actions/runs/40117911722). The acceptance run is `40123212253`.

| Check | Result |
| --- | --- |
| REAPER release | 2 instances x 20: PASS. 40/40 editor open/close. 0 X errors, 0 off-thread, 0 sanitizer lines |
| REAPER ASan | same: PASS. 40/40. 0 / 0 / 0 |
| REAPER input | PASS. 1/1 open/close. 0 / 0 / 0 |

Input pixel changes on that build: click 214705, Down 200426, wheel 15265. Corner drag 900x628 to 1020x708. Space forwarded; headless play state stays 0, as in ticket 07.

The first clipboard one-off, under `xvfb-run`, was `got=0 ms=99 cap=100`. That sample is what set the `kWaitMs + 50` ceiling. The GCC doctest now times the same wait; see "Clipboard wait bound".

## Red proofs

On the box, against `912018a9`, each break was rebuilt into the doctest and then restored (`git diff` in iPlug2 empty afterwards).

| Break | Result |
| --- | --- |
| `TryPortal` returns `TryZenity` immediately | `CHECK( zenity == portal )`, `REQUIRE( 0 == 1 )`. `RED_PORTAL_RC=1` |
| `DeliverNow` runs the completion before returning | `CHECK_FALSE( true )` on "every tool is missing". `RED_ASYNC_RC=1` |
| Both restored | portal case 9/9, async case 5/5, run-loop cases 4/4. all exit 0 |

An earlier head, `6b4e0515`, failed two tests (fake tools replaced `PATH`, so `basename` was missing, and a reap timer leaked into the run-loop registry). CI [40114463147](https://github.com/guitarlum/VoLum/actions/runs/40114463147) on that head failed `Linux tests`. `912018a9` is that fix. That run is not the acceptance run.

## Text entry and popup menus

`CreatePlatformTextEntry` and `CreatePlatformPopupMenu` are still the ticket 07 IGraphics controls (`ITextEntryControl`, `IPopupMenuControl`, menu `isAsync`). The REAPER input pass clicks, keys, wheels and resizes with no X error, and the sound list follows the keyboard. This pass did not type into a text field or choose a VoLum menu item. Nothing failed. Ticket 13's scenes are the place to finish that.

## Deferrals

- `PromptForColor` still returns false.
- With neither `zenity` nor `kdialog`, a message box completes as `kNoResult` and is not drawn.
- Without `VOLUM_REQUIRE_X`, a machine with no display still skips the live clipboard wait after a `MESSAGE`. The GCC job sets the variable and does not skip.
- Pack export, pack open, Manage import and Builder add through the asynchronous Linux dialog are ticket 11 (see the carry-over above).
