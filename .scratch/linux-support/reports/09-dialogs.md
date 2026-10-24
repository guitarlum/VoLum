# 09 File dialogs, clipboard, OpenURL, message boxes

Status: **PASS** on code head `912018a9`. The ticket Status line is unchanged.

## Heads

| | Branch | Sha |
| --- | --- | --- |
| Superproject code | `feature/linux-09-dialogs` | `912018a982d2b063df9292ed5ec30e7ddd9dd073` |
| iPlug2 | `volum/linux-09` | `81096efce99a3b6df11fcb714ed0451cc06c0166` |
| This report | a later commit on the same branch | see `git log -1` |

`volum/linux-09` is `volum/linux` `72593f1` plus the dialog commits. It is pushed to `https://github.com/guitarlum/iPlug2.git`. Not merged into `volum/linux`, `feature/linux-support`, or `dev`.

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

**Call sites.** Pack export, pack import, Manage import and Builder "add .nam" on Linux continue in the completion (import re-enters the same function with the paths stashed). Those branches are `#if defined(OS_LINUX)`. The Windows and macOS source paths are the previous synchronous calls.

New units: `IGraphicsLinuxDialogs.cpp`, `IGraphicsLinuxClipboard.cpp`. `IGraphicsLinux.cpp` only forwards. The blank line with trailing spaces in `IPlugVST3.cpp` `createView` is gone.

## Evidence

Box: WSL2, GCC and Clang, one build lock, localai guard passed. Worktree `912018a9`, iPlug2 `81096efce`.

| Check | Result |
| --- | --- |
| GCC doctest | 1496 passed, 0 failed, 1 skipped. `GCC_RC=0` |
| Clang ASan/UBSan doctest | 1461 passed, 0 failed, 36 skipped. `CLANG_RC=0` |
| REAPER release | 2 instances x 20: PASS. 40/40 editor open/close. 0 X errors, 0 off-thread, 0 sanitizer lines |
| REAPER ASan | same: PASS. 40/40. 0 / 0 / 0 |
| REAPER input | PASS. 1/1 open/close. 0 / 0 / 0 |
| Windows tests | 1470 passed, 0 failed, 1 skipped. exe exit 0 |
| CI | [40117911722](https://github.com/guitarlum/VoLum/actions/runs/40117911722) on `912018a9`: **success**. Linux tests, macOS (including standalone e2e and installer upgrade), Windows, formatting, NAMCore, coverage |

Input pixel changes on this build: click 214705, Down 200426, wheel 15265. Corner drag 900x628 to 1020x708. Space forwarded; headless play state stays 0, as in ticket 07.

Clipboard, under `xvfb-run`, a second connection owning `CLIPBOARD` and not answering: `got=0 ms=99 cap=100`. The doctest skips that live wait when it cannot bind its own X server. On this box `/tmp/.X11-unix` is a read-only mount, so a raw `Xvfb` cannot create a socket; `xvfb-run` can, and that is what the one-off used. CI's Linux tests job passed, so the case did not fail there.

## Red proofs

On the box, against `912018a9`, each break was rebuilt into the doctest and then restored (`git diff` in iPlug2 empty afterwards).

| Break | Result |
| --- | --- |
| `TryPortal` returns `TryZenity` immediately | `CHECK( zenity == portal )`, `REQUIRE( 0 == 1 )`. `RED_PORTAL_RC=1` |
| `DeliverNow` runs the completion before returning | `CHECK_FALSE( true )` on "every tool is missing". `RED_ASYNC_RC=1` |
| Both restored | portal case 9/9, async case 5/5, run-loop cases 4/4. all exit 0 |

An earlier head, `6b4e0515`, failed two tests (fake tools replaced `PATH`, so `basename` was missing, and a reap timer leaked into the run-loop registry). CI [40114463147](https://github.com/guitarlum/VoLum/actions/runs/40114463147) on that head failed `Linux tests`. `912018a9` is the fix. That run is not the acceptance run.

## Text entry and popup menus

`CreatePlatformTextEntry` and `CreatePlatformPopupMenu` are still the ticket 07 IGraphics controls (`ITextEntryControl`, `IPopupMenuControl`, menu `isAsync`). The REAPER input pass clicks, keys, wheels and resizes with no X error, and the sound list follows the keyboard. This pass did not type into a text field or choose a VoLum menu item. Nothing failed. Ticket 13's scenes are the place to finish that.

## Deferrals

- `PromptForColor` still returns false.
- With neither `zenity` nor `kdialog`, a message box completes as `kNoResult` and is not drawn.
- The doctest does not time the clipboard on a machine where it cannot start `Xvfb`. The 99 ms measurement is the `xvfb-run` one-off above.
