# 02 Headless tests

Status: pass. Ticket not marked resolved.

Branch `feature/linux-02-headless`. GCC and Clang ASan/UBSan each exited 0 at `766d1b3a`. `verify-linux.sh` repeats those two commands on the commit that adds this report. The report does not change the suite.

## Result

| Run | Result |
| --- | --- |
| `bash NeuralAmpModeler/scripts/run-tests-linux.sh` (GCC 15.2 Release) | `[doctest] test cases: 1468 \| 1468 passed \| 0 failed \| 1 skipped` / `[doctest] assertions: 30251251 \| 30251251 passed \| 0 failed` / `Status: SUCCESS!` |
| `bash NeuralAmpModeler/scripts/run-tests-linux.sh --clang --sanitize` (Clang 21 Debug ASan+UBSan) | `[doctest] test cases: 1433 \| 1433 passed \| 0 failed \| 36 skipped` / `[doctest] assertions: 29892771 \| 29892771 passed \| 0 failed` / `Status: SUCCESS!` |
| `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1` in the worktree | `[doctest] test cases: 1470 \| 1470 passed \| 0 failed \| 1 skipped` / `[doctest] assertions: 30255167 \| 30255167 passed \| 0 failed` / `Status: SUCCESS!` |

`verify-linux.sh` is those two Linux commands, then each later gate if the file exists. None of those gate scripts exist yet, so without `--final` they are skipped and the script prints `linux gates: MET`.

Sanitizer exclusions are read from `run-tests-linux.sh`'s copy of the single `--test-case-exclude` string in `run-tests-mac.sh`. They are not a second list.

## Initial failures

1. GCC 15: `WDL/heapbuf.h` calls `malloc`/`free` without `<cstdlib>` in view. iPlug2 was not edited. The test target compiles with `-include cstdlib`.
2. Link: `__atomic_store` / `__atomic_load` from the tuner. Linked `libatomic`.
3. Linux golden pins were empty on purpose so the first run printed the hashes. NAM Release hashes matched the MSVC pins. Pitch and golden DSP did not.
4. `Tuner analysis blocks cost a fraction of the scalar loop`: ratio 0.597 (analysis 228 us, scalar reference 382 us). GCC `-O3` had vectorized `ReferenceDifference`. The function is marked `no-tree-vectorize` for GCC only. The absolute deadline already passed (worst block 37 us).
5. Clang link: `libclang_rt.asan*.a` missing.
6. ASan `alloc-dealloc-mismatch` (`operator new` vs `free`) inside libstdc++ `std::stable_sort` / `get_temporary_buffer`, from `SelectPrefetchPaths` (`VoLumNamDspData.h`). libc++ on the macOS sanitizer job does not do this. The Linux sanitizer run sets `ASAN_OPTIONS=alloc_dealloc_mismatch=0`. The macOS exclusion list is unchanged.

`test_volum_golden_render` passed the existing tolerances on GCC Release. No Linux fingerprint set was added and no tolerance was widened.

## Fixes

- `NeuralAmpModeler/scripts/run-tests-linux.sh`: iPlug2 patches, ported parity / exec-bit / golden-changelog checks, VST3 SDK headers (no validator build), Ninja, GCC Release or Clang, `-j 14`, macOS sanitizer exclusions.
- `NeuralAmpModeler/scripts/linux/verify-linux.sh`: GCC suite, then Clang ASan/UBSan, then later gates. `--final` fails if a gate file is missing. Stops at the first failure.
- Linux branch on `PlatformExpected`. Windows and macOS hash literals are unchanged; a following argument was added.
- Linux pins for NAM (same bytes as Windows) and for pitch and golden DSP (GCC Release). Clang ASan Debug matched those pins, so there is no separate sanitized hash.
- `tests/golden/platform-null/*.wav`: Windows Release captures (IEEE float64). Linux Release requires peak `|live-windows|` <= -100 dBFS (0 dBFS = amplitude 1). Windows Release requires the live render to match the file.
- `VoLumHttpGet.cpp` is linked on non-Apple. On Linux it still returns false.

Skipped guards, ported in spirit or left to the Windows runner:

- `check-test-source-parity.ps1`, `check-shell-exec-bits.ps1`, `check-golden-changelog.ps1`: ported into `run-tests-linux.sh` (no PowerShell on the box).
- `check-agent-artifact-links.ps1`: PowerShell-only; `run-tests-mac.sh` does not run it.
- `check-local-guards.ps1`: the owner's Windows hooks and untracked denylist. CI no-ops it.
- `check-no-vendor-refs.ps1`: fail-closed without that denylist. CI skips the scan when the file is absent.

Prebuilt iPlug libs and the WAM SDK are not fetched. `run-tests-mac.sh` does not fetch them either; macOS CI fetches them for the package step in the same job.

## Peak residual vs the Windows captures (dBFS)

Threshold -100. More negative is closer. `-inf` is bit-identical.

| Group | Worst case | dBFS |
| --- | --- | --- |
| Golden DSP | pre-eq | -271.107 |
| Golden DSP | compressor | -315.57 |
| Golden DSP | tone-stack | -273.225 |
| Golden DSP | delay-digital, delay-analog, delay-reverse, delay-digital-same-lr, delay-analog-same-lr | -325.112 |
| Golden DSP | reverb-hall | -289.695 |
| Golden DSP | reverb-plate | -289.353 |
| Golden DSP | oktaverb-halo | -290.444 |
| Golden DSP | oktaverb-shimmer | -286.082 |
| Golden DSP | oktaverb-bloom | -299.368 |
| NAM exact | all 10 cases | -inf |
| Pitch | octaver modern 48k/64 (worst in the group) | -236.237 |
| Pitch | octaver vintage 48k/333 | -242.995 |
| Pitch | transpose instant -5 48k/64 | -243.426 |
| Pitch | transpose instant +7 48k/64 | -239.424 |
| Pitch | transpose drop -12 48k/128 | -244.385 |
| Pitch | transpose drop +7 48k/128 | -243.862 |
| Pitch | transpose poly -12 48k/64 | -319.092 |
| Pitch | transpose poly +7 48k/64 | -315.57 |
| Pitch | octaver modern 44.1k/96 | -253.024 |
| Pitch | transpose instant -7 96k/256 | -247.072 |
| Pitch | mode switches | -243.426 |

Loudest residual in the whole null set: **octaver modern 48k/64, -236.237 dBFS**. No escalation.

Golden render groups were not given a Linux set. They matched the committed fingerprints inside the existing tolerances.

## Red proof

On the box, the compressor Linux hash was replaced with 64 zeros and `kWindowsNullPeakDb` was set to -1000. `Golden DSP: PRE effects and tone stack hashes stay stable` then failed both ways:

- `CHECK( e29547f103c809afa4ea1be2b80194db010f18aa62ac9ad2d94e40ad7c86489e == 0000…0000 )`
- `CHECK( -315.57 <= -1000 )` and the same for pre-eq (-271.107) and tone-stack (-273.225)

Sources were restored and the GCC binary rebuilt. Exit of that doctest run was 1, as required.

## Missing apt packages

Not in `provision-linux.sh` (out of scope). Installed on the box for this ticket:

- `libclang-rt-21-dev` (pulls `libc6-i386`, `lib32gcc-s1`, `lib32stdc++6`)

Needed so Clang 21 can link ASan/UBSan (`libclang_rt.asan.a` and friends).
