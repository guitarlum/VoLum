#!/usr/bin/env bash
# Smoke-test the installed macOS standalone app by launching it and checking for a crash.

set -euo pipefail

APP_PATH="${1:-}"
if [[ -z "$APP_PATH" ]]; then
  for candidate in "/Applications/VoLum.app" "$HOME/Applications/VoLum.app"; do
    if [[ -d "$candidate" ]]; then
      APP_PATH="$candidate"
      break
    fi
  done
fi

if [[ -z "$APP_PATH" || ! -d "$APP_PATH" ]]; then
  echo "VoLum.app not found (checked /Applications and $HOME/Applications)." >&2
  exit 1
fi

echo "Using installed app: $APP_PATH"
SCREENSHOT_PATH="${VOLUM_MAC_SMOKE_SCREENSHOT:-/tmp/volum-mac-launch.png}"
LOG_PATH="${VOLUM_MAC_SMOKE_LOG:-/tmp/volum-mac-launch.log}"

cleanup() {
  osascript -e 'quit app "VoLum"' >/dev/null 2>&1 || true
  sleep 1
  pkill -x VoLum >/dev/null 2>&1 || true
}
trap cleanup EXIT

cleanup
open "$APP_PATH"

pid=""
for _ in $(seq 1 20); do
  pid="$(pgrep -x VoLum | head -n 1 || true)"
  if [[ -n "$pid" ]]; then
    break
  fi
  sleep 0.5
done

if [[ -z "$pid" ]]; then
  echo "VoLum did not stay running after launch." >&2
  exit 1
fi

sleep 5
if ! ps -p "$pid" >/dev/null 2>&1; then
  echo "VoLum exited during the launch smoke window." >&2
  exit 1
fi

screencapture -x "$SCREENSHOT_PATH" >/dev/null 2>&1 || true
log show --style compact --predicate 'process == "VoLum"' --last 30s > "$LOG_PATH" 2>/dev/null || true

if [[ -s "$LOG_PATH" ]] && grep -E 'EXC_|SIGSEGV|SIGBUS|dyld:|crashed|abort' "$LOG_PATH"; then
  echo "VoLum emitted crash-like log lines during launch smoke. See $LOG_PATH" >&2
  exit 1
fi

echo "VoLum macOS standalone launch smoke OK (pid=$pid)."
