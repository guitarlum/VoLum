#!/usr/bin/env bash
# Linux UI scenes: capture, compare with Windows GL2 shots, and the two host
# proofs carried from tickets 07 and 09.
#
#   bash NeuralAmpModeler/scripts/linux/ui-scenes.sh
#   bash NeuralAmpModeler/scripts/linux/ui-scenes.sh --capture --out DIR [--app PATH]
#   bash NeuralAmpModeler/scripts/linux/ui-scenes.sh --compare --linux DIR --windows DIR --report DIR
#   bash NeuralAmpModeler/scripts/linux/ui-scenes.sh --all --out DIR [--windows DIR] [--report DIR]
#
# With no arguments, a committed reference set under tests/linux-ui-refs/ is a
# gate (fresh capture vs those PNGs, plus the shifted-sidebar proof). Until that
# set exists the script prints a skip and exits 0, so verify-linux.sh can call
# it before the references land.
#
# Scenes, each at scale 1 and scale 2: main, signal, system, custom, pack, tuner.
# The framebuffer is the same self-capture the Windows harness uses
# (VOLUM_SELF_CAPTURE_DIR). Scale 2 is GDK_SCALE=2. Scale on Windows is
# VOLUM_SCREEN_SCALE, read by GetScaleForHWND only when set.
#
# Audio stays on a private jackd dummy. ALSA's default is a null PCM and
# PulseAudio points at a socket that does not exist, so nothing opens a
# hardware device.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
APP="${APP:-$ROOT/build-linux/standalone/VoLum}"
REFS="$ROOT/NeuralAmpModeler/tests/linux-ui-refs"
MANIFEST="$REFS/manifest.txt"
SEED="$ROOT/docs/screenshot-seed"
OUT=""
WIN=""
REPORT=""
MODE=""
SCALES="1 2"

usage() {
  echo "usage: $0 [--capture|--compare|--all] [--out DIR] [--app PATH] [--windows DIR] [--report DIR]" >&2
  exit 2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --capture|--compare|--all|--gate|--host|--red|--meter) MODE="${1#--}" ;;
    --out) OUT="$2"; shift ;;
    --app) APP="$2"; shift ;;
    --windows) WIN="$2"; shift ;;
    --report) REPORT="$2"; shift ;;
    -h|--help) usage ;;
    *) echo "unknown argument $1" >&2; usage ;;
  esac
  shift
done

if [[ -z "$MODE" ]]; then
  if compgen -G "$REFS/*.png" >/dev/null; then
    MODE=gate
  else
    echo "ui-scenes: no Linux reference set yet (skip)"
    exit 0
  fi
fi

SCENES=(main signal system custom pack tuner)
FAILS=0
CHECKS=0
pass() { CHECKS=$((CHECKS + 1)); echo "  PASS  $*"; }
fail() { CHECKS=$((CHECKS + 1)); FAILS=$((FAILS + 1)); echo "  FAIL  $*"; }

need() {
  command -v "$1" >/dev/null || { echo "ui-scenes: missing $1" >&2; exit 2; }
}

ssim() {
  # ImageMagick prints "<distortion> (<normalized>)". The normalized term is
  # 1-SSIM: 0 is identical. Return SSIM so a higher number is a closer match.
  local out paren
  out=$(compare -metric SSIM "$1" "$2" null: 2>&1) || true
  paren=$(printf '%s\n' "$out" | awk -F'[()]' 'NF >= 2 { print $2; exit }')
  if [[ -z "${paren:-}" ]]; then
    echo "0"
    return
  fi
  awk -v d="$paren" 'BEGIN { printf "%.6f\n", 1 - d }'
}

dims() { identify -format '%wx%h' "$1" 2>/dev/null || echo '?'; }

XVFB_PID=""
JACK_PID=""
APP_PID=""
XTOOL=""
TONE=""
TONE_PID=""
DATA=""
CAP=""
LOG=""
JACK_NAME=""
ALSA_CONF=""
PULSE=""
XDG=""
REDBAK=()

restore_red() {
  local f
  for f in "${REDBAK[@]}"; do
    if [[ -f "$f.redbak" ]]; then
      mv -f "$f.redbak" "$f"
    fi
  done
  REDBAK=()
}

# Replace one exact snippet. The backup is restored on every exit.
mutate_once() {
  local file="$1" old="$2" new="$3"
  cp -a "$file" "$file.redbak"
  REDBAK+=("$file")
  python3 - "$file" "$old" "$new" <<'PY'
import pathlib, sys
path, old, new = sys.argv[1:]
file = pathlib.Path(path)
text = file.read_text()
if old not in text:
    raise SystemExit("redproof: snippet missing in " + path)
file.write_text(text.replace(old, new, 1))
PY
}

cleanup() {
  restore_red
  [[ -n "${TONE_PID:-}" ]] && kill "$TONE_PID" 2>/dev/null || true
  [[ -n "$APP_PID" ]] && kill -KILL "$APP_PID" 2>/dev/null || true
  [[ -n "$JACK_PID" ]] && kill "$JACK_PID" 2>/dev/null || true
  [[ -n "$XVFB_PID" ]] && kill "$XVFB_PID" 2>/dev/null || true
  wait 2>/dev/null || true
}

