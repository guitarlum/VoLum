#!/usr/bin/env bash
# Verify macOS release-style artifacts: standalone DMG contains VoLumRigs; VST3 zip has VoLum.vst3 + sibling VoLumRigs.
# With `auto`, if build-mac/out/<archive>.dmg exists (makedist `full all` / `full installer`), also verifies VoLum Installer.pkg inside it.
# Usage:
#   ./verify-packaging-mac.sh /path/to/*-app.dmg /path/to/*-vst3.zip
# Or pass a single argument "auto" to resolve names from get_archive_name.py (run from repo root).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERIFY_DIR="${REPO_ROOT}/NeuralAmpModeler/build-mac/verify-packaging-ci"

INSTALLER_DMG=""

if [[ "${1:-}" == "auto" ]]; then
  ARCHIVE_NAME="$(python3 "${REPO_ROOT}/iPlug2/Scripts/get_archive_name.py" NeuralAmpModeler mac full)"
  APP_DMG="${REPO_ROOT}/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}-app.dmg"
  VST3_ZIP="${REPO_ROOT}/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}-vst3.zip"
  INSTALLER_DMG="${REPO_ROOT}/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}.dmg"
else
  if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <app.dmg> <vst3.zip>   OR $0 auto" >&2
    exit 1
  fi
  APP_DMG="$1"
  VST3_ZIP="$2"
fi

echo "Verifying macOS DMG: $APP_DMG"
echo "Verifying macOS VST3 zip: $VST3_ZIP"

test -f "$APP_DMG"
test -f "$VST3_ZIP"

rm -rf "$VERIFY_DIR"
mkdir -p "$VERIFY_DIR"

cleanup() {
  hdiutil detach "$VERIFY_DIR/installer-dmg" 2>/dev/null || true
  hdiutil detach "$VERIFY_DIR/dmg" 2>/dev/null || true
}
trap cleanup EXIT

hdiutil attach "$APP_DMG" -nobrowse -readonly -mountpoint "$VERIFY_DIR/dmg"

test -d "$VERIFY_DIR/dmg/VoLum.app"
test -d "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs"
test -f "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/Ampete One/AMP-Ampt-1.nam"

hdiutil detach "$VERIFY_DIR/dmg"

unzip -q "$VST3_ZIP" -d "$VERIFY_DIR/vst3"
test -d "$VERIFY_DIR/vst3/VoLum.vst3"
test -d "$VERIFY_DIR/vst3/VoLumRigs"
test -f "$VERIFY_DIR/vst3/VoLumRigs/Ampete One/AMP-Ampt-1.nam"

if [[ -n "$INSTALLER_DMG" ]] && [[ -f "$INSTALLER_DMG" ]]; then
  echo "Verifying macOS installer DMG: $INSTALLER_DMG"
  mkdir -p "$VERIFY_DIR/installer-dmg"
  hdiutil attach "$INSTALLER_DMG" -nobrowse -readonly -mountpoint "$VERIFY_DIR/installer-dmg"
  test -f "$VERIFY_DIR/installer-dmg/VoLum Installer.pkg"
  hdiutil detach "$VERIFY_DIR/installer-dmg"
elif [[ -n "$INSTALLER_DMG" ]]; then
  if [[ "${VERIFY_MAC_REQUIRE_INSTALLER:-}" == "1" ]]; then
    echo "ERROR: installer DMG required but missing: ${INSTALLER_DMG}" >&2
    exit 1
  fi
  echo "Note: installer DMG not present at ${INSTALLER_DMG} (expected for makedist full zip only)."
fi

trap - EXIT
cleanup

echo "macOS packaging OK."
