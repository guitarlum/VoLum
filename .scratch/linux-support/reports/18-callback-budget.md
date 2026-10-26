# 18 Callback budget gate

Status: the gate is in. CI run [40256879294](https://github.com/guitarlum/VoLum/actions/runs/40256879294) is green on the code head `8a8ed405`, macOS included. Cold verifier (cross-family) ACCEPTED with no MUST-FIX. This commit is report-only.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject | `feature/linux-18-callback-budget` | code `8a8ed405`; this report is the next commit |
| iPlug2 | `volum/linux-18` (off `2a3ba520c`) | `ccc4da534` |

No change to `main`, `dev` or `feature/linux-support`.

## What the gate does

`VOLUM_CALLBACK_TIMING=1` is read once in `IPlugAPPHost::Init`, before the audio thread. Unset, the callback does not read the clock. Set, each `AudioCallback` (the plug-in's processing included) stores its `CLOCK_MONOTONIC` duration in a fixed ring and an atomic max. No allocation, lock or stdio on that thread. The main thread prints the rows after `CloseAudio` has stopped the JACK thread.

The period for 256 frames at 48000 Hz is 5334 us (rounded up). Two rules, the same on the box and in CI:

- **Stall:** any callback in the window longer than the period fails (`fail-callback`).
- **Load:** the 99th-percentile callback longer than 70 percent of the period, 3733 us (`kVoLumLinuxCallbackBudgetThousandths = 700`), fails (`fail-load`).

`--strict-xruns` is what CI passes. Any jackd xrun in the 60 s window then fails, as before. The box does not pass that flag. A jackd xrun whose callbacks passed both rules prints `driver xruns: N (not gated on WSL, root cause reports/17-jack-xrun.md)` and does not fail. The script does not look at the kernel or the distro to decide.

`--sanitized` still prints both and gates neither. The instrumented build is not the real-time budget.

## Why the p99 and not the slowest callback

The worker's first gate held the slowest single callback against a fraction of the period. Every recorded window (each 60 s, about 11256 callbacks, every callback logged), us:

| Window | p50 | p99 | max |
| --- | --- | --- | --- |
| box, 16 windows (10 short, 3 full, 3 at `0d028f37`) | 824 to 851 | 1355 to 2159 | 1535 to **4172** |
| CI 40247591663 | 2663 | 2915 | **3905** |
| CI 40248222369 | 2676 | 2837 | 3004 |
| red proof: `usleep(4266)` on 4 callbacks | 862 | 1606 | 5465 |
| red proof: 10 ms mutex | 852 | 1643 | 10460 |

The maximum is scheduler noise. One quiet box window right after a build hit 4172 us (78 percent) with p99 2159; CI's 3905 us was the only callback over 3300 in its window. A max budget of 65 percent failed CI, a box/CI split at 70/80 failed the box 1 run in 3, and 80 percent passed that box window by 95 us. The p99 skips the slowest 113 callbacks of the window, and it sits at 25 to 40 percent of the period on the box and 53 to 55 percent on the runner, which is about three times slower than the box (p50 2670 against 830 us).

70 percent on the p99 leaves 30 points of headroom on the box and 15 on CI. A sustained cost rise of about 28 percent on the runner, or 70 percent on the box, fails it. Smaller trends are backlog `Q6`. Stalls, which the p99 hides on purpose, are the period rule's job: both red proofs exceed the period.

## Box

Three full smokes in a row on the shipped gate (`8a8ed405`, iPlug2 `ccc4da534`): **PASS, PASS, PASS**. Run 1 p99 1446, 1430, 1333 us; max 2536, 2674, 2473 us. Two of them printed driver xruns and did not fail.

Earlier, three full smokes on `d5ac20a2` (max gate at 65 percent) passed, and `verify-linux.sh --final` on `25ba2a1d` was **exit 0, `linux gates: MET`**: GCC tests, Clang ASan/UBSan, validator, REAPER, sanitizer hosts, the smoke, e2e (`ALL PASS 96 checks`), the private-sink real-device gate and UI scenes. The first attempt died because the worker held `gui.lock` around the whole script and e2e waits for that lock; the rerun did not hold it. The commits since only change the classifier, its doctests and the smoke's pass line.

Windows `run-tests-win.ps1` on `8a8ed405`: **1484 passed, 0 failed**.

## Red proofs

Source was mutated on the box, the app rebuilt, run 1 of the smoke executed, then `git checkout` restored the files. Not a switch in the product.

| Proof | Result |
| --- | --- |
| `usleep(4266)` on callbacks 8000..8003 (about 80 percent of the period) | `FAIL`, `window_max_us=5465`, `verdict=fail-callback` |
| Callback takes a mutex the main loop holds for 10 ms about every 2 s | `FAIL`, `window_max_us=10460`, `verdict=fail-callback` |
| Classifier, callbacks at 2672 us, 2 jackd xruns, `--strict` | exit 1, `verdict=fail-strict` |
| Same log, box mode | exit 0, `verdict=driver`, the driver-xrun line |
| 2400 us busy wait in every callback (`8a8ed405`) | `FAIL`, p50 3233, `window_p99_us=4662` (over 3733), max 7562, `verdict=fail-callback` |

The sleep and mutex proofs ran on the earlier max-gate builds; their recorded maxima (5465, 10460) are over the period, which the shipped stall rule fails. The busy-wait window also broke the period, so the stall verdict came first; its p99 alone fails the load rule. The doctests pin the load rule on its own: with it reading the max instead of the p99, `run-tests-win.ps1` fails 8 checks (1483 of 1484 cases). Splitting the budget 70/80 by box and CI was tried at `0d028f37` and dropped: its 70 percent max line failed the box 1 run in 3 (4172 us).

## Backlog

`Q5-realtime-sanitizer-audio-path.md` and `Q6-callback-timing-release-trend.md`, plus the README rows. Docs only.

## Uncertain

- The stall rule still reads the single slowest callback against the full period. No recorded window came closer than 4172 us (78 percent); a runner on slower hardware could. The fix would be a new sample and a look at what stalled, not a looser rule.
- The p99 on CI (53 to 55 percent) has two runner samples. GitHub may move the job to a slower CPU type; 70 percent is then the first thing to fail.

## Coordinator follow-ups

- The classifier does not compare parsed rows with the header `count`, so a torn dump with a non-empty window could pass (verifier SHOULD-FIX).
- The box hands the classifier only the jackd log count; an xrun the probe saw without a log line is labelled `pass` rather than `driver`. Both exit 0, so this is the label only. CI fails it on the strict check.
- Both go into the follow-up with the Fedora openh264 mirror fix.
