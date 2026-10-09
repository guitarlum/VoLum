#!/usr/bin/env bash
set -euo pipefail

EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
LOG="$EVIDENCE_DIR/auval-rosetta.log"
AU_PATH="/Library/Audio/Plug-Ins/Components/VoLum.component"
mkdir -p "$EVIDENCE_DIR"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

if [[ ! -d "$AU_PATH" ]]; then
  echo "FAIL AU bundle missing: $AU_PATH"
  exit 1
fi

echo "Installing Rosetta 2 (the command is harmless when already installed)"
sudo softwareupdate --install-rosetta --agree-to-license
if ! arch -x86_64 /usr/bin/true; then
  echo "FAIL Rosetta cannot execute an x86_64 process"
  exit 1
fi

PLIST="$AU_PATH/Contents/Info.plist"
read_code() {
  /usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$1" "$PLIST"
}
AU_TYPE="$(read_code type)"
AU_SUBTYPE="$(read_code subtype)"
AU_MANUFACTURER="$(read_code manufacturer)"

killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Running Intel auval under Rosetta: $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER"
set +e
arch -x86_64 /usr/bin/auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER"
ec=$?
set -e

if [[ "$ec" -ne 0 ]] || grep -q "FAILED" "$LOG" || ! grep -q "AU VALIDATION SUCCEEDED" "$LOG"; then
  echo "FAIL Rosetta auval exit=$ec"
  exit 1
fi
echo "PASS Rosetta x86_64 auval: AU VALIDATION SUCCEEDED"
