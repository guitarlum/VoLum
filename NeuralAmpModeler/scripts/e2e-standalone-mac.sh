#!/usr/bin/env bash
# End-to-end scenarios for the macOS standalone, the macOS counterpart of
# e2e-standalone-win.ps1: fresh, upgrade, midi and screenshots.
#
# CI only. It installs this run's package into /Applications with sudo, may install
# the BlackHole virtual audio device with Homebrew, and creates VoLum libraries. On a
# developer Mac that replaces the installed VoLum, so it refuses unless
# GITHUB_ACTIONS=true or --throwaway-vm is passed, before anything is mounted,
# installed, launched or written.
#
# Every launch runs the installed binary (VoLum.app/Contents/MacOS/VoLum) directly with
# HOME set to a temp folder: VoLum resolves every library path, settings.ini included,
# from $HOME, so each scenario has its own library and the runner's own is never
# opened. Each launch waits for the main window, answers any alert with OK (logged and
# listed in the job summary, as a user would click it away), settles, runs the
# scenario's drive step, then quits with the quit Apple event addressed to its pid.
# Graceful means it exits by itself with status 0. Every settings.ini names the
# BlackHole 2ch device, mirroring Write-SandboxAudioConfig on Windows: the standalone
# drains MIDI on the audio thread, so a stream has to open.
#
# Negative control first: the first-launch checker every fresh assertion uses runs
# against a real first launch with the right expectations, which must pass, and with a
# wrong Factory Sound, recall line and PLAY board, which must fail on all three; the
# screenshot blank check must reject a flat image and accept a noisy one. Only then
# does it print NEGATIVE CONTROL OK.
#
# Scenarios (mirroring Test-Fresh, Test-Upgrade and Test-Midi):
#   fresh    empty library: opens on PLAY with the "Tweak your sound" note pending, on
#            the first Factory Sound, program 0 recalled, the five Factory Sounds on
#            programs 0-4; a cleared board is not pre-filled again.
#   shots    PLAY (the note visible), BUILD (P key), Settings SIGNAL / MIDI / SYSTEM,
#            each window capture non-blank. A failed or blank capture retries once,
#            logged as RETRY. Without Screen Recording permission a shot is a SKIP with
#            the reason. BUILD saved as the view on quit proves the P key landed.
#   upgrade  a library this run's first launch wrote, rewound to what 1.2.0 wrote
#            (schema v2, no midiSoundMap, no IR trim keys) plus one IR this run
#            generates: launches, no content lost, IR trims measured, schema upgraded,
#            pre-migration backup kept, PLAY pre-filled; a relaunch changes nothing.
#   midi     a virtual CoreMIDI source "VoLum Loop" (python-rtmidi) stored as the MIDI
#            input in settings.ini the way the Mac Preferences dialog stores a pick
#            (indev2 and indev, RtMidi's port name): Program Change, CC 102, an empty
#            program, a burst, One channel = 2; read back from the [midi] lines in
#            volum.log and the saved settings.
#
# Needs python3 with python-rtmidi, pyobjc-framework-Quartz and Pillow, and UI
# scripting / event posting rights (GitHub's macOS runners grant both).
#
#   bash NeuralAmpModeler/scripts/e2e-standalone-mac.sh --artifact-dir <VoLum-mac artifact>

set -o pipefail

ARTIFACT_DIR=""
LAUNCH_TIMEOUT_SEC=60
SETTLE_SEC=6
THROWAWAY_VM=0
ARGS_ERROR=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --artifact-dir) ARTIFACT_DIR="${2:-}"; shift 2 ;;
    --throwaway-vm) THROWAWAY_VM=1; shift ;;
    *) ARGS_ERROR="unknown argument: $1"; shift ;;
  esac
done

if [[ "${GITHUB_ACTIONS:-}" != "true" && "$THROWAWAY_VM" != "1" ]]; then
  echo "REFUSED: e2e-standalone-mac.sh installs VoLum into /Applications with sudo, may install the BlackHole"
  echo "audio device and writes VoLum libraries. It runs on GitHub Actions (GITHUB_ACTIONS=true) or with"
  echo "--throwaway-vm on a machine you are prepared to lose. Nothing was changed."
  exit 2
fi

if [[ -n "$ARGS_ERROR" ]]; then
  echo "ERROR: $ARGS_ERROR" >&2
  exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
HELPER="$SCRIPT_DIR/e2e_standalone_mac.py"
PY="${PYTHON:-python3}"
WORK="${RUNNER_TEMP:-/tmp}/volum-e2e-standalone-mac"
DIAG="$WORK/diagnostics"
SHOTS="$WORK/shots"

APP="/Applications/VoLum.app"
APP_BIN="$APP/Contents/MacOS/VoLum"
AUDIO_DEVICE="BlackHole 2ch"
MIDI_PORT="VoLum Loop"
REAL_HOME="$HOME"
REAL_LIBRARY="$REAL_HOME/Library/Application Support/VoLum"
CRASH_DIRS="$REAL_HOME/Library/Logs/DiagnosticReports /Library/Logs/DiagnosticReports"
# The PLAY board a library that never stored a midiSoundMap gets, once:
# The bestest Clean, SLO Crunch, Modern Rhythm, Crack the Skye, Ampete Lead.
PREFILL="0:factory:12:factory:12:v1|1:factory:13:factory:13:v2|2:factory:6:factory:6:v1|3:factory:8:factory:8:v1|4:factory:0:factory:0:v2"
FIRST_PRESET="factory:12:v1"
FIRST_RECALL="recall slot=0 amp=factory:12 preset=factory:12:v1"

CHECKS=0
FAILED=0
FAILURES=""
SKIPS=""
RETRIES=""
NOTICES=""
LEG="setup"
MOUNT=""
LAUNCHED_PID=""
MIDI_SERVER_PID=""
WINDOWS_SEEN=""