seed_data() {
  local data="$1" amp="${2:-}"
  rm -rf "$data"
  mkdir -p "$data/capture"
  cp "$SEED/volum-settings.json" "$data/"
  cp "$SEED/volum-dual-amp-settings.json" "$data/"
  cp -a "$SEED/content" "$data/"
  if [[ -n "$amp" ]]; then
    sed -i -E 's/"lastAmpIdx"[[:space:]]*:[[:space:]]*[0-9]+/"lastAmpIdx": '"$amp"'/' "$data/volum-settings.json"
  fi
  cat >"$data/settings.ini" <<'INI'
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
}

quit_app() {
  [[ -n "$APP_PID" ]] || return 0
  kill -TERM "$APP_PID" 2>/dev/null || true
  local i
  for i in $(seq 80); do
    kill -0 "$APP_PID" 2>/dev/null || break
    sleep 0.1
  done
  if kill -0 "$APP_PID" 2>/dev/null; then
    kill -KILL "$APP_PID" 2>/dev/null || true
  fi
  wait "$APP_PID" 2>/dev/null || true
  APP_PID=""
}

plug_child() {
  "$XTOOL" child VoLum
}

drive() {
  local line
  line=$("$XTOOL" child VoLum) || return 1
  local child _x _y w h
  # shellcheck disable=SC2086
  read -r child _x _y w h <<<"$line"
  [[ -n "${child:-}" && "${w:-0}" -gt 0 ]] || return 1
  echo "  window $child ${w}x${h}" >&2
  local spec op rest a b px py
  for spec in "$@"; do
    op=${spec%% *}
    rest=${spec#* }
    case "$op" in
      key)
        xdotool key --window "$child" "$rest"
        ;;
      keys)
        local tok
        for tok in $rest; do
          if [[ "$tok" == shift+* ]]; then
            "$XTOOL" key "$child" "${tok#shift+}" shift
          else
            "$XTOOL" key "$child" "$tok"
          fi
          sleep 0.08
        done
        ;;
      click)
        a=${rest%% *}
        b=${rest#* }
        px=$((a * w / 900))
        py=$((b * h / 600))
        xdotool mousemove --window "$child" --sync "$px" "$py" click 1
        ;;
      wheel)
        a=${rest%% *}
        rest=${rest#* }
        b=${rest%% *}
        rest=${rest#* }
        px=$((a * w / 900))
        py=$((b * h / 600))
        xdotool mousemove --window "$child" --sync "$px" "$py" click --repeat "$rest" 5
        ;;
      type)
        xdotool type --window "$child" --delay 20 "$rest"
        ;;
      backs)
        xdotool key --window "$child" --repeat "$rest" BackSpace
        ;;
      *)
        echo "drive: bad step $spec" >&2
        return 1
        ;;
    esac
    sleep 0.4
  done
}

park() {
  local line child _x _y w h px py
  line=$("$XTOOL" child VoLum) || return 0
  read -r child _x _y w h <<<"$line"
  px=$((3 * w / 900))
  py=$((597 * h / 600))
  xdotool mousemove --window "$child" --sync "$px" "$py" || true
}

wait_editor() {
  local i
  for i in $(seq 80); do
    if [[ -f "$LOG" ]] && grep -a -q "screen scale" "$LOG"; then
      return 0
    fi
    kill -0 "$APP_PID" 2>/dev/null || return 1
    sleep 0.25
  done
  return 1
}

grab() {
  local name="$1"
  rm -f "$CAP/done.txt" "$CAP/$name.bmp"
  printf '%s' "$name" >"$CAP/request.txt"
  local i
  for i in $(seq 60); do
    if [[ -f "$CAP/done.txt" ]] && grep -a -q "^${name}.bmp " "$CAP/done.txt"; then
      convert "$CAP/$name.bmp" "$OUT/$name.png"
      echo "  shot $name $(grep -a "^${name}.bmp " "$CAP/done.txt")"
      rm -f "$CAP/$name.bmp"
      return 0
    fi
    sleep 0.1
  done
  echo "  capture timed out: $name" >&2
  return 1
}

