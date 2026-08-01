# iPlug2 patches

VoLum now uses a private `guitarlum/iPlug2` mirror for local iPlug2 changes.
The submodule should point at a pinned commit in that mirror, so normal
build/test runs should not leave `iPlug2/` dirty.

## How it works

- `.gitmodules` points `iPlug2` at `git@github.com:guitarlum/iPlug2.git`.
- The VoLum patch is committed in the iPlug2 mirror branch
  `volum/asio-channel-routing` and pinned by the parent repo submodule SHA.
- `apply-iplug2-patches.ps1` and `apply-iplug2-patches.sh` apply any `*.patch`
  files in this folder to the submodule working tree. If there are none, they
  print "nothing to apply" and exit successfully.
- Every build entry point runs one of them first (`run-app-win.ps1`,
  `run-tests-win.ps1`, `makedist-win.bat`, `run-tests-mac.sh`,
  `makedist-mac.sh`, and the Windows CI/release jobs), so a patch here reaches
  local builds and CI alike. A change that only lives in the working tree does
  not.

## Changing iPlug2

Commit iPlug2 changes in the private mirror, then update the parent repo
submodule pointer:

```pwsh
git -C iPlug2 switch volum/asio-channel-routing
git -C iPlug2 commit
git -C iPlug2 push
git add iPlug2
git commit
```

## Expected git status

After running a Windows build/test script, the parent repo should not show
`m iPlug2` solely from the VoLum ASIO channel routing change. If `iPlug2` is
dirty, inspect it before committing:

```pwsh
git -C iPlug2 status --short
```

## Current mirror patches

- `ad391b96b APP: route selected standalone audio channels` — Makes the
  standalone audio host honor the Audio Settings input/output channel selection
  on Windows ASIO and other RtAudio backends. Without it, multi-channel
  interfaces like the RME Babyface Pro FS effectively ignore the channel
  pickers and always use device input 1 / outputs 1+2.
- `3d93e147f APP: normalize macOS scroll-wheel direction` — Converts Cocoa
  scroll-wheel deltas back to device-relative direction when macOS natural
  scrolling has already inverted them, so trackpad knob gestures match the rest
  of the system.
- `0e7036b33 APP: tell the truth about the sample rate, and follow the driver` —
  Reads the ASIO rate back after setting it, follows a rate the user changes in
  their interface's own control panel, clamps an unopenable stored rate,
  reports startup audio failures once a window exists, resolves `settings.ini`
  through `LOCALAPPDATA`, waits for a previous instance that is still quitting,
  and bounds plugin teardown with a watchdog of its own. Also folds in the
  former working-tree patch exposing `GetIOBufferSize()` and
  `GetStreamLatencyFrames()`.

## Working-tree patches

None. All VoLum changes live in the mirror branch.
