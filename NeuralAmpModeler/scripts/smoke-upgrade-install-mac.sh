#!/usr/bin/env bash
# Smoke-test upgrading from a prior published macOS release to the freshly built installer.
# Installs the prior release, pre-seeds volum-settings.json, installs the new PKG, and
# asserts version + settings preservation.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
FROM_TAG="${VOLUM_UPGRADE_SMOKE_FROM_TAG:-}"
WORK_DIR="$REPO_ROOT/NeuralAmpModeler/build-mac/upgrade-smoke"
SETTINGS_DIR="$HOME/Library/Application Support/VoLum"
SETTINGS_PATH="$SETTINGS_DIR/volum-settings.json"
SENTINEL_VALUE="0.777777"
SENTINEL_AMP="Ampete One"

if [[ -z "$FROM_TAG" ]]; then
  FROM_TAG="$(gh release list --repo guitarlum/VoLum --limit 20 --json tagName,isDraft \
    --jq '.[] | select(.isDraft == false) | .tagName' 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$FROM_TAG" ]]; then
  FROM_TAG="v1.0.0"
fi

if ! gh release view "$FROM_TAG" --repo guitarlum/VoLum >/dev/null 2>&1; then
  echo "SKIP: prior release tag not found: $FROM_TAG"
  if [[ -n "${GITHUB_ENV:-}" ]]; then
    echo "VOLUM_UPGRADE_SMOKE_SKIPPED=1" >> "$GITHUB_ENV"
  fi
  exit 0
fi

ARCHIVE_NAME="$(python3 "$REPO_ROOT/iPlug2/Scripts/get_archive_name.py" NeuralAmpModeler mac full)"
NEW_INSTALLER_DMG="${1:-$REPO_ROOT/NeuralAmpModeler/build-mac/out/${ARCHIVE_NAME}.dmg}"

if [[ ! -f "$NEW_INSTALLER_DMG" ]]; then
  echo "ERROR: new installer DMG not found: $NEW_INSTALLER_DMG" >&2
  exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cleanup() {
  hdiutil detach "$WORK_DIR/prior-dmg" 2>/dev/null || true
  hdiutil detach "$WORK_DIR/new-dmg" 2>/dev/null || true
}
trap cleanup EXIT

echo "Downloading prior release $FROM_TAG for upgrade smoke..."
gh release download "$FROM_TAG" --repo guitarlum/VoLum \
  --pattern '*macos-installer.dmg' --dir "$WORK_DIR/prior-download"

PRIOR_DMG="$(find "$WORK_DIR/prior-download" -name '*macos-installer.dmg' -print -quit)"
if [[ -z "$PRIOR_DMG" ]]; then
  message="Prior release $FROM_TAG has no macos-installer.dmg asset."
  if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
    echo "ERROR: $message" >&2
    exit 1
  fi
  echo "SKIP: $message"
  if [[ -n "${GITHUB_ENV:-}" ]]; then
    echo "VOLUM_UPGRADE_SMOKE_SKIPPED=1" >> "$GITHUB_ENV"
  fi
  exit 0
fi

sudo rm -rf "/Applications/VoLum.app" "$HOME/Applications/VoLum.app" \
  "/Library/Audio/Plug-Ins/VST3/VoLum.vst3" \
  "/Library/Audio/Plug-Ins/Components/VoLum.component" \
  "/Library/Application Support/VoLum/VoLumRigs"
rm -rf "$SETTINGS_DIR"


CHOICES_XML="$WORK_DIR/installer-choices-ci.xml"
PRIOR_CHOICES_XML="$WORK_DIR/installer-choices-prior.xml"
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
    <string>com.Lum.au.pkg.VoLum</string>
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

</plist>
XML

cat > "$PRIOR_CHOICES_XML" <<'XML'
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

mkdir -p "$WORK_DIR/prior-dmg" "$WORK_DIR/new-dmg"
hdiutil attach "$PRIOR_DMG" -nobrowse -readonly -mountpoint "$WORK_DIR/prior-dmg"
echo "Installing prior release $FROM_TAG with PKG choices compatible with that release."
sudo installer -pkg "$WORK_DIR/prior-dmg/VoLum Installer.pkg" -target / -applyChoiceChangesXML "$PRIOR_CHOICES_XML"
hdiutil detach "$WORK_DIR/prior-dmg"

find_volum_app() {
  for candidate in "/Applications/VoLum.app" "$HOME/Applications/VoLum.app"; do
    if [[ -d "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

APP_PATH="$(find_volum_app || true)"
if [[ -z "$APP_PATH" ]]; then
  echo "ERROR: prior release install did not place VoLum.app" >&2
  echo "/Applications:" >&2
  ls -la /Applications >&2 || true
  echo "$HOME/Applications:" >&2
  ls -la "$HOME/Applications" >&2 || true
  pkgutil --pkgs | grep -i lum >&2 || true
  exit 1
fi

mkdir -p "$SETTINGS_DIR"
cat > "$SETTINGS_PATH" <<JSON
{
  "version": 6,
  "lastAmpIdx": 0,
  "amps": {
    "$SENTINEL_AMP": {
      "postDelayMix": $SENTINEL_VALUE
    }
  }
}
JSON

hdiutil attach "$NEW_INSTALLER_DMG" -nobrowse -readonly -mountpoint "$WORK_DIR/new-dmg"
sudo installer -pkg "$WORK_DIR/new-dmg/VoLum Installer.pkg" -target / -applyChoiceChangesXML "$CHOICES_XML"
hdiutil detach "$WORK_DIR/new-dmg"

APP_PATH="$(find_volum_app || true)"
if [[ -z "$APP_PATH" ]]; then
  echo "ERROR: upgrade install did not place VoLum.app" >&2
  exit 1
fi

APP_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP_PATH/Contents/Info.plist")"
EXPECTED_VERSION="$(grep '#define PLUG_VERSION_STR' "$REPO_ROOT/NeuralAmpModeler/config.h" | sed 's/.*"\(.*\)".*/\1/')"
if [[ "$APP_VERSION" != "$EXPECTED_VERSION" ]]; then
  echo "ERROR: upgraded app version is '$APP_VERSION', expected '$EXPECTED_VERSION'." >&2
  exit 1
fi

if [[ ! -f "$SETTINGS_PATH" ]]; then
  echo "ERROR: volum-settings.json missing after upgrade." >&2
  exit 1
fi

if ! grep -q "\"postDelayMix\": $SENTINEL_VALUE" "$SETTINGS_PATH"; then
  echo "ERROR: upgrade did not preserve seeded volum-settings.json sentinel." >&2
  cat "$SETTINGS_PATH" >&2 || true
  exit 1
fi

codesign --verify --deep --strict --verbose=2 "$APP_PATH"

VST3_PATH="/Library/Audio/Plug-Ins/VST3/VoLum.vst3"
AU_PATH="/Library/Audio/Plug-Ins/Components/VoLum.component"
RIGS_PATH="/Library/Application Support/VoLum/VoLumRigs"
for required in "$VST3_PATH" "$AU_PATH" "$RIGS_PATH"; do
  if [[ ! -d "$required" ]]; then
    echo "ERROR: expected installed directory missing after upgrade: $required" >&2
    exit 1
  fi
done
codesign --verify --deep --strict --verbose=2 "$VST3_PATH"
codesign --verify --deep --strict --verbose=2 "$AU_PATH"

if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "VOLUM_INSTALLED_APP=$APP_PATH" >> "$GITHUB_ENV"
fi
echo "macOS upgrade smoke OK ($FROM_TAG -> $EXPECTED_VERSION)."