check() {
  local what="$1" ok="$2" detail="${3:-}"
  CHECKS=$((CHECKS + 1))
  local label="[$LEG] $what"
  if [[ "$ok" == "1" ]]; then
    echo "  PASS  $label"
  else
    FAILED=$((FAILED + 1))
    FAILURES="${FAILURES}  - ${label}"$'\n'
    echo "  FAIL  $label${detail:+ - $detail}"
  fi
}

check_eq() {
  if [[ "$2" == "$3" ]]; then check "$1" 1; else check "$1" 0 "expected '$2', got '$3'"; fi
}

skip() {
  echo "  SKIP  [$LEG] $1 - $2"
  SKIPS="${SKIPS}- [$LEG] $1: $2"$'\n'
}

retry_note() {
  echo "  RETRY [$LEG] $1 - $2"
  RETRIES="${RETRIES}- [$LEG] $1: $2"$'\n'
}

truth() {
  if "$@" >/dev/null 2>&1; then echo 1; else echo 0; fi
}

one_line() {
  tr '\n' ';' | sed 's/;$//; s/;/; /g'
}

helper() {
  "$PY" "$HELPER" "$@"
}

json_get() {
  helper json-get "$1" "$2"
}

cleanup() {
  if [[ -n "$LAUNCHED_PID" ]] && kill -0 "$LAUNCHED_PID" 2>/dev/null; then
    kill -9 "$LAUNCHED_PID" 2>/dev/null || true
  fi
  stop_midi_server
  [[ -n "$MOUNT" ]] && { hdiutil detach "$MOUNT" >/dev/null 2>&1 || hdiutil detach -force "$MOUNT" >/dev/null 2>&1 || true; }
}
trap cleanup EXIT

# A child that exited but is not reaped yet is a zombie, which kill -0 still finds.
running() {
  local state
  state="$(ps -p "$1" -o stat= 2>/dev/null | tr -d ' ')"
  [[ -n "$state" && "$state" != Z* ]]
}

other_volum_pids() {
  pgrep -x VoLum 2>/dev/null | grep -vx "${1:-none}" | tr '\n' ' ' | sed 's/ $//'
}

tree_hash() {
  [[ -d "$1" ]] || return 0
  (cd "$1" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256)
}

write_ui_scripts() {
  # Lists the process's windows and answers every alert (a window with an OK
  # button) with OK, printing what it said. The main window has no OK button.
  cat > "$WORK/answer-alerts.applescript" <<'APPLESCRIPT'
on run argv
  set thePid to (item 1 of argv) as integer
  set report to ""
  with timeout of 20 seconds
    tell application "System Events"
      set procs to (every process whose unix id is thePid)
      if procs is {} then return "no process " & thePid
      set p to item 1 of procs
      repeat with w in (every window of p)
        set wname to ""
        try
          set wname to (name of w) as text
        end try
        if exists button "OK" of w then
          set msg to ""
          try
            set AppleScript's text item delimiters to " | "
            set msg to (value of every static text of w) as text
            set AppleScript's text item delimiters to ""
          end try
          click button "OK" of w
          set report to report & "alert: " & msg & linefeed
        else
          set report to report & "window: " & wname & linefeed
        end if
      end repeat
    end tell
  end timeout
  return report
end run
APPLESCRIPT
  # The quit Apple event, addressed to this exact process rather than to whichever
  # VoLum LaunchServices resolves by name.
  cat > "$WORK/quit-pid.js" <<'JXA'
ObjC.import('AppKit');
function run(argv) {
  const app = $.NSRunningApplication.runningApplicationWithProcessIdentifier(parseInt(argv[0], 10));
  if (app.isNil()) return 'not a registered application';
  return app.terminate ? 'quit sent' : 'quit not sent';
}
JXA
}

answer_alerts() {
  local pid="$1" label="$2" result line
  result="$(osascript "$WORK/answer-alerts.applescript" "$pid" 2>&1)"
  WINDOWS_SEEN="$(echo "$result" | sed -n 's/^window: //p')"
  while IFS= read -r line; do
    case "$line" in
      alert:*)
        echo "    $line (answered OK)"
        echo "::warning title=VoLum alert during $label::${line#alert: }"
        NOTICES="$NOTICES- $label: ${line#alert: }"$'\n'
        ;;
      window:*|"") ;;
      *) echo "    System Events: $line" ;;
    esac
  done <<< "$result"
}

library_of() {
  echo "$1/Library/Application Support/VoLum"
}

# BlackHole on both sides, as Write-SandboxAudioConfig does with the default
# DirectSound devices; $2 is appended verbatim (the midi scenario's [midi] section).
write_audio_ini() {
  local lib
  lib="$(library_of "$1")"
  mkdir -p "$lib"
  printf '[audio]\ndriver=0\nindev=%s\noutdev=%s\nin1=1\nout1=1\nout2=2\nbuffer=512\nsr=48000\n%s' \
    "$AUDIO_DEVICE" "$AUDIO_DEVICE" "${2:-}" > "$lib/settings.ini"
}

