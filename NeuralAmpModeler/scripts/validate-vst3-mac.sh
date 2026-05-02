#!/usr/bin/env bash
# Validate the built macOS VST3 bundle with pluginval.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

VST3_PATH="${1:-$HOME/Library/Audio/Plug-Ins/VST3/VoLum.vst3}"
if [[ ! -d "$VST3_PATH" ]]; then
  echo "VST3 bundle not found: $VST3_PATH" >&2
  exit 1
fi

TOOL_DIR="$PROJECT_DIR/build-mac/pluginval"
OUTPUT_DIR="$PROJECT_DIR/build-mac/pluginval-output"
rm -rf "$TOOL_DIR"
mkdir -p "$TOOL_DIR" "$OUTPUT_DIR"

curl -L "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip" -o "$TOOL_DIR/pluginval.zip"
unzip -q "$TOOL_DIR/pluginval.zip" -d "$TOOL_DIR"

PLUGINVAL="$TOOL_DIR/pluginval.app/Contents/MacOS/pluginval"
if [[ ! -x "$PLUGINVAL" ]]; then
  PLUGINVAL="$(find "$TOOL_DIR" -type f -name pluginval -perm -111 | head -n 1)"
fi
if [[ -z "$PLUGINVAL" || ! -x "$PLUGINVAL" ]]; then
  echo "pluginval executable not found under $TOOL_DIR" >&2
  exit 1
fi

emit_log_annotation() {
  local title="$1"
  local log_file="$2"
  if [[ -z "${GITHUB_ACTIONS:-}" || ! -f "$log_file" ]]; then
    return
  fi

  local message
  message="$(python3 - "$log_file" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text(errors="replace").splitlines()
tail = "\n".join(text[-120:])
tail = tail.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
print(tail[-7000:])
PY
)"
  echo "::error title=${title}::${message}"
}

run_pluginval() {
  local label="$1"
  shift
  local log_file="$OUTPUT_DIR/pluginval-${label}.log"

  set +e
  "$PLUGINVAL" "$@" --strictness-level 5 --output-dir "$OUTPUT_DIR" "$VST3_PATH" 2>&1 | tee "$log_file"
  local pluginval_ec=${PIPESTATUS[0]}

  if [[ "$pluginval_ec" -ne 0 ]]; then
    emit_log_annotation "pluginval ${label} failed" "$log_file"
  fi
  return "$pluginval_ec"
}

echo "Validating VST3 with pluginval: $VST3_PATH"
set +e
run_pluginval "in-process" --validate-in-process
PLUGINVAL_EC=$?
set -e
if [[ "$PLUGINVAL_EC" -ne 0 ]]; then
  echo "pluginval in-process validation failed with exit code $PLUGINVAL_EC."
  echo "Re-running pluginval out-of-process to capture more diagnostic output."
  set +e
  run_pluginval "out-of-process"
  set -e
  exit "$PLUGINVAL_EC"
fi

SDK_ROOT="$REPO_ROOT/iPlug2/Dependencies/IPlug/VST3_SDK"
STEINBERG_VALIDATOR="$(find "$SDK_ROOT" -type f \( -name validator -o -name validator.exe \) | head -n 1 || true)"
if [[ -n "$STEINBERG_VALIDATOR" && -x "$STEINBERG_VALIDATOR" ]]; then
  echo "Validating VST3 with Steinberg validator: $VST3_PATH"
  "$STEINBERG_VALIDATOR" "$VST3_PATH"
else
  echo "Steinberg validator not found under $SDK_ROOT; pluginval validation completed."
fi
