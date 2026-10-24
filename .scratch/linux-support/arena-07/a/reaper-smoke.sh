#!/usr/bin/env bash
# Throwaway ticket 07a gate (ticket 08 owns the product version): REAPER for
# Linux under Xvfb, with a throwaway HOME, XDG dirs and REAPER config, so
# neither the owner's REAPER config nor the VoLum library is touched.
#
#   reaper-smoke.sh --bundle PATH/VoLum.vst3 --out DIR [--mode cycles|shot]
#                   [--cycles 20] [--instances 2] [--scale 1|2] [--asan]
#                   [--shot FILE.png] [--screen WxHxD]
#
# PASS needs: the Lua script reports PASS, REAPER exits before the timeout,
# no "IGraphicsLinux: X error" or "off the UI thread" line, and no ASan or
# UBSan report.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REAPER="${REAPER:-$HOME/volum-linux/tools/reaper/reaper_linux_x86_64/REAPER/reaper}"
BUNDLE="${BUNDLE:-}"
OUT=""
MODE=cycles
CYCLES=20
INSTANCES=2
SCALE=1
ASAN=0
SHOT=""
SCREEN=1920x1080x24
XFTDPI=""
TIMEOUT=300
INNER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle) BUNDLE="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --mode) MODE="$2"; shift 2 ;;
    --cycles) CYCLES="$2"; shift 2 ;;
    --instances) INSTANCES="$2"; shift 2 ;;
    --scale) SCALE="$2"; shift 2 ;;
    --asan) ASAN=1; shift ;;
    --shot) SHOT="$2"; shift 2 ;;
    --screen) SCREEN="$2"; shift 2 ;;
    --xft-dpi) XFTDPI="$2"; shift 2 ;;
    --inner) INNER=1; shift ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done

if [[ "$INNER" == 0 ]]; then
  [[ -d "$BUNDLE" && -n "$OUT" ]] || { echo "need --bundle and --out" >&2; exit 2; }
  BUNDLE="$(cd "$BUNDLE" && pwd)"
  mkdir -p "$OUT"
  OUT="$(cd "$OUT" && pwd)"
  WORK="$(mktemp -d "$OUT/reaper-$MODE.XXXXXX")"
  export WORK BUNDLE
  args=(--inner --mode "$MODE" --cycles "$CYCLES" --instances "$INSTANCES" --scale "$SCALE")
  [[ "$ASAN" == 1 ]] && args+=(--asan)
  [[ -n "$SHOT" ]] && args+=(--shot "$SHOT")
  [[ -n "$XFTDPI" ]] && args+=(--xft-dpi "$XFTDPI")
  set +e
  # -noreset: otherwise the server resets when xrdb (its only client) exits
  # and drops the RESOURCE_MANAGER property that carries Xft.dpi.
  xvfb-run -a -s "-screen 0 $SCREEN -noreset" bash "$HERE/reaper-smoke.sh" "${args[@]}"
  rc=$?
  set -e

  verdict=PASS
  grep -qx PASS "$WORK/result.txt" 2>/dev/null || verdict="FAIL ($(tail -1 "$WORK/result.txt" 2>/dev/null || echo 'no result'))"
  ! grep -q "^FAIL" "$WORK/result.txt" 2>/dev/null || verdict="$(grep "^FAIL" "$WORK/result.txt" | head -1)"
  [[ "$rc" == 0 ]] || verdict="FAIL (runner exit $rc)"
  xerrors=$(grep -c "IGraphicsLinux: X error" "$WORK/reaper.log" || true)
  offthread=$(grep -c "off the UI thread" "$WORK/reaper.log" || true)
  # REAPER's helper child processes inherit LD_PRELOAD and fail to map ASan's
  # shadow ("ReserveShadowMemoryRange failed"); they never load VoLum, so only
  # real reports count.
  # UBSan and some ASan output go to REAPER's stderr rather than log_path.
  sanitizer=$( (grep -a -h "ERROR: AddressSanitizer:\|runtime error:" "$WORK"/asan.* "$WORK"/ubsan.* "$WORK/reaper.log" 2>/dev/null || true) | wc -l)
  [[ "$xerrors" == 0 ]] || verdict="FAIL ($xerrors X errors)"
  [[ "$offthread" == 0 ]] || verdict="FAIL ($offthread off-UI-thread registry calls)"
  [[ "$sanitizer" == 0 ]] || verdict="FAIL ($sanitizer sanitizer report lines)"
  echo "reaper $MODE scale=$SCALE asan=$ASAN cycles=$CYCLES instances=$INSTANCES: $verdict"
  opens=$(grep -c "IGraphicsLinux: editor .* open in" "$WORK/reaper.log" || true)
  closes=$(grep -c "IGraphicsLinux: editor .* closed" "$WORK/reaper.log" || true)
  echo "  steps: $(grep -c '^STEP' "$WORK/result.txt" 2>/dev/null || echo 0), editor opens/closes: $opens/$closes, X errors: $xerrors, off-thread calls: $offthread, sanitizer report lines: $sanitizer"
  echo "  log: $WORK"
  [[ "$verdict" == PASS ]]
  exit $?
fi

