#!/usr/bin/env bash
set -euo pipefail

EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
LOG="$EVIDENCE_DIR/universal-binaries.log"
mkdir -p "$EVIDENCE_DIR"

bundles=(
  "${VOLUM_INSTALLED_APP:-/Applications/VoLum.app}"
  "/Library/Audio/Plug-Ins/VST3/VoLum.vst3"
  "/Library/Audio/Plug-Ins/Components/VoLum.component"
)

fail=0
: > "$LOG"
for bundle in "${bundles[@]}"; do
  if [[ ! -d "$bundle" ]]; then
    echo "FAIL missing bundle: $bundle" | tee -a "$LOG"
    fail=1
    continue
  fi

  echo "BUNDLE $bundle" | tee -a "$LOG"
  found=0
  while IFS= read -r -d '' path; do
    description="$(file -b "$path" 2>/dev/null || true)"
    [[ "$description" == *"Mach-O"* ]] || continue
    found=1
    archs="$(lipo -archs "$path" 2>&1 || true)"
    rel="${path#"$bundle"/}"
    echo "  $rel: $archs" | tee -a "$LOG"
    if [[ "$archs" != *arm64* || "$archs" != *x86_64* ]]; then
      echo "FAIL $path is not universal arm64+x86_64" | tee -a "$LOG"
      fail=1
    fi
  done < <(find "$bundle" -type f -print0)

  if [[ "$found" -eq 0 ]]; then
    echo "FAIL no Mach-O files found in $bundle" | tee -a "$LOG"
    fail=1
  fi
done

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo "PASS every installed Mach-O contains arm64 and x86_64" | tee -a "$LOG"
