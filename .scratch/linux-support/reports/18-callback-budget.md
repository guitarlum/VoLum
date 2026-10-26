# 18 Callback budget gate

Status: the gate is in. CI run [40248222369](https://github.com/guitarlum/VoLum/actions/runs/40248222369) is green on the code head, including Formatting, Linux standalone and macOS. This commit is report-only.

## Heads

| | Branch | Head |
| --- | --- | --- |
| Superproject | `feature/linux-18-callback-budget` | code `04b26ef561d2ecef16cbb4391b9cb2df81cf7297`; this report is the next commit |
| iPlug2 | `volum/linux-18` (off `2a3ba520c`) | `a086b9280e54e29daa4e92ce1c694f1f672550bd` |

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

Three full smokes in a row on `d5ac20a2` (the 65 percent gate, which is stricter than what shipped): **PASS, PASS, PASS**. Run 1 maxima 2270, 2361, 3161. Two of them printed one driver xrun and did not fail.

`verify-linux.sh --final` on `25ba2a1d`: **exit 0, `linux gates: MET`**. GCC tests, Clang ASan/UBSan, validator, REAPER, sanitizer hosts, the smoke (`RESULT: PASS`), e2e (`ALL PASS 96 checks`), the private-sink real-device gate, and UI scenes. The first attempt died because this worker held `gui.lock` around the whole script and e2e waits for that lock; the rerun did not hold it. The scripts that need the lock take it themselves.

Windows `run-tests-win.ps1` on the 80 percent tests: **1484 passed, 0 failed**.

## Red proofs

Source was mutated on the box, the app rebuilt, run 1 of the smoke executed, then `git checkout` restored the files. Not a switch in the product.

| Proof | Result |
| --- | --- |
| `usleep(4266)` on callbacks 8000..8003 (about 80 percent of the period) | `FAIL`, `window_max_us=5465`, `verdict=fail-callback` |
| Callback takes a mutex the main loop holds for 10 ms about every 2 s | `FAIL`, `window_max_us=10460`, `verdict=fail-callback` |
| Classifier, callbacks at 2672 us, 2 jackd xruns, `--strict` | exit 1, `verdict=fail-strict` |
| Same log, box mode | exit 0, `verdict=driver`, the driver-xrun line |

5465 and 10460 are also over both shipped budgets (3733 and 4267 us). The sleep and mutex proofs were run against the 65 percent build; the recorded maxima still fail 80 percent. They were not re-run after the constant change.

## Backlog

`Q5-realtime-sanitizer-audio-path.md` and `Q6-callback-timing-release-trend.md`, plus the README rows. Docs only.

## Uncertain

- CI's 80 percent limit has two runner samples behind it (3905 and 3004 us). A later runner window could sit above 4267 us without a VoLum change. The report would then need a new sample, not a quieter fraction.
- The three full greens and `verify-linux --final` ran at 65 percent (`d5ac20a2` and `25ba2a1d`), stricter than the box's 70 percent.
