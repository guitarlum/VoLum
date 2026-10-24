Status: ready-for-agent
Blocked by: 02

# 03 VoLum platform code for Linux

## Scope

May write:
- `NeuralAmpModeler/VoLumPaths.h`
- `NeuralAmpModeler/VoLumHttpGet*.{h,cpp}`
- new `NeuralAmpModeler/VoLumHttpGetLinux.cpp`
- `NeuralAmpModeler/tests/test_volum_paths.cpp`
- new `tests/test_volum_http_get_linux.cpp`
- `NeuralAmpModeler/tests/CMakeLists.txt`

## Work

- **`VoLumPaths.h`:** Linux branches for every user path and for rigs discovery,
  per spec.md Paths.
  - Use `dladdr` for the module path and `/proc/self/exe` for the app.
  - Honour `XDG_DATA_HOME`.
- **Update check:** libcurl via `dlopen("libcurl.so.4")`, with timeouts that match
  WinHTTP and NSURLSession. If the library is missing, return false and do not
  crash.

## Acceptance

- New Linux doctest cases:
  - every path function returns a non-empty path under `$XDG_DATA_HOME/VoLum`;
  - rigs discovery honours the search order (temp dirs);
  - the HTTP getter works against a local `python3 -m http.server`;
  - the HTTP getter returns false with `LD_PRELOAD`/env forcing a missing curl.
- Each new case is proven red with its branch reverted.
- `run-tests-linux.sh` and `run-tests-win.ps1` exit 0.
