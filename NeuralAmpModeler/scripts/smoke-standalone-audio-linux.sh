#!/usr/bin/env bash
# Headless audio smoke of the Linux standalone: its own Xvfb, jackd's dummy
# driver at 48 kHz / 256 frames, and a sandboxed XDG_DATA_HOME. No sound card,
# no window manager, and nothing of the user's audio setup is touched.
#
#   bash NeuralAmpModeler/scripts/smoke-standalone-audio-linux.sh [--app PATH] [--out DIR] [--seconds N]
#
# It passes when:
#   1. VoLum opens its window and JACK ports and turns a -12 dBFS sine into a
#      finite output above -50 dBFS for --seconds (default 60; the first 5 may
#      be silent while the first capture loads) with zero xruns in that window,
#      counted by the server's xrun callback and in the jackd log (xrun lines
#      during each launch and quit are printed, not gated);
#   2. the close button (WM_DELETE_WINDOW) and SIGTERM quit with exit code 0
#      within 20 s, after the audio and plug-in teardowns, and settings.ini and
#      volum-settings.json are written;
#   3. two launches seeded with different lastAmpIdx (how the e2e harness
#      switches amps) stage different captures and keep their amp;
#   4. Preferences lists ALSA, JACK and PulseAudio, and with libjack hidden
#      (bubblewrap binds /dev/null over it) JACK alone is gone, both from
#      --list-audio-devices and from Preferences, and the app still quits cleanly.
#
# --require-hide fails step 4 when bubblewrap cannot run instead of skipping it.
# --require-rt fails when jackd cannot get real-time scheduling. Without sudo the
# script keeps the caller's rtprio and memlock limits.
# Red proofs: --quit kill (no graceful quit), --stall-ms N (one stalled JACK
# cycle), --amp-b same (the two seeded launches pick the same amp).
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
APP="$ROOT/build-linux/standalone/VoLum"
OUT=""
SECONDS_MAIN=60
QUIT_MODE="close"
STALL_MS=0
AMP_A=3
AMP_B=9
REQUIRE_HIDE=0
REQUIRE_RT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app) APP="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --seconds) SECONDS_MAIN="$2"; shift 2 ;;
    --quit) QUIT_MODE="$2"; shift 2 ;;
    --stall-ms) STALL_MS="$2"; shift 2 ;;
    --amp-b) [[ "$2" == same ]] && AMP_B=$AMP_A || AMP_B="$2"; shift 2 ;;
    --require-hide) REQUIRE_HIDE=1; shift ;;
    --require-rt) REQUIRE_RT=1; shift ;;
    *) echo "usage: $0 [--app PATH] [--out DIR] [--seconds N] [--require-hide] [--require-rt] [--quit close|kill] [--stall-ms N] [--amp-b N|same]" >&2; exit 2 ;;
  esac
done

[[ -x "$APP" ]] || { echo "smoke: no app at $APP (build it with NeuralAmpModeler/scripts/build-linux.sh)" >&2; exit 2; }
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
OUT="${OUT:-$(mktemp -d /tmp/volum-smoke-app.XXXXXX)}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
for tool in Xvfb jackd cc; do
  command -v "$tool" >/dev/null || { echo "smoke: $tool is missing (provision-linux.sh installs it)" >&2; exit 2; }
done

FAILS=0
CHECKS=0
pass() { CHECKS=$((CHECKS + 1)); echo "  PASS  $*"; }
fail() { CHECKS=$((CHECKS + 1)); FAILS=$((FAILS + 1)); echo "  FAIL  $*"; }
check() { local what="$1"; shift; if "$@"; then pass "$what"; else fail "$what"; fi; }

XVFB_PID=""
JACK_PID=""
APP_PID=""
cleanup() {
  [[ -n "$APP_PID" ]] && kill -9 "$APP_PID" 2>/dev/null
  [[ -n "$JACK_PID" ]] && kill "$JACK_PID" 2>/dev/null
  [[ -n "$XVFB_PID" ]] && kill "$XVFB_PID" 2>/dev/null
  wait 2>/dev/null
}
trap cleanup EXIT

