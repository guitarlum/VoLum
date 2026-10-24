#!/usr/bin/env bash
# Real-device gate for the Linux standalone on RtAudio PulseAudio.
#
#   bash NeuralAmpModeler/scripts/linux/real-device-gate.sh [--seconds N]
#        [--app PATH] [--out DIR] [--calibrate] [--mute] [--drop]
#        [--private] [--self-test]
#
# Default: WSLg's PulseServer (unix:/mnt/wslg/PulseServer). The app plays a
# 440 Hz sine at -24 dBFS (VOLUM_LINUX_GATE_TONE) into the default sink and
# does not run the amp, so a hardware loopback cannot feed back. parecord
# records the default source. The recording is aligned with the sine and
# nulled. NULL_PEAK_DB below is fixed from the calibration run; it is not
# fitted per run. --calibrate prints the numbers and does not enforce it.
#
# --mute and --drop are the red proofs (silence, and one zeroed buffer).
# --private starts a private pulseaudio with a null sink. It does not
# reconfigure WSLg. --self-test checks the aligner on synthetic wavs and
# does not open a device.
#
# Holds $VOLUM_LINUX_ROOT/locks/gui.lock for anything that plays or opens
# a window. Does not change the default sink, source, or any volume.
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
APP="$ROOT/build-linux/standalone/VoLum"
OUT=""
SECONDS_MAIN=300
CALIBRATE=0
MUTE=0
DROP=0
PRIVATE=0
SELF_TEST=0

# Fixed after calibration. -999 fails a normal run closed, so a number is
# never invented on the fly. Dropout samples use a fixed ratio in the aligner.
NULL_PEAK_DB=-999
DROPOUT_LIMIT=32

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app) APP="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --seconds) SECONDS_MAIN="$2"; shift 2 ;;
    --calibrate) CALIBRATE=1; shift ;;
    --mute) MUTE=1; shift ;;
    --drop) DROP=1; shift ;;
    --private) PRIVATE=1; shift ;;
    --self-test) SELF_TEST=1; shift ;;
    *) echo "usage: $0 [--app PATH] [--out DIR] [--seconds N] [--calibrate] [--mute] [--drop] [--private] [--self-test]" >&2; exit 2 ;;
  esac
done

LINUX_ROOT="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}"
OUT="${OUT:-$(mktemp -d /tmp/volum-real-device.XXXXXX)}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

cc -O2 -o "$OUT/volum-pulse-null" "$SCRIPT_DIR/smoke/volum-pulse-null.c" -lm || {
  echo "REASON: aligner failed to compile"
  exit 2
}

if [[ $SELF_TEST -eq 1 ]]; then
  "$OUT/volum-pulse-null" self-test
  exit $?
fi

[[ -x "$APP" ]] || { echo "REASON: no app at $APP" >&2; exit 2; }
APP="$(cd "$(dirname "$APP")" && pwd)/$(basename "$APP")"
for tool in Xvfb cc parecord pactl; do
  command -v "$tool" >/dev/null || { echo "REASON: $tool is missing (provision-linux.sh installs it)" >&2; exit 2; }
done

exec 8>>"$LINUX_ROOT/locks/gui.lock"
if ! flock -n 8; then
  echo "REASON: gui.lock is held"
  exit 1
fi

FAILS=0
reason() { echo "REASON: $*"; FAILS=1; }

