# 08 Plugin gates (box + CI)

Status: **PASS** on code head `d13168d1`. The coordinator merges. This report commit is later and does not change the gates.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject, code | `feature/linux-08-plugin-gates` | `d13168d1936acda90fd7073ef4a4c5eeaafd5c6f` |
| iPlug2 | `volum/linux` | `72593f1683be9baf2bc287af9d854e829750a152` (unchanged) |
| AudioDSPTools | from `feature/linux-support` | `3f28127e3125adb8befbdb56e26e986aaf9c0e55` |

No iPlug2 branch. The four `IPlugLinuxCheckUIThread` sites were already in `IGraphicsLinux.cpp`.

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

**Carla and iostream.** Static libstdc++ plus a host that sets the global C++ locale crashes `num_put` in `codecvt` (GNU unique symbols merge the facets). The diagnostic log and the atomic JSON temp path on Linux use `strftime` / `snprintf` / `fwrite`. The Linux CMake file also passes `-fno-gnu-unique`. Windows code in those headers is the previous ostream path.

**box.ps1.** A killed local client SIGHUPs the remote session. Proof: `box.ps1` reached `SESSION_UP` / `sleep 90` on `f678fa02`, the local `ssh -tt` was killed, and the remote `volum-box-runner` was gone (`REMOTE_GONE`). A later crash left `jackd -n volum-carla-…` holding the inherited lock fd; background `jackd` and REAPER now start with fds 7–9 closed, and that orphan was killed before the next job.

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
| UI-thread sites | the four `IPlugLinuxCheckUIThread` calls commented out, `.so` rebuilt | all four `missing IGraphicsLinux::…` lines, `count 0 -> 4`, `FAIL`, exit 1 |

The scripted redproof run removed only the first call (the substitution was not global). The harness still failed, on the missing OpenWindow line. A follow-up with `/g` removed all four and is the table above. The script now uses `/g`.

## Session kill

`box.ps1 -Sha f678fa02 -Command "echo SESSION_UP; sleep 90"` printed `SESSION_UP` and `box: lock build1`. The local `ssh -tt … exec -a volum-box-runner bash /tmp/volum-box-runner-feature-linux-08-plugin-gates.sh` was killed. A fresh ssh saw no `volum-box-runner` and no `sleep 90` (`REMOTE_GONE`). The local PowerShell exited because its ssh was killed. The build lock was free afterwards (the next job took `build1`).

## CI

[Run 40117645281](https://github.com/guitarlum/VoLum/actions/runs/40117645281) on `d13168d1`, dispatched with `ci-watch.ps1 -Dispatch`. Exit 0. Every job succeeded:

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

Artifact `VoLum-linux-vst3` (35 MB). This report commit is after that run and does not change the gates. The redproof script's substitution is now global (`/g`); the all-four revert above was run from that substitution on the box, and the `.so` was restored.

## Laptop

`pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` exit 0. doctest 1470 passed, 0 failed, 1 skipped; 30255184 assertions. An earlier pwsh attempt in the same hour died with MSVC `C1060` (compiler out of heap) while the box and CI were also building. The retry above is the one that finished. A `powershell.exe` 5.1 attempt is not the runner: `check-agent-artifact-links.ps1` turns a native `git check-ignore` message into a terminating error there, and the script already disables that for pwsh 7.

## Not in this ticket

Screenshot and input walks from the arena harness. Ticket 13 owns the visual set. The standalone app, JACK dummy audio and e2e stay with tickets 10 and 11. No changelog or user-guide line; ticket 15 owns them.
