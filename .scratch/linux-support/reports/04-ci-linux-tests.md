# 04 Linux doctest CI

Status: pass under D1 on archive gcc-12 / clang-15. Ticket not marked resolved.

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

That paragraph described the first toolchain. D1 below is what the branch does now: ubuntu-22.04 installs archive `gcc-12` and `clang-15` and does not add a PPA or apt.llvm.org. `x11-utils` stays (`xdpyinfo` is not in `x11-apps` on 22.04). 24.04 and 26.04 keep their distro compilers.

## CI runs

| Run | Head | Linux tests |
| --- | --- | --- |
| [40066792112](https://github.com/guitarlum/VoLum/actions/runs/40066792112) | `a2a2a24e` | job 120791640885 failure. Apt cache looked like it had the compilers, then `apt-get update` dropped them. `Unable to locate package gcc-15`. |
| [40067138324](https://github.com/guitarlum/VoLum/actions/runs/40067138324) | `ef4fade6` | job 120792921805 failure. `g++-15` had a candidate and `gcc-15` did not, so the toolchain archive was skipped. |
| [40067591512](https://github.com/guitarlum/VoLum/actions/runs/40067591512) | `d76fbf3f` | job 120794213821 failure. GCC 15.2.0-15ubuntu1~22~ppa2 and Clang 21.1.8 installed. `missing command after install: xdpyinfo`. |
| [40067942125](https://github.com/guitarlum/VoLum/actions/runs/40067942125) | `7f0a3d4d` | job 120795257046 failure. Provision succeeded. GCC doctest: 1468 cases, 1465 passed, 3 failed. Hashes and the ratio above. Sanitizer step skipped. |

There was no green run on the first toolchain. D1 below is the green one.

## D1

Linux exact-bit pins bind to the CI `Linux tests` job on ubuntu-22.04. The box keeps its native compilers. A different glibc or `__VERSION__` skips the exact compare and still runs the Windows null check.

### Toolchain

Archive gcc-12 failed first. Run [40069511486](https://github.com/guitarlum/VoLum/actions/runs/40069511486) job 120799811519:

```
AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h:89: error: declaration of
'using LanczosResampler = class dsp::LanczosResampler<T, NCHANS, A>' changes meaning
of 'LanczosResampler' [-fpermissive]
```

AudioDSPTools `f2cfd01` qualifies that alias. Archive gcc-12 then builds. The reference compiler is `g++-12 (Ubuntu 12.3.0-1ubuntu1~22.04.3) 12.3.0`. The sanitizer compiler is archive `clang++-15` (Ubuntu clang version 15.0.7). No toolchain PPA and no apt.llvm.org list are added. `provision-linux.sh` still deletes only `ubuntu-toolchain-r-ubuntu-test-*.list`, `llvm-toolchain-*-21.list`, and `apt.llvm.org.asc` / `.gpg` if a previous run left them. The box had none.

Clang 15.0.7 rejected one test: `test_volum_ui_regressions.cpp` `CAPTURE(name)` on a structured binding (doctest wraps the argument in a lambda). A commit briefly switched the sanitizer to clang-21 from apt.llvm.org for that error. The loop now names the map entry, archive clang-15 compiles the suite, and the llvm.org repo is not used.

Reference identity, one string in `platform_null.h`: `glibc 2.35 | 12.3.0`. GCC runs print `runtime="glibc 2.35 | 12.3.0" reference="glibc 2.35 | 12.3.0"`. The GCC step sets `VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1`. The sanitizer step does not.

### Pins on the archive compiler

gcc-12 reproduced every Linux pin already in the tree, including the five that had moved off the box's gcc-15. Nothing was re-pinned from the box. NAM Linux pins stay the same bytes as the Windows pins. Residuals below are from the gcc-12 run [40082631113](https://github.com/guitarlum/VoLum/actions/runs/40082631113) (GCC step success, identity matched). The same hashes and the same residuals printed again on [40085419768](https://github.com/guitarlum/VoLum/actions/runs/40085419768).

| Case | Pin (unchanged) | Residual dBFS |
| --- | --- | --- |
| oktaverb-halo | `732b960b94b03b725fcfcab6bb5fefcd4870293a147e4870d16d6484cae3afbe` | -290.444 |
| oktaverb-shimmer | `e683aaaca08266c38d427594a6ec6894f5fd039b4977e6e31d02243343b3dd05` | -286.082 |
| oktaverb-bloom | `c10c2db635c53140982bd6c91915002e3296b9a58dfa2d962af2e5c36642fdf5` | -299.368 |
| octaver vintage 48k/333 | `994060e37d162e77658677a5367f7d1b65a8cc4613adc38e1aa50cfa1c7f8fb8` | -242.995 |
| mode switches | `c0089ee0c700aaadf5bed4f7f863993b1ecbcba79d76d355e6e6fc30c7b0deda` | -243.426 |

Every NAM residual in that GCC step was `-inf`. All of the residuals above are under -100. The threshold was not changed.

### Tone stack

`ratio <= 0.5` was not widened. The earlier gcc-15 runs sat just over the line because the fixed kernel's ring wrap (`if (index < 0)`) is taken every third sample. GCC lays that out as a jump the fixed kernel pays and the any-degree loop does not, so the two paths measure almost exactly 2:1:

| Run | What was timed | Fixed us | Generic us | Ratio |
| --- | --- | --- | --- | --- |
| 40067942125 | `_ProcessGeneric`, 41 rounds | 1.08648 | 2.14611 | 0.506255 |
| 40069948079 | `_ProcessGeneric`, 41 | 1.07984 | 2.14975 | 0.502312 |
| 40071230436 | inline copy, attribute optimized away, 81 | 1.07992 | 2.14744 | 0.502889 |
| 40072097506 | `noinline` scalar copy | 0.968924 | 2.75873 | 0.351221 |
| 40078121038 | same `noinline` binary, other runner | 1.07906 | 2.14658 | 0.502688 |

`no-tree-vectorize` on an inline method did nothing (the ratio stayed 0.502). `noinline` moved one runner to 0.351 and the next runner, same bits, back to 0.503. That is a CPU difference, not a missing vectorizer. The degree-3 body is already scalar.

The fix is in the fixed kernel, not the threshold. `StepBack<Degree>` steps the ring with the sign bit (`prev + ((prev >> (width-1)) & Degree)`), which is the same index sequence as the branch (0 wraps to `Degree-1`). Windows doctest on that change: 1470 passed, 0 failed (`run-tests-win.ps1` exit 0). On archive gcc-12 the ratio is no longer near 0.5:

| Run | Head | Fixed us | Generic us | Ratio |
| --- | --- | --- | --- | --- |
| [40082631113](https://github.com/guitarlum/VoLum/actions/runs/40082631113) | `3736506e` | 0.578342 | 2.14685 | 0.26939 |
| [40085419768](https://github.com/guitarlum/VoLum/actions/runs/40085419768) attempt 1 | `a17f9a98` | 0.575636 | 2.16315 | 0.266111 |
| [40085419768](https://github.com/guitarlum/VoLum/actions/runs/40085419768) attempt 2 | `a17f9a98` | 0.576854 | 2.14808 | 0.268544 |

All three archive-compiler ratios are under 0.48 (0.26939, 0.266111, 0.268544). The threshold stayed 0.5. No further hardening.

### Sanitizer

Run 40085419768, clang 15.0.7, no `VOLUM_REQUIRE_LINUX_HASH_REFERENCE`. Identity `glibc 2.35 | Ubuntu Clang 15.0.7` does not match `glibc 2.35 | 12.3.0`. 22 mismatch messages (11 NAM + 11 pitch). The exact compare is skipped. The null check still runs: NAM residuals `-inf`, octaver vintage `-242.995`, mode switches `-243.426`. 1444 passed, 0 failed, 36 skipped. `Golden *` stays excluded. The log has no `VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1 but` failure.

### Red proofs

On the box GCC binary at `a17f9a98` (identity would be `glibc 2.43 | 15.2.0`), case `PRE effects and tone stack`:

- `VOLUM_LINUX_HASH_IDENTITY_FAKE=not-the-reference`: exit 0. Mismatch messages name runtime `not-the-reference` and reference `glibc 2.35 | 12.3.0`. Residuals -315.57, -271.107, -273.225. No hash `CHECK` failure.
- Same fake identity plus `VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1`: exit 1. `ERROR: VOLUM_REQUIRE_LINUX_HASH_REFERENCE=1 but this build is not the Linux hash reference`. Residuals still printed.

Forcing the fake identity to `glibc 2.35 | 12.3.0` makes the box take the CI compare. Oktaverb halo/shimmer/bloom fail `CHECK(actual == expected)` (box `1daf2177…` vs pin `732b960b…`, and the two siblings). Exit 1. Residuals still under -100. That is the same `CHECK` the CI GCC step runs when the identity matches. On CI, run 40085419768 matched `glibc 2.35 | 12.3.0` and every pin, with the require env set.

### Box

`verify-linux.sh` exit 0 at `a17f9a98`. The box is not the reference: GCC `glibc 2.43 | 15.2.0`, Clang `glibc 2.43 | Ubuntu Clang 21.1.8 (6ubuntu1)`, both against `glibc 2.35 | 12.3.0`. Exact hashes were skipped. Null checks still ran. GCC 1479 passed, Clang ASan 1444 passed, `linux gates: MET`.

### Green run

[40085419768](https://github.com/guitarlum/VoLum/actions/runs/40085419768) on `a17f9a98`, attempt 1 every job success, attempt 2 reran `Linux tests` (job 120858807927) and the run stayed success. GCC 1479 passed. Clang ASan 1444 passed, hashes skipped, nulls checked. `run-tests-win.ps1` exit 0 in the worktree (1470 passed). This report commit does not change that code.

## merge-verified refusal

Local `dev` is ahead of origin, so `-Into dev` stops before the Linux check. The proof uses `-Into feature/linux-support` (that ref matches origin) and `-DryRun`, with `gh` replaced by a stub. The stub returned a completed successful run whose `Linux tests` job has conclusion `failure` (databaseId 424242, head `a2a2a24e`).

```
REFUSED: CI run 424242 has a Linux job that is not green (Linux tests=failure). Dispatch the full matrix, or pass -AllowWindowsOnly if Linux cannot be affected.
```

Exit code 1. Nothing was merged.
