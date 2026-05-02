# iPlug2 patches

VoLum now uses a private `guitarlum/iPlug2` mirror for local iPlug2 changes.
The submodule should point at a pinned commit in that mirror, so normal
build/test runs should not leave `iPlug2/` dirty.

## How it works

- `.gitmodules` points `iPlug2` at `git@github.com:guitarlum/iPlug2.git`.
- The VoLum patch is committed in the iPlug2 mirror branch
  `volum/asio-channel-routing` and pinned by the parent repo submodule SHA.
- `apply-iplug2-patches.ps1` and `apply-iplug2-patches.sh` are retained as
  no-op compatible build hooks. If this folder has no `*.patch` files, they
  print "nothing to apply" and exit successfully.

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

## Current mirror patch

- `ad391b96b APP: route selected standalone audio channels` — Makes the
  standalone audio host honor the Audio Settings input/output channel selection
  on Windows ASIO and other RtAudio backends. Without it, multi-channel
  interfaces like the RME Babyface Pro FS effectively ignore the channel
  pickers and always use device input 1 / outputs 1+2.
