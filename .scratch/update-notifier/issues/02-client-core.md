# Update client core (S2)

Status: ready-for-agent
Blocked by: 01

## Goal

Pure check logic + sidecar + HTTP wrappers. No UI.

## Do this

F14 file list. WinHTTP with `#pragma comment(lib, "winhttp.lib")` (do not edit AdditionalDependencies across vcxproj configs). NSURLSession `.mm` on APP+VST3+AU Xcode targets. `VolumUpdateStateFilePath()` in `VoLumPaths.h`. `autoCheck` default on. `lastSeenVersion` updates only from the UI ticket's row/Check now — the core should expose `MarkSeen` separately from `OnSettingsOpened`.

## Tests (must fail with this ticket reverted)

`test_volum_update_check.cpp` as in the spec. Sidecar atomic write test. Register CMake + vcxproj (`check-test-source-parity.ps1`).

## Done when

Windows tests green. Revert-fail proven. HTTP not linked into the test target.
