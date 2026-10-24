#!/usr/bin/env bash
# Linux VST3 gates other than REAPER: the UI-thread call sites, the Steinberg
# validator, pluginval at strictness 10, and Carla's single-client engine.
#
#   validate-vst3-linux.sh [--bundle PATH/VoLum.vst3] [--no-build]
#                          [--asan] [--pluginval-only]
#
# --asan is the sanitizer pluginval: LD_PRELOAD=libasan.so, detect_leaks=0,
# and the one stb_image.h suppression. It does not run the validator, Carla
# or the UI-thread harness. LeakSanitizer stays off because pluginval is an
# uninstrumented host; the Clang ASan doctest still runs with leaks enabled.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=linux/host-env.sh
source "$SCRIPT_DIR/linux/host-env.sh"
linux_gate_root
cd "$REPO_ROOT"

# box.ps1 holds the build flock on fds 7-9. A child that ignores SIGHUP
# (Xvfb, jackd) would keep that lock after this script dies.
for fd in 7 8 9; do
  eval "exec ${fd}>&-" 2>/dev/null || true
done

# jackd ignores SIGHUP, so an ssh -tt drop or a killed gate reparents it to
# init. The pid is stopped from EXIT/INT/TERM/HUP, and the process itself gets
# SIGTERM when this shell dies (PR_SET_PDEATHSIG), including SIGKILL of the shell.
VOLUM_CARLA_JACK_PID=""
stop_carla_jack() {
  local pid="${VOLUM_CARLA_JACK_PID:-}"
  [[ -n "$pid" ]] || return 0
  VOLUM_CARLA_JACK_PID=""
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}
trap 'stop_carla_jack' EXIT
trap 'stop_carla_jack; exit 129' HUP
trap 'stop_carla_jack; exit 130' INT
trap 'stop_carla_jack; exit 143' TERM

BUNDLE=""
NO_BUILD=0
ASAN=0
PLUGINVAL_ONLY=0
SKIP_UI=0
SKIP_VALIDATOR=0
SKIP_PLUGINVAL=0
SKIP_CARLA=0
SKIP_STDLIB=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --bundle) BUNDLE="$2"; shift 2 ;;
    --no-build) NO_BUILD=1; shift ;;
    --asan) ASAN=1; shift ;;
    --pluginval-only) PLUGINVAL_ONLY=1; shift ;;
    --skip-ui-thread) SKIP_UI=1; shift ;;
    --skip-validator) SKIP_VALIDATOR=1; shift ;;
    --skip-pluginval) SKIP_PLUGINVAL=1; shift ;;
    --skip-carla) SKIP_CARLA=1; shift ;;
    --skip-stdlib-check) SKIP_STDLIB=1; shift ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done

if [[ "$ASAN" == 0 && -z "$BUNDLE" ]]; then
  BUNDLE="$REPO_ROOT/build-linux/VoLum.vst3"
fi
if [[ "$ASAN" == 1 && -z "$BUNDLE" ]]; then
  BUNDLE="$REPO_ROOT/build-linux-asan/VoLum.vst3"
fi

if [[ "$NO_BUILD" == 0 && "$ASAN" == 0 ]]; then
  bash "$REPO_ROOT/NeuralAmpModeler/scripts/build-linux.sh" Release
fi
[[ -d "$BUNDLE" ]] || { echo "validate-vst3-linux: missing bundle $BUNDLE" >&2; exit 2; }
BUNDLE="$(cd "$BUNDLE" && pwd)"
SO="$BUNDLE/Contents/x86_64-linux/VoLum.so"
[[ -f "$SO" ]] || { echo "validate-vst3-linux: missing $SO" >&2; exit 2; }

SHA="$(git rev-parse --short HEAD)"
OUT="$(linux_gate_out "$SHA")"
mkdir -p "$OUT"
PLUGINVAL="$(linux_tool PLUGINVAL "tools/bin/pluginval" pluginval)"
SCREEN=1920x1080x24

run_validator() {
  local validator="$REPO_ROOT/build-linux/validator"
  if [[ ! -x "$validator" ]]; then
    [[ "$NO_BUILD" == 1 ]] && { echo "validate-vst3-linux: missing $validator" >&2; return 1; }
    bash "$REPO_ROOT/NeuralAmpModeler/scripts/build-linux.sh" Release
  fi
  local home log rc
  home="$(linux_throwaway_home "$OUT")"
  log="$OUT/validator.log"
  set +e
  HOME="$home" XDG_DATA_HOME="$home/.local/share" "$validator" "$BUNDLE" > "$log" 2>&1
  rc=$?
  set -e
  grep -a -E "Result:" "$log" | tail -2 || true
  echo "validator exit $rc"
  [[ "$rc" == 0 ]]
}

