# Q6 — Callback-timing trend per release

Not a 1.4.0 blocker. Ticket 18 gates a single run: no callback over the JACK period, and the p99 callback under 70 percent of it. It does not say whether this release is slower than the last one.

## Problem

A change can raise the box's p99 callback time from 1.5 ms toward 3.5 ms and still pass the 70 percent budget (3.7 ms at 48000 Hz / 256 frames). The regression shows up only when it crosses that line, which is late.

## Scope

- The Linux standalone CI job already runs with `VOLUM_CALLBACK_TIMING=1` and writes `cb-timing-row` lines. Publish p50, p99 and max callback time from the 60 s window as a CI artifact.
- Keep a baseline from the last release (the three numbers, the period and the commit). Fail the job when p99 grows by more than 20 percent against that baseline, or when max grows by more than 20 percent.
- The baseline is updated only when a release is cut, not on every green `dev` run. Say the margin in the artifact so a failure is readable without the script.
- Windows and macOS golden hashes stay as they are.

## Out of scope

- Using the trend in place of the zero-xrun rule on CI or the callback budgets. They stay.
- Failing on a single noisy p99 sample. The comparison is one 60 s window on the GitHub runner, which is where the zero-xrun rule already holds.
