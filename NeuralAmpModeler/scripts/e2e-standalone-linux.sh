#!/usr/bin/env bash
# End-to-end scenarios for the Linux standalone, the Linux counterpart of
# e2e-standalone-win.ps1 and e2e-standalone-mac.sh.
#
# Its own Xvfb (no window manager), jackd's dummy driver, a null ALSA PCM and a
# dead Pulse socket, and a sandbox XDG_DATA_HOME per scenario. Nothing here
# opens the machine's speakers. On the box, each scenario holds
# ~/volum-linux/locks/gui.lock and drops it before the next one.
#
#   bash NeuralAmpModeler/scripts/e2e-standalone-linux.sh [--app PATH] [--out DIR] [--scenario NAME]
#
# Scenario names match the Windows harness where that scenario exists:
#   fresh        first launch, PLAY, Factory Sound, program 0, cleared board
#   presets      per-amp preset memory, then a PLAY Sound switch and a sidebar amp switch
#   pack         Everything export/open through VOLUM_PACK_SAVE_PATH / VOLUM_PACK_OPEN_PATH
#   import       Manage > Add for IRs and pedals through VOLUM_IMPORT_OPEN_PATHS
#   midi         Program Change and CC 102 on an ALSA virtual port (RtMidi)
#   preferences  --preferences lists the dummy JACK device ("system")
#   shutdown     SIGTERM quits like the close button; a second signal exits at once
# Carried from ticket 09 (fake zenity / kdialog; the VOLUM_* hooks do not run these):
#   dialog-export   fake zenity save path with a space; cancel writes nothing
#   dialog-open     fake zenity open; cancel leaves the library
#   dialog-import   no zenity, fake kdialog returns two IRs; cancel imports nothing
#   dialog-builder  fake zenity returns one .nam; the draft lists it; cancel does not
#
# MIDI needs /dev/snd/seq (snd-seq, or snd-virmidi which pulls it in). The script
# tries `sudo -n modprobe`. A missing sequencer fails the midi scenario.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP="$ROOT/build-linux/standalone/VoLum"
OUT=""
SCENARIO="all"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app) APP="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --scenario) SCENARIO="$2"; shift 2 ;;
    *) echo "usage: $0 [--app PATH] [--out DIR] [--scenario NAME]" >&2; exit 2 ;;
  esac
done

[[ -x "$APP" ]] || { echo "e2e: no app at $APP (build it with NeuralAmpModeler/scripts/build-linux.sh)" >&2; exit 2; }
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
OUT="${OUT:-$(mktemp -d /tmp/volum-e2e-linux.XXXXXX)}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

for tool in Xvfb jackd xdotool xwd convert python3 tesseract gcc; do
  command -v "$tool" >/dev/null || { echo "e2e: $tool is missing (provision-linux.sh installs it)" >&2; exit 2; }
done

CHECKS=0
FAILED=0
FAILURES=""
PREFILL="0:factory:12:factory:12:v1|1:factory:13:factory:13:v2|2:factory:6:factory:6:v1|3:factory:8:factory:8:v1|4:factory:0:factory:0:v2"

check() {
  local what="$1" ok="$2" detail="${3:-}"
  CHECKS=$((CHECKS + 1))
  if [[ "$ok" == 1 ]]; then
    echo "  PASS  [$LEG] $what"
  else
    FAILED=$((FAILED + 1))
    FAILURES="${FAILURES}  - [$LEG] ${what}"$'\n'
    echo "  FAIL  [$LEG] $what${detail:+ - $detail}"
  fi
}

check_eq() {
  if [[ "$2" == "$3" ]]; then check "$1" 1; else check "$1" 0 "expected '$2', got '$3'"; fi
}

want() { [[ "$SCENARIO" == "all" || "$SCENARIO" == "$1" ]]; }

# --- session: Xvfb, jackd dummy, sandbox library ---------------------------

SESSION=0
XVFB_PID=""
JACK_PID=""
APP_PID=""
GUI_LOCK_HELD=0
SESS=""
DATA=""
LOG=""
LOG_MARK=0
DISPLAY=""
LAUNCH_ENV=()

lock_gui() {
  GUI_LOCK_HELD=0
  local lock="${VOLUM_GUI_LOCK:-$HOME/volum-linux/locks/gui.lock}"
  if [[ -e "$lock" ]]; then
    exec 9>>"$lock"
    if ! flock -w 1800 9; then
      echo "e2e: gui.lock busy for 30 min" >&2
      return 1
    fi
    GUI_LOCK_HELD=1
    echo "  gui.lock held"
  fi
}

unlock_gui() {
  if [[ "$GUI_LOCK_HELD" == 1 ]]; then
    flock -u 9 || true
    exec 9>&- || true
    GUI_LOCK_HELD=0
    echo "  gui.lock released"
  fi
}

end_session() {
  [[ "$SESSION" == 1 ]] || return 0
  SESSION=0
  if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
    kill -9 "$APP_PID" 2>/dev/null || true
    wait "$APP_PID" 2>/dev/null || true
  fi
  APP_PID=""
  if [[ -n "$JACK_PID" ]]; then
    kill "$JACK_PID" 2>/dev/null || true
    wait "$JACK_PID" 2>/dev/null || true
  fi
  JACK_PID=""
  if [[ -n "$XVFB_PID" ]]; then
    kill "$XVFB_PID" 2>/dev/null || true
    wait "$XVFB_PID" 2>/dev/null || true
  fi
  XVFB_PID=""
  unlock_gui
}

trap end_session EXIT

