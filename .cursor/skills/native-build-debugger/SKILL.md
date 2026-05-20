---
name: native-build-debugger
description: Diagnose VoLum Windows/macOS native build, package, artifact, and CI failures. Use when tests, makedist scripts, GitHub Actions, installers, VST3 bundles, or portable zips fail.
---

# Native Build Debugger

## First Checks

- Read the failing command and platform from logs before changing code.
- Check branch/ref when artifact contents look wrong; CI manual dispatch can use a selected ref.
- Confirm submodules are initialized and at committed pointers.

## Windows

- Fast tests: `pwsh NeuralAmpModeler/scripts/run-tests-win.ps1`
- App smoke: `pwsh NeuralAmpModeler/scripts/run-app-win.ps1`
- Portable package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full zip`
- Installer package: `cmd /c NeuralAmpModeler\scripts\makedist-win.bat full installer`
- If postbuild copy/link fails, close stale `VoLum.exe` and rerun.
- Inno Setup is required for installer builds.

## macOS

- Release-equivalent build: `bash NeuralAmpModeler/scripts/makedist-mac.sh full all`
- CI currently ships APP and VST3; AU is opt-in via `MACOS_BUILD_ALL_TARGETS=1`.
- Keep plist filenames as `NeuralAmpModeler-*` even though product name is VoLum.
- For VST3 signing/package failures, use `bash NeuralAmpModeler/scripts/debug-mac-vst3-signature.sh <VoLum.vst3> [VoLum-v*-mac-vst3.zip]`.
- `resource fork, Finder information, or similar detritus not allowed` usually means extended attributes or AppleDouble metadata; run `xattr -cr`, remove `._*` / `.DS_Store`, then re-sign.
- `unsealed contents present in the bundle root` usually means an unexpected root file such as Finder custom-icon `Icon?`; inspect `find "$bundle" -maxdepth 1 -print` and remove the root metadata before signing/zipping.
- Do not apply Finder custom icons to signed VST3 bundles. They are cosmetic and can invalidate the bundle after zip/unzip.

## Packaging Invariants

- Windows portable zip: `VoLum_x64.exe`, `VoLum.vst3/`, and sibling `VoLumRigs/`.
- macOS standalone app embeds `VoLumRigs`.
- macOS VST3 zip has `VoLum.vst3` and sibling `VoLumRigs/`.
- Installer registry key: `HKLM\Software\VoLum\NeuralAmpModeler\VoLumRigsRoot`.
