# 14 Tarball, portability and the release leg

Status: **PASS** on code head `798834db`. The Linux package job and the release dry run are green on that commit. iPlug2 was not changed.

## Heads

| | Sha |
| --- | --- |
| Code head | `798834db9919f251ce5eb586dfedd433f61cc5fd` |
| Report commit | the commit that adds this file |
| iPlug2 `volum/linux` | `3d894cd8e7a4185e7b5b28d24e7c149d587ab1ba` (unchanged) |
| CI | [40169128646](https://github.com/guitarlum/VoLum/actions/runs/40169128646) success, every `ci.yml` job, including Formatting, macOS, macOS standalone e2e, and macOS installer upgrade |
| Release dry run | [40169127248](https://github.com/guitarlum/VoLum/actions/runs/40169127248) success |
| Windows tests | `run-tests-win.ps1` exit 0, 1478 passed, 30255258 assertions. The Windows CI job on this same head is also green |

The spec's portability list now includes the standalone's GDK 3 set. Changelog and user docs were left for ticket 15.

## Tarball

`VoLum-1.3.1-linux-x86_64.tar.gz`, built with g++-12 on Ubuntu 22.04. The measured archive is **72M (74,562,762 bytes)**, sha256 `c93042c0332262fcce9d943bc2646855ceb6afe730e7fc3cd2af7173665d5dda`.

Top directory `VoLum-1.3.1-linux-x86_64/`:

- `VoLum.vst3/` (`Contents/x86_64-linux/VoLum.so` plus `Contents/Resources`)
- `VoLum` (standalone) and `resources/` beside it, so the unpacked tree can launch
- `VoLumRigs/`, including `factory-presets.json`, `Diezel Herbert Mk1/V30-Herb-4.nam`, and `PrePedals/*.nam`
- `install.sh`, `VoLum.desktop`, `volum.png`
- `LICENSE`, `THIRD_PARTY_LICENSES.md`

`install.sh` writes user paths unless `--system` is set: `~/.vst3/VoLum.vst3`, `~/.local/bin/VoLum`, and `$XDG_DATA_HOME/VoLum` (default `~/.local/share/VoLum`) for the real binary, resources, and rigs, plus a desktop file and icon under the XDG data dir. `--system` writes `/usr/lib/vst3`, `/usr/local/bin`, `/usr/share/VoLum`, and `/usr/share/applications`. The script does not elevate. If it is not root, `--system` stops before any write.

## NEEDED

Direct dependencies, from `readelf -d` of the packaged binaries:

VST3: `ld-linux-x86-64.so.2 libGL.so.1 libX11.so.6 libatomic.so.1 libc.so.6 libfontconfig.so.1 libfreetype.so.6 libm.so.6`

Standalone: the VST3 set plus `libasound.so.2 libXi.so.6 libcairo.so.2 libgdk-3.so.0 libgdk_pixbuf-2.0.so.0 libglib-2.0.so.0 libgobject-2.0.so.0`

Neither list contains libjack, libpulse, libstdc++, or libcurl. GDK is only on the standalone.

## Container matrix

The same tarball was installed, checked, and removed in throwaway containers. Ubuntu 22.04, Ubuntu 24.04, and Fedora latest (the image's packages were Fedora 44) each ran the Steinberg validator (47 passed, 0 failed), opened the standalone window under Xvfb, quit 0 with `audio teardown complete`, uninstalled the user files, then installed and uninstalled the system paths. The CI `Linux package` job ran this matrix again and succeeded.

Ubuntu 24.04 without GTK 3 still installed the VST3 and printed the package name:

```
WARNING: install package libgtk-3-0t64 and the standalone can start. The VST3 does not need it.
The VST3 was installed.
nogtk: package libgtk-3-0t64
nogtk: VST3 installed
```

The validator then loaded that VST3. `libgdk-3` was absent in the container, which is what this case is for.

Fedora `ldd` resolved every direct dependency. The closure is system libraries only (X11, GL, fontconfig, freetype, the GDK set on the standalone, and their dependencies such as glib, pango, and Mesa). libstdc++ is not a direct dependency. It appears under the standalone only because the system GDK libraries need it.

## Red proofs

Each proof mutated a copy and then checked the packaged file again.

| Gate | Mutation | Failure line | Restored |
| --- | --- | --- | --- |
| Missing GTK 3 | `sed` on a copy of `install.sh` replaced the package names with `not-the-gtk-package` | `missing package name libgtk-3-0t64` | `install.sh byte-identical; bad copy was discarded` |
| VST3 must not link GDK | `patchelf --add-needed libgdk-3.so.0` on a copy of `VoLum.so` | `unexpected NEEDED on VST3: libgdk-3.so.0` | `VST3 NEEDED check passed on the unmutated plug-in` |
| Fedora ldd | `patchelf --add-needed libcurl.so.4` on a copy of the standalone | `ldd: unexpected library libcurl.so.4` | `ldd check passed on the unmutated standalone` |
| Uninstall | deleted the `VOLUM_UNINSTALL_VST3` line in a copy of `install.sh` | `left behind: /tmp/volum-home-red/.vst3/VoLum.vst3` | `install.sh byte-identical; bad copy was discarded` |

The uninstall copy still removed the wrapper and the other installed files. Only the VST3 remained, which is what the check is there to catch.

## Dry run cannot publish

[Release Native 40169127248](https://github.com/guitarlum/VoLum/actions/runs/40169127248) was `workflow_dispatch` on `feature/linux-14-package` with `dry_run=true`. Head `798834db`.

- **Prepare release** skipped.
- **Build** skipped. That is the job that can push, tag, and upload, and it only runs when the ref is `main` and `dry_run` is not set.
- **Linux dry run** ran with `permissions: contents: read`. It built the tarball and then tried to create a draft release.
- The create call returned **HTTP 403**. The log line is `DRY_RUN_PUBLISH_BLOCKED http=403`.

No tag was pushed, no GitHub release was created, and the appcast workflow did not run. A real release still uses the same Windows and macOS artifact names and signing steps. Those steps are inside the skipped Build job. Linux is an extra matrix entry there, and it uploads `VoLum-<version>-linux-x86_64.tar.gz` only when that job is actually publishing.
