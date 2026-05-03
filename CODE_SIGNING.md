# VoLum Code Signing Policy

This policy describes how VoLum release artifacts are built and approved for code signing.

## Project

- Project: VoLum
- Repository: https://github.com/guitarlum/VoLum
- License: MIT
- Maintainer: Steffen Dangmann
- Contact: steffen.dangmann@gmail.com

VoLum is an open-source NAM player for guitar amp profiles. It is a public fork of
`sdatkinson/NeuralAmpModelerPlugin` with its own release line, custom UI, bundled amp
profiles, and Windows standalone/VST3 packaging.

## Signing Scope

Windows release signing covers artifacts produced by the VoLum GitHub Actions release
workflow:

- `VoLum-vX.Y.Z-windows-setup.exe`
- `VoLum-vX.Y.Z-windows-portable.zip`
- Windows executable code inside those packages where supported by the signing service,
  including the standalone executable and VST3 plug-in binary.

Only binaries built from the public VoLum repository by GitHub-hosted Actions runners are
eligible for release signing.

## Roles

- Committer and reviewer: Steffen Dangmann
- Signing approver: Steffen Dangmann

The maintainer account uses multi-factor authentication for GitHub access. Changes from
outside contributors are reviewed before merge.

## Privacy And Security

VoLum does not collect or transmit user data. It loads local amp/profile files and stores
user settings locally on the user's machine.

VoLum does not include malware, potentially unwanted software, hacking tools, exploit
features, or functionality intended to bypass system security controls.

## Release Process

Release artifacts are built from a tagged source revision using GitHub Actions. The
Windows release job builds the standalone app, VST3 plug-in, installer, and portable ZIP,
then verifies package layout before upload.

When Windows code signing is enabled, signing requests must originate from the GitHub
Actions workflow for this repository. Signed release assets are uploaded to GitHub
Releases after successful signing and verification.

## Installation And Uninstallation

The Windows installer installs VoLum's standalone app, VST3 plug-in, and bundled amp
profiles. It also provides an uninstaller through the normal Windows application
uninstall flow. The portable ZIP can be removed by deleting the extracted folder.
