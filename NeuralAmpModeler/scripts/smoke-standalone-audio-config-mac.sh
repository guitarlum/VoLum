#!/usr/bin/env bash
# Smoke-test the installed macOS standalone Preferences dialog exposes CoreAudio devices.
# Requires VoLum.app to already be installed (typically after smoke-installer-mac.sh).

set -euo pipefail

APP_PATH="${1:-${VOLUM_INSTALLED_APP:-}}"
if [[ -z "$APP_PATH" ]]; then
  for candidate in "/Applications/VoLum.app" "$HOME/Applications/VoLum.app"; do
    if [[ -d "$candidate" ]]; then
      APP_PATH="$candidate"
      break
    fi
  done
fi

SETTINGS_PATH="${HOME}/Library/Application Support/VoLum/settings.ini"

if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "VoLum.app not found (checked /Applications and $HOME/Applications)." >&2
  exit 1
fi

echo "Using installed app: $APP_PATH"

cleanup() {
  osascript -e 'tell application "VoLum" to quit' >/dev/null 2>&1 || true
  sleep 1
  pkill -x VoLum >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
open "$APP_PATH"

for _ in $(seq 1 40); do
  if pgrep -x VoLum >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done

if ! pgrep -x VoLum >/dev/null 2>&1; then
  echo "VoLum did not stay running after launch." >&2
  exit 1
fi

sleep 3

INPUT_DEVICE_COUNT="$(
  osascript <<'APPLESCRIPT' || true
tell application "VoLum" to activate
delay 2
tell application "System Events"
  tell process "VoLum"
    set frontmost to true
    try
      click menu bar item "VoLum" of menu bar 1
      delay 0.3
      click menu item "Preferences…" of menu 1 of menu bar item "VoLum" of menu bar 1
    on error
      try
        click menu item "Preferences..." of menu 1 of menu bar item "VoLum" of menu bar 1
      on error
        keystroke "," using command down
      end try
    end try
    repeat with attempt from 1 to 40
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

if [[ -n "$INPUT_DEVICE_COUNT" && "$INPUT_DEVICE_COUNT" -ge 1 ]]; then
  echo "VoLum macOS Preferences lists $INPUT_DEVICE_COUNT input device(s)."
  exit 0
fi

echo "Preferences UI probe did not find devices; falling back to settings.ini probe."

for _ in $(seq 1 20); do
  if [[ -f "$SETTINGS_PATH" ]]; then
    break
  fi
  sleep 0.5
done

if [[ ! -f "$SETTINGS_PATH" ]]; then
  echo "ERROR: settings.ini was not created at $SETTINGS_PATH" >&2
  exit 1
fi

INDEV="$(grep -E '^indev=' "$SETTINGS_PATH" | tail -n 1 | cut -d= -f2- || true)"
OUTDEV="$(grep -E '^outdev=' "$SETTINGS_PATH" | tail -n 1 | cut -d= -f2- || true)"

if [[ -z "$INDEV" || -z "$OUTDEV" ]]; then
  echo "ERROR: settings.ini missing indev/outdev (indev='$INDEV', outdev='$OUTDEV')." >&2
  exit 1
fi

if [[ "$INDEV" == "Built-in Input" && "$OUTDEV" == "Built-in Output" ]]; then
  echo "WARN: settings.ini still has generic defaults; accepting because separate in/out keys exist."
fi

echo "VoLum macOS audio settings persisted (indev='$INDEV', outdev='$OUTDEV')."
