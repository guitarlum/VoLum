#!/usr/bin/env bash
# Validate the built macOS Audio Unit with Apple's auval.
# Usage: validate-au-mac.sh [path/to/VoLum.component]
# The type/subtype/manufacturer codes come from the bundle's Info.plist;
# AUVAL_TYPE / AUVAL_SUBTYPE / AUVAL_MANUFACTURER override them.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

COMPONENTS_DIR="$HOME/Library/Audio/Plug-Ins/Components"
AU_PATH="${1:-$COMPONENTS_DIR/VoLum.component}"
if [[ ! -d "$AU_PATH" ]]; then
  echo "AU bundle not found: $AU_PATH" >&2
  exit 1
fi

# auval only finds components in the system or user Components folders.
INSTALLED="$COMPONENTS_DIR/$(basename "$AU_PATH")"
if [[ ! "$AU_PATH" -ef "$INSTALLED" ]]; then
  mkdir -p "$COMPONENTS_DIR"
  rm -rf "$INSTALLED"
  cp -R "$AU_PATH" "$INSTALLED"
fi

PLIST="$INSTALLED/Contents/Info.plist"
read_code() {
  /usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$1" "$PLIST"
}
AU_TYPE="${AUVAL_TYPE:-$(read_code type)}"
AU_SUBTYPE="${AUVAL_SUBTYPE:-$(read_code subtype)}"
AU_MANUFACTURER="${AUVAL_MANUFACTURER:-$(read_code manufacturer)}"

# The registrar caches the component list; a fresh process rescans the folders.
killall -9 AudioComponentRegistrar 2>/dev/null || true

OUTPUT_DIR="$PROJECT_DIR/build-mac/auval-output"
mkdir -p "$OUTPUT_DIR"
LOG_FILE="$OUTPUT_DIR/auval.log"

echo "Validating AU with auval: $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER ($INSTALLED)"
set +e
auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER" 2>&1 | tee "$LOG_FILE"
AUVAL_EC=${PIPESTATUS[0]}
set -e

if [[ "$AUVAL_EC" -ne 0 ]] || grep -q "FAILED" "$LOG_FILE" || ! grep -q "AU VALIDATION SUCCEEDED" "$LOG_FILE"; then
  echo "auval failed (exit $AUVAL_EC) for $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER" >&2
  if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
    echo "::error title=auval failed::auval -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER exited $AUVAL_EC; see $LOG_FILE"
  fi
  exit 1
fi
echo "auval passed: $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER"
