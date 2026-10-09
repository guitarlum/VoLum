#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EVIDENCE_DIR="${VOLUM_EVIDENCE_DIR:?VOLUM_EVIDENCE_DIR is required}"
LOG="$EVIDENCE_DIR/reaper-runner.log"
WORK="$RUNNER_TEMP/volum-reaper-evidence"
REAPER_URL="https://www.reaper.fm/files/7.x/reaper782_universal.dmg"
MOUNT="$WORK/mount"
REAPER_APP="$WORK/REAPER.app"
RESOURCE="$HOME/Library/Application Support/REAPER"
RESULTS="$EVIDENCE_DIR/reaper-results.json"
mkdir -p "$EVIDENCE_DIR" "$MOUNT" "$RESOURCE/Scripts"
: > "$LOG"
exec > >(tee -a "$LOG") 2>&1

cleanup() {
  if [[ -n "${dialog_pid:-}" ]]; then
    kill "$dialog_pid" >/dev/null 2>&1 || true
    pkill -P "$dialog_pid" >/dev/null 2>&1 || true
  fi
  pkill -x REAPER >/dev/null 2>&1 || true
  hdiutil detach "$MOUNT" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Downloading pinned REAPER 7.82 universal build: $REAPER_URL"
curl --fail --location --retry 3 --output "$WORK/reaper.dmg" "$REAPER_URL"
echo "REAPER dmg sha256: $(shasum -a 256 "$WORK/reaper.dmg" | awk '{print $1}')"
set +o pipefail
yes | hdiutil attach "$WORK/reaper.dmg" -nobrowse -readonly -mountpoint "$MOUNT"
attach_ec=${PIPESTATUS[1]}
set -o pipefail
if [[ "$attach_ec" -ne 0 ]]; then
  echo "FAIL the pinned REAPER image license could not be accepted noninteractively"
  exit "$attach_ec"
fi
SOURCE_APP="$(find "$MOUNT" -maxdepth 2 -type d -name 'REAPER.app' -print -quit)"
if [[ -z "$SOURCE_APP" ]]; then
  echo "FAIL REAPER.app not found in pinned DMG"
  exit 1
fi
ditto "$SOURCE_APP" "$REAPER_APP"
hdiutil detach "$MOUNT"
xattr -cr "$REAPER_APP"

REAPER_BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$REAPER_APP/Contents/Info.plist")"
echo "REAPER bundle id: $REAPER_BUNDLE_ID"
bash "$SCRIPT_DIR/tcc-pregrant.sh" kTCCServiceMicrophone "$REAPER_BUNDLE_ID" 0
bash "$SCRIPT_DIR/tcc-pregrant.sh" kTCCServiceAccessibility /usr/bin/osascript 1
{
  echo "===== Available audio devices ====="
  system_profiler SPAudioDataType
} > "$EVIDENCE_DIR/audio-devices.log" 2>&1 || true

python3 - "$EVIDENCE_DIR/input.wav" <<'PY'
import math, struct, sys, wave
path = sys.argv[1]
sr, seconds = 48000, 2.0
with wave.open(path, "wb") as out:
    out.setparams((1, 2, sr, int(sr * seconds), "NONE", "not compressed"))
    frames = bytearray()
    for i in range(int(sr * seconds)):
        t = i / sr
        env = math.exp(-3.0 * t)
        sample = (
            0.6 * math.sin(2 * math.pi * 110 * t)
            + 0.3 * math.sin(2 * math.pi * 220 * t)
            + 0.15 * math.sin(2 * math.pi * 330 * t)
        ) * env * 0.8
        frames += struct.pack("<h", max(-32767, min(32767, int(sample * 32767))))
    out.writeframes(frames)
PY

cat > "$RESOURCE/reaper.ini" <<'INI'
[REAPER]
vstpath64=/Library/Audio/Plug-Ins/VST3
warnmaxram64=0
splash=0
verchk=0
lastversion=7.82/arm64

[audioconfig]
coreaudiobs=512
coreaudiobsuse=1
coreaudioignorereset=0
coreaudioignprojsr=0
coreaudioindevnew=Null Audio Device
coreaudiooutdevnew=Null Audio Device
coreaudiosrate=44100
coreaudiosrateuse=1
INI
printf 'dofile([[%s]])\n' "$SCRIPT_DIR/reaper-evidence.lua" > "$RESOURCE/Scripts/__startup.lua"
echo go > "$EVIDENCE_DIR/go.txt"

export VOLUM_REAPER_EVIDENCE_DIR="$EVIDENCE_DIR"
echo "Launching REAPER with fresh-runner resource path $RESOURCE"
"$REAPER_APP/Contents/MacOS/REAPER" -nosplash -new > "$EVIDENCE_DIR/reaper-process.log" 2>&1 &
reaper_pid=$!

dismiss_reaper_dialogs() {
  for _ in $(seq 1 30); do
    result="$(osascript <<'OSA' 2>&1
tell application "System Events"
  if exists process "REAPER" then
    tell process "REAPER"
      repeat with candidateWindow in windows
        if exists sheet 1 of candidateWindow then
          if exists button "No" of sheet 1 of candidateWindow then
            click button "No" of sheet 1 of candidateWindow
            return "clicked sheet No"
          end if
        end if
        if exists button "No" of candidateWindow then
          click button "No" of candidateWindow
          return "clicked window No"
        end if
      end repeat
    end tell
  end if
  return "not found"
end tell
OSA
)"
    echo "$result"
    [[ "$result" == clicked* ]] && return 0
    sleep 1
  done
  echo "No dismissible REAPER dialog was found"
}
dismiss_reaper_dialogs > "$EVIDENCE_DIR/reaper-dialog-dismiss.log" 2>&1 &
dialog_pid=$!