# Launch the installed binary on HOME=$1 and quit it the way a user would. $3 names a
# function run against the live window after the settle, given the pid. Sets R_STARTED,
# R_GRACEFUL and R_EXIT.
run_volum() {
  local home="$1" label="$2" drive="${3:-}"
  local out="$DIAG/$label-stdout.log" lib waited=0 exited=0 code="" attempt quit_out others
  lib="$(library_of "$home")"
  R_STARTED=0
  R_GRACEFUL=0
  R_EXIT=""
  echo "  $label: HOME=$home $APP_BIN"
  HOME="$home" "$APP_BIN" > "$out" 2>&1 &
  LAUNCHED_PID=$!
  WINDOWS_SEEN=""
  while [[ $waited -lt $LAUNCH_TIMEOUT_SEC ]]; do
    running "$LAUNCHED_PID" || break
    sleep 2
    waited=$((waited + 2))
    answer_alerts "$LAUNCHED_PID" "$label"
    if echo "$WINDOWS_SEEN" | grep -qx 'VoLum'; then R_STARTED=1; break; fi
  done
  if [[ $R_STARTED -eq 1 ]]; then
    waited=0
    while [[ $waited -lt $SETTLE_SEC ]]; do
      sleep 2
      waited=$((waited + 2))
      answer_alerts "$LAUNCHED_PID" "$label"
    done
    if [[ -n "$drive" ]]; then
      "$drive" "$LAUNCHED_PID"
      answer_alerts "$LAUNCHED_PID" "$label"
    fi
  else
    echo "    no main window within ${LAUNCH_TIMEOUT_SEC} s (windows: $(echo "$WINDOWS_SEEN" | one_line))"
    screencapture -x "$DIAG/$label-no-window.png" >/dev/null 2>&1 || true
  fi

  if running "$LAUNCHED_PID"; then
    for attempt in 1 2 3; do
      answer_alerts "$LAUNCHED_PID" "$label"
      quit_out="$(osascript -l JavaScript "$WORK/quit-pid.js" "$LAUNCHED_PID" 2>&1)"
      echo "    quit Apple event to pid $LAUNCHED_PID (attempt $attempt): $quit_out"
      waited=0
      while [[ $waited -lt 20 ]]; do
        if ! running "$LAUNCHED_PID"; then exited=1; break; fi
        sleep 1
        waited=$((waited + 1))
      done
      [[ $exited -eq 1 ]] && break
    done
  else
    echo "    the process was gone before the quit"
  fi
  if running "$LAUNCHED_PID"; then
    echo "    still running after three quit attempts; killed"
    screencapture -x "$DIAG/$label-stuck.png" >/dev/null 2>&1 || true
    kill -9 "$LAUNCHED_PID" 2>/dev/null || true
    wait "$LAUNCHED_PID" 2>/dev/null || true
  else
    wait "$LAUNCHED_PID"
    code=$?
    echo "    exit status $code"
  fi
  R_EXIT="$code"
  [[ $exited -eq 1 && "$code" == "0" ]] && R_GRACEFUL=1
  others="$(other_volum_pids "$LAUNCHED_PID")"
  check "no other VoLum process was started by the $label launch" "$([[ -z "$others" ]] && echo 1 || echo 0)" \
    "$(ps -o pid=,command= -p "${others// /,}" 2>/dev/null | one_line)"
  [[ -n "$others" ]] && kill -9 $others 2>/dev/null
  LAUNCHED_PID=""
  if [[ -d "$lib" ]]; then
    rm -rf "$DIAG/$label-library"
    ditto "$lib" "$DIAG/$label-library" 2>/dev/null || true
  fi
}

assert_launch() {
  local prefix="${1:-}"
  check "${prefix}app opened its main window" "$R_STARTED" "windows: $(echo "$WINDOWS_SEEN" | one_line)"
  check "${prefix}app closed gracefully (quit Apple event, exit status 0)" "$R_GRACEFUL" "exit status '$R_EXIT'"
}

# --------------------------------------------------------------------------
# The first-launch checker. Each expect_* prints one problem, or nothing.
# --------------------------------------------------------------------------
expect_setting() {
  local file="$1" key="$2" expected="$3" actual
  actual="$(json_get "$file" "$key")"
  [[ "$actual" == "$expected" ]] || echo "$key is '$actual', expected '$expected'"
}

expect_log_line() {
  [[ -f "$1" ]] && grep -qF "$2" "$1" || echo "volum.log has no line '$2'"
}

expect_map() {
  local actual
  actual="$(helper mapkey "$1")"
  [[ "$actual" == "$2" ]] || echo "PLAY board is '$actual', expected '$2'"
}

fresh_problems() {
  local lib="$1" preset="$2" recall="$3" map="$4"
  expect_setting "$lib/volum-settings.json" volumUiMode play
  expect_setting "$lib/volum-settings.json" buildTipSeen false
  expect_setting "$lib/volum-settings.json" volumActivePresetId "$preset"
  expect_log_line "$lib/volum.log" "$recall"
  expect_map "$lib/content/volum-content.json" "$map"
}

negative_control() {
  LEG="negative control"
  echo
  echo "[negative control] the first-launch checker and the blank check must reject wrong expectations"
  local home="$WORK/negative/home" lib right wrong rejected flat noisy
  lib="$(library_of "$home")"
  write_audio_ini "$home"
  run_volum "$home" "negative-control"
  right="$(fresh_problems "$lib" "$FIRST_PRESET" "$FIRST_RECALL" "$PREFILL")"
  wrong="$(fresh_problems "$lib" "factory:99:v9" "recall slot=0 amp=factory:99 preset=factory:99:v9" \
    "0:factory:99:factory:99:v9")"
  echo "  right expectations: ${right:-accepted}"
  echo "  wrong expectations: ${wrong:-accepted}"
  rejected="$(echo "$wrong" | grep -c "factory:99" || true)"
  helper flat-png "$WORK/flat.png"
  helper noise-png "$WORK/noise.png"
  flat="$(helper variance "$WORK/flat.png" && echo accepted || echo rejected)"
  noisy="$(helper variance "$WORK/noise.png" && echo accepted || echo rejected)"
  echo "  blank check: flat image ${flat##*$'\n'}, noisy image ${noisy##*$'\n'}"
  if [[ -n "$right" || "$rejected" -ne 3 || "${flat##*$'\n'}" != "rejected" || "${noisy##*$'\n'}" != "accepted" ]]; then
    echo "NEGATIVE CONTROL FAILED: the checker must accept the first launch as it is, reject the wrong Factory"
    echo "Sound, recall line and PLAY board (3 problems, got $rejected), reject a flat image and accept a noisy one."
    [[ -f "$lib/volum.log" ]] && sed 's/^/    volum.log: /' "$lib/volum.log"
    exit 1
  fi
  echo "NEGATIVE CONTROL OK"
}

# --------------------------------------------------------------------------
# Scenario: first launch on a machine that has never run VoLum
# --------------------------------------------------------------------------
test_fresh() {
  LEG="fresh"
  echo
  echo "[fresh] first launch with no existing state"
  local home="$WORK/fresh/home" lib settings content
  lib="$(library_of "$home")"
  settings="$lib/volum-settings.json"
  content="$lib/content/volum-content.json"
  write_audio_ini "$home"
  run_volum "$home" "fresh"
  assert_launch

  check "volum-settings.json written on first run" "$(truth helper parses "$settings")"
  check "settings carry a schema version" "$([[ "$(json_get "$settings" version)" != "<missing>" && "$(json_get "$settings" version)" != "null" ]] && echo 1 || echo 0)"
  check "settings carry per-amp scenes" "$(truth helper has-key "$settings" amps)"
  local p
  p="$(expect_setting "$settings" volumUiMode play)"; check "a fresh install opens on PLAY" "$([[ -z "$p" ]] && echo 1 || echo 0)" "$p"
  p="$(expect_setting "$settings" buildTipSeen false)"; check "the Tweak your sound note is still pending" "$([[ -z "$p" ]] && echo 1 || echo 0)" "$p"
  p="$(expect_setting "$settings" volumActivePresetId "$FIRST_PRESET")"; check "a fresh install starts on the first Factory Sound" "$([[ -z "$p" ]] && echo 1 || echo 0)" "$p"

  check "diagnostic log created" "$([[ -f "$lib/volum.log" ]] && echo 1 || echo 0)"
  check "log records startup" "$(truth grep -q 'startup' "$lib/volum.log")"
  check "log records the audio configuration" "$(truth grep -qE 'reset|samplerate|sample rate|block' "$lib/volum.log")"
  p="$(expect_log_line "$lib/volum.log" "$FIRST_RECALL")"; check "first launch recalls program 0" "$([[ -z "$p" ]] && echo 1 || echo 0)" "$p"

  check "first launch wrote volum-content.json" "$(truth helper parses "$content")"
  p="$(expect_map "$content" "$PREFILL")"; check "PLAY pre-filled with five Factory Sounds on programs 0-4" "$([[ -z "$p" ]] && echo 1 || echo 0)" "$p"

  if helper parses "$content"; then
    # A player who cleared every switch keeps the empty board.
    helper clear-map "$content"
    run_volum "$home" "fresh-cleared"
    assert_launch "relaunch with a cleared board: "
    check "cleared board keeps its midiSoundMap key" "$(truth helper has-key "$content" midiSoundMap)"
    check_eq "cleared board is not pre-filled again" "" "$(helper mapkey "$content")"
  fi
}

# --------------------------------------------------------------------------
# Scenario: screenshots of PLAY, BUILD and the three Settings tabs
# --------------------------------------------------------------------------
SHOT_N=0
shot_window() {
  local pid="$1" out="$2" wid
  wid="$(helper window "$pid" 2>/dev/null | awk 'NF == 5 {print $1}')"
  [[ -n "$wid" ]] || { echo "no on-screen window for pid $pid"; return 1; }
  rm -f "$out"
  screencapture -x -o -l "$wid" "$out" 2>&1 || return 1
  [[ -s "$out" ]] || { echo "screencapture wrote nothing"; return 1; }
  helper variance "$out"
}

# One shot: $2 is the name, $3.. the step that reaches the state. Only the capture is
# retried: the steps toggle (P, the gear), so running one twice would undo it.
take_shot() {
  local pid="$1" name="$2"
  shift 2
  SHOT_N=$((SHOT_N + 1))
  local out
  out="$SHOTS/$(printf '%02d' "$SHOT_N")-$name.png"
  local result attempt
  if [[ $# -gt 0 ]]; then "$@" > "$DIAG/shot-$name-step.log" 2>&1; fi
  for attempt in 1 2; do
    sleep 1.2
    result="$(shot_window "$pid" "$out")"
    if [[ $? -eq 0 ]]; then
      echo "    $name: $result"
      SHOT_OK="$SHOT_OK $name"
      return 0
    fi
    [[ $attempt -eq 1 ]] && retry_note "screenshot $name" "$(echo "$result" | one_line)"
  done
  rm -f "$out"
  if [[ "$SCREEN_ACCESS" == "denied" ]]; then
    skip "screenshot $name" "this runner has not granted Screen Recording (CGPreflightScreenCaptureAccess is false): $(echo "$result" | one_line)"
  else
    SHOT_FAILED="$SHOT_FAILED $name"
    SHOT_DETAIL="$SHOT_DETAIL $name: $(echo "$result" | one_line);"
  fi
}

click_canvas() {
  helper click-canvas "$1" "$2" "$3"
}

press_p() {
  click_canvas "$1" 10 10 && sleep 0.5 && helper key "$1" p
}

open_settings_signal() {
  click_canvas "$1" 869 22 && sleep 1 && click_canvas "$1" 300 113
}

drive_shots() {
  local pid="$1"
  take_shot "$pid" play
  take_shot "$pid" build press_p "$pid"
  take_shot "$pid" settings-signal open_settings_signal "$pid"
  take_shot "$pid" settings-midi click_canvas "$pid" 450 113
  take_shot "$pid" settings-system click_canvas "$pid" 600 113
  screencapture -x "$DIAG/shots-screen.png" >/dev/null 2>&1 || true
}

test_shots() {
  LEG="shots"
  echo
  echo "[shots] PLAY with the Tweak your sound note, BUILD (P), Settings SIGNAL / MIDI / SYSTEM"
  local home="$WORK/shots-home" lib name
  lib="$(library_of "$home")"
  mkdir -p "$SHOTS"
  SCREEN_ACCESS="$(helper screen-access 2>/dev/null || echo unknown)"
  echo "  Screen Recording preflight: $SCREEN_ACCESS"
  SHOT_OK=""
  SHOT_FAILED=""
  SHOT_DETAIL=""
  write_audio_ini "$home"
  run_volum "$home" "shots" drive_shots
  assert_launch
  for name in play build settings-signal settings-midi settings-system; do
    if [[ " $SHOT_OK " == *" $name "* ]]; then
      check "screenshot $name captured and not blank" 1
    elif [[ " $SHOT_FAILED " == *" $name "* ]]; then
      check "screenshot $name captured and not blank" 0 "$SHOT_DETAIL"
    elif [[ "$SKIPS" != *"screenshot $name:"* ]]; then
      check "screenshot $name captured and not blank" 0 "the drive step never reached it"
    fi
  done
  check_eq "the P key reached VoLum: BUILD is saved as the view on quit" "build" "$(json_get "$lib/volum-settings.json" volumUiMode)"
  echo "    buildTipSeen after the BUILD switch: $(json_get "$lib/volum-settings.json" buildTipSeen)"
}

# --------------------------------------------------------------------------
# Scenario: in-place upgrade from a 1.2.0-shaped library
# --------------------------------------------------------------------------
test_upgrade() {
  LEG="upgrade"
  echo
  echo "[upgrade] a library this run wrote, rewound to schema v2, upgraded in place"
  local home="$WORK/upgrade/home" lib content before measurable after_ir again_ir after_map kind line
  lib="$(library_of "$home")"
  content="$lib/content/volum-content.json"
  before="$WORK/upgrade/before.json"
  write_audio_ini "$home"
  run_volum "$home" "upgrade-seed"
  assert_launch "seed launch: "
  if ! helper parses "$content"; then
    check "the seed launch wrote volum-content.json" 0
    return
  fi
  rm -f "$lib/volum.log"

  # What 1.2.0 would have written: schema v2, no midiSoundMap and no IR shaping keys.
  helper rewind-v2 "$content" "ir_e2e_cab" "ir/ir_e2e_cab__cab.wav"
  cp "$content" "$before"
  measurable="$(helper measurable "$content")"
  echo "  rewound: schema $(json_get "$before" schemaVersion), midiSoundMap $(truth helper has-key "$before" midiSoundMap | sed 's/1/present/; s/0/absent/'), $(helper count "$before" irLibrary) IR(s), $measurable readable"

  run_volum "$home" "upgrade"
  assert_launch
  check "upgraded registry still parses" "$(truth helper parses "$content")"
  while IFS= read -r line; do
    kind="${line%%=*}"
    check "no $kind lost across launch" "$([[ -z "${line#*=}" ]] && echo 1 || echo 0)" "missing: ${line#*=}"
  done < <(helper lost "$before" "$content")
  # What a launch rewrites is decided by the IR trim migration alone (it saves when it
  # measured at least one IR), as Test-Upgrade asserts on Windows.
  if [[ "$measurable" -gt 0 ]]; then
    check_eq "registry upgraded to the current schema v$SCHEMA" "$SCHEMA" "$(json_get "$content" schemaVersion)"
    check_eq "every readable IR gained a measured trim" "$measurable" "$(helper calibrated "$content")"
    check "pre-migration backup kept" "$([[ -f "$lib/content/volum-content.json.pre-1.2.1.bak" ]] && echo 1 || echo 0)"
  else
    check_eq "nothing to migrate: the schema-v2 file is not rewritten at launch" "2" "$(json_get "$content" schemaVersion)"
  fi
  check "corrupt-file backup NOT triggered" "$([[ ! -e "$lib/content/volum-content.json.bak" ]] && echo 1 || echo 0)"
  check_eq "1.2.x library gets the PLAY pre-fill on programs 0-4" "$PREFILL" "$(helper mapkey "$content")"

  # Second launch must be a no-op, and the pre-fill is one-time: a moved switch stays moved.
  helper move-slot "$content" 2 "factory:6:v2" >/dev/null
  after_ir="$(helper ir-json "$content")"
  after_map="$(helper mapkey "$content")"
  run_volum "$home" "upgrade-relaunch"
  assert_launch "second launch: "
  again_ir="$(helper ir-json "$content")"
  check_eq "migration is idempotent across relaunch" "$after_ir" "$again_ir"
  check_eq "pre-fill does not repeat over the player's edit" "$after_map" "$(helper mapkey "$content")"
}

# --------------------------------------------------------------------------
# Scenario: Program Change and CC 102 through CoreMIDI
#
# python-rtmidi publishes a virtual CoreMIDI source; the standalone opens it as its
# MIDI input from settings.ini, so every message takes a pedalboard's path: RtMidi,
# the audio callback, ProcessMidiMsg, the latest-wins queue, OnIdle. OnIdle writes one
# [midi] line per drained program to volum.log - "recall slot=N amp=.. preset=.." or
# "slot=N has no playable Sound; ignored" - which is what is read back.
# --------------------------------------------------------------------------
start_midi_server() {
  local fifo="$WORK/midi.fifo" ready="$WORK/midi.ready" i
  rm -f "$fifo" "$ready"
  mkfifo "$fifo"
  "$PY" "$HELPER" midi-serve "$MIDI_PORT" "$ready" < "$fifo" > "$DIAG/midi-serve.log" 2>&1 &
  MIDI_SERVER_PID=$!
  exec 7> "$fifo"
  for i in $(seq 1 100); do
    [[ -f "$ready" ]] && break
    running "$MIDI_SERVER_PID" || break
    sleep 0.1
  done
  MIDI_INPUT_NAME="$( [[ -f "$ready" ]] && head -n 1 "$ready")"
  echo "  MIDI inputs RtMidi lists: $( [[ -f "$ready" ]] && tail -n +2 "$ready" | one_line)"
}

stop_midi_server() {
  if [[ -n "$MIDI_SERVER_PID" ]]; then
    running "$MIDI_SERVER_PID" && echo "quit" >&7
    exec 7>&-
    sleep 0.5
    kill "$MIDI_SERVER_PID" 2>/dev/null || true
    wait "$MIDI_SERVER_PID" 2>/dev/null || true
    MIDI_SERVER_PID=""
  fi
}

midi_send() {
  running "$MIDI_SERVER_PID" && echo "$*" >&7
}

midi_lines() {
  [[ -f "$1" ]] && grep -F '[midi] ' "$1" | sed 's/^.*\[midi\] //'
}

midi_count() {
  midi_lines "$1" | grep -c . || true
}

# The [midi] lines written after the first $2, once one of them equals $3 or $4 ms run
# out. An empty $3 just waits $4 ms, for messages that must leave no line.
wait_midi() {
  local log="$1" after="$2" want="$3" ms="$4" spent=0 new
  while :; do
    sleep 0.1
    spent=$((spent + 100))
    new="$(midi_lines "$log" | tail -n +$((after + 1)))"
    if [[ -n "$want" ]] && grep -qxF "$want" <<< "$new"; then break; fi
    [[ $spent -ge $ms ]] && break
  done
  printf '%s' "$new"
}

recall_of() {
  case "$1" in
    3) echo "recall slot=3 amp=factory:12 preset=factory:12:v1" ;;    # The bestest Clean
    9) echo "recall slot=9 amp=factory:6 preset=factory:6:v1" ;;      # Modern Rhythm
    42) echo "recall slot=42 amp=factory:0 preset=factory:0:v2" ;;    # Ampete Lead
    100) echo "recall slot=100 amp=factory:8 preset=factory:8:v1" ;;  # Crack the Skye
    101) echo "recall slot=101 amp=factory:13 preset=factory:13:v2" ;; # SLO Crunch
  esac
}

