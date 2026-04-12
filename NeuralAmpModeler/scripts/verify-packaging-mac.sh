#!/usr/bin/env bash
# Verify macOS release-style artifacts: standalone DMG contains VoLumRigs; VST3 zip has VoLum.vst3 + sibling VoLumRigs.
# Usage:
#   ./verify-packaging-mac.sh /path/to/VoLum-vX.Y.Z-mac-app.dmg /path/to/VoLum-vX.Y.Z-mac-vst3.zip
# Or pass a single argument "auto" to resolve names from get_archive_name.py (run from repo root).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VERIFY_DIR="${REPO_ROOT}/NeuralAmpModeler/build-mac/verify-packaging-ci"

if [[ "${1:-}" == "auto" ]]; then
  ARCHIVE_NAME="$(python3 "${REPO_ROOT}/iPlug2/Scripts/get_archive_name.py" NeuralAmpModeler mac full)"
  APP_DMG="${REPO_ROOT}/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}-app.dmg"
  VST3_ZIP="${REPO_ROOT}/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}-vst3.zip"
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

hdiutil attach "$APP_DMG" -nobrowse -readonly -mountpoint "$VERIFY_DIR/dmg"
cleanup() { hdiutil detach "$VERIFY_DIR/dmg" || true; }
trap cleanup EXIT

test -d "$VERIFY_DIR/dmg/VoLum.app"
test -d "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs"
test -f "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/Ampete One/AMP-Ampt-1.nam"

unzip -q "$VST3_ZIP" -d "$VERIFY_DIR/vst3"
test -d "$VERIFY_DIR/vst3/VoLum.vst3"
test -d "$VERIFY_DIR/vst3/VoLumRigs"
test -f "$VERIFY_DIR/vst3/VoLumRigs/Ampete One/AMP-Ampt-1.nam"

echo "macOS packaging OK."