# Inside xvfb-run.
home="$WORK/home"
cfg="$home/reaper"
vst="$WORK/vst3"
mkdir -p "$cfg/Scripts" "$vst" "$home/.config" "$home/.local/share"
ln -s "$BUNDLE" "$vst/VoLum.vst3"
cp "$HERE/reaper-smoke.lua" "$cfg/Scripts/__startup.lua"
cat > "$cfg/reaper.ini" <<EOF
[REAPER]
vstpath=$vst
vstpath64=$vst
loadlastproj=0
splashfeedback=0
newprojtmpl=
[verchk]
lastt=$(date +%s)
EOF

export HOME="$home" XDG_CONFIG_HOME="$home/.config" XDG_DATA_HOME="$home/.local/share"
luamode="$MODE"
[[ "$MODE" == input ]] && luamode=shot
export IGRAPHICS_LINUX_TRACE=1
export VOLUM_SMOKE_MODE="$luamode" VOLUM_SMOKE_CYCLES="$CYCLES" VOLUM_SMOKE_INSTANCES="$INSTANCES"
export VOLUM_SMOKE_RESULT="$WORK/result.txt" VOLUM_SMOKE_SHOT_DONE="$WORK/shot.done" VOLUM_SMOKE_PROJECT="$WORK/smoke.rpp"
[[ "$SCALE" == 1 ]] || export GDK_SCALE="$SCALE"
if [[ -n "$XFTDPI" ]]; then
  echo "Xft.dpi: $XFTDPI" | xrdb -nocpp -merge
  xrdb -query > "$WORK/xrdb.txt"
fi
preload=()
if [[ "$ASAN" == 1 ]]; then
  # The host is not instrumented; leaks in REAPER itself are not ours.
  preload=(env "LD_PRELOAD=$(gcc -print-file-name=libasan.so)"
    "ASAN_OPTIONS=detect_leaks=0:log_path=$WORK/asan"
    "UBSAN_OPTIONS=print_stacktrace=1:log_path=$WORK/ubsan:suppressions=$HERE/ubsan.supp")
fi
# GDB=1 prints a backtrace of every thread when REAPER crashes.
[[ "${GDB:-0}" == 1 ]] && preload+=(gdb -q -batch -ex run -ex "thread apply all bt 25" --args)

"${preload[@]}" "$REAPER" -newinst -nosplash -ignoreerrors -cfgfile "$cfg/reaper.ini" > "$WORK/reaper.log" 2>&1 &
pid=$!

if [[ "$MODE" == shot || "$MODE" == input ]]; then
  for ((i = 0; i < 240; i++)); do
    grep -qx "STEP shot-ready" "$WORK/result.txt" 2>/dev/null && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.5
  done
  sleep 4 # several draw-timer frames
  wid=""
  for w in $(xdotool search --onlyvisible --name "VoLum" 2>/dev/null); do
    wid="$w"
  done
  if [[ -n "$wid" && -n "$SHOT" ]]; then
    xdotool getwindowgeometry "$wid" > "$WORK/shot-geometry.txt" || true
    import -window "$wid" "$SHOT"
    import -window root "${SHOT%.png}-root.png"
    echo "STEP shot $wid" >> "$WORK/result.txt"
  fi
  if [[ -n "$wid" && "$MODE" == input ]]; then
    # REAPER's FX header sits above the editor: editor y = window y - header.
    height=$(xdotool getwindowgeometry "$wid" | sed -n 's/.*Geometry: [0-9]*x\([0-9]*\).*/\1/p')
    top=$((height - 600))
    snap() {
      import -window "$wid" "$WORK/input-$1.png"
      xdotool getwindowgeometry "$wid" | sed -n 's/.*Geometry: //p' > "$WORK/input-$1.geometry"
      echo "STEP input $1 $(cat "$WORK/input-$1.geometry")" >> "$WORK/result.txt"
    }
    snap 0-initial
    xdotool mousemove --window "$wid" 800 $((top + 242)) click 1; sleep 1; snap 1-click-sound-02
    xdotool key Down; sleep 1; snap 2-key-down
    xdotool mousemove --window "$wid" 800 $((top + 300)) click 5 click 5 click 5; sleep 1; snap 3-wheel-list
    xdotool mousemove --window "$wid" 450 $((top + 250)) click --repeat 2 --delay 60 1; sleep 1; snap 4-double-click
    xdotool mousemove --window "$wid" 896 $((top + 596)) mousedown 1 mousemove_relative 60 40 sleep 0.3 mousemove_relative 60 40 sleep 0.5 mouseup 1
    sleep 2; snap 5-corner-drag
    xdotool key space; sleep 1; echo "STEP input 6-key-space (forwarded to the host)" >> "$WORK/result.txt"
    for pair in "0-initial 1-click-sound-02" "1-click-sound-02 2-key-down" "2-key-down 3-wheel-list"; do
      set -- $pair
      diff=$(compare -metric AE "$WORK/input-$1.png" "$WORK/input-$2.png" null: 2>&1 || true)
      echo "STEP diff $1 -> $2: $diff pixels" >> "$WORK/result.txt"
    done
  fi
  touch "$WORK/shot.done"
fi

waited=0
while kill -0 "$pid" 2>/dev/null; do
  if ((waited >= TIMEOUT)); then
    import -window root "$WORK/hang.png" || true
    kill -9 "$pid" 2>/dev/null || true
    echo "FAIL timeout" >> "$WORK/result.txt"
    break
  fi
  sleep 1
  waited=$((waited + 1))
done
wait "$pid" 2>/dev/null || true
exit 0