begin_session() {
  local name="$1"
  end_session
  lock_gui || return 1
  SESSION=1
  SESS="$OUT/$name"
  rm -rf "$SESS"
  mkdir -p "$SESS"
  DATA=""
  LOG=""
  if ! command -v xdpyinfo >/dev/null; then
    echo "e2e: xdpyinfo missing" >&2
    unlock_gui
    return 1
  fi
  XVFB_PID=""
  local n
  for n in $(seq 190 240); do
    [[ -e "/tmp/.X$n-lock" ]] && continue
    Xvfb ":$n" -screen 0 1600x1000x24 -nolisten tcp -noreset >"$SESS/xvfb.log" 2>&1 &
    XVFB_PID=$!
    local _
    for _ in $(seq 50); do
      DISPLAY=":$n" xdpyinfo >/dev/null 2>&1 && break
      kill -0 "$XVFB_PID" 2>/dev/null || break
      sleep 0.1
    done
    if DISPLAY=":$n" xdpyinfo >/dev/null 2>&1; then export DISPLAY=":$n"; break; fi
    kill "$XVFB_PID" 2>/dev/null || true
    wait "$XVFB_PID" 2>/dev/null || true
    XVFB_PID=""
  done
  if [[ -z "$XVFB_PID" ]]; then
    echo "e2e: Xvfb did not start" >&2
    unlock_gui
    return 1
  fi

  if command -v prlimit >/dev/null && sudo -n true 2>/dev/null; then
    sudo -n prlimit --pid $$ --rtprio=95:95 --memlock=unlimited:unlimited || true
  fi
  if [[ ! -x "$OUT/volum-jack-probe" ]]; then
    cc -O2 -o "$OUT/volum-jack-probe" "$SCRIPT_DIR/linux/smoke/volum-jack-probe.c" -ljack -lm || return 1
  fi
  export JACK_DEFAULT_SERVER="volum-e2e-$$-$name"
  export JACK_NO_AUDIO_RESERVATION=1
  export JACK_NO_START_SERVER=1
  # Real-time first, the way the audio smoke does. WSL often refuses it; the
  # dummy server still runs with -r, and these scenarios do not count xruns.
  local jack_rt=""
  jackd --name "$JACK_DEFAULT_SERVER" -d dummy -r 48000 -p 256 >"$SESS/jackd.log" 2>&1 &
  JACK_PID=$!
  if ! "$OUT/volum-jack-probe" wait-server 8; then
    kill "$JACK_PID" 2>/dev/null || true
    wait "$JACK_PID" 2>/dev/null || true
    JACK_PID=""
    jack_rt="-r"
    jackd -r --name "$JACK_DEFAULT_SERVER" -d dummy -r 48000 -p 256 >"$SESS/jackd.log" 2>&1 &
    JACK_PID=$!
    if ! "$OUT/volum-jack-probe" wait-server 8; then
      echo "e2e: jackd did not come up" >&2
      cat "$SESS/jackd.log" >&2 || true
      end_session
      return 1
    fi
  fi
  echo "  jackd up${jack_rt:+ (no realtime)}"

  cat >"$SESS/asound.conf" <<'ALSA'
pcm.!default { type null }
ALSA
  export ALSA_CONFIG_PATH="$SESS/asound.conf"
  export PULSE_SERVER="unix:$SESS/no-pulse-server"
  export XDG_DATA_HOME="$SESS/xdg"
  DATA="$XDG_DATA_HOME/VoLum"
  mkdir -p "$DATA"
  LOG="$DATA/volum.log"
  cat >"$DATA/settings.ini" <<'INI'
[audio]
driver=1
indev=system
outdev=system
in1=1
out1=1
out2=2
buffer=256
sr=48000
INI
  echo "  session $name DISPLAY=$DISPLAY JACK=$JACK_DEFAULT_SERVER"
}

log_mark() {
  if [[ -f "$LOG" ]]; then LOG_MARK=$(wc -c <"$LOG"); else LOG_MARK=0; fi
}

log_since() {
  if [[ ! -f "$LOG" ]]; then return 0; fi
  tail -c +"$((LOG_MARK + 1))" "$LOG" 2>/dev/null || true
}

find_gfx() {
  xwininfo -root -tree >"$SESS/tree.txt" 2>/dev/null || return 1
  python3 - "$SESS/tree.txt" <<'PY'
import re, sys
text = open(sys.argv[1], errors="replace").read()
exact = None
best = None
for line in text.splitlines():
    m = re.search(r"(0x[0-9a-fA-F]+).*\b(\d+)x(\d+)\+", line)
    if not m:
        continue
    wid, w, h = m.group(1), int(m.group(2)), int(m.group(3))
    if w == 900 and h == 600:
        exact = wid
        break
    if w >= 800 and h >= 500 and (best is None or w * h > best[0]):
        best = (w * h, wid)
if exact:
    print(exact)
elif best:
    print(best[1])
else:
    sys.exit(1)
PY
}

find_named() {
  local name="$1" id
  id=$(xdotool search --name "$name" 2>/dev/null | head -n 1 || true)
  [[ -n "$id" ]] && echo "$id"
}

wait_gfx() {
  local i id
  for i in $(seq 150); do
    id=$(find_gfx 2>/dev/null || true)
    if [[ -n "$id" ]]; then echo "$id"; return 0; fi
    if [[ -n "$APP_PID" ]] && ! kill -0 "$APP_PID" 2>/dev/null; then return 1; fi
    sleep 0.1
  done
  return 1
}

click_at() {
  local x="$1" y="$2" id
  id=$(find_gfx) || { echo "  no 900x600 window for click $x $y"; return 1; }
  xdotool mousemove --window "$id" --sync "$x" "$y" click 1
  sleep "${3:-0.6}"
}

shot() {
  local dest="$1" id
  id=$(find_gfx 2>/dev/null || true)
  [[ -n "$id" ]] || return 0
  xwd -id "$id" -silent 2>/dev/null | convert xwd:- "$dest" 2>/dev/null || true
}

launch() {
  log_mark
  # CWD is the repo so ./rigs (appended after the XDG candidates) resolves the
  # dev factory bank. exec keeps this pid as VoLum, so SIGTERM reaches it.
  if [[ ${#LAUNCH_ENV[@]} -gt 0 ]]; then
    (cd "$ROOT" && exec env "${LAUNCH_ENV[@]}" "$APP" "$@") >"$SESS/app.out" 2>&1 &
  else
    (cd "$ROOT" && exec "$APP" "$@") >"$SESS/app.out" 2>&1 &
  fi
  APP_PID=$!
}

quit_app() {
  local how="${1:-term}"
  [[ -n "$APP_PID" ]] || { echo hung; return 0; }
  case "$how" in
    term) kill -TERM "$APP_PID" 2>/dev/null || true ;;
    term2)
      kill -TERM "$APP_PID" 2>/dev/null || true
      sleep 0.05
      kill -TERM "$APP_PID" 2>/dev/null || true
      ;;
    kill) kill -KILL "$APP_PID" 2>/dev/null || true ;;
  esac
  local i
  for i in $(seq 200); do
    kill -0 "$APP_PID" 2>/dev/null || break
    sleep 0.1
  done
  if kill -0 "$APP_PID" 2>/dev/null; then
    kill -9 "$APP_PID" 2>/dev/null || true
    wait "$APP_PID" 2>/dev/null || true
    APP_PID=""
    echo hung
    return 0
  fi
  local rc=0
  wait "$APP_PID" || rc=$?
  APP_PID=""
  echo "$rc"
}

wait_log() {
  local pattern="$1" secs="${2:-8}" i
  for i in $(seq $((secs * 10))); do
    if log_since | grep -a -E -q "$pattern"; then return 0; fi
    sleep 0.1
  done
  return 1
}

json_py() {
  python3 - "$@"
}

# --- fakes -----------------------------------------------------------------