drive_midi_omni() {
  local log="$MIDI_LOG" n p
  n="$(midi_count "$log")"; midi_send send 0xC0 9
  M_PC="$(wait_midi "$log" "$n" "$(recall_of 9)" 5000)"
  n="$(midi_count "$log")"; midi_send send 0xB0 102 42
  M_CC="$(wait_midi "$log" "$n" "$(recall_of 42)" 5000)"
  n="$(midi_count "$log")"; midi_send repeat 64 0xFE; midi_send send 0xC0 9
  M_FLOOD="$(wait_midi "$log" "$n" "$(recall_of 9)" 5000)"
  n="$(midi_count "$log")"; midi_send send 0xC0 7
  M_EMPTY="$(wait_midi "$log" "$n" "slot=7 has no playable Sound; ignored" 3000)"
  n="$(midi_count "$log")"
  for p in 3 100 9 42 101; do midi_send send 0xC0 "$p"; done
  wait_midi "$log" "$n" "$(recall_of 101)" 5000 >/dev/null
  # Anything still queued behind the last one would land now.
  M_BURST="$(wait_midi "$log" "$n" "" 1500)"
  M_DRIVEN=1
}

drive_midi_ch2() {
  local log="$MIDI_LOG" n
  n="$(midi_count "$log")"; midi_send send 0xC0 3; midi_send send 0xB0 102 9
  M_CH1="$(wait_midi "$log" "$n" "" 2000)"
  n="$(midi_count "$log")"; midi_send send 0xB1 102 100
  M_CC2="$(wait_midi "$log" "$n" "$(recall_of 100)" 5000)"
  n="$(midi_count "$log")"; midi_send send 0xC1 42
  M_PC2="$(wait_midi "$log" "$n" "$(recall_of 42)" 5000)"
  M_DRIVEN=1
}

