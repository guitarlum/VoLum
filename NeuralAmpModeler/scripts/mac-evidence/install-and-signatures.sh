#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
ARTIFACT_DIR="${VOLUM_MAC_ARTIFACT_DIR:?VOLUM_MAC_ARTIFACT_DIR is required}"
LOG="$EVIDENCE_DIR/install-signatures.log"
mkdir -p "$EVIDENCE_DIR"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

DMG="$(find "$ARTIFACT_DIR" -type f -name '*-macos-installer.dmg' -print -quit)"
if [[ -z "$DMG" ]]; then
  DMG="$(find "$ARTIFACT_DIR" -type f -name '*.dmg' -print -quit)"
fi
if [[ -z "$DMG" ]]; then
  echo "FAIL no installer DMG under $ARTIFACT_DIR"
  exit 1
fi

echo "Installer DMG: $DMG"
echo "sha256: $(shasum -a 256 "$DMG" | awk '{print $1}')"
echo
bash "$REPO_ROOT/NeuralAmpModeler/scripts/smoke-installer-mac.sh" "$DMG"

APP_PATH="/Applications/VoLum.app"
if [[ ! -d "$APP_PATH" && -d "$HOME/Applications/VoLum.app" ]]; then
  APP_PATH="$HOME/Applications/VoLum.app"
fi
echo "VOLUM_INSTALLED_APP=$APP_PATH" >> "$GITHUB_ENV"

bundles=(
  "$APP_PATH"
  "/Library/Audio/Plug-Ins/VST3/VoLum.vst3"
  "/Library/Audio/Plug-Ins/Components/VoLum.component"
)

broken=0
for bundle in "${bundles[@]}"; do
  echo
  echo "===== $bundle ====="
  echo "--- spctl --assess -vv --type execute ---"
  set +e
  spctl --assess -vv --type execute "$bundle"
  spctl_ec=$?
  set -e
  echo "spctl exit: $spctl_ec (informational; unsigned/ad-hoc rejection is expected)"
  echo "--- codesign -dv --verbose=4 ---"
  set +e
  codesign -dv --verbose=4 "$bundle"
  display_ec=$?
  set -e
  echo "codesign display exit: $display_ec"
  echo "--- codesign --verify --deep --strict --verbose=4 ---"
  set +e
  codesign --verify --deep --strict --verbose=4 "$bundle"
  verify_ec=$?
  set -e
  echo "codesign verify exit: $verify_ec"

  if [[ "$display_ec" -ne 0 || "$verify_ec" -ne 0 ]]; then
    echo "FAIL broken or absent code signature: $bundle"
    broken=1
  fi
done

if [[ "$broken" -ne 0 ]]; then
  exit 1
fi
echo "PASS installed payloads have internally valid signatures; spctl verdicts are recorded above"