write_fakes() {
  local dir="$1" mode="$2"
  mkdir -p "$dir"
  cat >"$dir/gdbus" <<'EOF'
#!/bin/bash
printf 'TOOL gdbus\n' >> "${VOLUM_E2E_TOOL_LOG:-/tmp/volum-e2e-tool.log}"
printf 'ARG %s\n' "$@" >> "${VOLUM_E2E_TOOL_LOG:-/tmp/volum-e2e-tool.log}"
exit 1
EOF
  cat >"$dir/dialog-fake" <<'EOF'
#!/bin/bash
log="${VOLUM_E2E_TOOL_LOG:-/tmp/volum-e2e-tool.log}"
{
  printf 'TOOL %s\n' "$(basename "$0")"
  printf 'ARG %s\n' "$@"
} >> "$log"
if [[ "${VOLUM_E2E_DIALOG_CANCEL:-}" == 1 ]]; then
  exit 1
fi
  if [[ -n "${VOLUM_E2E_DIALOG_FILE:-}" && -f "${VOLUM_E2E_DIALOG_FILE}" ]]; then
  cat "${VOLUM_E2E_DIALOG_FILE}"
  exit 0
fi
  if [[ -n "${VOLUM_E2E_DIALOG_STDOUT:-}" ]]; then
    printf '%s\n' "$VOLUM_E2E_DIALOG_STDOUT"
  fi
  exit 0
EOF
  chmod +x "$dir/gdbus" "$dir/dialog-fake"
  rm -f "$dir/zenity" "$dir/kdialog"
  if [[ "$mode" == kdialog ]]; then
    cp "$dir/dialog-fake" "$dir/kdialog"
    chmod +x "$dir/kdialog"
  else
    cp "$dir/dialog-fake" "$dir/zenity"
    chmod +x "$dir/zenity"
  fi
}

# --- scenarios -------------------------------------------------------------

scenario_fresh() {
  LEG=fresh
  echo ""
  echo "[fresh] first launch with no library"
  begin_session fresh || { check "session started" 0; return; }
  launch
  local id
  id=$(wait_gfx || true)
  check "app opened a window" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  local rc
  rc=$(quit_app term)
  check_eq "SIGTERM quits with exit 0" 0 "$rc"
  check "volum-settings.json written" "$([[ -f "$DATA/volum-settings.json" ]] && echo 1 || echo 0)"
  check "volum.log written" "$([[ -f "$LOG" ]] && echo 1 || echo 0)"
  local mode tip preset
  mode=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("volumUiMode",""))
PY
)
  tip=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
v=j.get("buildTipSeen")
print("true" if v is True else "false" if v is False else "")
PY
)
  preset=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("volumActivePresetId",""))
PY
)
  check_eq "a fresh install opens on PLAY" play "$mode"
  check_eq "the Tweak your sound note is still pending" false "$tip"
  check_eq "a fresh install starts on the first Factory Sound" "factory:12:v1" "$preset"
  local recall=0
  if [[ -f "$LOG" ]] && grep -a -q "recall slot=0 amp=factory:12 preset=factory:12:v1" "$LOG"; then recall=1; fi
  check "first launch recalls program 0" "$recall"
  local startup=0
  if [[ -f "$LOG" ]] && grep -a -q "startup" "$LOG"; then startup=1; fi
  check "log records startup" "$startup"
  local stream=0
  if [[ -f "$LOG" ]] && grep -a -q "stream open:" "$LOG"; then stream=1; fi
  check "log records the audio stream" "$stream"
  local key
  key=$(json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
rows=j.get("midiSoundMap") or []
parts=[]
for row in sorted(rows, key=lambda r: int(r["slot"])):
    parts.append("%s:%s:%s" % (row["slot"], row["ampId"], row["presetId"]))
print("|".join(parts))
PY
)
  check_eq "PLAY pre-filled with five Factory Sounds on programs 0-4" "$PREFILL" "$key"
  json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys
p=sys.argv[1]
j=json.load(open(p))
j["midiSoundMap"]=[]
json.dump(j, open(p,"w"))
PY
  launch
  id=$(wait_gfx || true)
  check "relaunch with a cleared board opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 1
  rc=$(quit_app term)
  check_eq "relaunch with a cleared board closed" 0 "$rc"
  local cleared
  cleared=$(json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print("yes" if "midiSoundMap" in j else "no")
print("|".join("%s:%s:%s" % (r["slot"], r["ampId"], r["presetId"]) for r in sorted(j.get("midiSoundMap") or [], key=lambda r: int(r["slot"]))))
PY
)
  local has empty
  has=$(printf '%s\n' "$cleared" | sed -n '1p')
  empty=$(printf '%s\n' "$cleared" | sed -n '2p')
  check_eq "cleared board keeps its midiSoundMap key" yes "$has"
  check_eq "cleared board is not pre-filled again" "" "$empty"
  end_session
}

scenario_presets() {
  LEG=presets
  echo ""
  echo "[presets] per-amp preset memory, then amp and preset switch"
  begin_session presets || { check "session started" 0; return; }
  launch
  local id
  id=$(wait_gfx || true)
  check "prep launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  quit_app term >/dev/null
  json_py "$DATA/content/volum-content.json" "$DATA/volum-settings.json" <<'PY'
import json,sys
content, settings = sys.argv[1], sys.argv[2]
reg=json.load(open(content))
banks=reg.setdefault("presetBanks", {})
banks["factory:0"]=[{"id":"preset_e2e_focused","name":"E2E Focused","settings":{}}]
banks["factory:5"]=[{"id":"preset_e2e_away","name":"E2E Away","settings":{}}]
json.dump(reg, open(content,"w"))
s=json.load(open(settings))
s["volumCustomMainId"]=""
s["lastAmpIdx"]=0
s["volumActivePresetId"]="preset_e2e_focused"
s["volumActivePresetIdByOwner"]={"factory:0":"preset_e2e_focused","factory:5":"preset_e2e_away"}
json.dump(s, open(settings,"w"))
PY
  local before
  before=$(python3 -c 'import os,sys; print(os.stat(sys.argv[1]).st_mtime_ns)' "$DATA/volum-settings.json")
  launch
  id=$(wait_gfx || true)
  check "seeded launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  local rc
  rc=$(quit_app term)
  check_eq "seeded launch closed" 0 "$rc"
  local after
  after=$(python3 -c 'import os,sys; print(os.stat(sys.argv[1]).st_mtime_ns)' "$DATA/volum-settings.json")
  check "settings rewritten on shutdown" "$([[ "$before" != "$after" ]] && echo 1 || echo 0)"
  local away focused
  away=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
m=j.get("volumActivePresetIdByOwner") or {}
print(m.get("factory:5",""))
PY
)
  focused=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
