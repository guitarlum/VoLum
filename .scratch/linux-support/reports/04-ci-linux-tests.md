# 04 Linux doctest CI

Status: pass under D1. Ticket not marked resolved.

Branch `feature/linux-04-ci`. The `Linux tests` job is on `ubuntu-22.04`. It checks out submodules the same way the other jobs do, downloads the VST3 SDK headers, runs `provision-linux.sh --ci`, then `run-tests-linux.sh` and `run-tests-linux.sh --clang --sanitize`. The display name is `Linux tests`. `merge-verified-branch.ps1` refuses unless every job whose name starts with `Linux` is green, unless `-AllowWindowsOnly` is passed.

## Toolchain decision

Option 1: the same GCC major on the box and on CI, selected by `CC`/`CXX`.

| Host | GCC | Clang |
| --- | --- | --- |
| Box, Ubuntu 26.04 | `g++-15` 15.2.0-16ubuntu1 (archive) | `clang++-21` 21.1.8-6ubuntu1 (archive) |
| CI, ubuntu-22.04 | `g++-15` 15.2.0-15ubuntu1~22~ppa2 (`ppa:ubuntu-toolchain-r/test`) | `clang++-21` 21.1.8 from apt.llvm.org (`llvm-toolchain-jammy-21`) |

26.04 also packages GCC 13 (13.4.0, not installed). A common major exists, so the docker fallback was not used. `run-tests-linux.sh` exports `CC=gcc-15` `CXX=g++-15`, or `clang-21` / `clang++-21` for the sanitizer run.

The sanitizer exclusion string is the single `--test-case-exclude` in `run-tests-mac.sh`:

`Golden *,Load *NAM*,Cached NAM dspData can construct multiple models when copied,Core slimmable NAM example loads and processes,Process one block through every bundled main NAM file,A2 core load and prewarm timing is visible in test logs,A2 container can lazily activate the Lite submodel after load`

`Golden *` skips the golden DSP and golden-render hash cases. It does not skip `Pitch burst: pedal renders keep the 1.3.0 engine's exact bits`. Those pitch hashes still run under ASan.

## Why this stops

