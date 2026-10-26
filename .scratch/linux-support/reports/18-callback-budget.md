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

The period for 256 frames at 48000 Hz is 5334 us (rounded up). The budget is 80 percent of that, 4267 us (`kVoLumLinuxCallbackBudgetThousandths = 800`). A callback over the period or over the budget fails on the box and in CI.

`--strict-xruns` is what CI passes. Any jackd xrun in the 60 s window then fails, as before. The box does not pass that flag. A jackd xrun whose callbacks stayed inside the budget prints `driver xruns: N (not gated on WSL, root cause reports/17-jack-xrun.md)` and does not fail. The script does not look at the kernel or the distro to decide.

`--sanitized` still prints both and gates neither. The instrumented build is not the real-time budget.

## Why 80 percent

Ten box windows at 48000/256, then three full smokes. Window maxima, us:

| | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| short | 2613 | 2847 | 1535 | 2726 | 2828 | 2518 | 2960 | 2690 | 2499 | 2724 |
| full | 2270 | 2361 | 3161 | | | | | | | |

The box peak is 3161 us, 59 percent of 5334. 65 percent (3467 us) passed every one of those, and passed `verify-linux.sh --final` on `25ba2a1d`.

CI run [40247591663](https://github.com/guitarlum/VoLum/actions/runs/40247591663) on that same 65 percent head failed Linux standalone: the runner's window max was **3905 us**, 73 percent of the period, `verdict=fail-callback`. 70 percent is 3734 us, which is still under 3905. The budget is 80 percent (4267 us): 1106 us above the box peak and 362 us above that runner sample. A callback that uses the whole period still fails. The replacement run is 40248222369.

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

5465 and 10460 are also over the shipped 4267 us budget. The sleep and mutex proofs were run against the 65 percent build; the recorded maxima still fail 80 percent. They were not re-run after the constant change.

## Backlog

`Q5-realtime-sanitizer-audio-path.md` and `Q6-callback-timing-release-trend.md`, plus the README rows. Docs only.

## Uncertain

- The 80 percent limit has one CI sample behind it (3905 us). A later runner window could sit above 4267 us without a VoLum change. The report would then need a new sample, not a quieter fraction.
- The three full greens and `verify-linux --final` ran at 65 percent (`d5ac20a2` and `25ba2a1d`). `04b26ef5` only raises the budget. CI's Linux standalone job on `04b26ef5` is the run of the shipped gate on the GitHub runner.