m=j.get("volumActivePresetIdByOwner") or {}
print(m.get("factory:0",""))
PY
)
  check_eq "focused amp keeps its preset" preset_e2e_focused "$focused"
  check_eq "amp never visited keeps its preset" preset_e2e_away "$away"

  launch
  id=$(wait_gfx || true)
  check "preset-switch launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  wait_log "recall slot=0 " 10 || true
  sleep 1
  click_at 803 175 0.8
  shot "$SESS/preset-switch.png"
  sleep 0.5
  rc=$(quit_app term)
  check_eq "preset-switch launch closed" 0 "$rc"
  local slot active
  slot=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("lastPlaySlot",""))
PY
)
  active=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("volumActivePresetId",""))
PY
)
  away=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
m=j.get("volumActivePresetIdByOwner") or {}
print(m.get("factory:5",""))
PY
)
  check_eq "clicking PLAY sound 1 selects program 1" 1 "$slot"
  check_eq "clicking PLAY sound 1 recalls SLO Crunch" "factory:13:v2" "$active"
  check_eq "the unvisited amp still keeps its preset" preset_e2e_away "$away"

  json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
p=sys.argv[1]
j=json.load(open(p))
j["volumUiMode"]="build"
j["buildTipSeen"]=True
json.dump(j, open(p,"w"))
PY
  launch
  id=$(wait_gfx || true)
  check "amp-switch launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  click_at 80 172 0.8
  shot "$SESS/amp-switch.png"
  rc=$(quit_app term)
  check_eq "amp-switch launch closed" 0 "$rc"
  local amp
  amp=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("lastAmpIdx",""))
PY
)
  check_eq "clicking sidebar amp 3 selects it" 3 "$amp"
  end_session
}

pack_clicks_export() {
  click_at 869 22 0.8
  click_at 600 113 0.8
  click_at 525 346 0.8
  click_at 529 473 0.4
}

pack_clicks_import() {
  click_at 869 22 0.8
  click_at 600 113 0.8
  click_at 718 346 0.4
}

scenario_pack() {
  LEG=pack
  echo ""
  echo "[pack] export and open through VOLUM_PACK_SAVE_PATH / VOLUM_PACK_OPEN_PATH"
  begin_session pack || { check "session started" 0; return; }
  launch
  local id
  id=$(wait_gfx || true)
  check "source library opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  quit_app term >/dev/null
  local drop="$SESS/drop.volumpack"
  LAUNCH_ENV=("VOLUM_PACK_SAVE_PATH=$drop")
  launch
  id=$(wait_gfx || true)
  check "export launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  pack_clicks_export
  local i got=0
  for i in $(seq 30); do
    [[ -f "$drop" ]] && { got=1; break; }
    sleep 0.1
  done
  check "export wrote the .volumpack" "$got"
  local wrote=0
  if log_since | grep -a -q "export wrote "; then wrote=1; fi
  check "log line is export wrote" "$wrote"
  local rc
  rc=$(quit_app term)
  check_eq "export launch closed" 0 "$rc"
  LAUNCH_ENV=()
  local zipok=0
  if [[ -f "$drop" ]] && python3 -c 'import zipfile,sys; z=zipfile.ZipFile(sys.argv[1]); sys.exit(0 if "manifest.json" in z.namelist() else 1)' "$drop"; then zipok=1; fi
  check "pack is a zip with manifest.json" "$zipok"

  local open_root="$SESS/open-xdg"
  export XDG_DATA_HOME="$open_root"
  DATA="$XDG_DATA_HOME/VoLum"
  mkdir -p "$DATA"
  LOG="$DATA/volum.log"
  cat >"$DATA/settings.ini" <<'INI'
[audio]
driver=1
indev=system
outdev=system
in1=1
out1=1
out2=2
buffer=256
sr=48000
INI
  LAUNCH_ENV=("VOLUM_PACK_OPEN_PATH=$drop")
  launch
  id=$(wait_gfx || true)
  check "import launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  log_mark
  pack_clicks_import
  local opened=0
  if wait_log "opened " 8; then opened=1; fi
  check "log line is opened" "$opened"
  rc=$(quit_app term)
  check_eq "import launch closed" 0 "$rc"
  LAUNCH_ENV=()
  end_session
}

make_wav() {
  python3 - "$1" "$2" <<'PY'
import math, struct, sys, os
path, seed = sys.argv[1], int(sys.argv[2])
os.makedirs(os.path.dirname(path), exist_ok=True)
frames = 256
data = b"".join(struct.pack("<h", int(math.exp(-i/24.0)*12000*math.cos((i+seed)*0.7))) for i in range(frames))
body = b"RIFF" + struct.pack("<I", 36+len(data)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 48000, 96000, 2, 16) + b"data" + struct.pack("<I", len(data)) + data
open(path, "wb").write(body)
PY
}

make_nam() {
  python3 - "$1" "$2" <<'PY'
import os, sys
path, seed = sys.argv[1], int(sys.argv[2])
os.makedirs(os.path.dirname(path), exist_ok=True)
open(path, "wb").write(b"NAM\n" + bytes([seed & 0xFF]) * 64 + b"\n")
PY
}

import_batch() {
  local kind="$1" ext="$2"
  local src="$SESS/picked-$kind"
  local stem="Cab_XY_asdasd_Studio_412_Brit_V30_D57"
  local f1="$src/${stem}_1.$ext" f2="$src/${stem}_2.$ext" f3="$src/${stem}_3.$ext"
  local f4="$src/Copy of take 1.$ext" f5="$src/other/${stem}_1.$ext"
  if [[ "$ext" == wav ]]; then
    make_wav "$f1" 1; make_wav "$f2" 2; make_wav "$f3" 3; cp "$f1" "$f4"; make_wav "$f5" 4
  else
    make_nam "$f1" 1; make_nam "$f2" 2; make_nam "$f3" 3; cp "$f1" "$f4"; make_nam "$f5" 4
  fi
  local joined
  joined=$(python3 -c 'import sys; print("|".join(sys.argv[1:]))' "$f1" "$f2" "$f3" "$f4" "$f5")
  LAUNCH_ENV=("VOLUM_IMPORT_OPEN_PATHS=$joined")
  launch
  local id
  id=$(wait_gfx || true)
  check "[$kind] app opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  if [[ "$kind" == IR ]]; then
    click_at 665 80 0.6
    click_at 690 108 0.7
  else
    local gfx
    gfx=$(find_gfx || true)
    if [[ -n "$gfx" ]]; then xdotool windowfocus "$gfx" >/dev/null 2>&1 || true; xdotool key 1; fi
    sleep 0.4
    click_at 455 230 0.5
    click_at 455 230 0.6
    click_at 490 576 0.7
  fi
  click_at 450 175 1.2
  shot "$SESS/import-$kind.png"
  local rc
  rc=$(quit_app term)
  check_eq "[$kind] app closed" 0 "$rc"
  LAUNCH_ENV=()
  local count names uniq
  count=$(json_py "$DATA/content/volum-content.json" "$kind" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
key="irLibrary" if sys.argv[2]=="IR" else "customPedals"
items=j.get(key) or []
names=[it.get("name","") for it in items]
print(len(items))
print(len(set(names)))
print("|".join(names))
PY
)
  local n u listed
  n=$(printf '%s\n' "$count" | sed -n '1p')
  u=$(printf '%s\n' "$count" | sed -n '2p')
  listed=$(printf '%s\n' "$count" | sed -n '3p')
  check_eq "[$kind] four of five picked files imported" 4 "$n"
  check_eq "[$kind] every imported name is distinct" "$n" "$u"
  echo "  [$kind] names: $listed"
}

