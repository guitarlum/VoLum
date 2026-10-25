# 08 Plugin gates (box + CI)

Status: **PASS** after hiding static libstdc++. The shared headers are back to the ostream code. The CI run is the dispatch of this commit.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject | `feature/linux-08-plugin-gates` | this commit. The code is `5edf9324` (release exports are the three VST3 entry points; `VOLUM_UI_THREAD_PROBE` builds the gate variant). It merges `feature/linux-support` `fdc226e5`. |
| iPlug2 | `volum/linux` | `ee4ce0e694fec7e57aef9e4b7f8a1277bbd3300c` |
| AudioDSPTools | from `feature/linux-support` | `3f28127e3125adb8befbdb56e26e986aaf9c0e55` |

No new iPlug2 branch. The four `IPlugLinuxCheckUIThread` sites stay in `IGraphicsLinux.cpp`.

## What was built

Product scripts, from the ticket 07 arena harness (those scratch copies are deleted):

- `NeuralAmpModeler/scripts/validate-vst3-linux.sh` — UI-thread sites, Steinberg validator, pluginval strictness 10, Carla single-client.
- `NeuralAmpModeler/scripts/smoke-reaper-linux.sh` and `smoke-reaper-linux.lua` — load, open the editor, render 10 s, two instances, close. `--mode cycles` is the sanitizer open/close loop.
- `NeuralAmpModeler/scripts/linux/smoke-hosts-sanitize.sh` — REAPER 2×20 and pluginval against `VOLUM_SANITIZE`.
- `NeuralAmpModeler/scripts/linux/ui-thread-sites.sh` — the four call sites.
- `NeuralAmpModeler/scripts/linux/ubsan.supp` — one line, `shift-base:stb_image.h`.
- `NeuralAmpModeler/scripts/linux/provision-linux.sh` — `gdb`, and `--ci --hosts` so CI installs pluginval, REAPER and Carla without the 50 GiB check.
- `NeuralAmpModeler/scripts/linux/box.ps1` — the remote job is a file run under `ssh -tt`, and the next run kills a leftover `volum-box-runner` for the same branch.
- `.github/workflows/ci.yml` job `linux-plugin`, display name **Linux plugin gates**. It builds the VST3 and uploads `VoLum-linux-vst3`.

`verify-linux.sh` now runs the sanitizer host smoke after the release gates.

## Decisions

**LeakSanitizer stays off for host smokes** (`ASAN_OPTIONS=detect_leaks=0`). LeakSanitizer reports every allocation still live when the process exits. REAPER, pluginval and Carla are uninstrumented hosts; their own allocations are not VoLum bugs. `run-tests-linux.sh --clang --sanitize` does not set `detect_leaks=0`, so leaks in the doctest binary still fail that job. `LD_PRELOAD=libasan.so` (from `gcc -print-file-name`) stays in the scripts, not in CMake. The only UBSan suppression is `shift-base:stb_image.h`.

**Carla.** Ubuntu's `/usr/bin/carla-single` is the plug-in bridge: it execs `carla-bridge-native` and does not process audio. The gate drives `libcarla_standalone2` in `ENGINE_PROCESS_MODE_SINGLE_CLIENT` (what `carla-jack-single` is) against a private `jackd -d dummy -r 48000 -p 256`. The server name is per process, so it does not take the default JACK server.

**Render null.** The bar is peak residual ≤ -100 dBFS, the same number as `platform_null.h`, with 0 dBFS = amplitude 1. It is not a bit-exact compare: the box is GCC 15 / glibc 2.43 and CI is g++-12 / glibc 2.35. The live render must also peak above -40 dBFS, so silence cannot pass. On the box the two takes and the committed file were bit-identical (`-inf`).

**Carla and iostream.** Static libstdc++ (`-static-libstdc++`, still skipped when `VOLUM_SANITIZE`) puts GNU-unique facet ids in the plug-in's dynamic table, and a host locale then binds `num_put` to the host's `codecvt`. `-fno-gnu-unique` does not apply to the prebuilt `libstdc++.a`. The stdio forks only avoided two call sites. They are gone: both headers match `43ecf015`. The link hides the runtime with `--exclude-libs,ALL` and a version script. The shipped plug-in exports `GetPluginFactory`, `ModuleEntry` and `ModuleExit` only. `VOLUM_UI_THREAD_PROBE` (default off) is a second build the UI-thread gate links; `ld` ignored `extern "C++"` patterns, so that script names the six harness symbols in mangled form.

`nm -D --defined-only` / `readelf --dyn-syms` on the box, same reverted sources:

| Build | Defined dynamic symbols | `std::` / `__gnu_cxx` | GNU-unique |
| --- | --- | --- | --- |
| Before (no version script, no `--exclude-libs`) | 12952 | 4461 | 256, including `std::num_put<char>::id` |
| After `1781e04f` | 9 (factory, entry, exit, six gate symbols) | 0 | 0 |
| Red: those two link options removed, static libstdc++ kept | 12953 | 4462 | 256, `num_put::id` back |