has_line() {
  grep -qxF "$2" <<< "$1" && echo 1 || echo 0
}

test_midi() {
  LEG="midi"
  echo
  echo "[midi] Program Change and CC 102 recall PLAY Sounds through the virtual CoreMIDI source '$MIDI_PORT'"
  local home="$WORK/midi/home" lib ini settings content seeded burst_last
  lib="$(library_of "$home")"
  ini="$lib/settings.ini"
  settings="$lib/volum-settings.json"
  content="$lib/content/volum-content.json"
  MIDI_LOG="$lib/volum.log"

  start_midi_server
  check "the virtual CoreMIDI source '$MIDI_PORT' is listed as a MIDI input" "$([[ -n "$MIDI_INPUT_NAME" ]] && echo 1 || echo 0)" \
    "$(one_line < "$DIAG/midi-serve.log")"
  if [[ -z "$MIDI_INPUT_NAME" ]]; then
    stop_midi_server
    return
  fi

  # A pick in the Mac Preferences dialog stores the RtMidi port name as indev2 (1.3+)
  # and indev (1.2.x); see IPlugAPPHost::UpdateINI.
  write_audio_ini "$home" "$(printf '[midi]\nindev=%s\nindev2=%s\noutdev=off\ninchan=0\noutchan=0\n' "$MIDI_INPUT_NAME" "$MIDI_INPUT_NAME")"
  helper seed-board "$content" "$SCHEMA"
  seeded="$(helper mapkey "$content")"

  # Omni (no midiCh stored yet): Program Change, CC 102, an empty program, a burst.
  M_DRIVEN=0
  run_volum "$home" "midi" drive_midi_omni
  assert_launch "" 
  check "the standalone opened an audio stream (MIDI is drained on the audio thread)" "$(truth grep -q 'stream open:' "$MIDI_LOG")" \
    "$(grep -F '[audio]' "$MIDI_LOG" 2>/dev/null | sed 's/^.*\[audio\] //' | one_line)"
  check "the standalone opened '$MIDI_INPUT_NAME'" "$([[ -f "$MIDI_LOG" ]] && ! grep -q 'could not open the saved MIDI port' "$MIDI_LOG" && echo 1 || echo 0)"
  check "settings.ini keeps '$MIDI_INPUT_NAME' as the MIDI input (indev2)" "$(truth grep -qxF "indev2=$MIDI_INPUT_NAME" "$ini")" "$(grep -E '^indev' "$ini" | one_line)"
  check "settings.ini keeps the legacy MIDI spelling for older builds (indev)" "$(truth grep -qxF "indev=$MIDI_INPUT_NAME" "$ini")" "$(grep -E '^indev' "$ini" | one_line)"
  if [[ "$M_DRIVEN" != "1" ]]; then
    check "drive step ran" 0
  else
    check "Program Change 9 recalls program 9 (Modern Rhythm)" "$(has_line "$M_PC" "$(recall_of 9)")" "midi lines: $(echo "$M_PC" | one_line)"
    check "CC 102 value 42 recalls program 42 (Ampete Lead)" "$(has_line "$M_CC" "$(recall_of 42)")" "midi lines: $(echo "$M_CC" | one_line)"
    check "64 Active Sensing messages cannot crowd out the following Program Change" "$(has_line "$M_FLOOD" "$(recall_of 9)")" \
      "midi lines: $(echo "$M_FLOOD" | one_line)"
    check "Program Change 7 reaches VoLum and is ignored: program 7 is empty" \
      "$(has_line "$M_EMPTY" "slot=7 has no playable Sound; ignored")" "midi lines: $(echo "$M_EMPTY" | one_line)"
    check "the empty program recalls nothing" "$(grep -q '^recall ' <<< "$M_EMPTY" && echo 0 || echo 1)" "midi lines: $(echo "$M_EMPTY" | one_line)"
    burst_last="$(grep '^recall ' <<< "$M_BURST" | tail -n 1)"
    check_eq "a burst of Program Changes 3, 100, 9, 42, 101 ends on 101 (SLO Crunch)" "$(recall_of 101)" "$burst_last"
  fi
  check_eq "saved PLAY cursor is the last program played" "101" "$(json_get "$settings" lastPlaySlot)"
  check_eq "saved active preset is the last Sound recalled" "factory:13:v2" "$(json_get "$settings" volumActivePresetId)"
  check_eq "MIDI recalls leave the PLAY board as seeded" "$seeded" "$(helper mapkey "$content")"

  # One channel = 2: the decoder drops channel-1 messages on the audio thread, so
  # they leave no line at all; channel 2 still recalls.
  LEG="midi one channel 2"
  if helper parses "$settings"; then
    helper json-set "$settings" midiCh 2
  fi
  M_DRIVEN=0
  run_volum "$home" "midi-ch2" drive_midi_ch2
  assert_launch
  if [[ "$M_DRIVEN" != "1" ]]; then
    check "drive step ran" 0
  else
    check_eq "channel-1 Program Change 3 and CC 102 value 9 are ignored" "" "$(echo "$M_CH1" | one_line)"
    check "channel-2 CC 102 value 100 recalls program 100 (Crack the Skye)" "$(has_line "$M_CC2" "$(recall_of 100)")" "midi lines: $(echo "$M_CC2" | one_line)"
    check "channel-2 Program Change 42 recalls program 42 (Ampete Lead)" "$(has_line "$M_PC2" "$(recall_of 42)")" "midi lines: $(echo "$M_PC2" | one_line)"
  fi
  check_eq "One channel = 2 survives the quit" "2" "$(json_get "$settings" midiCh)"
  check_eq "saved PLAY cursor is program 42" "42" "$(json_get "$settings" lastPlaySlot)"
  check_eq "saved active preset is program 42's Sound" "factory:0:v2" "$(json_get "$settings" volumActivePresetId)"
  stop_midi_server
}