echo "== build the probes"
cc -O2 -o "$OUT/volum-jack-probe" "$SCRIPT_DIR/linux/smoke/volum-jack-probe.c" -ljack -lm || exit 2
cc -O2 -o "$OUT/volum-x-tool" "$SCRIPT_DIR/linux/smoke/volum-x-tool.c" -lX11 || exit 2
PROBE="$OUT/volum-jack-probe"
XTOOL="$OUT/volum-x-tool"

echo "== Xvfb"
# The first free display from :90. Not -displayfd: under WSLg /tmp/.X11-unix
# is read-only and -displayfd gives up on every display, while a fixed one
# still listens on the abstract socket.
for n in $(seq 90 189); do
  [[ -e "/tmp/.X$n-lock" ]] && continue
  Xvfb ":$n" -screen 0 1600x1000x24 -nolisten tcp -noreset >"$OUT/xvfb.log" 2>&1 &
  XVFB_PID=$!
  for _ in $(seq 50); do
    DISPLAY=":$n" "$XTOOL" ping 2>/dev/null && break
    kill -0 "$XVFB_PID" 2>/dev/null || break
    sleep 0.1
  done
  if DISPLAY=":$n" "$XTOOL" ping 2>/dev/null; then export DISPLAY=":$n"; break; fi
  kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null; XVFB_PID=""
done
[[ -n "$XVFB_PID" ]] || { echo "smoke: Xvfb did not start" >&2; tail -5 "$OUT/xvfb.log" >&2; exit 2; }
echo "DISPLAY=$DISPLAY"

echo "== jackd dummy 48000 / 256"
# JACK runs real-time on a configured system: jackd2 gives the audio group
# rtprio 95 and unlimited memlock. Give this process tree the same limits
# (its own rlimits only, nothing system-wide) when it can.
if [[ "$(ulimit -r)" == 0 ]] && command -v prlimit >/dev/null && sudo -n true 2>/dev/null; then
  sudo -n prlimit --pid $$ --rtprio=95:95 --memlock=unlimited:unlimited
fi
echo "rtprio limit $(ulimit -r), memlock limit $(ulimit -l)"
# A private server name, so nothing else's JACK server is used or disturbed.
export JACK_DEFAULT_SERVER="volum-smoke-$$"
export JACK_NO_AUDIO_RESERVATION=1
jackd --name "$JACK_DEFAULT_SERVER" -d dummy -r 48000 -p 256 >"$OUT/jackd.log" 2>&1 &
JACK_PID=$!
"$PROBE" wait-server 15 || { echo "smoke: jackd did not come up" >&2; cat "$OUT/jackd.log" >&2; exit 2; }
grep -ai "real-time\|realtime" "$OUT/jackd.log" | head -3
if grep -aqi "cannot use real-time" "$OUT/jackd.log"; then
  [[ $REQUIRE_RT -eq 1 ]] && fail "[jackd] runs with real-time scheduling" || echo "  jackd is not real-time: xruns are likely"
else
  pass "[jackd] runs with real-time scheduling"
fi

# Nothing may reach a real output: ALSA's default is a null PCM and PulseAudio
# points at a socket that does not exist. The fallback to ALSA in run 5 and the
# device listings would otherwise open the machine's default device.
cat >"$OUT/asound.conf" <<'ALSA'
pcm.!default { type null }
ALSA
export ALSA_CONFIG_PATH="$OUT/asound.conf"
export PULSE_SERVER="unix:$OUT/no-pulse-server"

export XDG_DATA_HOME="$OUT/xdg"
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

