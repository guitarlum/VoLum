#!/usr/bin/env bash
# Smoke-test the installed macOS standalone Preferences dialog exposes CoreAudio devices.
# Requires VoLum.app to already be installed (typically after smoke-installer-mac.sh).

set -euo pipefail

APP_PATH="${1:-${VOLUM_INSTALLED_APP:-/Applications/VoLum.app}}"

if [[ ! -d "$APP_PATH" ]]; then
  echo "VoLum.app not found: $APP_PATH" >&2
  exit 1
fi

cleanup() {
  osascript -e 'tell application "VoLum" to quit' >/dev/null 2>&1 || true
  sleep 1
  pkill -x VoLum >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
open "$APP_PATH"

for _ in $(seq 1 20); do
  if pgrep -x VoLum >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

if ! pgrep -x VoLum >/dev/null 2>&1; then
  echo "VoLum did not stay running after launch." >&2
  exit 1
fi

sleep 2

INPUT_DEVICE_COUNT="$(
  osascript <<'APPLESCRIPT'
tell application "VoLum" to activate
delay 1
tell application "System Events"
  tell process "VoLum"
    set frontmost to true
    keystroke "," using command down
    repeat with attempt from 1 to 20
      if exists window "Preferences" then exit repeat
      delay 0.25
    end repeat
    if not (exists window "Preferences") then error "Preferences window did not open"
    set prefWin to window "Preferences"
    set inputCount to 0
    repeat with pb in (every pop up button of prefWin)
      try
        click pb
        delay 0.2
        set inputCount to count of menu items of menu 1 of pb
        key code 53
        delay 0.1
        if inputCount > 0 then exit repeat
      end try
    end repeat
    if inputCount < 1 then error "No audio input devices listed in Preferences"
    return inputCount
  end tell
end tell
APPLESCRIPT
)"

if [[ -z "$INPUT_DEVICE_COUNT" || "$INPUT_DEVICE_COUNT" -lt 1 ]]; then
  echo "Expected at least one input device in Preferences, got '$INPUT_DEVICE_COUNT'." >&2
  exit 1
fi

echo "VoLum macOS Preferences lists $INPUT_DEVICE_COUNT input device(s)."
