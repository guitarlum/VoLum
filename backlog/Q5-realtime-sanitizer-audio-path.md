# Q5 — RealtimeSanitizer on the audio path

Not a 1.4.0 blocker. The callback-budget gate (ticket 18) catches a callback that ran long. It does not name the allocation or the lock that made it long.

## Problem

clang 20 and newer ship RealtimeSanitizer (`-fsanitize=realtime`, `[[clang::nonblocking]]`). The audio callback still allocates, takes mutexes and does other work the 1.2.1 audit recorded in `B7-audio-thread-rt-violations.md`. A sanitizer job would fail at the call, instead of only when that call happens to blow the JACK period.

## Scope

- A Linux CI job, clang 20 or newer, that builds the standalone or the VST3 processing path with `-fsanitize=realtime`.
- Mark the audio callback `[[clang::nonblocking]]` and let the sanitizer report the existing violations. The first run is expected to fail on allocations already in the DSP path. Do not suppress those to make the job green.
- Do not retune Windows or macOS golden hashes. Do not add a global sanitizer suppression list.
- Fixing the violations is `B7`, not this job. This job is the tripwire.

## Out of scope

- Replacing the callback-budget smoke. That gate stays.
- Running RTSan on the GitHub-hosted Windows or macOS images until those images have clang 20.