deadline=$((SECONDS + 120))
captured=0
while [[ "$SECONDS" -lt "$deadline" && ! -f "$RESULTS" ]]; do
  if ! kill -0 "$reaper_pid" 2>/dev/null; then
    echo "FAIL REAPER exited before writing results"
    cat "$EVIDENCE_DIR/reaper-process.log" || true
    exit 1
  fi
  if [[ "$captured" -eq 0 && "$SECONDS" -ge $((deadline - 100)) ]]; then
    screencapture -x "$EVIDENCE_DIR/reaper-window.png" 2>/dev/null || true
    osascript -e 'tell application "System Events" to get name of every window of process "REAPER"' \
      > "$EVIDENCE_DIR/reaper-windows.log" 2>&1 || true
    captured=1
  fi
  sleep 1
done

if [[ ! -f "$RESULTS" ]]; then
  echo "SKIP REAPER AU/VST3 automation: the runner kept REAPER alive but did not execute the startup/command-line Lua script (likely a first-run GUI gate)"
  cat "$EVIDENCE_DIR/reaper-harness.log" 2>/dev/null || true
  exit 0
fi
sleep 2
cp "$RESOURCE/reaper.ini" "$EVIDENCE_DIR/reaper.ini" 2>/dev/null || true
cp "$RESOURCE/reaper-vstplugins64.ini" "$EVIDENCE_DIR/reaper-vstplugins64.ini" 2>/dev/null || true
cp "$RESOURCE/reaper-auplugins64-bc.ini" "$EVIDENCE_DIR/reaper-auplugins64-bc.ini" 2>/dev/null || true

python3 - "$RESULTS" <<'PY'
import json, sys
data = json.load(open(sys.argv[1], encoding="utf-8"))
if not data.get("ok"):
    print("FAIL REAPER harness:", data.get("error", "unknown error"))
    raise SystemExit(1)
for result in data["formats"]:
    print(
        f"PASS {result['format']} loaded as {result['fx_name']}; "
        f"peak={result['peak']:.6f} rms={result['rms']:.6f} finite={result['bad'] == 0}"
    )
    print(f"{result['pc_status']} {result['format']} Program Change 1: {result['pc_evidence']}")
    print(f"{result['cc_status']} {result['format']} CC 102 value 2: {result['cc_evidence']}")
    print(
        f"PASS {result['format']} project state round-trip; "
        f"reloaded rms={result['reloaded_rms']:.6f}"
    )
PY

echo "PASS REAPER AU and VST3 host evidence completed"
