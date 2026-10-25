# 17 JACK-dummy xruns on the box

Status: **not a VoLum regression**. The 60 s zero-xrun window fails on ticket 10's merge `34d42a7b` as well as on `624e3719`. The plug-in callback stays under 3 ms while JACK reports it. The gate was not loosened. No product code changed, so CI was not dispatched.

Code head is still `624e3719ac4a432a4a60a45e66b075757cdb7b57` (iPlug2 `2a3ba520c`, AudioDSPTools `3f28127`). This commit only adds this report.

## What the log line means

jackdmp 1.9.22's dummy backend is `JackTimedDriver`. After each graph cycle it computes how long to sleep to stay on an anchor. When that sleep would be negative it resets the anchor and logs `JackTimedDriver::Process XRun = N usec`. `N` is the time since this cycle began (`now - fBeginDateUst`), not the amount the sleep overshot. On the 14-xrun window those lines were 7, 12, 16, 20, 21 and 33 usec: the cycle that noticed was itself short.

`JackEngine::CheckXRun` then runs because the previous graph was not finished:

- `state = Running`: the client had entered its process callback and had not returned.
- `state = Triggered`: the client had been signalled and had not entered the callback.
- `finished after current callback`: it returned, but the finish timestamp is after the new cycle's begin.

`JackAudioDriver::ProcessGraphAsyncMaster: Process error` is the same event. The engine returns false on that path, and the driver logs it as an error.

The server is SCHED_FIFO priority 10. VoLum's JACK thread is SCHED_FIFO priority 5 and, between cycles, waits in `futex_wait_queue`.

## The callback is not late

On `624e3719`, a throwaway timer around `IPlugAPPHost::AudioCallback` (box only, `clock_gettime` wall time, not committed) printed once a second. In a window that failed with one `state = Running` line:

- no callback reached 3 ms, and none reached 5 ms (`ge3=0`, `ge5=0` for the whole 5 s warm-up plus 60 s)
- the slowest callback in any one-second bucket was 2672 us
- the period at 48000 / 256 is 5333 us

RtAudio's JACK wrapper around that function is a few memcpys and `convertBuffer`. A 2.7 ms callback does not fill a 5.3 ms period. The cycle that JACK calls late started while a normal callback was still marked Running, which is the dummy schedule slipping, not the amp model holding the audio thread. DSP load on the passing and failing windows stays about 17–21 %. Ticket 12's earlier note (a few microseconds, clients only scheduled, no VoLum lock) is the same driver path; the `Running` lines are that path too, not a new lock.

`perf` is not installed on the box. The thread state came from `ps -L` and `/proc/<pid>/task/<tid>/wchan`.

## Same failure on the ticket 10 merge

`smoke-standalone-audio-linux.sh` is unchanged between `34d42a7b` and `624e3719`. The short runs below are that script cut after run 1, same jackd line, same 60 s gate. Full smokes are the unmodified script.

| Build | Run | Window xruns |
| --- | --- | --- |
| `34d42a7b` (iPlug2 `95da4e747`) | full smoke, first try | 0, 55/55 PASS, peak −8.5 dBFS, max DSP 18.6 % |
| `34d42a7b` | four short windows | 1, 3, 0, 0 (2/4 fail). The 1-xrun log is `state = Running` plus `Process error` |
| `119aac62` (through ticket 08) | one full smoke | 0, 55/55 PASS, max DSP 21.3 % |
| `6303fcc1` (through ticket 11) | two full smokes | 17, then 0 |
| `c69bb2bd` (through ticket 13) | one full smoke | 2 |
| `624e3719` | one full smoke plus ten short windows | full smoke 0. Short windows: 14, 0, 0, then with the timer 0, 0, 1, then 0, 3, 0, 0. 3 of 11 fail |

Ticket 10's code fails the same gate on this box now. A first-parent bisect cannot name a commit: the later merges fail and pass at the same kind of rate. The three failures the final predicate saw at `624e3719` match that rate, not a change in the audio callback. Single full-smoke passes (the original 55/55 at merge time, and one 55/55 here) are the other side of the same coin.

## Box, while this was measured

WSL2 `6.6.114.1-microsoft-standard-WSL2`, clocksource `tsc` (also available: `hyperv_clocksource_tsc_page`, `hyperv_clocksource_msr`, `acpi_pm`). No cpufreq governor in sysfs. PSI cpu, memory and io `avg10/60/300` were all 0.00, including across the failing windows. 24028 MB RAM, about 13660 MB used, about 10360 MB available, swap 483 MB of 6144 MB. `llama-server` RSS about 11.5 GB (the ~49 % figure). Load on a failing short window was 0.08. Nothing else was SCHED_FIFO/RR when the smoke was not running. jackd was realtime at priority 10 whenever the smoke's `prlimit` applied.

The GitHub `Linux standalone` job stays green because that runner's timer does not slip this way. The box's dummy server does, on the current sources and on `34d42a7b`.

## Not done

No change to the smoke, the 60 s window, or the zero-xrun rule. No iPlug2 branch. No CI run. The in-callback timer was not kept.