Same upstream GCC 15.2.0, different Ubuntu package revisions, different bits. The box suite at `a2a2a24e` passed the existing Linux pins (GCC 1468 passed, Clang ASan 1433 passed, `verify-linux.sh` exit 0, `linux gates: MET`). CI run [40067942125](https://github.com/guitarlum/VoLum/actions/runs/40067942125) on `7f0a3d4d` (job 120795257046) compiled with `g++-15 (Ubuntu 15.2.0-15ubuntu1~22~ppa2)` and failed 3 cases / 6 assertions. No Linux hash was edited. The Windows and macOS literals were not edited. The tone-stack ratio threshold was not widened.

Hashes that differ (expected is the pin from the box's GCC 15.2.0-16ubuntu1):

| Case | Box pin | ubuntu-22.04 g++-15 actual |
| --- | --- | --- |
| oktaverb-halo | `1daf2177d39c99feebef9eb7229c8b340c2fe55647874a2fe647d4d6bcfdd444` | `732b960b94b03b725fcfcab6bb5fefcd4870293a147e4870d16d6484cae3afbe` |
| oktaverb-shimmer | `ad15f8ba5903d4f24850cf67d25c91dc39e18d4fed24a4f7a1c2c1d5e1ec5c0a` | `e683aaaca08266c38d427594a6ec6894f5fd039b4977e6e31d02243343b3dd05` |
| oktaverb-bloom | `d91f61f60ebf77fa633df8dc8d8b97a98bcff44db4a6d08929c3a4765de23cf3` | `c10c2db635c53140982bd6c91915002e3296b9a58dfa2d962af2e5c36642fdf5` |
| octaver vintage 48k/333 | `7fc77b2e59d4f02c6187ee5a926e7bf32c9b329653fcd98d4df5fdc20900d006` | `994060e37d162e77658677a5367f7d1b65a8cc4613adc38e1aa50cfa1c7f8fb8` |
| mode switches | `1c3ecb667240f5a6242a232c54a7dd5a2ad64031859603c843e87c1f7e489c65` | `c0089ee0c700aaadf5bed4f7f863993b1ecbcba79d76d355e6e6fc30c7b0deda` |

The other golden DSP groups and the other pitch pins matched. The Windows null checks in those cases did not fail; the six failures are the five hashes above plus one timing ratio.

`test_volum_filter_core.cpp` tone stack, 64 frames, on the CI runner: fixed kernels 1.08648 us, any-degree loop 2.14611 us, best of 41 rounds, ratio 0.506255. The check is `ratio <= 0.5`. The box run passed that check. The threshold was left at 0.5.

Re-pinning the Linux hashes to the CI compiler would make the box fail, and the reverse is what this run already shows. Docker was not used: both machines already have GCC 15.

## Job

`linux-tests` in `.github/workflows/ci.yml`, `runs-on: ubuntu-22.04`, `name: Linux tests`. Skipped when `workflow_dispatch` sets `windows_only`. Steps: checkout (`submodules: false`), `checkout-submodules.sh`, `download-vst3-sdk.sh master`, `provision-linux.sh --ci`, GCC doctest, Clang ASan/UBSan. No existing job's steps were edited.

`provision-linux.sh` on 22.04 and 24.04 always enables `ppa:ubuntu-toolchain-r/test` and apt.llvm.org clang-21, and installs `x11-utils` (`xdpyinfo` is not in `x11-apps` on 22.04). 26.04 keeps its archive compilers.

## CI runs

| Run | Head | Linux tests |
| --- | --- | --- |
| [40066792112](https://github.com/guitarlum/VoLum/actions/runs/40066792112) | `a2a2a24e` | job 120791640885 failure. Apt cache looked like it had the compilers, then `apt-get update` dropped them. `Unable to locate package gcc-15`. |
| [40067138324](https://github.com/guitarlum/VoLum/actions/runs/40067138324) | `ef4fade6` | job 120792921805 failure. `g++-15` had a candidate and `gcc-15` did not, so the toolchain archive was skipped. |
| [40067591512](https://github.com/guitarlum/VoLum/actions/runs/40067591512) | `d76fbf3f` | job 120794213821 failure. GCC 15.2.0-15ubuntu1~22~ppa2 and Clang 21.1.8 installed. `missing command after install: xdpyinfo`. |
| [40067942125](https://github.com/guitarlum/VoLum/actions/runs/40067942125) | `7f0a3d4d` | job 120795257046 failure. Provision succeeded. GCC doctest: 1468 cases, 1465 passed, 3 failed. Hashes and the ratio above. Sanitizer step skipped. |

There was no green run on the first toolchain. D1 below is the green one.

## D1

Linux exact-bit pins bind to the CI `Linux tests` job. The box keeps its native compilers. A different glibc or `__VERSION__` skips the exact compare and still runs the Windows null check.

### Toolchain

Archive gcc-12 on ubuntu-22.04 does not build. The exact error, from run [40069511486](https://github.com/guitarlum/VoLum/actions/runs/40069511486) job 120799811519:

```
AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h:89: error: declaration of
'using LanczosResampler = class dsp::LanczosResampler<T, NCHANS, A>' changes meaning
of 'LanczosResampler' [-fpermissive]
```

CI therefore keeps `g++-15` from `ppa:ubuntu-toolchain-r/test` (15.2.0-15ubuntu1~22~ppa2) and `clang++-21` from apt.llvm.org (21.1.8). Those sources are added only on Ubuntu 22.04. The box had none of the files this script used to add (`ubuntu-toolchain-r-ubuntu-test-*.list`, `llvm-toolchain-*-21.list`, `apt.llvm.org.asc`); `provision-linux.sh` still removes only those paths if they appear.

Reference identity, from the passing GCC run: `glibc 2.35 | 15.2.0`.

Box GCC run: `glibc 2.43 | 15.2.0`. Box Clang run: `glibc 2.43 | Ubuntu Clang 21.1.8 (6ubuntu1)`. Both skipped the exact compare. `verify-linux.sh` exit 0 at `d22da417` (GCC 1468 passed, Clang ASan 1433 passed).

CI sanitizer identity: `glibc 2.35 | Ubuntu Clang 21.1.8 (++20251221032842+2078da43e25a-1~exp1~20251221153008.77)`. It does not set `VOLUM_REQUIRE_LINUX_HASH_REFERENCE`. 22 mismatch messages (11 NAM + 11 pitch), then the null residuals, then 1433 passed. Golden cases stay excluded by `Golden *`.

### Re-pinned from run 40069948079

GCC `g++-15 (Ubuntu 15.2.0-15ubuntu1~22~ppa2)`, identity matched `glibc 2.35 | 15.2.0`. Five pins moved. The rest already matched this compiler and were left as they were. NAM Linux pins are the same bytes as the Windows pins; every NAM residual in that run was `-inf`.

| Case | New pin | Residual dBFS |
| --- | --- | --- |
| oktaverb-halo | `732b960b94b03b725fcfcab6bb5fefcd4870293a147e4870d16d6484cae3afbe` | -290.444 |
| oktaverb-shimmer | `e683aaaca08266c38d427594a6ec6894f5fd039b4977e6e31d02243343b3dd05` | -286.082 |
| oktaverb-bloom | `c10c2db635c53140982bd6c91915002e3296b9a58dfa2d962af2e5c36642fdf5` | -299.368 |
| octaver vintage 48k/333 | `994060e37d162e77658677a5367f7d1b65a8cc4613adc38e1aa50cfa1c7f8fb8` | -242.995 |
| mode switches | `c0089ee0c700aaadf5bed4f7f863993b1ecbcba79d76d355e6e6fc30c7b0deda` | -243.426 |

All are under -100. The threshold was not changed.

### Tone stack

`ratio <= 0.5` was not widened. On CI gcc-15 the any-degree loop and the fixed kernels measured:

| Run | Rounds | Fixed us | Generic us | Ratio |
| --- | --- | --- | --- | --- |
| 40067942125 (`_ProcessGeneric`) | 41 | 1.08648 | 2.14611 | 0.506255 |
| 40069948079 (`_ProcessGeneric`) | 41 | 1.07984 | 2.14975 | 0.502312 |
| 40071230436 (inline copy; the attribute was optimized away) | 81 | 1.07992 | 2.14744 | 0.502889 |

An inline method with `no-tree-vectorize,no-unroll-loops` compiled to the same speed, so GCC was unrolling that degree-3 loop back into the caller. The baseline is now `RunScalarAnyDegree`, a `noinline` function with the same attribute. Run [40072097506](https://github.com/guitarlum/VoLum/actions/runs/40072097506) job 120807816637: fixed 0.968924 us, scalar loop 2.75873 us, ratio **0.351221**. That is under 0.48. Bit-exact tests still call `_ProcessGeneric`.

### Red proofs

On the box binary at `d22da417`, `VOLUM_LINUX_HASH_IDENTITY_FAKE=not-the-reference`, case `PRE effects and tone stack`:

- Without the require env: exit 0. Mismatch messages for compressor, pre-eq, and tone-stack. Residuals -315.57, -271.107, -273.225. No hash `CHECK` failure.
- With `VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1`: exit 1. `ERROR: VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1 but this build is not the Linux hash reference`. Residuals still printed.

Forcing the fake identity to `glibc 2.35 | 15.2.0` makes the box compare against the CI pins. Oktaverb halo/shimmer/bloom then fail `CHECK(actual == expected)` (box `1daf2177…` vs pin `732b960b…`, and the two siblings). Exit 1. Residuals still under -100. That is the same `CHECK` the CI reference build uses.

### Green run

[40072097506](https://github.com/guitarlum/VoLum/actions/runs/40072097506) on `d22da417`. Linux tests job 120807816637 success. GCC 1468 passed. Clang ASan 1433 passed. `run-tests-win.ps1` exit 0 in the worktree after these test edits.

## merge-verified refusal

Local `dev` is ahead of origin, so `-Into dev` stops before the Linux check. The proof uses `-Into feature/linux-support` (that ref matches origin) and `-DryRun`, with `gh` replaced by a stub. The stub returned a completed successful run whose `Linux tests` job has conclusion `failure` (databaseId 424242, head `a2a2a24e`).

```
REFUSED: CI run 424242 has a Linux job that is not green (Linux tests=failure). Dispatch the full matrix, or pass -AllowWindowsOnly if Linux cannot be affected.
```

Exit code 1. Nothing was merged.