prepare_build() {
  launch
  local id
  id=$(wait_gfx || true)
  check "build prep opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  quit_app term >/dev/null
  json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
p=sys.argv[1]
j=json.load(open(p))
j["volumUiMode"]="build"
j["buildTipSeen"]=True
json.dump(j, open(p,"w"))
PY
}

scenario_import() {
  LEG=import
  echo ""
  echo "[import] Manage > Add through VOLUM_IMPORT_OPEN_PATHS"
  begin_session import-ir || { check "session started" 0; return; }
  prepare_build
  import_batch IR wav
  end_session
  begin_session import-pedal || { check "pedal session started" 0; return; }
  prepare_build
  import_batch pedal nam
  end_session
}

ensure_seq() {
  [[ -e /dev/snd/seq ]] && return 0
  if sudo -n true 2>/dev/null; then
    sudo -n modprobe snd-seq 2>/dev/null || true
    sudo -n modprobe snd-virmidi 2>/dev/null || true
    if [[ -e /dev/snd/seq ]]; then sudo -n chmod a+rw /dev/snd/seq || true; fi
  fi
  [[ -e /dev/snd/seq ]]
}

scenario_midi() {
  LEG=midi
  echo ""
  echo "[midi] Program Change and CC 102 through an ALSA virtual port"
  if ! ensure_seq; then
    check "ALSA sequencer /dev/snd/seq is present" 0 "snd-seq and snd-virmidi did not load"
    return
  fi
  check "ALSA sequencer /dev/snd/seq is present" 1
  begin_session midi || { check "session started" 0; return; }
  cat >"$SESS/volum-alsa-midi.c" <<'EOF'
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  snd_seq_t *seq = NULL;
  if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, 0) < 0) {
    fprintf(stderr, "snd_seq_open failed\n");
    return 1;
  }
  snd_seq_set_client_name(seq, "VoLum Loop");
  int port = snd_seq_create_simple_port(seq, "out",
      SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
  if (port < 0) return 1;
  int client = snd_seq_client_id(seq);
  printf("RTMIDI VoLum Loop:out %d:%d\n", client, port);
  fflush(stdout);
  char line[128];
  while (fgets(line, sizeof line, stdin)) {
    char cmd[16];
    int a = 0, b = 0, c = 0;
    int n = sscanf(line, "%15s %d %d %d", cmd, &a, &b, &c);
    if (n < 2) continue;
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    if (!strcmp(cmd, "pc") && n >= 3) snd_seq_ev_set_pgmchange(&ev, a, b);
    else if (!strcmp(cmd, "cc") && n >= 4) snd_seq_ev_set_controller(&ev, a, b, c);
    else if (!strcmp(cmd, "as")) { ev.type = SND_SEQ_EVENT_SENSING; ev.data.control.channel = a; }
    else continue;
    snd_seq_event_output(seq, &ev);
    snd_seq_drain_output(seq);
    printf("SENT %s\n", cmd);
    fflush(stdout);
  }
  return 0;
}
EOF
  gcc -O2 -o "$SESS/volum-alsa-midi" "$SESS/volum-alsa-midi.c" -lasound || { check "midi sender built" 0; end_session; return; }
  check "midi sender built" 1
  rm -f "$SESS/midi.cmd"
  mkfifo "$SESS/midi.cmd"
  "$SESS/volum-alsa-midi" <"$SESS/midi.cmd" >"$SESS/midi.out" 2>"$SESS/midi.err" &
  local midi_pid=$!
  exec 8>"$SESS/midi.cmd"
  local i name=""
  for i in $(seq 50); do
    name=$(sed -n 's/^RTMIDI //p' "$SESS/midi.out" | head -n 1)
    [[ -n "$name" ]] && break
    sleep 0.1
  done
  check "virtual port is listed by name" "$([[ -n "$name" ]] && echo 1 || echo 0)" "$name"
  if [[ -z "$name" ]]; then
    exec 8>&- || true
    kill "$midi_pid" 2>/dev/null || true
    end_session
    return
  fi
  python3 - "$DATA/settings.ini" "$name" <<'PY'
import sys
path, name = sys.argv[1], sys.argv[2]
with open(path, "a", encoding="ascii") as f:
    f.write("[midi]\nindev=%s\nindev2=%s\noutdev=off\ninchan=0\noutchan=0\n" % (name, name))
PY
  python3 - "$DATA/content/volum-content.json" <<'PY'
