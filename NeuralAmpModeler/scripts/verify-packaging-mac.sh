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

plist_value() {
  /usr/libexec/PlistBuddy -c "Print :$2" "$1" 2>/dev/null
}

require_plist_value() {
  local plist="$1"
  local key="$2"
  local expected="$3"
  local actual
  actual="$(plist_value "$plist" "$key")"
  if [[ "$actual" != "$expected" ]]; then
    echo "ERROR: $plist $key is '$actual', expected '$expected'" >&2
    exit 1
  fi
}

require_nonempty_plist_value() {
  local plist="$1"
  local key="$2"
  local actual
  actual="$(plist_value "$plist" "$key")"
  if [[ -z "$actual" ]]; then
    echo "ERROR: $plist $key is missing or empty" >&2
    exit 1
  fi
}

verify_bundle_signature() {
  local bundle="$1"
  if ! codesign --verify --deep --strict --verbose=2 "$bundle"; then
    echo "ERROR: signature verification failed for $bundle" >&2
    echo "Bundle root contents:" >&2
    find "$bundle" -maxdepth 1 -print | sort >&2
    echo "Suspicious metadata files:" >&2
    find "$bundle" \( -name 'Icon?' -o -name '._*' -o -name '.DS_Store' \) -print | sort >&2
    if command -v xattr >/dev/null 2>&1; then
      echo "Extended attributes:" >&2
      xattr -lr "$bundle" 2>&1 | sed 's/^/  /' >&2 || true
    fi
    return 1
  fi
}

verify_standalone_app_identity() {
  local app="$1"
  local plist="$app/Contents/Info.plist"
  local entitlements
  local has_mic_entitlement=0

  test -d "$app"
  test -f "$plist"
  require_plist_value "$plist" CFBundleExecutable VoLum
  require_plist_value "$plist" CFBundleIdentifier com.Lum.app.VoLum
  require_plist_value "$plist" CFBundlePackageType APPL
  require_nonempty_plist_value "$plist" NSMicrophoneUsageDescription

  entitlements="$(codesign -d --entitlements :- "$app" 2>/dev/null || true)"
  if [[ "$entitlements" == *"com.apple.security.device.microphone"* && "$entitlements" == *"<true/>"* ]]; then
    has_mic_entitlement=1
  fi

  if [[ "$has_mic_entitlement" == "1" ]]; then
    echo "APP microphone entitlement: present"
  else
    echo "APP microphone entitlement: not present (expected for unsigned/non-sandbox CI builds)"
    if [[ "${VERIFY_MAC_REQUIRE_MIC_ENTITLEMENT:-}" == "1" ]]; then
      echo "ERROR: signed release verification requires com.apple.security.device.microphone" >&2
      exit 1
    fi
  fi

  if [[ "${VERIFY_MAC_REQUIRE_SIGNED_APP:-}" == "1" ]]; then
    verify_bundle_signature "$app"
  fi
}

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
verify_standalone_app_identity "$VERIFY_DIR/dmg/VoLum.app"
verify_bundle_signature "$VERIFY_DIR/dmg/VoLum.app"
test -d "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs"
test -d "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/PrePedals"
test -f "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/Ampete One/AMP-Ampt-1.nam"
test -f "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/Diezel Herbert Mk1/V30-Herb-4.nam"
while IFS= read -r pre_capture; do
  pre_name="$(basename "$pre_capture")"
  test -f "$VERIFY_DIR/dmg/VoLum.app/Contents/Resources/VoLumRigs/PrePedals/$pre_name"
done < <(find "$REPO_ROOT/rigs/PrePedals" -maxdepth 1 -type f -name '*.nam' | sort)

hdiutil detach "$VERIFY_DIR/dmg"

unzip -q "$VST3_ZIP" -d "$VERIFY_DIR/vst3"
test -d "$VERIFY_DIR/vst3/VoLum.vst3"
verify_bundle_signature "$VERIFY_DIR/vst3/VoLum.vst3"
test -d "$VERIFY_DIR/vst3/VoLumRigs"
test -d "$VERIFY_DIR/vst3/VoLumRigs/PrePedals"
test -f "$VERIFY_DIR/vst3/VoLumRigs/Ampete One/AMP-Ampt-1.nam"
test -f "$VERIFY_DIR/vst3/VoLumRigs/Diezel Herbert Mk1/V30-Herb-4.nam"
while IFS= read -r pre_capture; do
  pre_name="$(basename "$pre_capture")"
  test -f "$VERIFY_DIR/vst3/VoLumRigs/PrePedals/$pre_name"
done < <(find "$REPO_ROOT/rigs/PrePedals" -maxdepth 1 -type f -name '*.nam' | sort)

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
