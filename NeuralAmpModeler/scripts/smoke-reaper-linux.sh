#!/usr/bin/env bash
# REAPER for Linux, headless under Xvfb, with a throwaway HOME.
#
#   smoke-reaper-linux.sh [--bundle PATH/VoLum.vst3] [--mode gates|cycles]
#                         [--cycles 20] [--instances 2] [--asan]
#                         [--reference FILE.wav] [--write-reference]
#
# gates (the plugin gate): load VoLum, open the editor, render 10 s of the
# fixture DI through the default rig twice, null the two renders against each
# other and against the committed Linux reference (peak residual <= -100 dBFS,
# the platform_null.h bar; bits differ across libm builds), then open two
# instances and close them.
# cycles: two instances opened and closed --cycles times. The sanitizer host
# smoke uses --asan --mode cycles --cycles 20 --instances 2.
#
# LeakSanitizer stays off (detect_leaks=0). REAPER is an uninstrumented host;
# its own leaks are not VoLum's. LD_PRELOAD=libasan.so is required for that
# host and is set here, not in CMake.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=linux/host-env.sh
source "$SCRIPT_DIR/linux/host-env.sh"
linux_gate_root

# box.ps1 holds the build flock on fds 7-9. Close them before any child.
for fd in 7 8 9; do
  eval "exec ${fd}>&-" 2>/dev/null || true
done

BUNDLE=""
OUT=""
MODE=gates
CYCLES=20
INSTANCES=2
ASAN=0
REFERENCE=""
WRITE_REF=0
SCREEN=1920x1080x24
TIMEOUT=900
INNER=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle) BUNDLE="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --mode) MODE="$2"; shift 2 ;;
    --cycles) CYCLES="$2"; shift 2 ;;
    --instances) INSTANCES="$2"; shift 2 ;;
    --asan) ASAN=1; shift ;;
    --reference) REFERENCE="$2"; shift 2 ;;
    --write-reference) WRITE_REF=1; shift ;;
    --screen) SCREEN="$2"; shift 2 ;;
    --inner) INNER=1; shift ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done

if [[ "$INNER" == 0 ]]; then
  cd "$REPO_ROOT"
  if [[ -z "$BUNDLE" ]]; then
    BUNDLE="$REPO_ROOT/build-linux/VoLum.vst3"
  fi
  [[ -d "$BUNDLE" ]] || { echo "smoke-reaper-linux: missing bundle $BUNDLE" >&2; exit 2; }
  BUNDLE="$(cd "$BUNDLE" && pwd)"
  if [[ -z "$OUT" ]]; then
    SHA="$(git rev-parse --short HEAD)"
    OUT="$(linux_gate_out "$SHA")"
  fi
  mkdir -p "$OUT"
  OUT="$(cd "$OUT" && pwd)"
  [[ -n "$REFERENCE" ]] || REFERENCE="$REAPER_REFERENCE"
  WORK="$(mktemp -d "$OUT/reaper-$MODE.XXXXXX")"
  export WORK BUNDLE REFERENCE WRITE_REF
  args=(--inner --bundle "$BUNDLE" --mode "$MODE" --cycles "$CYCLES" --instances "$INSTANCES" --screen "$SCREEN")
  [[ "$ASAN" == 1 ]] && args+=(--asan)
  set +e
  xvfb-run -a -s "-screen 0 $SCREEN -noreset" bash "$SCRIPT_DIR/smoke-reaper-linux.sh" "${args[@]}"
  rc=$?
  set -e

  verdict=PASS
  grep -qx PASS "$WORK/result.txt" 2>/dev/null || verdict="FAIL ($(tail -1 "$WORK/result.txt" 2>/dev/null || echo 'no result'))"
  ! grep -q "^FAIL" "$WORK/result.txt" 2>/dev/null || verdict="$(grep "^FAIL" "$WORK/result.txt" | head -1)"
  [[ "$rc" == 0 ]] || verdict="FAIL (runner exit $rc)"
  xerrors=0
  offthread=0
  if [[ -f "$WORK/reaper.log" ]]; then
    xerrors=$(grep -a -c "IGraphicsLinux: X error" "$WORK/reaper.log" || true)
    offthread=$(grep -a -c "off the UI thread" "$WORK/reaper.log" || true)
  fi
  [[ -n "$xerrors" ]] || xerrors=0
  [[ -n "$offthread" ]] || offthread=0
  sanitizer=$( (grep -a -h "ERROR: AddressSanitizer:\|runtime error:" "$WORK"/asan.* "$WORK"/ubsan.* "$WORK/reaper.log" 2>/dev/null || true) | wc -l)
  [[ "$xerrors" == 0 ]] || verdict="FAIL ($xerrors X errors)"
  [[ "$offthread" == 0 ]] || verdict="FAIL ($offthread off-UI-thread calls)"
  [[ "$sanitizer" == 0 ]] || verdict="FAIL ($sanitizer sanitizer report lines)"

  if [[ "$MODE" == gates && "$verdict" == PASS ]]; then
    if [[ ! -f "$WORK/render/render-a.wav" || ! -f "$WORK/render/render-b.wav" ]]; then
      verdict="FAIL (render wav missing)"
    else
      set +e
      python3 "$LINUX_DIR/null-wav.py" "$WORK/render/render-b.wav" "$WORK/render/render-a.wav" | tee "$WORK/self-null.txt"
      self_rc=${PIPESTATUS[0]}
      set -e
      [[ "$self_rc" == 0 ]] || verdict="FAIL (the two renders do not null)"
      if [[ "$verdict" == PASS ]]; then
        if [[ "$WRITE_REF" == 1 ]]; then
          mkdir -p "$(dirname "$REFERENCE")"
          cp "$WORK/render/render-b.wav" "$REFERENCE"
          echo "wrote reference $REFERENCE"
        fi
        if [[ ! -f "$REFERENCE" ]]; then
          verdict="FAIL (no reference $REFERENCE; rerun with --write-reference)"
        else
          set +e
          python3 "$LINUX_DIR/null-wav.py" "$WORK/render/render-b.wav" "$REFERENCE" | tee "$WORK/ref-null.txt"
          ref_rc=${PIPESTATUS[0]}
          set -e
          [[ "$ref_rc" == 0 ]] || verdict="FAIL (render does not null against the reference)"
        fi
      fi
    fi
  fi

  echo "reaper $MODE asan=$ASAN cycles=$CYCLES instances=$INSTANCES: $verdict"
  opens=$(grep -a -c "IGraphicsLinux: editor .* open in" "$WORK/reaper.log" || true)
  closes=$(grep -a -c "IGraphicsLinux: editor .* closed" "$WORK/reaper.log" || true)
  echo "  steps: $(grep -c '^STEP' "$WORK/result.txt" 2>/dev/null || echo 0), editor opens/closes: $opens/$closes, X errors: $xerrors, off-thread calls: $offthread, sanitizer report lines: $sanitizer"
  echo "  log: $WORK"
  [[ "$verdict" == PASS ]]
  exit $?
