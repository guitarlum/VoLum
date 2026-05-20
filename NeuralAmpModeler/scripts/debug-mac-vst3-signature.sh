#!/usr/bin/env bash
# Diagnose macOS VST3 bundle signing failures caused by root files, xattrs, or zip extraction.

set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
  echo "Usage: $0 <VoLum.vst3> [VoLum-v*-mac-vst3.zip]" >&2
  exit 2
fi

BUNDLE="$1"
ZIP_PATH="${2:-}"

dump_bundle() {
  local bundle="$1"
  echo "== Bundle: $bundle"
  if [[ ! -d "$bundle" ]]; then
    echo "missing bundle"
    return
  fi

  echo "-- root files"
  find "$bundle" -maxdepth 1 -print | sort

  echo "-- suspicious metadata"
  find "$bundle" \( -name 'Icon?' -o -name '._*' -o -name '.DS_Store' \) -print | sort

  echo "-- xattrs"
  if command -v xattr >/dev/null 2>&1; then
    xattr -lr "$bundle" 2>&1 | sed 's/^/  /' || true
  else
    echo "xattr not available"
  fi

  echo "-- codesign"
  codesign --verify --deep --strict --verbose=4 "$bundle" || true
}

dump_bundle "$BUNDLE"

if [[ -n "$ZIP_PATH" ]]; then
  echo "== Zip: $ZIP_PATH"
  unzip -l "$ZIP_PATH" | sed -n '1,120p'

  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TMP_DIR"' EXIT
  unzip -q "$ZIP_PATH" -d "$TMP_DIR"
  dump_bundle "$TMP_DIR/VoLum.vst3"
fi
