#!/usr/bin/env bash
# Crash smoke for the installed VoLum on macOS: REAPER with the AU and the VST3,
# and the standalone app. Every launch starts on an empty VoLum library, so it
# runs VoLum's first-run work and its update check (the 1.3.0 crash), and REAPER
# on its own config folder. MallocScribble makes a read of freed memory crash
# instead of passing by luck. A launch fails on a new crash report, a hang past
# --timeout, or a REAPER script that does not report PASS.
#
#   bash smoke-reaper-mac.sh [--format au|vst3|app|all] [--runs N] [--cycles N]
#                            [--timeout S] [--reaper /path/REAPER.app]
#                            [--app /path/VoLum.app] [--out DIR]
#
# Tests the plug-ins installed in /Library/Audio/Plug-Ins; it installs nothing.
# HOME stays real: REAPER lists no third-party AU under a substitute HOME. The
# VoLum library (~/Library/Application Support/VoLum) is moved aside for the run
# and put back on exit.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FORMAT=all
RUNS=5
CYCLES=3
TIMEOUT=120
REAPER_APP=/Applications/REAPER.app
VOLUM_APP=/Applications/VoLum.app
OUT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --format) FORMAT="$2"; shift 2 ;;
    --runs) RUNS="$2"; shift 2 ;;
    --cycles) CYCLES="$2"; shift 2 ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    --reaper) REAPER_APP="$2"; shift 2 ;;
    --app) VOLUM_APP="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 2 ;;
  esac
done

case "$FORMAT" in
  au) FORMATS=(au) ;;
  vst3) FORMATS=(vst3) ;;
  app) FORMATS=(app) ;;
  all) FORMATS=(au vst3 app) ;;
  *) echo "--format must be au, vst3, app or all" >&2; exit 2 ;;
esac

[[ -n "$OUT" ]] || OUT="$(mktemp -d "${TMPDIR:-/tmp}/volum-smoke.XXXXXX")"
mkdir -p "$OUT"
CRASH_DIR="$HOME/Library/Logs/DiagnosticReports"
LIBRARY="$HOME/Library/Application Support/VoLum"
LIBRARY_BACKUP="$OUT/library-backup"
APP_BIN="$VOLUM_APP/Contents/MacOS/VoLum"

plist_version() {
  /usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$1/Contents/Info.plist" 2>/dev/null || echo "missing"
}
echo "AU   $(plist_version /Library/Audio/Plug-Ins/Components/VoLum.component)"
echo "VST3 $(plist_version /Library/Audio/Plug-Ins/VST3/VoLum.vst3)"
echo "APP  $(plist_version "$VOLUM_APP")"
echo "REAPER $(plist_version "$REAPER_APP")"
echo "Output: $OUT"

if [[ -e "$LIBRARY" ]]; then
  [[ ! -e "$LIBRARY_BACKUP" ]] || { echo "$LIBRARY_BACKUP already exists" >&2; exit 2; }
  mv "$LIBRARY" "$LIBRARY_BACKUP"
fi
restore_library() {
  rm -rf "$LIBRARY"
  [[ ! -e "$LIBRARY_BACKUP" ]] || mv "$LIBRARY_BACKUP" "$LIBRARY"
}

# After a crash or a killed launch, AppKit asks whether to reopen the windows and
# the app waits for an answer. Switched off for the run, put back on exit.
PERSISTENCE_DOMAINS=()
for bundle in "$REAPER_APP" "$VOLUM_APP"; do
  id="$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)"
  [[ -n "$id" ]] || continue
  if defaults read "$id" ApplePersistenceIgnoreState > /dev/null 2>&1; then
    continue
  fi
  defaults write "$id" ApplePersistenceIgnoreState -bool YES
  PERSISTENCE_DOMAINS+=("$id")
done
restore_persistence() {
  for id in "${PERSISTENCE_DOMAINS[@]+"${PERSISTENCE_DOMAINS[@]}"}"; do
    defaults delete "$id" ApplePersistenceIgnoreState > /dev/null 2>&1 || true
  done
}
trap 'restore_library; restore_persistence' EXIT

FAILURES=0
MARKER="$OUT/.start-marker"

new_crash_reports() {
  find "$CRASH_DIR" -maxdepth 1 -newer "$MARKER" \( -name 'REAPER*.ips' -o -name 'VoLum*.ips' \) 2>/dev/null || true
}

# ReportCrash writes the report a few seconds after the process is gone.
collect_crashes() {
  local label="$1" settle="$2" found=0
  for ((i = 0; i < settle; i++)); do
    [[ -z "$(new_crash_reports)" ]] || break
    sleep 1
  done
  while IFS= read -r report; do
    [[ -n "$report" ]] || continue
    cp "$report" "$OUT/$label-$(basename "$report")"
    found=1
  done < <(new_crash_reports)
  return $((1 - found))
}

# Kill $1 after $TIMEOUT seconds; returns 0 if the process exited on its own.
wait_or_kill() {
  local pid="$1" waited=0
  while kill -0 "$pid" 2>/dev/null; do
    if (( waited >= TIMEOUT )); then
      kill -9 "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
      return 1
    fi
    sleep 1
    waited=$((waited + 1))
  done
  wait "$pid" 2>/dev/null || true
  return 0
}

begin_launch() {
  rm -rf "$LIBRARY"
  touch "$MARKER"
  sleep 1
}

