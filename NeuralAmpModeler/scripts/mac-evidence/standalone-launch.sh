#!/usr/bin/env bash
set -euo pipefail

EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
APP_PATH="${VOLUM_INSTALLED_APP:-/Applications/VoLum.app}"
LOG="$EVIDENCE_DIR/standalone-launch.log"
SHOT="$EVIDENCE_DIR/standalone-launch.png"
REPORTS="$EVIDENCE_DIR/DiagnosticReports"
MARKER="$EVIDENCE_DIR/standalone-start.marker"
mkdir -p "$EVIDENCE_DIR" "$REPORTS"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

if [[ ! -d "$APP_PATH" ]]; then
  echo "FAIL installed app missing: $APP_PATH"
  exit 1
fi

cleanup() {
  osascript -e 'tell application "VoLum" to quit' >/dev/null 2>&1 || true
  pkill -x VoLum >/dev/null 2>&1 || true
}
trap cleanup EXIT
cleanup
touch "$MARKER"

echo "Launching $APP_PATH"
open -n "$APP_PATH"
pid=""
for _ in $(seq 1 30); do
  pid="$(pgrep -x VoLum | head -n 1 || true)"
  [[ -n "$pid" ]] && break
  sleep 0.5
done
if [[ -z "$pid" ]]; then
  echo "FAIL VoLum did not start"
  exit 1
fi

echo "VoLum pid=$pid; observing for 15 seconds"
# Hosted runners present the first-use microphone consent alert. Deny it with
# Escape so the screenshot proves the app reached its own UI, not only the TCC
# prompt. Escape is a no-op in VoLum itself if the alert is absent.
sleep 2
osascript -e 'tell application "VoLum" to activate' \
  -e 'tell application "System Events" to key code 53' >/dev/null 2>&1 || true
echo "Dismissed the optional first-use microphone prompt when present"
sleep 13
if ! kill -0 "$pid" 2>/dev/null; then
  echo "FAIL VoLum exited during the 15-second launch window"
  exit 1
fi

if screencapture -x "$SHOT" && [[ -s "$SHOT" ]]; then
  echo "PASS screenshot captured: $(basename "$SHOT")"
else
  rm -f "$SHOT"
  echo "SKIP screenshot: this GitHub runner has no screen-capture-capable GUI session"
fi

log show --style compact --predicate 'process == "VoLum"' --last 45s \
  > "$EVIDENCE_DIR/standalone-unified.log" 2>&1 || true

echo "Requesting normal quit with AppleScript"
osascript -e 'tell application "VoLum" to quit'
for _ in $(seq 1 20); do
  if ! kill -0 "$pid" 2>/dev/null; then
    break
  fi
  sleep 0.5
done
if kill -0 "$pid" 2>/dev/null; then
  echo "FAIL VoLum did not exit within 10 seconds"
  exit 1
fi

report_count=0
for report_dir in "$HOME/Library/Logs/DiagnosticReports" "/Library/Logs/DiagnosticReports"; do
  [[ -d "$report_dir" ]] || continue
  while IFS= read -r -d '' report; do
    cp "$report" "$REPORTS/"
    echo "FAIL new diagnostic report: $report"
    report_count=$((report_count + 1))
  done < <(find "$report_dir" -type f -newer "$MARKER" \
    \( -iname 'VoLum*' -o -iname '*VoLum*.crash' -o -iname '*VoLum*.ips' \) -print0)
done
if [[ "$report_count" -ne 0 ]]; then
  exit 1
fi

echo "PASS VoLum stayed alive for 15 seconds and exited normally within 10 seconds"