LAUNCH_LOG_START=0
APP_RC=""
PHASE_XRUNS=0
# launch <name> [launcher words...] -- [app args...]
launch() {
  local name="$1"; shift
  local -a wrap=()
  while [[ $# -gt 0 && "$1" != "--" ]]; do wrap+=("$1"); shift; done
  [[ "${1:-}" == "--" ]] && shift
  LAUNCH_LOG_START=$( [[ -f "$LOG" ]] && wc -l <"$LOG" || echo 0 )
  "${wrap[@]}" "$APP" "$@" >"$OUT/$name.out" 2>&1 &
  APP_PID=$!
  # Signals go to VoLum itself, not to a bubblewrap parent.
  APP_SIG_PID=$APP_PID
  if [[ ${#wrap[@]} -gt 0 ]]; then
    for _ in $(seq 50); do
      APP_SIG_PID=$(pgrep -n -x VoLum -P "$APP_PID") && break
      sleep 0.1
    done
    [[ -n "$APP_SIG_PID" ]] || APP_SIG_PID=$APP_PID
  fi
}

run_log() { [[ -f "$LOG" ]] && tail -n +"$((LAUNCH_LOG_START + 1))" "$LOG"; }

# quit <close|term|kill>: returns once the app has exited, or kills it after 20 s.
quit() {
  local how="$1" t0
  t0=$(date +%s)
  case "$how" in
    close) "$XTOOL" close VoLum ;;
    term) kill -TERM "$APP_SIG_PID" ;;
    kill) kill -KILL "$APP_SIG_PID" ;;
  esac
  for _ in $(seq 200); do kill -0 "$APP_PID" 2>/dev/null || break; sleep 0.1; done
  if kill -0 "$APP_PID" 2>/dev/null; then
    echo "  app still running 20 s after $how; killed"
    kill -9 "$APP_PID" 2>/dev/null
    wait "$APP_PID" 2>/dev/null
    APP_RC="hung"
  else
    wait "$APP_PID"
    APP_RC=$?
  fi
  APP_PID=""
  echo "  quit by $how: exit $APP_RC after $(( $(date +%s) - t0 )) s; jackd xrun lines during the quit: $(( $(jack_xrun_lines) - PHASE_XRUNS ))"
  PHASE_XRUNS=$(jack_xrun_lines)
}

# measure <name> <seconds>: the probe's summary goes to $OUT/<name>.probe
jack_xrun_lines() { grep -aci "xrun" "$OUT/jackd.log"; }

# measure <name> <seconds>: the probe's summary goes to $OUT/<name>.probe. The
# jackd log's xrun lines are counted per phase: launch, measured window, quit.
measure() {
  local name="$1" secs="$2" before
  before=$(jack_xrun_lines)
  echo "  jackd xrun lines during the launch: $((before - PHASE_XRUNS))"
  "$PROBE" measure VoLum "$secs" "$STALL_MS" >"$OUT/$name.probe" 2>&1
  tail -1 "$OUT/$name.probe"
  PHASE_XRUNS=$(jack_xrun_lines)
  echo "jackd_xrun_lines=$((PHASE_XRUNS - before))" >>"$OUT/$name.probe"
}

field() { sed -n "s/.*\b$2=\([^ ]*\).*/\1/p" "$1" | tail -1; }

audio_checks() {
  local name="$1" secs="$2" probe="$OUT/$1.probe"
  local xr nf pk act
  xr=$(field "$probe" xruns); nf=$(field "$probe" nonfinite); pk=$(field "$probe" peak_dbfs); act=$(field "$probe" active_seconds)
  local jl
  jl=$(field "$probe" jackd_xrun_lines)
  check "[$name] zero JACK xruns over ${secs} s: xrun callback ${xr:-none}, jackd log ${jl:-none}" test "${xr:-x}" = 0 -a "${jl:-x}" = 0
  check "[$name] output finite (${nf:-?} non-finite samples)" test "${nf:-x}" = 0
  check "[$name] output audible: peak ${pk:-?} dBFS, ${act:-?}/${secs} s above -50 dBFS" \
    test "${act:-0}" -ge "$((secs > 5 ? secs - 5 : secs))"
}

quit_checks() {
  local name="$1"
  check "[$name] exit code 0 (got $APP_RC)" test "$APP_RC" = 0
  check "[$name] audio teardown complete" grep -aq "audio teardown complete" <(run_log)
  check "[$name] plug-in teardown complete" grep -aq "plugin teardown complete" <(run_log)
  check "[$name] volum-settings.json written" test -s "$DATA/volum-settings.json"
  check "[$name] settings.ini written" test -s "$DATA/settings.ini"
}

json_get() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get(sys.argv[2], ""))' "$DATA/volum-settings.json" "$1" 2>/dev/null; }

seed_amp() {
  python3 - "$DATA/volum-settings.json" "$1" <<'PY'
import json, sys
path, idx = sys.argv[1], int(sys.argv[2])
s = json.load(open(path))
s["volumUiMode"] = "build"
s["lastAmpIdx"] = idx
s["lastPlaySlot"] = -1
s["volumActivePresetId"] = ""
s["volumCustomMainId"] = ""
json.dump(s, open(path, "w"), indent=2)
PY
}

first_capture() { run_log | grep -a "MAIN read" | grep -ao 'VoLumRigs/[^/]*/[^/]*\.nam' | head -1; }

shot() { command -v import >/dev/null && import -window root "$OUT/$1.png" 2>/dev/null && echo "  screenshot $OUT/$1.png"; }

echo "== run 1: fresh library on JACK, ${SECONDS_MAIN} s of audio, close button"
launch run1 --
check "[run1] main window opened" "$XTOOL" wait VoLum 60
check "[run1] JACK ports VoLum:inport 0 / outport 0" "$PROBE" wait-ports VoLum 60
measure run1 "$SECONDS_MAIN"
shot run1
[[ "$QUIT_MODE" == kill ]] && quit kill || quit close
audio_checks run1 "$SECONDS_MAIN"
quit_checks run1
check "[run1] stream opened at 48000 Hz / 256" grep -aq "stream open: 48000 Hz, buffer 256" <(run_log)
check "[run1] settings.ini keeps JACK on system" grep -q "^driver=1" "$DATA/settings.ini"
check "[run1] settings.ini keeps 256 frames" grep -q "^buffer=256" "$DATA/settings.ini"
check "[run1] settings.ini in the XDG library" test -f "$XDG_DATA_HOME/VoLum/settings.ini"

for pass_name in run2:$AMP_A:term run3:$AMP_B:close; do
  IFS=: read -r name amp how <<<"$pass_name"
  echo "== $name: seeded lastAmpIdx=$amp, quit by $how"
  if [[ ! -s "$DATA/volum-settings.json" ]]; then fail "[$name] no volum-settings.json to seed"; continue; fi
  seed_amp "$amp"
  launch "$name" --
  check "[$name] main window opened" "$XTOOL" wait VoLum 60
  check "[$name] JACK ports" "$PROBE" wait-ports VoLum 60
  measure "$name" 10
  quit "$how"
  audio_checks "$name" 10
  quit_checks "$name"
  check "[$name] keeps lastAmpIdx=$amp" test "$(json_get lastAmpIdx)" = "$amp"
  capture=$(first_capture)
  echo "  first capture staged: ${capture:-none}"
  eval "CAPTURE_$name=\"\$capture\""
done
check "[amps] the two seeded amps staged different captures (${CAPTURE_run2:-none} vs ${CAPTURE_run3:-none})" \
  test -n "${CAPTURE_run2:-}" -a -n "${CAPTURE_run3:-}" -a "${CAPTURE_run2:-a}" != "${CAPTURE_run3:-a}"

echo "== run 4: Preferences, quit by SIGTERM while it is open"
launch run4 -- --preferences
check "[run4] main window opened" "$XTOOL" wait VoLum 60
check "[run4] Preferences opened" "$XTOOL" wait Preferences 20
sleep 2
shot run4-preferences
check "[run4] Preferences lists ALSA, JACK, PulseAudio" grep -aq "Preferences drivers: ALSA, JACK, PulseAudio" <(run_log)
quit term
quit_checks run4

"$APP" --list-audio-devices >"$OUT/devices.txt" 2>"$OUT/devices.err"
echo "-- --list-audio-devices"; cat "$OUT/devices.txt"
check "[devices] JACK lists the system device" grep -Pzoq 'JACK\n  system: in 2, out 2' "$OUT/devices.txt"

echo "== run 5: libjack hidden"
JACK_LIB="$( (ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null) | sed -n 's/.*libjack\.so\.0 (libc6,x86-64) => //p' | head -1)"
[[ -z "$JACK_LIB" && -e /usr/lib/x86_64-linux-gnu/libjack.so.0 ]] && JACK_LIB=/usr/lib/x86_64-linux-gnu/libjack.so.0
JACK_LIB="${JACK_LIB:+$(readlink -f "$JACK_LIB")}"
HIDE=()
if [[ -n "$JACK_LIB" ]] && command -v bwrap >/dev/null && bwrap --dev-bind / / --ro-bind /dev/null "$JACK_LIB" true 2>"$OUT/bwrap.err"; then
  HIDE=(bwrap --dev-bind / / --ro-bind /dev/null "$JACK_LIB")
fi
if [[ ${#HIDE[@]} -eq 0 ]]; then
  echo "  bubblewrap cannot hide ${JACK_LIB:-libjack}: $(cat "$OUT/bwrap.err" 2>/dev/null)"
  if [[ $REQUIRE_HIDE -eq 1 ]]; then fail "[nojack] libjack could be hidden"; else echo "  SKIP  libjack hidden"; fi
else
  echo "  hiding $JACK_LIB"
  "${HIDE[@]}" "$APP" --list-audio-devices >"$OUT/devices-nojack.txt" 2>"$OUT/devices-nojack.err"
  echo "-- --list-audio-devices without libjack"; cat "$OUT/devices-nojack.txt"
  check "[nojack] JACK reported as library not found" grep -Pzoq 'JACK\n  \(library not found\)' "$OUT/devices-nojack.txt"
  check "[nojack] no JACK device listed" bash -c "! grep -Pzoq 'JACK\n  system' '$OUT/devices-nojack.txt'"
  check "[nojack] ALSA still listed" grep -q '^ALSA$' "$OUT/devices-nojack.txt"
  check "[nojack] PulseAudio still listed" grep -Pzoq 'PulseAudio\n  PulseAudio' "$OUT/devices-nojack.txt"

  launch run5 "${HIDE[@]}" -- --preferences
  check "[run5] main window opened without libjack" "$XTOOL" wait VoLum 60
  check "[run5] Preferences opened" "$XTOOL" wait Preferences 20
  sleep 2
  shot run5-preferences-nojack
  check "[run5] startup fell back from JACK to ALSA" grep -aq "the library of driver 1 is missing; using ALSA" <(run_log)
  check "[run5] Preferences lists ALSA, PulseAudio only" grep -aq "Preferences drivers: ALSA, PulseAudio$" <(run_log)
  quit term
  check "[run5] exit code 0 (got $APP_RC)" test "$APP_RC" = 0
  check "[run5] plug-in teardown complete" grep -aq "plugin teardown complete" <(run_log)
fi

echo "== jackd"
echo "  xrun lines over the whole session, launches and quits included: $(jack_xrun_lines)"
grep -ai "xrun\|real-time" "$OUT/jackd.log" | sort | uniq -c
kill "$JACK_PID" 2>/dev/null; wait "$JACK_PID" 2>/dev/null; JACK_PID=""

echo
echo "smoke-standalone-audio-linux: $((CHECKS - FAILS))/$CHECKS checks passed; output in $OUT"
[[ $FAILS -eq 0 ]] && echo "RESULT: PASS" || echo "RESULT: FAIL"
[[ $FAILS -eq 0 ]]