import json, os, sys
path = sys.argv[1]
os.makedirs(os.path.dirname(path), exist_ok=True)
board = [
    (3, "factory:12", "factory:12:v1"),
    (9, "factory:6", "factory:6:v1"),
    (42, "factory:0", "factory:0:v2"),
    (100, "factory:8", "factory:8:v1"),
    (101, "factory:13", "factory:13:v2"),
]
json.dump({"schemaVersion": 4, "midiSoundMap": [
    {"slot": s, "ampId": a, "presetId": p} for s, a, p in board
]}, open(path, "w"))
PY
  launch
  local id
  id=$(wait_gfx || true)
  check "app opened a window" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  local missed=0
  if grep -a -q "could not open the saved MIDI port" "$LOG" 2>/dev/null; then missed=1; fi
  check "the standalone opened the virtual port" "$([[ $missed == 0 ]] && echo 1 || echo 0)"

  midi_send() { echo "$1" >&8; }

  log_mark
  midi_send "pc 0 9"
  local ok=0
  if wait_log "recall slot=9 amp=factory:6 preset=factory:6:v1" 6; then ok=1; fi
  check "Program Change 9 recalls program 9" "$ok" "$(log_since | tr '\n' ' ')"

  log_mark
  midi_send "cc 0 102 42"
  ok=0
  if wait_log "recall slot=42 amp=factory:0 preset=factory:0:v2" 6; then ok=1; fi
  check "CC 102 value 42 recalls program 42" "$ok" "$(log_since | tr '\n' ' ')"

  log_mark
  local n
  for n in $(seq 64); do midi_send "as 0"; done
  midi_send "pc 0 9"
  ok=0
  if wait_log "recall slot=9 amp=factory:6 preset=factory:6:v1" 6; then ok=1; fi
  check "64 Active Sensing messages cannot crowd out the following Program Change" "$ok"

  log_mark
  midi_send "pc 0 7"
  ok=0
  if wait_log "slot=7 has no playable Sound; ignored" 6; then ok=1; fi
  check "Program Change 7 is ignored: program 7 is empty" "$ok" "$(log_since | tr '\n' ' ')"
  local recalled=0
  if log_since | grep -a -q "^.*recall "; then recalled=1; fi
  # The ignored line is the only new midi line; a recall in this slice is a failure.
  local bad=0
  if log_since | grep -a "\[midi\]" | grep -a -q "recall "; then bad=1; fi
  check "the empty program recalls nothing" "$([[ $bad == 0 ]] && echo 1 || echo 0)"

  log_mark
  for n in 3 100 9 42 101; do midi_send "pc 0 $n"; sleep 0.05; done
  ok=0
  if wait_log "recall slot=101 amp=factory:13 preset=factory:13:v2" 6; then ok=1; fi
  check "a burst ends on program 101" "$ok" "$(log_since | tr '\n' ' ')"
  local rc
  rc=$(quit_app term)
  check_eq "app closed gracefully with the MIDI port open" 0 "$rc"
  local saved
  saved=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
print(j.get("lastPlaySlot",""))
print(j.get("volumActivePresetId",""))
PY
)
  check_eq "saved PLAY cursor is the last program played" 101 "$(printf '%s\n' "$saved" | sed -n '1p')"
  check_eq "saved active preset is the last Sound recalled" "factory:13:v2" "$(printf '%s\n' "$saved" | sed -n '2p')"

  json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
p=sys.argv[1]
j=json.load(open(p))
j["midiCh"]=2
json.dump(j, open(p,"w"))
PY
  launch
  id=$(wait_gfx || true)
  check "[one channel 2] app opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  log_mark
  midi_send "pc 0 3"
  midi_send "cc 0 102 9"
  sleep 2
  local ch1
  ch1=$(log_since | grep -a "\[midi\]" | grep -a -E "recall |ignored" || true)
  check_eq "[one channel 2] channel-1 messages are ignored" "" "$ch1"
  log_mark
  midi_send "cc 1 102 100"
  ok=0
  if wait_log "recall slot=100 amp=factory:8 preset=factory:8:v1" 6; then ok=1; fi
  check "[one channel 2] channel-2 CC 102 value 100 recalls program 100" "$ok"
  log_mark
  midi_send "pc 1 42"
  ok=0
  if wait_log "recall slot=42 amp=factory:0 preset=factory:0:v2" 6; then ok=1; fi
  check "[one channel 2] channel-2 Program Change 42 recalls program 42" "$ok"
  rc=$(quit_app term)
  check_eq "[one channel 2] app closed" 0 "$rc"
  local mch
  mch=$(json_py "$DATA/volum-settings.json" <<'PY'
import json,sys
print(json.load(open(sys.argv[1])).get("midiCh",""))
PY
)
  check_eq "[one channel 2] One channel = 2 survives the quit" 2 "$mch"
  exec 8>&- || true
  kill "$midi_pid" 2>/dev/null || true
  wait "$midi_pid" 2>/dev/null || true
  end_session
}

scenario_preferences() {
  LEG=preferences
  echo ""
  echo "[preferences] --preferences lists the dummy JACK device"
  begin_session preferences || { check "session started" 0; return; }
  local listed
  listed=$("$APP" --list-audio-devices 2>"$SESS/list.err" || true)
  printf '%s\n' "$listed" >"$SESS/list.txt"
  local has_jack=0 has_system=0
  if printf '%s\n' "$listed" | grep -a -q "JACK"; then has_jack=1; fi
  if printf '%s\n' "$listed" | grep -a -q "system"; then has_system=1; fi
  check "--list-audio-devices lists JACK" "$has_jack"
  check "--list-audio-devices lists the dummy device system" "$has_system"
  launch --preferences
  local id
  id=$(wait_gfx || true)
  check "app opened a window" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  local pref="" i
  for i in $(seq 50); do
    pref=$(find_named "Preferences" || true)
    [[ -n "$pref" ]] && break
    sleep 0.1
  done
  check "Preferences dialog opened" "$([[ -n "$pref" ]] && echo 1 || echo 0)"
  local drivers=0
  if wait_log "Preferences drivers: .*JACK" 8; then drivers=1; fi
  check "log lists JACK among Preferences drivers" "$drivers"
  if [[ -n "$pref" ]]; then
    xwd -id "$pref" -silent >"$SESS/preferences.xwd" 2>/dev/null || true
    convert "$SESS/preferences.xwd" "$SESS/preferences.png" 2>/dev/null || true
  fi
  local ocr=""
  if [[ -f "$SESS/preferences.png" ]]; then
    ocr=$(tesseract "$SESS/preferences.png" stdout --psm 6 2>/dev/null || true)
    printf '%s\n' "$ocr" >"$SESS/preferences.txt"
  fi
  local saw_jack=0 saw_system=0
  if printf '%s\n' "$ocr" | grep -a -q "JACK"; then saw_jack=1; fi
  if printf '%s\n' "$ocr" | grep -a -qi "system"; then saw_system=1; fi
  check "Preferences dialog shows JACK" "$saw_jack"
  check "Preferences dialog shows the dummy device system" "$saw_system"
  local rc
  rc=$(quit_app term)
  check_eq "SIGTERM while Preferences is open quits with exit 0" 0 "$rc"
  local ini_jack=0
  if [[ -f "$DATA/settings.ini" ]] && grep -a -q "^driver=1" "$DATA/settings.ini" && grep -a -q "^outdev=system" "$DATA/settings.ini"; then ini_jack=1; fi
  check "settings.ini still names the JACK system device" "$ini_jack"
  check "volum-settings.json persisted" "$([[ -f "$DATA/volum-settings.json" ]] && echo 1 || echo 0)"
  end_session
}

