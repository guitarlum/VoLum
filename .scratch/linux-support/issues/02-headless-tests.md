Status: ready-for-agent
Blocked by: 01

# 02 Doctest suite on Linux

## Scope

May write:
- `NeuralAmpModeler/tests/**`
- `NeuralAmpModeler/scripts/run-tests-linux.sh`
- `NeuralAmpModeler/scripts/linux/**`
- portable fixes in `NeuralAmpModeler/*.h|*.cpp`, under `#if` only where needed

## Work

- **`run-tests-linux.sh`** mirrors `run-tests-mac.sh`:
  - apply the iplug2 patches;
  - run the guard scripts that are not Windows-only (port them or explain why not);
  - `cmake -S NeuralAmpModeler/tests -B build-tests-linux`;
  - build and run the suite.
  - `--clang --sanitize` builds Debug with ASan and UBSan, using the macOS
    sanitizer exclusions.
- Make `NeuralAmpModeler-Tests` build and pass with GCC (Release) and with Clang
  ASan/UBSan.
- **Golden DSP:** add a Linux branch to `PlatformExpected` in `test_golden_dsp.cpp`
  - Generate the Linux set.
  - Add a null test of the Linux renders against the Windows renders (≤ -100 dBFS
    peak residual).
  - The same rule applies to the `_WIN32`-only pins in `test_volum_nam_exact.cpp`
    and `test_volum_pitch_burst.cpp`: give Linux its own pins, backed by the
    null test.
  - The Windows and macOS sets stay byte-identical.

- **`NeuralAmpModeler/scripts/linux/verify-linux.sh`:** the box half of the
  predicate.
  - It runs `run-tests-linux.sh` (GCC), then `--clang --sanitize`, then every
    gate script that exists from later tickets: plugin gates, standalone audio
    smoke, e2e, real-device gate, UI scenes.
  - With `--final`, a missing gate script is a failure.
  - Later tickets append their gate to it.

## Acceptance

- On the box: `run-tests-linux.sh` and `run-tests-linux.sh --clang --sanitize`
  both exit 0, with the test count in the report.
- On the laptop: `run-tests-win.ps1` exits 0, and the Windows golden hashes are
  unchanged (`git diff` shows no edits to the Windows or macOS expected values).
- The null test is proven red: the Linux branch is pointed at a wrong hash and
  the residual threshold is set to an impossible value, both fail, then both
  are restored.