# --------------------------------------------------------------------------
# Setup: this run's package, the audio device, the helpers
# --------------------------------------------------------------------------
write_choices() {
  local pkg="$1" out="$2"
  installer -showChoicesXML -pkg "$pkg" > "$out.offered" || return 1
  "$PY" - "$out.offered" "$out" <<'PY'
import plistlib
import sys

with open(sys.argv[1], "rb") as f:
    offered = plistlib.load(f)
ids = []

def walk(items):
    for item in items:
        cid = item.get("choiceIdentifier", "")
        if cid.startswith("com.Lum.") and cid not in ids:
            ids.append(cid)
        walk(item.get("childItems", []))

walk(offered)
with open(sys.argv[2], "wb") as f:
    plistlib.dump([{"choiceIdentifier": i, "choiceAttribute": "selected", "attributeSetting": 1} for i in ids], f)
print(" ".join(ids))
PY
}

setup() {
  LEG="setup"
  echo
  echo "[setup] this run's package, the audio device, the helpers"
  local dmg="$ARTIFACT_DIR/VoLum-v$NEW_VERSION-mac.dmg" pkg ids code reported
  check "no VoLum.app in /Applications before the install" "$([[ ! -e "$APP" ]] && echo 1 || echo 0)"
  check "this run's installer DMG is in the VoLum-mac artifact" "$([[ -f "$dmg" ]] && echo 1 || echo 0)" "$(ls "$ARTIFACT_DIR" 2>/dev/null | one_line)"
  [[ -f "$dmg" ]] || return 1
  MOUNT="$WORK/dmg"
  mkdir -p "$MOUNT"
  if ! hdiutil attach "$dmg" -nobrowse -readonly -mountpoint "$MOUNT" >/dev/null; then
    check "mounted $dmg" 0
    MOUNT=""
    return 1
  fi
  pkg="$MOUNT/VoLum Installer.pkg"
  ids="$(write_choices "$pkg" "$WORK/choices.plist")"
  echo "  sudo installer -pkg \"$pkg\" -target / (choices: $ids)"
  sudo installer -pkg "$pkg" -target / -applyChoiceChangesXML "$WORK/choices.plist" 2>&1 | tee "$DIAG/installer.log" | sed 's/^/    /'
  code=${PIPESTATUS[0]}
  hdiutil detach "$MOUNT" >/dev/null 2>&1 || hdiutil detach -force "$MOUNT" >/dev/null 2>&1 || true
  MOUNT=""
  check_eq "this run's package installs (exit 0)" 0 "$code"
  reported="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP/Contents/Info.plist" 2>/dev/null)"
  check_eq "the installed standalone is this run's $NEW_VERSION" "$NEW_VERSION" "$reported"
  [[ -x "$APP_BIN" ]] || return 1

  if ! system_profiler SPAudioDataType 2>/dev/null | grep -qF "$AUDIO_DEVICE"; then
    echo "  no '$AUDIO_DEVICE' on this runner; brew install --cask blackhole-2ch"
    HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_CLEANUP=1 brew install --cask blackhole-2ch 2>&1 | tail -n 5 | sed 's/^/    /'
    sudo killall coreaudiod >/dev/null 2>&1 || true
    sleep 8
  fi
  system_profiler SPAudioDataType 2>/dev/null > "$DIAG/audio-devices.txt"
  check "CoreAudio lists '$AUDIO_DEVICE'" "$(truth grep -qF "$AUDIO_DEVICE" "$DIAG/audio-devices.txt")"

  check "the helpers' Python packages import (rtmidi, Quartz, PIL)" "$(truth "$PY" -c 'import rtmidi, Quartz, PIL')"
  REAL_LIBRARY_EXISTED="$([[ -e "$REAL_LIBRARY" ]] && echo 1 || echo 0)"
  REAL_LIBRARY_HASH="$(tree_hash "$REAL_LIBRARY")"
  echo "  runner library $REAL_LIBRARY: $([[ $REAL_LIBRARY_EXISTED -eq 1 ]] && echo "present, $(echo "$REAL_LIBRARY_HASH" | grep -c .) files" || echo absent)"
  return 0
}