run_pluginval() {
  local log="$OUT/pluginval.log" home rc preload=()
  [[ "$ASAN" == 0 ]] || log="$OUT/pluginval-asan.log"
  home="$(linux_throwaway_home "$OUT")"
  if [[ "$ASAN" == 1 ]]; then
    local lib
    lib="$(linux_asan_lib)"
    preload=(env "LD_PRELOAD=$lib"
      "ASAN_OPTIONS=detect_leaks=0"
      "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1:suppressions=$UBSAN_SUPP")
  fi
  set +e
  HOME="$home" XDG_CONFIG_HOME="$home/.config" XDG_DATA_HOME="$home/.local/share" IGRAPHICS_LINUX_TRACE=1 \
    xvfb-run -a -s "-screen 0 $SCREEN" "${preload[@]}" "$PLUGINVAL" --strictness-level 10 --validate-in-process \
    --timeout-ms 1800000 --output-dir "$OUT/pluginval-logs" --validate "$BUNDLE" > "$log" 2>&1
  rc=$?
  set -e
  echo "pluginval exit $rc ($log), editor opens/closes: $(grep -a -c 'IGraphicsLinux: editor .* open in' "$log" || true)/$(grep -a -c 'IGraphicsLinux: editor .* closed' "$log" || true)"
  grep -a -E "^(SUCCESS|FAILURE)|ALL TESTS PASSED|tests? (passed|failed)" "$log" | tail -8 || true
  linux_clean_log "$log" && [[ "$rc" == 0 ]]
}

run_carla() {
  local server="volum-carla-$$" log="$OUT/carla.log" jack_log="$OUT/carla-jackd.log" home rc
  home="$(linux_throwaway_home "$OUT")"
  # Parent of jackd is this shell. prctl survives exec, so the server dies
  # with the shell even when a trap does not run.
  python3 - "$server" "$jack_log" <<'PY' &
import ctypes, os, signal, sys
server, log = sys.argv[1], sys.argv[2]
libc = ctypes.CDLL("libc.so.6", use_errno=True)
if libc.prctl(1, signal.SIGTERM) != 0:
    os._exit(1)
if os.getppid() == 1:
    os._exit(1)
fd = os.open(log, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
os.dup2(fd, 1)
os.dup2(fd, 2)
os.close(fd)
for n in (7, 8, 9):
    try:
        os.close(n)
    except OSError:
        pass
os.environ["JACK_DEFAULT_SERVER"] = server
os.execvp("jackd", ["jackd", "-n", server, "-d", "dummy", "-r", "48000", "-p", "256"])
PY
  VOLUM_CARLA_JACK_PID=$!
  local jack_pid="$VOLUM_CARLA_JACK_PID"
  local i up=0
  for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
    if ! kill -0 "$jack_pid" 2>/dev/null; then
      echo "carla-single: jackd exited early" >&2
      cat "$jack_log" >&2 || true
      return 1
    fi
    if compgen -G "/dev/shm/*${server}*" > /dev/null; then up=1; break; fi
    sleep 0.25
  done
  if [[ "$up" != 1 ]]; then
    echo "carla-single: jackd did not come up" >&2
    cat "$jack_log" >&2 || true
    stop_carla_jack
    return 1
  fi
  set +e
  HOME="$home" XDG_CONFIG_HOME="$home/.config" XDG_DATA_HOME="$home/.local/share" \
    JACK_DEFAULT_SERVER="$server" \
    xvfb-run -a -s "-screen 0 $SCREEN" \
    python3 "$LINUX_DIR/carla-single.py" "$BUNDLE" > "$log" 2>&1
  rc=$?
  set -e
  cat "$log"
  echo "carla-single exit $rc"
  stop_carla_jack
  [[ "$rc" == 0 ]]
}

if [[ "$SKIP_STDLIB" == 0 ]]; then
  echo "=== stdlib exports ==="
  bash "$LINUX_DIR/check-stdlib-exports.sh" "$SO"
fi
if [[ "$ASAN" == 0 && "$PLUGINVAL_ONLY" == 0 && "$SKIP_UI" == 0 ]]; then
  echo "=== ui-thread sites ==="
  bash "$LINUX_DIR/ui-thread-sites.sh" --so "$SO"
fi
if [[ "$ASAN" == 0 && "$PLUGINVAL_ONLY" == 0 && "$SKIP_VALIDATOR" == 0 ]]; then
  echo "=== validator ==="
  run_validator
fi
if [[ "$SKIP_PLUGINVAL" == 0 ]]; then
  echo "=== pluginval ==="
  run_pluginval
fi
if [[ "$ASAN" == 0 && "$PLUGINVAL_ONLY" == 0 && "$SKIP_CARLA" == 0 ]]; then
  echo "=== carla-single ==="
  run_carla
fi
echo "validate-vst3-linux: PASS"