scenario_shutdown() {
  LEG=shutdown
  echo ""
  echo "[shutdown] SIGTERM persists settings; a second signal exits at once"
  begin_session shutdown || { check "session started" 0; return; }
  launch
  local id
  id=$(wait_gfx || true)
  check "app opened a window" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  local rc
  rc=$(quit_app term)
  check_eq "SIGTERM exits 0" 0 "$rc"
  local audio=0 plugin=0
  if [[ -f "$LOG" ]] && grep -a -q "audio teardown complete" "$LOG"; then audio=1; fi
  if [[ -f "$LOG" ]] && grep -a -q "plugin teardown complete" "$LOG"; then plugin=1; fi
  check "audio teardown complete" "$audio"
  check "plugin teardown complete" "$plugin"
  check "settings.ini persisted" "$([[ -f "$DATA/settings.ini" ]] && echo 1 || echo 0)"
  check "volum-settings.json persisted" "$([[ -f "$DATA/volum-settings.json" ]] && echo 1 || echo 0)"
  launch
  id=$(wait_gfx || true)
  check "second launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 1
  rc=$(quit_app term2)
  check_eq "a second SIGTERM exits at once with code 3" 3 "$rc"
  end_session
}

dialog_env() {
  local mode="$1" stdout="$2" cancel="$3"
  local dir="$SESS/fakes"
  write_fakes "$dir" "$mode"
  export VOLUM_E2E_TOOL_LOG="$SESS/tools.log"
  : >"$VOLUM_E2E_TOOL_LOG"
  printf '%s\n' "$stdout" >"$SESS/dialog-stdout.txt"
  LAUNCH_ENV=("PATH=$dir:$PATH" "VOLUM_E2E_TOOL_LOG=$VOLUM_E2E_TOOL_LOG" "VOLUM_E2E_DIALOG_FILE=$SESS/dialog-stdout.txt" "VOLUM_E2E_DIALOG_CANCEL=$cancel")
}

scenario_dialog_export() {
  LEG=dialog-export
  echo ""
  echo "[dialog-export] fake zenity save path, including a space"
  begin_session dialog-export || { check "session started" 0; return; }
  launch
  wait_gfx >/dev/null || true
  sleep 2
  quit_app term >/dev/null
  local dest="$SESS/My VoLum Pack.volumpack"
  dialog_env zenity "$dest" 0
  launch
  local id
  id=$(wait_gfx || true)
  check "export launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  log_mark
  pack_clicks_export
  local got=0 i
  for i in $(seq 40); do [[ -f "$dest" ]] && { got=1; break; }; sleep 0.1; done
  check "the .volumpack is written at the zenity path" "$got"
  local wrote=0
  if log_since | grep -a -q "export wrote "; then wrote=1; fi
  check "log line is the same export wrote text" "$wrote"
  local used=0
  if grep -a -q "TOOL zenity" "$SESS/tools.log"; then used=1; fi
  check "zenity ran (the env hook did not replace it)" "$used"
  quit_app term >/dev/null
  LAUNCH_ENV=()
  local cancel_dest="$SESS/should-not-exist.volumpack"
  dialog_env zenity "$cancel_dest" 1
  launch
  id=$(wait_gfx || true)
  check "cancel launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  log_mark
  : >"$SESS/tools.log"
  pack_clicks_export
  sleep 1.5
  check "cancel writes nothing" "$([[ ! -f "$cancel_dest" ]] && echo 1 || echo 0)"
  local wrote2=0
  if log_since | grep -a -q "export wrote "; then wrote2=1; fi
  check "cancel does not log export wrote" "$([[ $wrote2 == 0 ]] && echo 1 || echo 0)"
  quit_app term >/dev/null
  LAUNCH_ENV=()
  end_session
}

scenario_dialog_open() {
  LEG=dialog-open
  echo ""
  echo "[dialog-open] fake zenity returns a pack; cancel leaves the library"
  begin_session dialog-open || { check "session started" 0; return; }
  launch
  wait_gfx >/dev/null || true
  sleep 2
  quit_app term >/dev/null
  local pack="$SESS/library.volumpack"
  LAUNCH_ENV=("VOLUM_PACK_SAVE_PATH=$pack")
  launch
  wait_gfx >/dev/null || true
  sleep 2
  pack_clicks_export
  local i
  for i in $(seq 30); do [[ -f "$pack" ]] && break; sleep 0.1; done
  check "setup export wrote a pack" "$([[ -f "$pack" ]] && echo 1 || echo 0)"
  quit_app term >/dev/null
  LAUNCH_ENV=()
  local hash_before
  hash_before=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$DATA/content/volum-content.json")
  dialog_env zenity "$pack" 0
  launch
  local id
  id=$(wait_gfx || true)
  check "open launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  shot "$SESS/before-open.png"
  log_mark
  pack_clicks_import
  local opened=0
  if wait_log "\[pack\] opened " 8; then opened=1; fi
  check "log line is opened" "$opened"
  sleep 0.4
  shot "$SESS/after-open.png"
  local differ=0
  if [[ -f "$SESS/before-open.png" && -f "$SESS/after-open.png" ]]; then
    local ae
    ae=$(compare -metric AE "$SESS/before-open.png" "$SESS/after-open.png" null: 2>&1 || true)
    echo "  open overlay AE=$ae"
    if [[ "${ae%% *}" -gt 200 ]]; then differ=1; fi
  fi
  check "the import screen is showing" "$differ"
  quit_app term >/dev/null
  LAUNCH_ENV=()

  hash_before=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$DATA/content/volum-content.json")
  dialog_env zenity "$pack" 1
  launch
  id=$(wait_gfx || true)
  check "cancel launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  shot "$SESS/before-cancel.png"
  log_mark
  pack_clicks_import
  sleep 1.5
  shot "$SESS/after-cancel.png"
  local opened2=0
  if log_since | grep -a -q "\[pack\] opened "; then opened2=1; fi
  check "cancel does not log opened" "$([[ $opened2 == 0 ]] && echo 1 || echo 0)"
  local same=0
  if [[ -f "$SESS/before-cancel.png" && -f "$SESS/after-cancel.png" ]]; then
    local ae2
    ae2=$(compare -metric AE "$SESS/before-cancel.png" "$SESS/after-cancel.png" null: 2>&1 || true)
    echo "  cancel overlay AE=$ae2"
    if [[ "${ae2%% *}" -lt 400 ]]; then same=1; fi
  fi
  check "cancel leaves the overlay hidden" "$same"
  quit_app term >/dev/null
  local hash_after
  hash_after=$(python3 -c 'import hashlib,sys; print(hashlib.sha256(open(sys.argv[1],"rb").read()).hexdigest())' "$DATA/content/volum-content.json")
  check_eq "cancel leaves the library unchanged" "$hash_before" "$hash_after"
  LAUNCH_ENV=()
  end_session
}

