---
name: native-build-debugger
description: Diagnose VoLum Windows/macOS native build, package, artifact, and CI failures. Use when tests, makedist scripts, GitHub Actions, installers, VST3 bundles, or portable zips fail.
---

# Native Build Debugger

## First Checks

- Read the failing command and platform from logs before changing code.
- Check branch/ref when artifact contents look wrong; CI manual dispatch can use a selected ref.
- Confirm submodules are initialized and at committed pointers.

Commands for tests, app smoke, and packaging are in `AGENTS.md` "Fast Commands".

## Windows

- If postbuild copy/link fails, close stale `VoLum.exe` and rerun.
- Inno Setup is required for installer builds.

## macOS

- CI currently ships APP and VST3; AU is opt-in via `MACOS_BUILD_ALL_TARGETS=1`.
- For VST3 signing/package failures, use `bash NeuralAmpModeler/scripts/debug-mac-vst3-signature.sh <VoLum.vst3> [VoLum-v*-mac-vst3.zip]`.
- `resource fork, Finder information, or similar detritus not allowed` usually means extended attributes or AppleDouble metadata; run `xattr -cr`, remove `._*` / `.DS_Store`, then re-sign.
- `unsealed contents present in the bundle root` usually means an unexpected root file such as Finder custom-icon `Icon?`; inspect `find "$bundle" -maxdepth 1 -print` and remove the root metadata before signing/zipping.

## Packaging Invariants

Expected artifact layout (portable zip contents, embedded `VoLumRigs`, installer registry key) lives in `volum-release-packaging.mdc` "Packaging layout".