Green, forks reverted and the link options on: symbol check PASS, UI-thread PASS, validator `47 tests passed`, pluginval exit 0 (editor 5/5), Carla `transport frame 96256` exit 0, REAPER null `-inf` dBFS (editors 3/3). Red, link options removed: `check-stdlib-exports.sh` exit 1 (`4619 bad symbols`), Carla `Segmentation fault` / `carla-single exit 139`. The good link was restored afterwards and the check passed again.

ASan keeps shared libstdc++ (`NEEDED libstdc++` 1, plus `libasan.so.8` and `libubsan.so.1`). Host smoke on the earlier head: REAPER 2×20 PASS, pluginval exit 0.

**Release versus probe, box at `5edf9324`.** `nm -D --defined-only` on the release `.so`:

```
T GetPluginFactory
T ModuleEntry
T ModuleExit
```

`check-stdlib-exports.sh` PASS. UI-thread on the probe `.so`: `count 0 -> 10`, PASS. Validator `47 tests passed`. pluginval exit 0, editor 5/5. Carla `transport frame 96000`, exit 0. REAPER null `-inf` dBFS, editors 3/3. The probe `.so` adds only:

```
T _ZN5iplug23IPlugLinuxCheckUIThreadEPKc
T _ZN5iplug24IPlugLinuxOffThreadCallsEv
T _ZN5iplug9igraphics14IGraphicsLinux10OpenWindowEPv
T _ZN5iplug9igraphics14IGraphicsLinux11CloseWindowEv
T _ZN5iplug9igraphics14IGraphicsLinux14OnDisplayTimerEv
T _ZN5iplug9igraphics14IGraphicsLinux16ProcessX11EventsEv
```

Red, those six added to the release build (`-DVOLUM_UI_THREAD_PROBE=ON`): the check exited 1, `6 extra symbols`, listing those names. Turning the option off restored the three and the check passed. `redproof-plugin-gates.sh` on the probe variant removed all four sites: four `missing IGraphicsLinux::…` lines, `count 0 -> 4`, `OpenWindow signal 11`, exit 1. The script restored `IGraphicsLinux.cpp` (`grep -c` 4). GCC tests 1503 passed. Clang ASan/UBSan 1468 passed, 36 skipped. No shared C++ change in this step, so Windows tests were not re-run.

**box.ps1 and jackd.** A killed local client SIGHUPs the remote session (`REMOTE_GONE` on the `sleep 90` check). jackd ignores SIGHUP. The old gate only had `trap cleanup_jack RETURN`, so a kill left the server with ppid 1. The gate now traps EXIT, HUP, INT and TERM, closes inherited lock fds 7–9, and starts jackd with `PR_SET_PDEATHSIG` so the kernel sends SIGTERM when that shell dies.

Red, old script `2791e749`: `kill -TERM` on `validate-vst3-linux.sh` (pid 270091) left `jackd -n volum-carla-270092` pid 270106, ppid 1. Killing the local `ssh -tt` during the same RETURN-only launch (held open with `sleep 25` so the kill could land) left `jackd -n volum-carla-274877` pid 274878, ppid 1. Both orphans were then stopped.

Green, fixed script `6bdc74ab`: `kill -TERM` on `validate-vst3-linux.sh` (pid 275806) while `jackd -n volum-carla-275807` was up, then `ps`, printed `JACKD_GONE`. Killing the local `ssh -tt` (pid 46796) once that script had reached `=== carla-single ===` also printed `JACKD_GONE`.

## Gate evidence (box, code head `d13168d1`)

Box: WSL2 Ubuntu 26.04, GCC 15.2, `xvfb-run -a`. pluginval 1.0.4, REAPER 7.82, Carla 2.5.10. One build lock. `iPlug2` `72593f1`.

| Gate | Result |
| --- | --- |
| UI-thread sites | `count 0 -> 10, OpenWindow signal 11` then `PASS`. All four `IGraphicsLinux::… called off the UI thread` lines present. |
| Steinberg validator | `Result: 47 tests passed, 0 tests failed`, exit 0 |
| pluginval 10 | exit 0, `SUCCESS`, editor opens/closes 5/5, 0 X-error/off-thread/sanitizer lines |
| Carla single-client | `loaded count=1 name='VoLum'`, `transport frame 96000` (2 s at 48 kHz), `PASS`, exit 0 |
| REAPER gates | `PASS`. steps 10, editor opens/closes 3/3, X errors 0, off-thread 0, sanitizer 0. Self-null and reference null both `null peak -inf dBFS over 528000 frames x 1 ch (live peak -9.24 dBFS, limit -100, floor -40)` |
| Sanitizer REAPER | `reaper cycles asan=1 cycles=20 instances=2: PASS`, steps 22, opens/closes 40/40, 0 / 0 / 0 |
| Sanitizer pluginval | exit 0, `SUCCESS`, 5/5, 0 X-error/off-thread/sanitizer lines |
| Sanitizer NEEDED | `libasan.so.8`, `libubsan.so.1`, and `libstdc++` (static libstdc++ is off when `VOLUM_SANITIZE` is on) |

`runtime error:` lines in the sanitizer pluginval log and the REAPER cycle log: 0. The reference WAV is `NeuralAmpModeler/scripts/linux/fixtures/reaper-default-10s.wav` (1 584 736 bytes).