finish_checks() {
  LEG="teardown"
  echo
  echo "[teardown] the runner's own state"
  local reports dir
  check "the runner's own library was not touched by any temp-HOME launch" \
    "$([[ "$([[ -e "$REAL_LIBRARY" ]] && echo 1 || echo 0)" == "$REAL_LIBRARY_EXISTED" && "$(tree_hash "$REAL_LIBRARY")" == "$REAL_LIBRARY_HASH" ]] && echo 1 || echo 0)" \
    "$(ls -la "$REAL_LIBRARY" 2>/dev/null | one_line)"
  sleep 5
  reports="$(for dir in $CRASH_DIRS; do [[ -d "$dir" ]] && find "$dir" -maxdepth 2 -type f -iname 'VoLum*' -newer "$START_MARKER" 2>/dev/null; done)"
  check "no VoLum crash report under DiagnosticReports" "$([[ -z "$reports" ]] && echo 1 || echo 0)" "$(echo "$reports" | one_line)"
  if [[ -n "$reports" ]]; then
    mkdir -p "$DIAG/crash-reports"
    echo "$reports" | while IFS= read -r f; do cp "$f" "$DIAG/crash-reports/" 2>/dev/null; done
  fi
}

summary_section() {
  local title="$1" body="$2"
  [[ -n "$body" ]] || return 0
  echo
  echo "$title:"
  printf '%s' "$body" | sed 's/^/  /'
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    { echo "### $title"; echo; printf '%s' "$body"; echo; } >> "$GITHUB_STEP_SUMMARY"
  fi
}