fi

# Inside xvfb-run. WORK, BUNDLE, REFERENCE and WRITE_REF come from the parent.
REAPER="$(linux_tool REAPER "tools/reaper/reaper_linux_x86_64/REAPER/reaper" reaper)"
home="$WORK/home"
cfg="$home/reaper"
vst="$WORK/vst3"
render="$WORK/render"
mkdir -p "$cfg/Scripts" "$vst" "$render" "$home/.config" "$home/.local/share"
ln -s "$BUNDLE" "$vst/VoLum.vst3"
cp "$SCRIPT_DIR/smoke-reaper-linux.lua" "$cfg/Scripts/__startup.lua"
python3 "$LINUX_DIR/write-di.py" "$WORK/di.wav"
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
export IGRAPHICS_LINUX_TRACE=1
export VOLUM_SMOKE_MODE="$MODE" VOLUM_SMOKE_CYCLES="$CYCLES" VOLUM_SMOKE_INSTANCES="$INSTANCES"
export VOLUM_SMOKE_RESULT="$WORK/result.txt" VOLUM_SMOKE_PROJECT="$WORK/smoke.rpp"
export VOLUM_SMOKE_DI="$WORK/di.wav" VOLUM_SMOKE_RENDER_DIR="$render"
preload=()
if [[ "$ASAN" == 1 ]]; then
  lib="$(linux_asan_lib)"
  preload=(env "LD_PRELOAD=$lib"
    "ASAN_OPTIONS=detect_leaks=0:log_path=$WORK/asan"
    "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:log_path=$WORK/ubsan:suppressions=$UBSAN_SUPP")
fi
[[ "${GDB:-0}" == 1 ]] && preload+=(gdb -q -batch -ex run -ex "thread apply all bt 25" --args)

# Close the box.ps1 lock fds so a REAPER that outlives this session cannot
# keep the build flock.
"${preload[@]}" "$REAPER" -newinst -nosplash -ignoreerrors -cfgfile "$cfg/reaper.ini" > "$WORK/reaper.log" 2>&1 7>&- 8>&- 9>&- &
pid=$!
waited=0
while kill -0 "$pid" 2>/dev/null; do
  if ((waited >= TIMEOUT)); then
    import -window root "$WORK/hang.png" 2>/dev/null || true
    kill -9 "$pid" 2>/dev/null || true
    echo "FAIL timeout" >> "$WORK/result.txt"
    break
  fi
  sleep 1
  waited=$((waited + 1))
done
wait "$pid" 2>/dev/null || true
exit 0