scenario_dialog_import() {
  LEG=dialog-import
  echo ""
  echo "[dialog-import] fake kdialog (no zenity) returns two IRs"
  begin_session dialog-import || { check "session started" 0; return; }
  prepare_build
  local a="$SESS/one.wav" b="$SESS/two.wav"
  make_wav "$a" 11
  make_wav "$b" 12
  dialog_env kdialog "$a"$'\n'"$b" 0
  launch
  local id
  id=$(wait_gfx || true)
  check "import launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  log_mark
  click_at 665 80 0.6
  click_at 690 108 0.7
  click_at 450 175 0.4
  local i n=0
  for i in $(seq 40); do
    n=$(json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys,os
p=sys.argv[1]
if not os.path.exists(p):
    print(0); raise SystemExit
print(len(json.load(open(p)).get("irLibrary") or []))
PY
)
    [[ "$n" == 2 ]] && break
    sleep 0.1
  done
  check_eq "both paths are imported" 2 "$n"
  local tool=0
  if grep -a -q "TOOL kdialog" "$SESS/tools.log"; then tool=1; fi
  local zen=0
  if grep -a -q "TOOL zenity" "$SESS/tools.log"; then zen=1; fi
  check "kdialog ran" "$tool"
  check "zenity was not on PATH" "$([[ $zen == 0 ]] && echo 1 || echo 0)"
  quit_app term >/dev/null
  LAUNCH_ENV=()

  dialog_env kdialog "$a"$'\n'"$b" 1
  local before
  before=$(json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys
print(len(json.load(open(sys.argv[1])).get("irLibrary") or []))
PY
)
  launch
  id=$(wait_gfx || true)
  check "cancel launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  click_at 665 80 0.6
  click_at 690 108 0.7
  click_at 450 175 0.4
  sleep 1.5
  quit_app term >/dev/null
  local after
  after=$(json_py "$DATA/content/volum-content.json" <<'PY'
import json,sys
print(len(json.load(open(sys.argv[1])).get("irLibrary") or []))
PY
)
  check_eq "cancel imports nothing" "$before" "$after"
  LAUNCH_ENV=()
  end_session
}

scenario_dialog_builder() {
  LEG=dialog-builder
  echo ""
  echo "[dialog-builder] fake zenity returns one .nam; the draft lists it"
  begin_session dialog-builder || { check "session started" 0; return; }
  prepare_build
  local nam="$SESS/V30-E2E-1.nam"
  make_nam "$nam" 7
  dialog_env zenity "$nam" 0
  launch
  local id
  id=$(wait_gfx || true)
  check "builder launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  click_at 156 579 0.8
  shot "$SESS/builder-empty.png"
  log_mark
  click_at 277 162 0.3
  local i listed=0
  for i in $(seq 30); do
    if [[ -f "$SESS/tools.log" ]] && grep -a -q "TOOL zenity" "$SESS/tools.log"; then listed=1; break; fi
    sleep 0.1
  done
  sleep 0.6
  shot "$SESS/builder-added.png"
  check "zenity returned the .nam" "$listed"
  local ae=0 differ=0
  if [[ -f "$SESS/builder-empty.png" && -f "$SESS/builder-added.png" ]]; then
    convert "$SESS/builder-empty.png" -crop 400x40+70+180 "$SESS/row-empty.png" 2>/dev/null || true
    convert "$SESS/builder-added.png" -crop 400x40+70+180 "$SESS/row-added.png" 2>/dev/null || true
    if [[ -f "$SESS/row-empty.png" && -f "$SESS/row-added.png" ]]; then
      ae=$(compare -metric AE "$SESS/row-empty.png" "$SESS/row-added.png" null: 2>&1 || true)
      echo "  draft row AE=$ae"
      if [[ "${ae%% *}" -gt 150 ]]; then differ=1; fi
    fi
  fi
  local ocr=""
  if [[ -f "$SESS/row-added.png" ]]; then
    convert "$SESS/row-added.png" -resize 400% "$SESS/row-added-big.png" 2>/dev/null || true
    ocr=$(tesseract "$SESS/row-added-big.png" stdout --psm 6 2>/dev/null || true)
    printf '%s\n' "$ocr" >"$SESS/row.txt"
  fi
  local saw=0
  if printf '%s\n' "$ocr" | grep -a -E -q "E2E|V30"; then saw=1; fi
  check "the draft row changed" "$differ"
  check "the draft lists V30-E2E-1.nam" "$saw" "$ocr"
  quit_app term >/dev/null
  LAUNCH_ENV=()

  dialog_env zenity "$nam" 1
  launch
  id=$(wait_gfx || true)
  check "cancel launch opened" "$([[ -n "$id" ]] && echo 1 || echo 0)"
  sleep 2
  click_at 156 579 0.8
  shot "$SESS/builder-cancel-before.png"
  click_at 277 162 0.3
  sleep 1.2
  shot "$SESS/builder-cancel-after.png"
  local ae2 quiet=0
  if [[ -f "$SESS/builder-cancel-before.png" && -f "$SESS/builder-cancel-after.png" ]]; then
    convert "$SESS/builder-cancel-before.png" -crop 400x40+70+180 "$SESS/row-c1.png" 2>/dev/null || true
    convert "$SESS/builder-cancel-after.png" -crop 400x40+70+180 "$SESS/row-c2.png" 2>/dev/null || true
    if [[ -f "$SESS/row-c1.png" && -f "$SESS/row-c2.png" ]]; then
      ae2=$(compare -metric AE "$SESS/row-c1.png" "$SESS/row-c2.png" null: 2>&1 || true)
      echo "  cancel draft AE=$ae2"
      if [[ "${ae2%% *}" -lt 80 ]]; then quiet=1; fi
    fi
  fi
  check "cancel leaves the draft unchanged" "$quiet"
  quit_app term >/dev/null
  local has=0
  if [[ -f "$DATA/content/volum-content.json" ]] && grep -a -q "V30-E2E-1" "$DATA/content/volum-content.json"; then has=1; fi
  check "cancel did not save the .nam into the library" "$([[ $has == 0 ]] && echo 1 || echo 0)"
  LAUNCH_ENV=()
  end_session
}

echo "VoLum Linux standalone end-to-end scenarios"
echo "  app: $APP"
echo "  out: $OUT"
echo "  scenario: $SCENARIO"

want fresh && scenario_fresh
want presets && scenario_presets
want pack && scenario_pack
want import && scenario_import
want midi && scenario_midi
want preferences && scenario_preferences
want shutdown && scenario_shutdown
want dialog-export && scenario_dialog_export
want dialog-open && scenario_dialog_open
want dialog-import && scenario_dialog_import
want dialog-builder && scenario_dialog_builder

echo ""
if [[ "$FAILED" -eq 0 ]]; then
  echo "ALL PASS  $CHECKS checks"
  exit 0
fi
echo "FAILED  $FAILED of $CHECKS checks"
printf '%s' "$FAILURES"
exit 1