rm -rf "$WORK"
mkdir -p "$DIAG" "$SHOTS"
START_MARKER="$WORK/start.marker"
touch "$START_MARKER"
write_ui_scripts

if [[ -z "$ARTIFACT_DIR" || ! -d "$ARTIFACT_DIR" ]]; then
  echo "ERROR: pass --artifact-dir: the downloaded VoLum-mac artifact of this run." >&2
  exit 1
fi
NEW_VERSION="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)"/\1/p' "$REPO_ROOT/NeuralAmpModeler/config.h")"
SCHEMA="$(sed -n 's/.*kContentSchemaVersion *= *\([0-9][0-9]*\) *;.*/\1/p' "$REPO_ROOT/NeuralAmpModeler/VoLumContentStore.h" | head -n 1)"
if [[ -z "$NEW_VERSION" || -z "$SCHEMA" ]]; then
  echo "ERROR: PLUG_VERSION_STR (config.h) or kContentSchemaVersion (VoLumContentStore.h) not found" >&2
  exit 1
fi

echo "VoLum macOS standalone end-to-end scenarios"
echo "  artifact: $ARTIFACT_DIR ($NEW_VERSION, content schema v$SCHEMA)"
echo "  app:      $APP_BIN"
sw_vers | sed 's/^/  /'

if ! setup; then
  echo
  echo "FAILED $FAILED of $CHECKS checks (setup did not complete)"
  printf '%s' "$FAILURES"
  exit 1
fi

negative_control
test_fresh
test_shots
test_upgrade
test_midi
finish_checks

summary_section "Alerts the standalone showed (answered OK)" "$NOTICES"
summary_section "Retried GUI steps" "$RETRIES"
summary_section "Skipped" "$SKIPS"

echo
if [[ $FAILED -eq 0 ]]; then
  echo "ALL PASS $CHECKS checks"
  [[ -n "${GITHUB_STEP_SUMMARY:-}" ]] && echo "macOS standalone e2e: ALL PASS $CHECKS checks" >> "$GITHUB_STEP_SUMMARY"
  exit 0
fi
echo "FAILED $FAILED of $CHECKS checks"
printf '%s' "$FAILURES"
exit 1