end_launch() {
  local work="$1"
  cp "$LIBRARY/volum.log" "$work/volum.log" 2>/dev/null || true
  cp "$LIBRARY/volum-update-state.json" "$work/" 2>/dev/null || true
}

# A blank REAPER config opens the audio device dialog first and never gets to the
# script, so the device, plug-in paths and version-check stamp are preset. The
# owner's license and nag state come along when this machine has them.
write_reaper_config() {
  local cfgdir="$1" real="$HOME/Library/Application Support/REAPER"
  for f in "$real"/reaper-reginfo*.ini; do
    [[ -f "$f" ]] && cp "$f" "$cfgdir/"
  done
  {
    if [[ -f "$real/reaper.ini" ]]; then
      awk '/^\[nag\]/{p=1; print; next} /^\[/{p=0} p' "$real/reaper.ini"
      echo
    fi
    echo "[reaper]"
    echo "coreaudioindevnew=<default system devices>"
    echo "coreaudiooutdevnew=<default system devices>"
    echo "coreaudiosrate=48000"
    echo "coreaudiobs=512"
    echo "loadlastproj=0"
    echo "splashfeedback=0"
    echo "vstpath_arm64=/Library/Audio/Plug-Ins/VST;/Library/Audio/Plug-Ins/VST3;~/Library/Audio/Plug-Ins/VST;~/Library/Audio/Plug-Ins/VST3"
    echo "vstpath=/Library/Audio/Plug-Ins/VST;/Library/Audio/Plug-Ins/VST3;~/Library/Audio/Plug-Ins/VST;~/Library/Audio/Plug-Ins/VST3"
    echo
    echo "[verchk]"
    echo "lastt=$(date +%s)"
  } > "$cfgdir/reaper.ini"
}

run_reaper() {
  local format="$1" run="$2" label="$format-$run"
  local work="$OUT/$label" cfgdir="$OUT/$label/reaper" fx
  mkdir -p "$cfgdir/Scripts"
  cp "$SCRIPT_DIR/smoke-reaper-mac.lua" "$cfgdir/Scripts/__startup.lua"
  write_reaper_config "$cfgdir"
  if [[ "$format" == "au" ]]; then fx="AU:VoLum"; else fx="VST3:VoLum"; fi

  begin_launch
  # Through LaunchServices, so REAPER runs in the login session: launched straight
  # from an SSH shell it lists no third-party AU.
  open -n -W -a "$REAPER_APP" --env MallocScribble=1 \
    --env "VOLUM_SMOKE_FX=$fx" --env "VOLUM_SMOKE_CYCLES=$CYCLES" \
    --env "VOLUM_SMOKE_RESULT=$work/result.txt" --env "VOLUM_SMOKE_PROJECT=$work/smoke.rpp" \
    --stdout "$work/reaper-stdout.log" --stderr "$work/reaper-stdout.log" \
    --args -newinst -nosplash -cfgfile "$cfgdir/reaper.ini" &
  local pid=$! hung=0 settle=2
  wait_or_kill "$pid" || hung=1
  if (( hung )); then
    screencapture -x "$OUT/$label-hang.png" >/dev/null 2>&1 || true
    pkill -9 -f "cfgfile $cfgdir/reaper.ini" 2>/dev/null || true
  fi
  grep -qx PASS "$work/result.txt" 2>/dev/null || settle=15

  local verdict="PASS"
  if collect_crashes "$label" "$settle"; then
    verdict="CRASH"
  elif (( hung )); then
    verdict="HANG (last: $(tail -1 "$work/result.txt" 2>/dev/null || echo none))"
  elif ! grep -qx PASS "$work/result.txt" 2>/dev/null; then
    verdict="$(grep '^FAIL' "$work/result.txt" 2>/dev/null | head -1 || true)"
    [[ -n "$verdict" ]] || verdict="FAIL no PASS from the REAPER script (last: $(tail -1 "$work/result.txt" 2>/dev/null || echo none))"
  fi
  end_launch "$work"
  echo "$label: $verdict"
  [[ "$verdict" == "PASS" ]] || FAILURES=$((FAILURES + 1))
}

run_app() {
  local run="$1" label="app-$run"
  local work="$OUT/$label"
  begin_launch
  MallocScribble=1 "$APP_BIN" > "$work/app-stdout.log" 2>&1 &
  local pid=$! waited=0 died=0
  # The update check starts when the window opens and finishes within seconds.
  while (( waited < 15 )); do
    if ! kill -0 "$pid" 2>/dev/null; then died=1; break; fi
    sleep 1
    waited=$((waited + 1))
  done
  kill -9 "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true

  local verdict="PASS"
  if collect_crashes "$label" $(( died ? 15 : 2 )); then
    verdict="CRASH"
  elif (( died )); then
    verdict="FAIL exited after ${waited}s without a crash report"
  fi
  end_launch "$work"
  echo "$label: $verdict"
  [[ "$verdict" == "PASS" ]] || FAILURES=$((FAILURES + 1))
}

for format in "${FORMATS[@]}"; do
  for ((run = 1; run <= RUNS; run++)); do
    mkdir -p "$OUT/$format-$run"
    if [[ "$format" == "app" ]]; then run_app "$run"; else run_reaper "$format" "$run"; fi
  done
done

echo "Failures: $FAILURES"
(( FAILURES == 0 ))
