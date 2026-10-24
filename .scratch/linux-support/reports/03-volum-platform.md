# 03 VoLum platform code for Linux

Status: pass. Ticket not marked resolved.

Branch `feature/linux-03-platform`. Code head for the Linux gates below is `a468a56b` (`becd103d` plus the readelf fix). Windows and macOS function bodies are unchanged: `VoLumHttpGet.cpp` only turns the non-Windows stub into `#elif !defined(__linux__)`, and `VoLumPaths.h` only adds `__linux__` branches. No golden hash or tolerance was edited.

## Design

User files (`volum-settings.json`, `volum-update-state.json`, `volum-dual-amp-settings.json`, `volum.log`, `content/`) live under `$XDG_DATA_HOME/VoLum` when that variable is an absolute path. An empty or relative value is ignored. The fallback is `$HOME/.local/share/VoLum`. Both empty means the path functions return empty.

Rigs search order, then the existing CWD `VoLumRigs` and `rigs` candidates every platform already appends:

1. `VoLumRigs` beside the module file.
2. `Contents/Resources/VoLumRigs`. The VST3 `.so` is `VoLum.vst3/Contents/x86_64-linux/VoLum.so`, so Resources is the parent of that directory.
3. `$XDG_DATA_HOME/VoLum/VoLumRigs` (or the HOME fallback).
4. `/usr/share/VoLum/VoLumRigs`.

The module file is `dladdr` on `LinuxModuleFile`. When that file is the same as `/proc/self/exe` (the standalone app), the readlink path is used. When it is a different file (the VST3 `.so`, not the host), the `dladdr` path is used.

`VolumHttpGetString` on Linux is `NeuralAmpModeler/VoLumHttpGetLinux.cpp`. It `dlopen`s `libcurl.so.4` and calls curl through `dlsym` function pointers. No curl header is included. `curl_global_init(CURL_GLOBAL_DEFAULT)` runs once under `std::call_once`. Each call uses its own easy handle. `CURLOPT_NOSIGNAL` is set. If the library or a symbol is missing, the function returns false and does not crash.

`VOLUM_TEST_CURL_SONAME`, when set and non-empty, is the soname passed to `dlopen` instead of `libcurl.so.4`. It is test-only and is honored in Release, because the Linux GCC suite is Release. Production leaves it unset.

## Timeouts and the return contract

The update check calls this with `kVolumUpdateTimeoutMs` (3500). Linux uses that same integer for every knob.

| Knob | WinHTTP | NSURLSession | Linux libcurl |
| --- | --- | --- | --- |
| Resolve | `timeoutMs` (`WinHttpSetTimeouts` arg 1) | inside the request timeout | inside `CURLOPT_TIMEOUT_MS` |
| Connect | `timeoutMs` | inside the request timeout | `CURLOPT_CONNECTTIMEOUT_MS = timeoutMs` |
| Send | `timeoutMs` | `timeoutIntervalForRequest = timeoutMs/1000` | inside `CURLOPT_TIMEOUT_MS` |
| Receive | `timeoutMs` | `timeoutIntervalForRequest` | inside `CURLOPT_TIMEOUT_MS` |
| Whole call | the four phases are separate, so the worst case is their sum | resource timeout `timeoutMs/1000` and a semaphore wait of `timeoutMs` | `CURLOPT_TIMEOUT_MS = timeoutMs`, the macOS whole-call cap |
| Redirects | automatic, max 10, HTTPS to HTTP refused | automatic | `FOLLOWLOCATION`, `MAXREDIRS` 10, an `https` URL may redirect only to `https` |
| Body cap | 1 MiB; a too-large chunk returns false and can leave a partial `out` | 1 MiB; failure leaves `out` empty | 1 MiB; failure leaves `out` empty |
| Status | 200 | 200 | 200 |
| User-Agent | `VoLum update notifier` | not set (session default) | `VoLum update notifier` |
| TLS | Schannel verification on | system verification on | `SSL_VERIFYPEER` 1, `SSL_VERIFYHOST` 2 |
| Scheme | https only | https only | http and https |

Linux accepts `http://` so the doctest can use `python3 -m http.server`. The production appcast URL stays `https://`. TLS verification is not disabled for https.

## Tests

All eleven cases are under `__linux__`. On Windows and macOS `test_volum_http_get_linux.cpp` compiles to no tests. It is listed in `tests/CMakeLists.txt` and `NeuralAmpModeler-Tests.vcxproj` because the parity check requires both.

- `Linux user paths sit under XDG_DATA_HOME/VoLum`
- `Linux user paths fall back to HOME/.local/share when XDG_DATA_HOME is unset or relative`
- `Linux module path uses dladdr for a plugin .so and /proc/self/exe for the app`
- `Linux rigs search order is beside the binary, bundle Resources, XDG, then /usr/share`
- `Linux HTTP get rejects a non-http URL`
- `Linux HTTP get reads a local python3 http.server body`
- `Linux HTTP get returns false when VOLUM_TEST_CURL_SONAME cannot be loaded`
- `Linux HTTP get sends the update-notifier user agent and follows a redirect`
- `Linux HTTP get gives up at its timeout`
- `Linux HTTP get rejects a self-signed certificate`
- `Linux test binary does not DT_NEEDED libcurl`

