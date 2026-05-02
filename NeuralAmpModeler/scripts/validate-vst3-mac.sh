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

echo "Validating VST3 with pluginval: $VST3_PATH"
"$PLUGINVAL" --validate-in-process --strictness-level 5 --output-dir "$OUTPUT_DIR" "$VST3_PATH"

SDK_ROOT="$REPO_ROOT/iPlug2/Dependencies/IPlug/VST3_SDK"
STEINBERG_VALIDATOR="$(find "$SDK_ROOT" -type f \( -name validator -o -name validator.exe \) | head -n 1 || true)"
if [[ -n "$STEINBERG_VALIDATOR" && -x "$STEINBERG_VALIDATOR" ]]; then
  echo "Validating VST3 with Steinberg validator: $VST3_PATH"
  "$STEINBERG_VALIDATOR" "$VST3_PATH"
else
  echo "Steinberg validator not found under $SDK_ROOT; pluginval validation completed."
fi