# run_scene <scale> <name> <lastAmpIdx or empty> [drive steps...]
run_scene() {
  local scale="$1" name="$2" amp="$3"
  shift 3
  local tag="${name}-s${scale}"
  XDG="$OUT/xdg-$tag"
  DATA="$XDG/VoLum"
  seed_data "$DATA" "$amp"
  LOG="$DATA/volum.log"
  CAP="$DATA/capture"
  local -a extra=()
  [[ "$scale" == 2 ]] && extra+=(GDK_SCALE=2)
  # shellcheck disable=SC2086
  env "${extra[@]}" \
    XDG_DATA_HOME="$XDG" \
    VOLUM_SELF_CAPTURE_DIR="$CAP" \
    JACK_DEFAULT_SERVER="$JACK_NAME" \
    JACK_NO_AUDIO_RESERVATION=1 \
    ALSA_CONFIG_PATH="$ALSA_CONF" \
    PULSE_SERVER="$PULSE" \
    "$APP" >"$OUT/${tag}.log" 2>&1 &
  APP_PID=$!
  if ! "$XTOOL" wait VoLum 30; then
    echo "  no window for $tag" >&2
    quit_app
    return 1
  fi
  # The window is up. The "screen scale" line is trace-only, so don't wait on it.
  sleep 4
  if [[ $# -gt 0 ]]; then
    drive "$@" || { quit_app; return 1; }
  fi
  park
  sleep 0.7
  grab "$tag" || { quit_app; return 1; }
  if [[ "$name" == main && "$scale" == 1 ]]; then
    sleep 1
    park
    grab main-s1-repeat || true
  fi
  quit_app
  return 0
}

start_display() {
  need Xvfb
  need xdotool
  need convert
  need compare
  need identify
  need jackd
  [[ -x "$APP" ]] || { echo "ui-scenes: no app at $APP" >&2; exit 2; }
  mkdir -p "$OUT"
  cc -O2 -o "$OUT/volum-x-tool" "$SCRIPT_DIR/smoke/volum-x-tool.c" -lX11 || exit 2
  cc -O2 -o "$OUT/volum-jack-tone" "$SCRIPT_DIR/smoke/volum-jack-tone.c" -ljack -lm || exit 2
  XTOOL="$OUT/volum-x-tool"
  TONE="$OUT/volum-jack-tone"

  local lock="${VOLUM_GUI_LOCK:-$HOME/volum-linux/locks/gui.lock}"
  if [[ -e "$lock" ]]; then
    exec 8>>"$lock"
    if ! flock -w 900 8; then
      echo "ui-scenes: gui.lock busy" >&2
      exit 1
    fi
    echo "ui-scenes: holding gui.lock"
  fi

  trap cleanup EXIT
  trap 'exit 1' INT TERM
  local n
  for n in $(seq 90 189); do
    [[ -e "/tmp/.X$n-lock" ]] && continue
    Xvfb ":$n" -screen 0 3840x2160x24 -nolisten tcp -noreset >"$OUT/xvfb.log" 2>&1 &
    XVFB_PID=$!
    local k
    for k in $(seq 50); do
      DISPLAY=":$n" "$XTOOL" ping 2>/dev/null && break
      kill -0 "$XVFB_PID" 2>/dev/null || break
      sleep 0.1
    done
    if DISPLAY=":$n" "$XTOOL" ping 2>/dev/null; then
      export DISPLAY=":$n"
      break
    fi
    kill "$XVFB_PID" 2>/dev/null || true
    wait "$XVFB_PID" 2>/dev/null || true
    XVFB_PID=""
  done
  [[ -n "$XVFB_PID" ]] || { echo "ui-scenes: Xvfb did not start" >&2; exit 2; }
  echo "DISPLAY=$DISPLAY"

  JACK_NAME="volum-ui-$$"
  export JACK_DEFAULT_SERVER="$JACK_NAME"
  export JACK_NO_AUDIO_RESERVATION=1
  # Same real-time limits the audio smoke uses, on this process only.
  if [[ "$(ulimit -r)" == 0 ]] && command -v prlimit >/dev/null && sudo -n true 2>/dev/null; then
    sudo -n prlimit --pid $$ --rtprio=95:95 --memlock=unlimited:unlimited || true
  fi
  jackd --name "$JACK_NAME" -d dummy -r 48000 -p 256 >"$OUT/jackd.log" 2>&1 &
  JACK_PID=$!
  local up=0 i
  for i in $(seq 80); do
    if ! kill -0 "$JACK_PID" 2>/dev/null; then
      break
    fi
    if command -v jack_lsp >/dev/null && jack_lsp >/dev/null 2>&1; then
      up=1
      break
    fi
    sleep 0.25
  done
  if [[ "$up" != 1 ]] && kill -0 "$JACK_PID" 2>/dev/null; then
    up=1
  fi
  [[ "$up" == 1 ]] || { echo "ui-scenes: jackd did not start" >&2; cat "$OUT/jackd.log" >&2; exit 2; }

  cat >"$OUT/asound.conf" <<'ALSA'
pcm.!default { type null }
ALSA
  ALSA_CONF="$OUT/asound.conf"
  PULSE="unix:$OUT/no-pulse-server"
  export ALSA_CONFIG_PATH="$ALSA_CONF"
  export PULSE_SERVER="$PULSE"
}

capture_all() {
  local scale name
  for scale in $SCALES; do
    run_scene "$scale" main "" || fail "scene main-s$scale"
    run_scene "$scale" signal "" "key h" || fail "scene signal-s$scale"
    run_scene "$scale" system "" "key h" "click 600 113" || fail "scene system-s$scale"
    run_scene "$scale" tuner "" "key t" || fail "scene tuner-s$scale"
    run_scene "$scale" custom "" "key Down" "click 122 575" || fail "scene custom-s$scale"
    run_scene "$scale" pack "" "key h" "click 600 113" "click 525 346" || fail "scene pack-s$scale"
  done
  for scale in $SCALES; do
    for name in "${SCENES[@]}"; do
      [[ -f "$OUT/${name}-s${scale}.png" ]] && pass "captured ${name}-s${scale} ($(dims "$OUT/${name}-s${scale}.png"))" || fail "missing ${name}-s${scale}.png"
    done
  done
}

rebuild_target() {
  echo "  rebuild $1"
  cmake --build "$ROOT/build-linux" -j "${JOBS:-14}" --target "$1"
}

prove_shift() {
  local ref="${1:-$OUT/main-s1.png}"
  local layout="$ROOT/NeuralAmpModeler/VoLumLayoutBuild.inc.cpp"
  local old='const IRECT ampListArea(b.L + 6.f, logoArea.B + 4.f, b.L + sidebarW - 6.f, b.B - 8.f);'
  local new='const IRECT ampListArea(b.L + 24.f, logoArea.B + 4.f, b.L + sidebarW - 6.f, b.B - 8.f);'
  [[ -f "$ref" ]] || { fail "shift proof has no reference $ref"; return; }
  mutate_once "$layout" "$old" "$new" || { fail "shift proof could not edit the sidebar"; return; }
  if ! rebuild_target VoLum-app; then
    fail "shift proof rebuild"
    return
  fi
  if run_scene 1 main ""; then
    mv -f "$OUT/main-s1.png" "$OUT/main-s1-shifted.png"
  fi
  restore_red
  rebuild_target VoLum-app || fail "shift proof restore rebuild"
  [[ -f "$OUT/main-s1-shifted.png" ]] || { fail "shifted capture missing"; return; }
  local moved work
  work=$(mktemp -d)
  apply_masks "$ref" "$work/a.png" main
  apply_masks "$OUT/main-s1-shifted.png" "$work/b.png" main
  moved=$(ssim "$work/a.png" "$work/b.png")
  rm -rf "$work"
  echo "  SSIM reference vs shifted sidebar $moved"
  awk -v b="$moved" 'BEGIN { exit !(b + 0 < 0.99) }' \
    && pass "shifted sidebar is below 0.99 ($moved)" \
    || fail "shifted sidebar stayed at or above 0.99 ($moved)"
}

host_keys() {
  local tag="keys" 
  XDG="$OUT/xdg-$tag"
  DATA="$XDG/VoLum"
  seed_data "$DATA" ""
  LOG="$DATA/volum.log"
  CAP="$DATA/capture"
  local hostlog="$OUT/host-keys.txt" fwd="$OUT/forward-keys.txt"
  rm -f "$hostlog" "$fwd"
  env \
    XDG_DATA_HOME="$XDG" \
    VOLUM_SELF_CAPTURE_DIR="$CAP" \
    VOLUM_HOST_KEY_LOG="$hostlog" \
    VOLUM_FORWARD_KEY_LOG="$fwd" \
    JACK_DEFAULT_SERVER="$JACK_NAME" \
    JACK_NO_AUDIO_RESERVATION=1 \
    ALSA_CONFIG_PATH="$ALSA_CONF" \
    PULSE_SERVER="$PULSE" \
    "$APP" >"$OUT/keys.log" 2>&1 &
  APP_PID=$!
  if ! "$XTOOL" wait VoLum 30; then
    quit_app
    fail "key-forward launch"
    return
  fi
  sleep 3
  : >"$hostlog"
  : >"$fwd"
  drive "click 400 300" "key q" || true
  sleep 0.6
  quit_app
  echo "  forward log: $(grep -a . "$fwd" 2>/dev/null | tr '\n' ' ')"
  echo "  host log: $(grep -a . "$hostlog" 2>/dev/null | tr '\n' ' ')"
  if grep -a -q '^down ' "$hostlog"; then
    pass "unhandled q reached the standalone window"
  else
    fail "unhandled q did not reach the standalone window"
  fi
}

write_reaper_lua() {
  local path="$1"
  cat >"$path" <<'LUA'
local resultPath = os.getenv("VOLUM_SMOKE_RESULT") or ""
local shotDone = os.getenv("VOLUM_SMOKE_SHOT_DONE") or ""
local projectPath = os.getenv("VOLUM_SMOKE_PROJECT") or ""
local function report(line)
  local f = io.open(resultPath, "a")
  if f then f:write(line, "\n"); f:close() end
end
local steps, stepIndex, waitUntil, waitFor = {}, 1, 0, nil
local track
local function wait(seconds) waitUntil = reaper.time_precise() + seconds end
local function quit() reaper.Main_OnCommand(40004, 0) end
local function fail(reason) report("FAIL " .. reason); quit() end
steps[#steps + 1] = function()
  reaper.InsertTrackAtIndex(0, true)
  track = reaper.GetTrack(0, 0)
  if reaper.TrackFX_AddByName(track, "VST3:VoLum", false, -1) < 0 then
    return fail("could not insert VST3:VoLum")
  end
  report("STEP inserted")
  wait(2)
end
steps[#steps + 1] = function()
  reaper.TrackFX_Show(track, 0, 3)
  wait(1)
end
steps[#steps + 1] = function()
  if reaper.TrackFX_GetOpen(track, 0) == 0 then return fail("editor did not open") end
  report("STEP shot-ready")
  waitFor = function()
    local f = io.open(shotDone, "r")
    if f then f:close(); return true end
    return false
  end
end
steps[#steps + 1] = function()
  reaper.Main_SaveProjectEx(0, projectPath, 0)
  report("STEP saved")
  wait(0.3)
end
steps[#steps + 1] = function()
  reaper.TrackFX_Show(track, 0, 2)
  wait(0.5)
end
steps[#steps + 1] = function()
  reaper.DeleteTrack(track)
  report("STEP deleted")
  report("PASS")
  quit()
end
local function tick()
  if waitFor then
    if not waitFor() then reaper.defer(tick); return end
    waitFor = nil
  end
  if reaper.time_precise() >= waitUntil then
    local step = steps[stepIndex]
    if not step then return end
    stepIndex = stepIndex + 1
    step()
  end
  reaper.defer(tick)
end
reaper.defer(tick)
LUA
}

# One REAPER launch. Prints PASS/FAIL lines. Leaves the sandbox in $1.
reaper_session() {
  local work="$1" amp="${2:-}"
  local reaper="${REAPER:-$HOME/volum-linux/tools/reaper/reaper_linux_x86_64/REAPER/reaper}"
  local bundle="$ROOT/build-linux/VoLum.vst3"
  [[ -x "$reaper" ]] || { echo "ui-scenes: reaper missing" >&2; return 2; }
  [[ -d "$bundle" ]] || { echo "ui-scenes: vst3 missing at $bundle" >&2; return 2; }
  rm -rf "$work"
  mkdir -p "$work/cfg/Scripts" "$work/vst3" "$work/home/.config"
  seed_data "$work/home/.local/share/VoLum" "$amp"
  ln -s "$bundle" "$work/vst3/VoLum.vst3"
  cat >"$work/cfg/reaper.ini" <<EOF
[REAPER]
vstpath=$work/vst3
vstpath64=$work/vst3
loadlastproj=0
splashfeedback=0
newprojtmpl=
[verchk]
lastt=$(date +%s)
EOF
  write_reaper_lua "$work/cfg/Scripts/__startup.lua"
  env \
    VOLUM_KEY_LOG="$work/keys.txt" \
    HOME="$work/home" \
    XDG_CONFIG_HOME="$work/home/.config" \
    XDG_DATA_HOME="$work/home/.local/share" \
    VOLUM_SELF_CAPTURE_DIR="$work/home/.local/share/VoLum/capture" \
    VOLUM_SMOKE_RESULT="$work/result.txt" \
    VOLUM_SMOKE_SHOT_DONE="$work/shot.done" \
    VOLUM_SMOKE_PROJECT="$work/smoke.rpp" \
    JACK_DEFAULT_SERVER="$JACK_NAME" \
    JACK_NO_AUDIO_RESERVATION=1 \
    ALSA_CONFIG_PATH="$ALSA_CONF" \
    PULSE_SERVER="$PULSE" \
    "$reaper" -newinst -nosplash -ignoreerrors -cfgfile "$work/cfg/reaper.ini" \
    >"$work/reaper.log" 2>&1 &
  local rpid=$!
  local i ready=0
  for i in $(seq 120); do
    if [[ -f "$work/result.txt" ]] && grep -a -qx "STEP shot-ready" "$work/result.txt"; then
      ready=1
      break
    fi
    kill -0 "$rpid" 2>/dev/null || break
    sleep 0.5
  done
  if [[ "$ready" != 1 ]]; then
    kill -KILL "$rpid" 2>/dev/null || true
    wait "$rpid" 2>/dev/null || true
    echo "  reaper did not open the editor" >&2
    return 1
  fi
  sleep 4
  REAPER_PID=$rpid
  return 0
}

finish_reaper() {
  local work="$1" rpid="$2"
  touch "$work/shot.done"
  local i
  for i in $(seq 80); do
    kill -0 "$rpid" 2>/dev/null || break
    sleep 0.25
  done
  if kill -0 "$rpid" 2>/dev/null; then
    kill -TERM "$rpid" 2>/dev/null || true
    sleep 2
    kill -KILL "$rpid" 2>/dev/null || true
  fi
  wait "$rpid" 2>/dev/null || true
}

chunk_has() {
  # REAPER stores the plug-in state as base64 under the VST line.
  python3 - "$1" "$2" <<'PY'
import base64, pathlib, sys
text = pathlib.Path(sys.argv[1]).read_text(errors="replace")
lines, grab = [], False
for line in text.splitlines():
    if "<VST " in line:
        grab = True
        continue
    if grab:
        s = line.strip()
        if s.startswith("<") or s == ">":
            break
        lines.append(s)
raw = "".join(lines)
raw += "=" * ((4 - len(raw) % 4) % 4)
try:
    data = base64.b64decode(raw)
except Exception:
    sys.exit(1)
sys.exit(0 if sys.argv[2].encode() in data else 1)
PY
}

host_menu() {
  local work="$OUT/reaper-menu"
  reaper_session "$work" "" || { fail "popup menu: reaper did not start"; return; }
  local before after
  before=$(md5sum "$work/home/.local/share/VoLum/volum-settings.json" | awk '{print $1}')
  # Preset bar, then the first row under the open menu.
  drive "click 743 22" || { fail "popup menu: no editor window"; return; }
  sleep 0.8
  drive "click 539 23" "click 450 66" || { fail "popup menu: no editor window"; return; }
  sleep 0.8
  CAP="$work/home/.local/share/VoLum/capture"
  grab reaper-menu || true
  finish_reaper "$work" "$REAPER_PID"
  if chunk_has "$work/smoke.rpp" "V30-THC-1"; then
    pass "popup menu saved the factory reset in the REAPER project"
  else
    fail "popup menu did not save the factory reset in the REAPER project"
  fi
}

host_text() {
  local work="$OUT/reaper-text"
  reaper_session "$work" "" || { fail "text entry: reaper did not start"; return; }
  # Plugin editors open in PLAY. The header toggle is the BUILD switch.
  drive "click 743 22" || { fail "text entry: no editor window"; return; }
  sleep 0.8
  # One wheel notch brings the custom row up to the pen.
  drive "wheel 90 400 1" || { fail "text entry: no editor window"; return; }
  sleep 0.4
  drive "click 122 575" || { fail "text entry: no editor window"; return; }
  sleep 0.8
  CAP="$work/home/.local/share/VoLum/capture"
  grab reaper-text-builder || true
  # The name field opens with the current name selected, so typing replaces it.
  drive "click 200 121" || { fail "text entry: no editor window"; return; }
  sleep 0.6
  grab reaper-text-dialog || true
  drive "keys shift+l i n u x shift+p r o o f shift+a m p Return" || { fail "text entry: no editor window"; return; }
  sleep 0.6
  grab reaper-text-typed || true
  drive "click 656 526" || { fail "text entry: no editor window"; return; }
  sleep 1.2
  grab reaper-text || true
  finish_reaper "$work" "$REAPER_PID"
  local content="$work/home/.local/share/VoLum/content/volum-content.json"
  if grep -a -q "LinuxProofAmp" "$content" 2>/dev/null; then
    pass "text entry committed LinuxProofAmp"
  else
    fail "text entry did not commit LinuxProofAmp"
  fi
}

host_red() {
  local layout="$ROOT/NeuralAmpModeler/VoLumLayoutBuild.inc.cpp"
  local overlay="$ROOT/NeuralAmpModeler/VoLumCustomOverlay.h"
  local reset='pPlugin->_VolumResetAmpToFactory();'
  local reset_off='/* redproof */ (void) 0;'
  mutate_once "$layout" "$reset" "$reset_off" || { fail "popup red could not edit the menu"; return; }
  if ! rebuild_target VoLum-vst3; then
    fail "popup red rebuild"
    return
  fi
  local work="$OUT/reaper-menu-red"
  if reaper_session "$work" ""; then
    drive "click 743 22" || true
    sleep 0.6
    drive "click 539 23" "click 450 66" || true
    sleep 0.8
    finish_reaper "$work" "$REAPER_PID"
    if chunk_has "$work/smoke.rpp" "V30-THC-1"; then
      fail "popup menu still saved a reset with Default removed"
    else
      pass "popup menu is red when Default is removed"
    fi
  else
    fail "popup red: reaper did not start"
  fi
  restore_red
  rebuild_target VoLum-vst3 || { fail "popup red restore rebuild"; return; }

  local entry='void StartTextEntry(TextTarget target, const IRECT& bounds, const std::string& current, const IText* style = nullptr)
  {
    auto* ui = GetUI();'
  local entry_off='void StartTextEntry(TextTarget target, const IRECT& bounds, const std::string& current, const IText* style = nullptr)
  {
    return;
    auto* ui = GetUI();'
  mutate_once "$overlay" "$entry" "$entry_off" || { fail "text red could not edit text entry"; return; }
  if ! rebuild_target VoLum-vst3; then
    fail "text red rebuild"
    return
  fi
  work="$OUT/reaper-text-red"
  if reaper_session "$work" ""; then
    drive "click 743 22" || true
    sleep 0.8
    drive "wheel 90 400 1" || true
    sleep 0.4
    drive "click 122 575" || true
    sleep 0.8
    drive "click 200 121" || true
    sleep 0.6
    drive "keys shift+l i n u x shift+p r o o f shift+a m p Return" || true
    sleep 0.5
    drive "click 656 526" || true
    sleep 1.2
    finish_reaper "$work" "$REAPER_PID"
    if grep -a -q "LinuxProofAmp" "$work/home/.local/share/VoLum/content/volum-content.json" 2>/dev/null; then
      fail "text entry still committed with StartTextEntry removed"
    else
      pass "text entry is red when StartTextEntry returns immediately"
    fi
  else
    fail "text red: reaper did not start"
  fi
  restore_red
  rebuild_target VoLum-vst3 || fail "text red restore rebuild"
}

# Feed a 440 Hz tone and look at the BUILD OUT meter well (logical 789,430 10x40).
# want=lit expects the well to fill; want=dark expects the broken draw to stay empty.
prove_meter() {
  local want="$1"
  local tag="meter-$want"
  XDG="$OUT/xdg-$tag"
  DATA="$XDG/VoLum"
  seed_data "$DATA" ""
  LOG="$DATA/volum.log"
  CAP="$DATA/capture"
  env \
    XDG_DATA_HOME="$XDG" \
    VOLUM_SELF_CAPTURE_DIR="$CAP" \
    JACK_DEFAULT_SERVER="$JACK_NAME" \
    JACK_NO_AUDIO_RESERVATION=1 \
    ALSA_CONFIG_PATH="$ALSA_CONF" \
    PULSE_SERVER="$PULSE" \
    "$APP" >"$OUT/${tag}.log" 2>&1 &
  APP_PID=$!
  if ! "$XTOOL" wait VoLum 30; then
    quit_app
    fail "meter: no window"
    return
  fi
  sleep 4
  "$TONE" >"$OUT/${tag}-tone.log" 2>&1 &
  TONE_PID=$!
  sleep 1.2
  park
  grab "$tag" || { kill "$TONE_PID" 2>/dev/null || true; TONE_PID=""; quit_app; fail "meter: no capture"; return; }
  kill "$TONE_PID" 2>/dev/null || true
  TONE_PID=""
  quit_app
  local mean
  mean=$(convert "$OUT/${tag}.png" -crop 10x40+789+430 -colorspace Gray -threshold 12% -format '%[fx:mean]' info: 2>/dev/null || echo 0)
  echo "  meter well mean $mean ($want)"
  local lit=0
  awk -v m="$mean" 'BEGIN { exit !(m + 0 > 0.08) }' && lit=1
  if [[ "$want" == lit && "$lit" == 1 ]]; then
    pass "OUT meter lights under a 440 Hz tone ($mean)"
  elif [[ "$want" == dark && "$lit" == 0 ]]; then
    pass "OUT meter stays dark when its update is removed ($mean)"
  elif [[ "$want" == lit ]]; then
    fail "OUT meter did not light ($mean)"
  else
    fail "OUT meter still lit with drawing broken ($mean)"
  fi
}

prove_meter_red() {
  local header="$ROOT/NeuralAmpModeler/NeuralAmpModelerControls.h"
  local old='if (IsDisabled() || msgTag != ISender<>::kUpdateMessage)'
  local new='if (true || IsDisabled() || msgTag != ISender<>::kUpdateMessage)'
  mutate_once "$header" "$old" "$new" || { fail "meter red could not edit the meter"; return; }
  if ! rebuild_target VoLum-app; then
    fail "meter red rebuild"
    return
  fi
  prove_meter dark
  restore_red
  rebuild_target VoLum-app || fail "meter red restore rebuild"
}

scene_of() {
  local label="$1" base
  base=${label##* }
  printf '%s\n' "${base%%-s*}"
}

# Paint manifest rectangles the same colour on both shots. Coordinates are
# logical 900x600 and scale with the framebuffer. A shifted control leaves
# the rectangle, so the 18 px sidebar proof still fails.
apply_masks() {
  local src="$1" dest="$2" scene="$3"
  cp -f "$src" "$dest"
  [[ -f "$MANIFEST" ]] || return 0
  local w scale x y ww hh sc kind rest
  w=$(identify -format '%w' "$src")
  scale=$((w / 900))
  [[ "$scale" -ge 1 ]] || scale=1
  while read -r kind sc x y ww hh rest; do
    [[ "$kind" == mask ]] || continue
    [[ "$sc" == "*" || "$sc" == "$scene" || "$sc" == "$scene-s1" || "$sc" == "$scene-s2" ]] || continue
    local x1 y1 x2 y2
    x1=$((x * scale))
    y1=$((y * scale))
    x2=$(((x + ww) * scale - 1))
    y2=$(((y + hh) * scale - 1))
    convert "$dest" -fill '#101014' -draw "rectangle ${x1},${y1} ${x2},${y2}" "$dest"
  done <"$MANIFEST"
}

compare_pair() {
  local a="$1" b="$2" label="$3" min="$4"
  if [[ ! -f "$a" || ! -f "$b" ]]; then
    fail "$label missing ($([[ -f "$a" ]] && echo have-a || echo no-a) $([[ -f "$b" ]] && echo have-b || echo no-b))"
    return
  fi
  local da db score scene work
  da=$(dims "$a")
  db=$(dims "$b")
  if [[ "$da" != "$db" ]]; then
    fail "$label size $da vs $db"
    return
  fi
  scene=$(scene_of "$label")
  work=$(mktemp -d)
  apply_masks "$a" "$work/a.png" "$scene"
  apply_masks "$b" "$work/b.png" "$scene"
  score=$(ssim "$work/a.png" "$work/b.png")
  rm -rf "$work"
  echo "  SSIM $label $score ($da)"
  awk -v s="$score" -v m="$min" 'BEGIN { exit !(s+0 >= m+0) }' \
    && pass "$label $score >= $min" \
    || fail "$label $score < $min"
}

do_compare() {
  local linux_dir="$1" win_dir="$2" min="${3:-0}" report_dir="${4:-}"
  [[ -n "$report_dir" ]] && mkdir -p "$report_dir"
  local name scale
  for name in "${SCENES[@]}"; do
    for scale in $SCALES; do
      local L="$linux_dir/${name}-s${scale}.png" W="$win_dir/${name}-s${scale}.png"
      if [[ "$min" != 0 ]]; then
        compare_pair "$W" "$L" "${name}-s${scale}" "$min"
      else
        if [[ -f "$L" && -f "$W" ]]; then
          echo "  SSIM ${name}-s${scale} $(ssim "$W" "$L") ($(dims "$L"))"
        else
          echo "  SSIM ${name}-s${scale} missing"
        fi
      fi
      if [[ -n "$report_dir" && -f "$L" && -f "$W" ]]; then
        convert "$W" "$L" +append "$report_dir/${name}-s${scale}-pair.png"
        convert "$W" "$L" -resize 640x +append "$report_dir/${name}-s${scale}-pair-small.png"
      fi
    done
  done
  if [[ -n "$report_dir" ]]; then
    local smalls=()
    for name in "${SCENES[@]}"; do
      for scale in $SCALES; do
        [[ -f "$report_dir/${name}-s${scale}-pair-small.png" ]] && smalls+=("$report_dir/${name}-s${scale}-pair-small.png")
      done
    done
    if [[ ${#smalls[@]} -gt 0 ]]; then
      montage "${smalls[@]}" -tile 2x6 -geometry +6+6 -background '#1a1a1a' "$report_dir/contact.png"
      echo "  contact $report_dir/contact.png"
    fi
  fi
}

gate_refs() {
  local min="0.99"
  if [[ -f "$MANIFEST" ]]; then
    min=$(awk '$1 == "threshold" { print $2; exit }' "$MANIFEST")
  elif [[ -f "$REFS/threshold.txt" ]]; then
    min=$(awk 'NR==1 { print $1; exit }' "$REFS/threshold.txt")
  fi
  local name scale
  for name in "${SCENES[@]}"; do
    for scale in $SCALES; do
      compare_pair "$REFS/${name}-s${scale}.png" "$OUT/${name}-s${scale}.png" "ref ${name}-s${scale}" "$min"
    done
  done
}

case "$MODE" in
  compare)
    [[ -n "$OUT" && -n "$WIN" ]] || usage
    do_compare "$OUT" "$WIN" 0 "${REPORT:-}"
    ;;
  capture|all|gate)
    [[ -n "$OUT" ]] || OUT=$(mktemp -d /tmp/volum-ui-scenes.XXXXXX)
    echo "ui-scenes out $OUT"
    start_display
    capture_all
    if [[ "$MODE" == gate ]]; then
      prove_meter lit
    fi
    if [[ "$MODE" == all ]]; then
      host_keys
      host_menu
      host_text
    fi
    if [[ "$MODE" == gate ]]; then
      gate_refs
    fi
    if [[ -n "$WIN" ]]; then
      do_compare "$OUT" "$WIN" 0 "${REPORT:-$OUT/report}"
    fi
    ;;
  host)
    [[ -n "$OUT" ]] || OUT=$(mktemp -d /tmp/volum-ui-scenes.XXXXXX)
    echo "ui-scenes out $OUT"
    start_display
    host_keys
    host_menu
    host_text
    ;;
  meter)
    [[ -n "$OUT" ]] || OUT=$(mktemp -d /tmp/volum-ui-scenes.XXXXXX)
    echo "ui-scenes out $OUT"
    start_display
    prove_meter lit
    ;;
  red)
    [[ -n "$OUT" ]] || OUT=$(mktemp -d /tmp/volum-ui-scenes.XXXXXX)
    echo "ui-scenes out $OUT"
    start_display
    prove_shift "${RED_MAIN_REF:-$HOME/volum-linux/out/13/scenes/main-s1.png}"
    prove_meter_red
    host_red
    if git -C "$ROOT" diff --exit-code -- NeuralAmpModeler/VoLumLayoutBuild.inc.cpp NeuralAmpModeler/VoLumCustomOverlay.h >/dev/null; then
      pass "red proofs restored the source"
    else
      fail "red proofs left the source dirty"
    fi
    ;;
  *)
    usage
    ;;
esac

echo "ui-scenes: $CHECKS checks, $FAILS failed"
[[ "$FAILS" -eq 0 ]]
