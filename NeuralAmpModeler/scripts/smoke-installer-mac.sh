#!/usr/bin/env bash
# Smoke-test the macOS installer DMG by installing the product PKG and verifying installed payloads.
#
# DESTRUCTIVE: this script deletes any existing /Applications/VoLum.app,
# system VST3 bundle, and shared VoLumRigs folder before installing. It is
# intended to run on CI runners or scratch VMs; running it on a dev machine
# with a real VoLum install will wipe that install.
#
# To opt in on a dev machine, export VOLUM_SMOKE_ALLOW_DESTRUCTIVE=1. Without
# that variable the script aborts on non-CI hosts.
set -euo pipefail

if [[ -z "${CI:-}" && -z "${GITHUB_ACTIONS:-}" && "${VOLUM_SMOKE_ALLOW_DESTRUCTIVE:-0}" != "1" ]]; then
  echo "ERROR: smoke-installer-mac.sh is destructive and refuses to run outside CI." >&2
  echo "       Set VOLUM_SMOKE_ALLOW_DESTRUCTIVE=1 to opt in (your existing VoLum install will be wiped)." >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

INSTALLER_DMG="${1:-}"
if [[ -z "$INSTALLER_DMG" ]]; then
  ARCHIVE_NAME="$(python3 "$REPO_ROOT/iPlug2/Scripts/get_archive_name.py" NeuralAmpModeler mac full)"
  INSTALLER_DMG="$REPO_ROOT/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}.dmg"
fi

MOUNT_DIR="$REPO_ROOT/NeuralAmpModeler/build-mac/installer-install-ci"
CHOICES_XML="$REPO_ROOT/NeuralAmpModeler/build-mac/installer-choices-ci.xml"
VST3_PATH="/Library/Audio/Plug-Ins/VST3/VoLum.vst3"
RIGS_PATH="/Library/Application Support/VoLum/VoLumRigs"

rm -rf "$MOUNT_DIR"
mkdir -p "$MOUNT_DIR"

cleanup() {
  hdiutil detach "$MOUNT_DIR" 2>/dev/null || true
}
trap cleanup EXIT

hdiutil attach "$INSTALLER_DMG" -nobrowse -readonly -mountpoint "$MOUNT_DIR"
sudo rm -rf "/Applications/VoLum.app" "$HOME/Applications/VoLum.app" "$VST3_PATH" "$RIGS_PATH"

cat > "$CHOICES_XML" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>choiceIdentifier</key>
    <string>com.Lum.app.pkg.VoLum</string>
    <key>choiceAttribute</key>
    <string>selected</string>
    <key>attributeSetting</key>
    <integer>1</integer>
  </dict>
  <dict>
    <key>choiceIdentifier</key>
    <string>com.Lum.vst3.pkg.VoLum</string>
    <key>choiceAttribute</key>
    <string>selected</string>
    <key>attributeSetting</key>
    <integer>1</integer>
  </dict>
  <dict>
    <key>choiceIdentifier</key>
    <string>com.Lum.rigs.pkg.VoLum</string>
    <key>choiceAttribute</key>
    <string>selected</string>
    <key>attributeSetting</key>
    <integer>1</integer>
  </dict>
</array>
</plist>
XML

installer -showChoicesXML -pkg "$MOUNT_DIR/VoLum Installer.pkg"
sudo installer -pkg "$MOUNT_DIR/VoLum Installer.pkg" -target / -applyChoiceChangesXML "$CHOICES_XML"

APP_PATH=""
for candidate in "/Applications/VoLum.app" "$HOME/Applications/VoLum.app"; do
  if [[ -d "$candidate" ]]; then
    APP_PATH="$candidate"
    break
  fi
done

if [[ -z "$APP_PATH" ]]; then
  echo "VoLum.app was not installed. /Applications:" >&2
  ls -la /Applications >&2 || true
  echo "$HOME/Applications:" >&2
  ls -la "$HOME/Applications" >&2 || true
  exit 1
fi

for required in "$VST3_PATH" "$RIGS_PATH"; do
  if [[ ! -d "$required" ]]; then
    echo "Expected installed directory missing: $required" >&2
    ls -la "$(dirname "$required")" >&2 || true
    exit 1
  fi
done

pkgutil --pkg-info com.Lum.app.pkg.VoLum
pkgutil --pkg-info com.Lum.vst3.pkg.VoLum
pkgutil --pkg-info com.Lum.rigs.pkg.VoLum
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$VST3_PATH"

if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "VOLUM_INSTALLED_APP=$APP_PATH" >> "$GITHUB_ENV"
fi

echo "Installed VoLum app: $APP_PATH"