## Red proofs

Each of these exited non-zero. The tree was restored afterwards (the UI-thread `.so` was rebuilt with the four checks back; `grep -c IPlugLinuxCheckUIThread` was 4).

| Gate | How | Output |
| --- | --- | --- |
| validator | VST3 whose `.so` is an empty `plug()` | `validator exit 255` |
| pluginval | same bundle | `pluginval exit 1`, `FAILED!!  1 test failed, out of a total of 1`, `FAILURE`, 0 editor opens |
| Carla | same bundle | `add_plugin failed: b'Failed to get plugin description'`, exit 1 |
| REAPER | `--reference` a 10 s silence file | `null FAIL residual -9.24 dBFS is above -100` |
| UI-thread sites | `redproof-plugin-gates.sh` at checkout `482b0ce9` (perl `/g` removes all four `IPlugLinuxCheckUIThread` calls, then rebuilds) | all four `missing IGraphicsLinux::…` lines, `count 0 -> 4`, `OpenWindow signal 11`, `FAIL`, `ui-thread sites exit 1` |

That UI-thread row is the script itself, not a hand-edited substitution. On the box, `bash NeuralAmpModeler/scripts/linux/redproof-plugin-gates.sh` at `482b0ce9` printed:

```
ui-thread sites: missing IGraphicsLinux::OpenWindow called off the UI thread
ui-thread sites: missing IGraphicsLinux::CloseWindow called off the UI thread
ui-thread sites: missing IGraphicsLinux::ProcessX11Events called off the UI thread
ui-thread sites: missing IGraphicsLinux::OnDisplayTimer called off the UI thread
ui-thread sites: count 0 -> 4, OpenWindow signal 11
ui-thread sites: FAIL
ui-thread sites exit 1
red ui-thread exit 1
RED PROOFS PASSED (each gate exited non-zero)
```

The same invocation also re-failed the other gates: validator exit 255, pluginval `FAILED!!  1 test failed, out of a total of 1`, Carla `add_plugin failed: b'Failed to get plugin description'` exit 1, REAPER `null FAIL residual -9.24 dBFS is above -100`. The script then checked `IGraphicsLinux.cpp` back out and rebuilt `VoLum-vst3`. After it exited, `grep -c IPlugLinuxCheckUIThread` was 4, the worktree was clean, and no `jackd` was left. An earlier scripted run, before `/g`, removed only OpenWindow; that is not the proof above.

## Session kill

`box.ps1 -Sha f678fa02 -Command "echo SESSION_UP; sleep 90"` printed `SESSION_UP` and `box: lock build1`. The local `ssh -tt … exec -a volum-box-runner bash /tmp/volum-box-runner-feature-linux-08-plugin-gates.sh` was killed. A fresh ssh saw no `volum-box-runner` and no `sleep 90` (`REMOTE_GONE`). The local PowerShell exited because its ssh was killed. The build lock was free afterwards (the next job took `build1`).

## CI

[Run 40143690185](https://github.com/guitarlum/VoLum/actions/runs/40143690185) on `d21ad10a` was green, including Linux plugin gates and macOS. That head still exported the six UI-thread symbols from the shipped `.so`, and it did not contain `feature/linux-support`. The run for this commit is dispatched with `ci-watch.ps1 -Dispatch` and is the one that has to be green. The numeric id is the run whose `headSha` is this commit; it is not written back into a later commit.

[Run 40117645281](https://github.com/guitarlum/VoLum/actions/runs/40117645281) jobs, for the record:

| Job | Result |
| --- | --- |
| Linux plugin gates | success |
| Linux tests | success |
| Windows (tests + installer + portable zip + plugin validation) | success |
| Windows installer upgrade | success |
| macOS (tests + full distribution + plugin validation) | success |
| macOS installer upgrade | success |
| macOS standalone e2e | success |
| NAMCore regression tests | success |
| Formatting | success |
| Coverage report | success |

On ubuntu-22.04 the plugin job matched the box reference bit for bit:

```
ui-thread sites: PASS
Result: 47 tests passed, 0 tests failed
validator exit 0
pluginval exit 0, editor opens/closes: 5/5
carla-single: PASS
null peak -inf dBFS over 528000 frames x 1 ch (live peak -9.24 dBFS, limit -100, floor -40)
reaper gates asan=0 cycles=20 instances=2: PASS
```

Artifact `VoLum-linux-vst3` (35 MB) on that earlier run. The `/g` redproof above is from `redproof-plugin-gates.sh` at `482b0ce9`, and the `.so` was restored by that script.

## Laptop

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` on this commit's parent exited 0 after the header revert: doctest 1471 passed, 0 failed, 1 skipped; 30255204 assertions. It ran alone. `bf5ce757` (the MSBuild mutex on `feature/linux-support`) is not merged.

## Not in this ticket

Screenshot and input walks from the arena harness. Ticket 13 owns the visual set. The standalone app, JACK dummy audio and e2e stay with tickets 10 and 11. No changelog or user-guide line; ticket 15 owns them.