`python3` missing skips the HTTP cases with `MESSAGE`. On the box, python3, openssl, and readelf were present.

## Red proofs

Each row is the doctest line after that branch was reverted, then the sources were restored. The rigs filter must not contain a comma: doctest splits filters on commas, and the first attempt skipped the case (`0` tests, exit 0). The rerun used `*Linux rigs*`.

| Case | Revert | Failing check |
| --- | --- | --- |
| XDG paths | `LinuxVoLumDataDir` returns `{}` | `CHECK_FALSE( settings.empty() )` with `CHECK_FALSE( true )`, and `CHECK( "" == ".../VoLum/volum-settings.json" )` |
| HOME fallback | a relative `XDG_DATA_HOME` is accepted | `CHECK( "relative/not-absolute/VoLum/volum-dual-amp-settings.json" == ".../.local/share/VoLum/volum-dual-amp-settings.json" )` |
| Module path | always return `/proc/self/exe` when it is non-empty | `CHECK( ".../VoLum" == ".../VoLum.so" )` |
| Rigs order | `LinuxRigsCandidates` returns `{}` | `REQUIRE( 0 == 4 )` |
| python http.server | `VolumHttpGetString` returns false immediately | `CHECK( false )` on `VolumHttpGetString(.../hello.txt...)` |
| Missing curl | ignore `VOLUM_TEST_CURL_SONAME` | `CHECK_FALSE( true )` (the real libcurl still fetched the body) |
| User-Agent | send `not-volum` | `CHECK( not-volum == VoLum update notifier )` |
| Timeout | `CURLOPT_TIMEOUT_MS` set to 0 (no cap) | `CHECK( 30003701718ns < 3s )` |
| TLS | `SSL_VERIFYPEER` 0 and `SSL_VERIFYHOST` 0 | `CHECK_FALSE( true )` and the server logged `GET /hello.txt` 200 |
| DT_NEEDED | link `-Wl,--no-as-needed -l:libcurl.so.4` | `CHECK( 242 == 18446744073709551615 )` (`libcurl` found in `readelf -d`) |

`Linux HTTP get rejects a non-http URL` stayed green when the scheme gate and the protocol mask were both removed. This libcurl still refuses `file:///etc/passwd`, so that CHECK does not go red on its own. The http.server case is the one that goes red when the getter is stubbed out.

The first DT_NEEDED attempt linked `-l:libcurl.so.4` without `--no-as-needed`. The dynamic linker recorded `libcurl.so.4`, but the test inspected `/proc/self/exe` inside `popen`, which is the shell. `a468a56b` readelfs the test binary path from `readlink` instead. After that fix, the `--no-as-needed` link fails the CHECK, and the restored binary's `readelf -d` NEEDED list is `libatomic.so.1`, `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libc.so.6`. No `libcurl`.

## Box

`verify-linux.sh` on `becd103d` (before the readelf fix), then again on `a468a56b`:

`a468a56b` GCC Release: `[doctest] test cases: 1479 | 1479 passed | 0 failed | 1 skipped` / `assertions: 30251348` / `Status: SUCCESS!`

`a468a56b` Clang ASan+UBSan: `[doctest] test cases: 1444 | 1444 passed | 0 failed | 36 skipped` / `assertions: 29892868` / `Status: SUCCESS!` / `linux gates: MET`

`becd103d` (the same suite before the readelf fix) was GCC `1479` passed / `30251347` assertions and Clang `1444` passed / `29892867` assertions, also `linux gates: MET`. The extra assertion is the `readlink` check in the DT_NEEDED test.

Targeted GCC run of the eleven cases on `becd103d`, before the full suite: `[doctest] test cases: 11 | 11 passed | 0 failed | 1469 skipped` / `assertions: 82`.

## Laptop

`pwsh -NoProfile -File NeuralAmpModeler\scripts\run-tests-win.ps1` in the worktree, exit 0:

`[doctest] test cases: 1470 | 1470 passed | 0 failed | 1 skipped` / `assertions: 30255181` / `Status: SUCCESS!`

The new file adds no Windows cases. Case count matches the ticket 02 Windows run.

## CI

`ci-watch.ps1 -Ref feature/linux-03-platform -Dispatch` on `a468a56b`:

https://github.com/guitarlum/VoLum/actions/runs/40070382969

Windows and macOS jobs were in progress when this report was written. The worker waits for them.

## Ticket 06

The Linux plugin and app CMake must compile `NeuralAmpModeler/VoLumHttpGetLinux.cpp`. `VoLumHttpGet.cpp` still compiles on Linux and defines no `VolumHttpGetString` there. Link `${CMAKE_DL_LIBS}` so `dlopen` resolves. Do not link `libcurl` and do not add `-Wl,--no-as-needed` for it. `readelf -d` on the `.so` and the standalone must not list `libcurl.so.4`. At runtime, a missing `libcurl.so.4` makes the update check return false.