XVFB_PID=""
APP_PID=""
REC_PID=""
PULSE_PID=""
cleanup() {
  if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
    kill -TERM "$APP_PID" 2>/dev/null || true
    sleep 0.5
    kill -KILL "$APP_PID" 2>/dev/null || true
  fi
  if [[ -n "$REC_PID" ]] && kill -0 "$REC_PID" 2>/dev/null; then
    kill -INT "$REC_PID" 2>/dev/null || true
  fi
  if [[ -n "$PULSE_PID" ]] && kill -0 "$PULSE_PID" 2>/dev/null; then
    kill "$PULSE_PID" 2>/dev/null || true
  fi
  if [[ -n "$XVFB_PID" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
    kill "$XVFB_PID" 2>/dev/null || true
  fi
  wait 2>/dev/null || true
}
trap cleanup EXIT

cc -O2 -o "$OUT/volum-x-tool" "$SCRIPT_DIR/smoke/volum-x-tool.c" -lX11 || {
  echo "REASON: x tool failed to compile"
  exit 2
}
XTOOL="$OUT/volum-x-tool"

echo "== Xvfb"
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
  kill "$XVFB_PID" 2>/dev/null || true
  wait "$XVFB_PID" 2>/dev/null || true
  XVFB_PID=""
done
[[ -n "$XVFB_PID" ]] || { echo "REASON: Xvfb did not start"; exit 2; }
echo "DISPLAY=$DISPLAY"

SINK=""
SOURCE=""
REC_DEVICE=""
if [[ $PRIVATE -eq 1 ]]; then
  command -v pulseaudio >/dev/null || { echo "REASON: pulseaudio daemon is not installed"; exit 2; }
  echo "== private pulse null sink (WSLg is not touched)"
  pulseaudio -n --daemonize=no --exit-idle-time=-1 --disable-shm=yes \
    -L "module-native-protocol-unix auth-anonymous=1 socket=$OUT/pulse.sock" \
    -L "module-null-sink sink_name=gate_out rate=48000 channels=2" \
    >"$OUT/pulse.log" 2>&1 &
  PULSE_PID=$!
  export PULSE_SERVER="unix:$OUT/pulse.sock"
  for _ in $(seq 50); do
    pactl info >/dev/null 2>&1 && break
    sleep 0.1
  done
  pactl info >/dev/null 2>&1 || { echo "REASON: private pulse did not start"; cat "$OUT/pulse.log" >&2; exit 2; }
  SINK="gate_out"
  SOURCE="gate_out.monitor"
  REC_DEVICE="gate_out.monitor"
else
  [[ -S /mnt/wslg/PulseServer ]] || { echo "REASON: WSLg Pulse socket is missing"; exit 2; }
  export PULSE_SERVER="unix:/mnt/wslg/PulseServer"
  SINK="$(pactl info | sed -n 's/^Default Sink: //p')"
  SOURCE="$(pactl info | sed -n 's/^Default Source: //p')"
  REC_DEVICE="$SOURCE"
fi
echo "PULSE_SERVER=$PULSE_SERVER"
echo "sink=$SINK source=$SOURCE record=$REC_DEVICE"
[[ -n "$SINK" && -n "$SOURCE" ]] || { echo "REASON: could not read the Pulse default devices"; exit 2; }

# A Pulse failure must not open a real ALSA device.
cat >"$OUT/asound.conf" <<'ALSA'
pcm.!default { type null }
ALSA
export ALSA_CONFIG_PATH="$OUT/asound.conf"

export XDG_DATA_HOME="$OUT/xdg"
DATA="$XDG_DATA_HOME/VoLum"
mkdir -p "$DATA"
LOG="$DATA/volum.log"
cat >"$DATA/settings.ini" <<'INI'
[audio]
driver=2
indev=PulseAudio
outdev=PulseAudio
in1=1
out1=1
out2=2
buffer=256
sr=48000
INI

TONE="sine"
[[ $MUTE -eq 1 ]] && TONE="mute"
[[ $DROP -eq 1 ]] && TONE="drop"
export VOLUM_LINUX_GATE_TONE="$TONE"
export VOLUM_LINUX_GATE_DROP_AT=400

echo "== devices"
PULSE_SINK="$SINK" PULSE_SOURCE="${SINK}.monitor" "$APP" --list-audio-devices >"$OUT/devices.txt" 2>"$OUT/devices.err" || true
if ! grep -a -q "PulseAudio" "$OUT/devices.txt"; then
  echo "REASON: --list-audio-devices did not list PulseAudio"
  cat "$OUT/devices.txt" "$OUT/devices.err" >&2
  exit 1
fi

echo "== record $REC_DEVICE and launch (${SECONDS_MAIN}s, tone=$TONE)"
parecord --device="$REC_DEVICE" --file-format=wav "$OUT/rec.wav" >"$OUT/parecord.log" 2>&1 &
REC_PID=$!
sleep 0.3

PULSE_SINK="$SINK" PULSE_SOURCE="${SINK}.monitor" "$APP" --preferences >"$OUT/app.out" 2>"$OUT/app.err" &
APP_PID=$!

opened=0
for _ in $(seq 90); do
  if [[ -f "$LOG" ]] && grep -a -q "stream open:" "$LOG"; then opened=1; break; fi
  kill -0 "$APP_PID" 2>/dev/null || break
  sleep 0.2
done
if [[ $opened -ne 1 ]]; then
  echo "REASON: stream did not open"
  cat "$OUT/app.err" >&2 || true
  exit 1
fi
echo "stream: $(grep -a "stream open:" "$LOG" | head -1)"
if grep -a -q "using ALSA" "$LOG"; then
  echo "REASON: Pulse was not opened; the app fell back to ALSA"
  exit 1
fi
if ! grep -a -q "gate-tone mode=$TONE" "$OUT/app.err"; then
  echo "REASON: gate tone mode $TONE was not logged"
  exit 1
fi
if ! "$XTOOL" wait VoLum 30; then
  echo "REASON: main window did not open"
  exit 1
fi
if ! "$XTOOL" wait Preferences 20; then
  echo "REASON: Preferences did not open"
  exit 1
fi

alive=1
for _ in $(seq "$SECONDS_MAIN"); do
  if ! kill -0 "$APP_PID" 2>/dev/null; then alive=0; break; fi
  sleep 1
done
if [[ $alive -ne 1 ]]; then
  echo "REASON: app exited during the run"
  wait "$APP_PID" 2>/dev/null || true
  echo "app_exit=$?"
  exit 1
fi

# Xruns from open through the measured window, before quit. stream_time under
# 0.5 s is the device open and is reported, not failed.
xrun_mid() {
  awk -F'stream_time=' '/gate-xrun/ { split($2, a, " "); t = a[1] + 0; if (t >= 0.5) c++ } END { print c + 0 }' "$1" 2>/dev/null || echo 0
}
MID_XRUNS="$(xrun_mid "$OUT/app.err")"
echo "xruns_after_open_before_quit=$MID_XRUNS"
grep -a "gate-xrun" "$OUT/app.err" || true

echo "== SIGTERM"
kill -TERM "$APP_PID" 2>/dev/null || true
app_rc="hung"
for _ in $(seq 200); do
  if ! kill -0 "$APP_PID" 2>/dev/null; then
    wait "$APP_PID"
    app_rc=$?
    break
  fi
  sleep 0.1
done
if kill -0 "$APP_PID" 2>/dev/null; then
  echo "REASON: app still running 20 s after SIGTERM"
  kill -KILL "$APP_PID" 2>/dev/null || true
  wait "$APP_PID" 2>/dev/null || true
  APP_PID=""
  exit 1
fi
APP_PID=""
echo "app_exit=$app_rc"
QUIT_XRUNS="$(xrun_mid "$OUT/app.err")"
echo "xruns_including_quit_window=$QUIT_XRUNS"

sleep 0.4
if [[ -n "$REC_PID" ]]; then
  kill -INT "$REC_PID" 2>/dev/null || true
  wait "$REC_PID" 2>/dev/null || true
  REC_PID=""
fi

if [[ "$app_rc" != 0 ]]; then
  reason "app exit $app_rc"
fi
if ! grep -a -q "audio teardown complete" "$LOG"; then
  reason "audio teardown was not logged"
fi
if ! grep -a -q "plugin teardown complete" "$LOG"; then
  reason "plugin teardown was not logged"
fi
if ! grep -a -q "Preferences drivers:" "$LOG"; then
  reason "Preferences drivers were not logged"
fi
if [[ $DROP -eq 1 ]] && ! grep -a -q "gate-drop " "$OUT/app.err"; then
  reason "dropped buffer was not injected"
fi

TOL="$NULL_PEAK_DB"
DLIM="$DROPOUT_LIMIT"
if [[ $CALIBRATE -eq 1 ]]; then
  TOL=0
  DLIM=1000000000
fi
# A drop is judged by the dropout counter. Keep the null check from hiding it
# when the tolerance is not calibrated yet.
if [[ $DROP -eq 1 && "$NULL_PEAK_DB" == -999 ]]; then
  TOL=0
fi

echo "== align"
"$OUT/volum-pulse-null" analyze "$OUT/rec.wav" --tol-peak-db "$TOL" --dropout-limit "$DLIM" >"$OUT/null.txt"
null_rc=$?
cat "$OUT/null.txt"
if [[ $null_rc -ne 0 ]]; then
  FAILS=1
fi

if [[ $CALIBRATE -eq 0 && $MUTE -eq 0 && $DROP -eq 0 && "$NULL_PEAK_DB" == -999 && $null_rc -eq 0 ]]; then
  reason "null tolerance is not calibrated"
fi
if [[ $CALIBRATE -eq 0 && $MUTE -eq 0 && $DROP -eq 0 && "$MID_XRUNS" != 0 ]]; then
  reason "$MID_XRUNS xrun(s) between 0.5 s after stream open and quit"
fi

# Playback must stay under the -18 dBFS cap. Mute is silence, so skip it.
if [[ $MUTE -eq 0 ]]; then
  peak="$(sed -n 's/^rec_peak_dbfs=//p' "$OUT/null.txt" | head -1)"
  awk -v p="${peak:--999}" 'BEGIN { if (p+0 > -18) exit 1 }' || reason "recording peak ${peak} dBFS is louder than -18"
fi

echo "out=$OUT"
if [[ $FAILS -ne 0 ]]; then
  grep -a '^REASON:' "$OUT/null.txt" || true
  exit 1
fi
echo "PASS"
exit 0
