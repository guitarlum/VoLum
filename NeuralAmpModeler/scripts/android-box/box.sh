#!/usr/bin/env bash
# Box side of the Android shootout pipeline. It runs from the box worktree of the
# commit under test, so it always builds the project it belongs to.
#
#   box.sh <build|unit|test|capture|motion|pack> <flavor> <x> <sha>
#
# The box shares its RAM with Ollama: at most two Gradle builds (locks/build1|2.lock)
# and one emulator user (locks/emu.lock). Never touches ~/localai-platform.
set -euo pipefail

STEP="${1:?step}"
FLAVOR="${2:?flavor}"
X="${3:?candidate}"
SHA="${4:?sha}"

ROOT="$HOME/volum-android"
# shellcheck disable=SC1091
source "$ROOT/env.sh"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
PROJECT="$REPO/NeuralAmpModeler/projects/android"
OUT="$ROOT/out/$X/$SHA"
RES="$OUT/results"
LOGS="$OUT/logs"
mkdir -p "$RES" "$LOGS"

SERIAL="emulator-5554"
# Every adb call closes the lock fds (an adb server it spawns daemonizes and would keep
# the lock) and reads no stdin (adb shell would swallow the registry lists the loops read).
# A dead or wedged emulator makes adb block forever while this script holds emu.lock, so
# every call has a deadline; timeout kills the call's whole process group.
adbt() {
  local limit="$1" rc=0
  shift
  timeout -k 5 "$limit" adb -s "$SERIAL" "$@" < /dev/null 8>&- 9>&- || rc=$?
  if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then log "adb ${*:1:4} gave no answer within ${limit}s" >&2; fi
  return "$rc"
}
adbx() { adbt 60 "$@"; }
AVD="volum_phone"
PKG="com.volum.android.$FLAVOR"
TEST_PKG="$PKG.test"
CAPTURE_ACTIVITY="com.volum.harness.CaptureActivity"
FFMPEG="$HOME/hubris/bin/ffmpeg"
SCOPE="candidate"
[ "$FLAVOR" = "design" ] && SCOPE="design"
FLAVOR_CAP="$(tr '[:lower:]' '[:upper:]' <<< "${FLAVOR:0:1}")${FLAVOR:1}"

log() { printf '[box %s %s/%s] %s\n' "$(date +%H:%M:%S)" "$STEP" "$X" "$*"; }

die() { log "$*" >&2; exit 3; }

json_write() { python3 - "$@"; }

take_build_lock() {
  exec 8>"$ROOT/locks/build1.lock"
  if flock -n 8; then return; fi
  exec 8>"$ROOT/locks/build2.lock"
  if flock -n 8; then return; fi
  log "both build slots busy, waiting for build1"
  exec 8>"$ROOT/locks/build1.lock"
  flock -w 7200 8
}

# The owner line is only advisory (box.sh copies on older branches truncate the file when
# they open it), so the holders are read from /proc: a lock whose owner is gone is held by
# a child that owner leaked.
lock_holder() {
  local lock="$1" owner pid fd holders=""
  owner="$(head -n 1 "$lock" 2>/dev/null || true)"
  for fd in $(find /proc/[0-9]*/fd -lname "$lock" 2>/dev/null); do
    pid="${fd#/proc/}"
    pid="${pid%%/*}"
    grep -q '^lock:' "/proc/$pid/fdinfo/${fd##*/}" 2>/dev/null || continue
    holders="$holders $pid ($(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null | cut -c1-60))"
  done
  pid="${owner%% *}"
  if [ -z "$owner" ]; then
    printf 'owner unrecorded; held by:%s' "${holders:- unknown}"
  elif kill -0 "$pid" 2>/dev/null; then
    printf 'owner pid %s' "$owner"
  else
    printf 'STALE: owner pid %s is gone; held by:%s' "$owner" "${holders:- unknown}"
  fi
}

# Same-session only: the emulator runs in its own session (setsid) and must outlive the step.
stop_children() {
  local pid
  for pid in $(pgrep -P "$1" || true); do
    [ "$(ps -o sid= -p "$pid" 2>/dev/null | tr -d ' ')" = "$MY_SID" ] || continue
    stop_children "$pid"
    kill "$pid" 2>/dev/null || true
  done
}

RESET_FONT_SCALE=false
# Background children (screenrecord, the watchdog) and pipeline subshells hold a copy of the
# lock fd, so the lock is only free once they are gone too.
on_exit() {
  if $RESET_FONT_SCALE; then adbt 10 shell settings put system font_scale 1.0 >/dev/null 2>&1 || true; fi
  stop_children "$$"
}

take_emu_lock() {
  local budget="${BOX_STEP_BUDGET:-$1}" lock="$ROOT/locks/emu.lock"
  MY_SID="$(ps -o sid= -p $$ | tr -d ' ')"
  trap on_exit EXIT
  # The adb server daemonizes and would inherit (and keep) the lock fd.
  timeout 30 adb start-server >/dev/null 2>&1 8>&- 9>&- || true
  exec 9<>"$lock"
  if ! flock -n 9; then
    log "waiting for the emulator lock ($(lock_holder "$lock"))"
    flock -w 7200 9 || die "emulator lock still busy after 7200s ($(lock_holder "$lock"))"
  fi
  printf '%s %s/%s %s\n' "$$" "$STEP" "$X" "$SHA" > "$lock"
  # A hang nothing else bounds still ends the step, and on_exit releases the lock.
  ( sleep "$budget" && { log "step still running after its ${budget}s budget; stopping it" >&2; kill -TERM "$$"; } ) 8>&- 9>&- &
}

require_device() {
  local state
  state="$(adbt 10 get-state 2>/dev/null | tr -d '\r')" || true
  [ "$state" = "device" ] || die "emulator $SERIAL is gone (adb get-state: ${state:-no answer}); see $ROOT/logs/emulator.log"
}

boot_completed() {
  [ "$(adbt 10 shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]
}

ensure_emulator() {
  if ! { adbt 10 get-state 2>/dev/null | grep -q device && boot_completed; }; then boot_emulator; fi
  # Without this, Android's "Viewing full screen" confirmation covers the immersive app in every capture.
  adbx shell settings put secure immersive_mode_confirmations confirmed >/dev/null
}

boot_emulator() {
  log "booting $AVD"
  setsid nohup emulator -avd "$AVD" -no-window -no-audio -no-boot-anim -no-snapshot -gpu swiftshader_indirect -accel on -port 5554 \
    > "$ROOT/logs/emulator.log" 2>&1 < /dev/null 8>&- 9>&- &
  adbt 180 wait-for-device || die "emulator not on adb within 180s; see $ROOT/logs/emulator.log"
  local t=0 start=$SECONDS
  until boot_completed; do
    sleep 3; t=$((SECONDS - start))
    [ "$t" -lt 300 ] || die "emulator boot not complete within 300s"
  done
  adbx shell svc power stayon true >/dev/null 2>&1 || true
  adbx shell input keyevent 82 >/dev/null 2>&1 || true
  adbx shell settings put system font_scale 1.0 >/dev/null
  log "emulator up after ${t}s"
}

install_apks() {
  adbt 300 install -r -t "$OUT/apk/app.apk" > "$LOGS/install-app.txt" 2>&1 || die "app install failed; see $LOGS/install-app.txt"
  if [ "$1" = "with-tests" ]; then
    adbt 300 install -r -t "$OUT/apk/test.apk" > "$LOGS/install-test.txt" 2>&1 || die "test install failed; see $LOGS/install-test.txt"
  fi
}

scan_crashes() {
  local tag="$1"
  { adbt 60 logcat -d -b crash -v time; adbt 60 logcat -d -b main,system -v time; } 2>/dev/null \
    | grep -E "Process: com\.volum\.android|ANR in com\.volum\.android" \
    | sed "s/^/[$tag] /" >> "$RES/crash.txt" || true
}

step_build() {
  take_build_lock
  log "gradle assemble $FLAVOR_CAP (debug + androidTest)"
  rm -rf "$OUT/apk" "$OUT/registry"
  mkdir -p "$OUT/apk"
  local ok=true
  # The Gradle daemon outlives this script; it must not inherit the lock fd.
  (cd "$PROJECT" && ./gradlew --console=plain \
      ":app:assemble${FLAVOR_CAP}Debug" ":app:assemble${FLAVOR_CAP}DebugAndroidTest" ":core:dumpRegistry") \
    > "$LOGS/build.txt" 2>&1 8>&- || ok=false
  if $ok; then
    cp "$PROJECT/app/build/outputs/apk/$FLAVOR/debug/app-$FLAVOR-debug.apk" "$OUT/apk/app.apk"
    cp "$PROJECT/app/build/outputs/apk/androidTest/$FLAVOR/debug/app-$FLAVOR-debug-androidTest.apk" "$OUT/apk/test.apk"
    cp -r "$PROJECT/core/build/registry" "$OUT/registry"
  fi
  json_write "$RES/build.json" "$ok" "$LOGS/build.txt" <<'PY'
import json, sys
out, ok, logf = sys.argv[1], sys.argv[2] == "true", sys.argv[3]
tail = open(logf, errors="replace").read().splitlines()[-40:]
json.dump({"pass": ok, "log_tail": [] if ok else tail}, open(out, "w"), indent=2)
PY
  $ok
}

step_unit() {
  take_build_lock
  log "gradle :core:test"
  local ok=true
  (cd "$PROJECT" && ./gradlew --console=plain ":core:test") > "$LOGS/unit.txt" 2>&1 8>&- || ok=false
  rm -rf "$OUT/unit-xml" && mkdir -p "$OUT/unit-xml"
  cp "$PROJECT"/core/build/test-results/test/*.xml "$OUT/unit-xml/" 2>/dev/null || true
  json_write "$RES/unit.json" "$ok" "$OUT/unit-xml" <<'PY'
import json, sys, glob, xml.etree.ElementTree as ET
out, ok, d = sys.argv[1], sys.argv[2] == "true", sys.argv[3]
tests = failures = 0
failed = []
for f in glob.glob(d + "/*.xml"):
    root = ET.parse(f).getroot()
    tests += int(root.get("tests", 0))
    failures += int(root.get("failures", 0)) + int(root.get("errors", 0))
    for case in root.iter("testcase"):
        if case.find("failure") is not None or case.find("error") is not None:
            failed.append(case.get("classname", "") + "." + case.get("name", ""))
json.dump({"pass": ok and tests > 0 and failures == 0, "tests": tests, "failures": failures, "failed": failed[:200]}, open(out, "w"), indent=2)
PY
  [ "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["pass"])' "$RES/unit.json")" = "True" ]
}

instrument() {
  local limit="$1" cls="$2" rc=0
  adbt "$limit" shell am instrument -w -r -e class "com.volum.harness.tests.$cls" \
    "$TEST_PKG/androidx.test.runner.AndroidJUnitRunner" > "$LOGS/instrument-$cls.txt" 2>&1 || rc=$?
  if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then log "instrumented $cls still running after ${limit}s; stopped it"; fi
  require_device
}

run_instrumented() {
  local cls="$1" report="$2"
  log "instrumented $cls"
  adbx shell run-as "$PKG" rm -f "files/harness/$report" >/dev/null 2>&1 || true
  instrument 1800 "$cls"
  if ! adbx shell run-as "$PKG" cat "files/harness/$report" > "$RES/$report" 2>/dev/null || [ ! -s "$RES/$report" ]; then
    python3 - "$RES/$report" "$LOGS/instrument-$cls.txt" <<'PY'
import json, sys
tail = open(sys.argv[2], errors="replace").read().splitlines()[-30:]
json.dump({"pass": False, "error": "no report written (test crashed or did not run)", "instrument_tail": tail}, open(sys.argv[1], "w"), indent=2)
PY
  fi
}

step_test() {
  rm -f "$RES/harness.json" "$RES/coverage.json" "$RES/reachability.json" "$RES/touch.json"
  take_emu_lock 9000
  ensure_emulator
  install_apks with-tests
  adbx logcat -c || true
  rm -f "$RES/crash.txt"
  log "instrumented HarnessSelfTest"
  instrument 900 HarnessSelfTest
  python3 - "$RES/harness.json" "$LOGS/instrument-HarnessSelfTest.txt" <<'PY'
import json, sys
text = open(sys.argv[2], errors="replace").read()
ok = "\nOK (" in text or text.startswith("OK (")
json.dump({"pass": ok, "log_tail": [] if ok else text.splitlines()[-40:]}, open(sys.argv[1], "w"), indent=2)
PY
  run_instrumented CoverageTest coverage.json
  run_instrumented ReachabilityTest reachability.json
  run_instrumented TouchTargetTest touch.json
  scan_crashes test
}

# The limit counts polls, not adb time; the wall-clock cap covers polls that hang.
wait_log() {
  local pattern="$1" limit="$2" t=0 deadline=$((SECONDS + 2 * $2 + 10))
  while ! adbt 10 logcat -d -s VoLumCapture:I 2>/dev/null | grep -q -- "$pattern"; do
    sleep 0.25; t=$((t + 1))
    if [ "$t" -ge $((limit * 4)) ] || [ "$SECONDS" -ge "$deadline" ]; then
      require_device
      return 1
    fi
  done
}

step_capture() {
  rm -f "$RES/captures.json"
  take_emu_lock 5400
  ensure_emulator
  install_apks no-tests
  rm -rf "$OUT/captures" && mkdir -p "$OUT/captures"
  local list="$OUT/registry/captures-$SCOPE.tsv"
  local seq=0 bad=() count=0 expected=0
  RESET_FONT_SCALE=true
  for scale in 1.0 1.3; do
    adbx shell settings put system font_scale "$scale" >/dev/null
    adbx logcat -c || true
    local first=true
    while IFS=$'\t' read -r screen fixture; do
      [ -n "$screen" ] || continue
      expected=$((expected + 1))
      seq=$((seq + 1))
      local stop=()
      if $first; then stop=(-S); first=false; fi
      adbx shell am start "${stop[@]}" -W -n "$PKG/$CAPTURE_ACTIVITY" --es screen "$screen" --es fixture "$fixture" --ei seq "$seq" >/dev/null 2>&1 || true
      local name="${screen}__${fixture}__fs${scale}.png"
      if ! wait_log "ready seq=$seq " 20; then
        bad+=("$name: not ready within 20 s")
        continue
      fi
      local focus focus_ok=true
      focus="$(adbx shell dumpsys window 2>/dev/null | grep -m1 mCurrentFocus || true)"
      if [[ "$focus" != *"$PKG/"* ]]; then
        bad+=("$name: another window has focus (${focus//[$'\r\n']/})")
        focus_ok=false
      fi
      adbx exec-out screencap -p > "$OUT/captures/$name"
      local dims
      dims="$(python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(24); print("%dx%d" % struct.unpack(">II", d[16:24]) if d[:8]==b"\x89PNG\r\n\x1a\n" else "not-png")' "$OUT/captures/$name")"
      if [ "$dims" != "2400x1080" ]; then
        bad+=("$name: $dims, expected 2400x1080")
      elif $focus_ok; then
        count=$((count + 1))
      fi
    done < "$list"
  done
  scan_crashes capture
  printf '%s\n' "${bad[@]:-}" > "$RES/captures-bad.txt"
  python3 - "$RES/captures.json" "$count" "$expected" "$RES/captures-bad.txt" <<'PY'
import json, sys
bad = [l for l in open(sys.argv[4]).read().splitlines() if l.strip()]
count, expected = int(sys.argv[2]), int(sys.argv[3])
json.dump({"pass": not bad and count == expected and expected > 0, "count": count, "expected": expected, "bad": bad}, open(sys.argv[1], "w"), indent=2)
PY
}

step_motion() {
  rm -f "$RES/motion.json"
  take_emu_lock 3600
  ensure_emulator
  install_apks no-tests
  adbx shell settings put system font_scale 1.0 >/dev/null
  rm -rf "$OUT/motion" && mkdir -p "$OUT/motion"
  local list="$OUT/registry/motion-$SCOPE.tsv"
  local seq=1000 bad=() sheets=() expected=0
  while IFS=$'\t' read -r id duration_ms; do
    [ -n "$id" ] || continue
    expected=$((expected + 1))
    seq=$((seq + 1))
    adbx logcat -c || true
    adbx shell rm -f /sdcard/volum-motion.mp4 >/dev/null 2>&1 || true
    local limit=$(( (duration_ms + 999) / 1000 + 6 ))
    local t0
    t0="$(date +%s.%N)"
    adbt $((limit + 30)) shell screenrecord --time-limit "$limit" --size 1200x540 --bit-rate 6000000 /sdcard/volum-motion.mp4 > "$LOGS/screenrecord-$id.txt" 2>&1 &
    local rec=$!
    sleep 0.5
    adbx shell am start -S -W -n "$PKG/$CAPTURE_ACTIVITY" --es motion "$id" --ei seq "$seq" >/dev/null 2>&1 || true
    if ! wait_log "motion-start seq=$seq " 20; then
      bad+=("$id: motion did not start")
      wait "$rec" || true
      continue
    fi
    local t1
    t1="$(date +%s.%N)"
    wait_log "done seq=$seq " $((limit + 5)) || bad+=("$id: motion did not finish")
    wait "$rec" || true
    adbt 120 pull /sdcard/volum-motion.mp4 "$OUT/motion/$id.mp4" > /dev/null 2>&1 || { bad+=("$id: no recording"); continue; }
    local skip dur fps
    skip="$(python3 -c 'import sys; print(max(0.0, float(sys.argv[2]) - float(sys.argv[1]) - 0.2))' "$t0" "$t1")"
    dur="$(python3 -c 'import sys; print(int(sys.argv[1]) / 1000.0)' "$duration_ms")"
    fps="$(python3 -c 'import sys; print(16.0 / float(sys.argv[1]))' "$dur")"
    # screenrecord only writes a frame when the screen changes: resample to a constant
    # rate (filling the gaps) and clone the last frame before cutting the script window.
    if "$FFMPEG" -nostdin -loglevel error -y -i "$OUT/motion/$id.mp4" \
        -vf "tpad=stop_mode=clone:stop_duration=$limit,fps=30,trim=start=$skip:duration=$dur,setpts=PTS-STARTPTS,fps=$fps,scale=480:-1,tile=4x4:padding=4:margin=4:color=0x202020" \
        -frames:v 1 "$OUT/motion/$id.png" \
        > "$LOGS/ffmpeg-$id.txt" 2>&1 && [ -s "$OUT/motion/$id.png" ]; then
      sheets+=("$id")
    else
      bad+=("$id: contact sheet failed")
    fi
  done < "$list"
  scan_crashes motion
  printf '%s\n' "${bad[@]:-}" > "$RES/motion-bad.txt"
  python3 - "$RES/motion.json" "$expected" "$RES/motion-bad.txt" "${sheets[@]:-}" <<'PY'
import json, sys
bad = [l for l in open(sys.argv[3]).read().splitlines() if l.strip()]
expected = int(sys.argv[2])
sheets = [s for s in sys.argv[4:] if s]
json.dump({"pass": not bad and len(sheets) == expected and expected > 0, "count": len(sheets), "expected": expected,
           "sheets": ["motion/%s.png" % s for s in sheets], "bad": bad}, open(sys.argv[1], "w"), indent=2)
PY
}

step_pack() {
  python3 - "$RES/crash.txt" "$RES/crash_anr.json" <<'PY'
import json, os, sys
lines = open(sys.argv[1]).read().splitlines() if os.path.exists(sys.argv[1]) else []
events = [l for l in lines if l.strip()]
json.dump({"pass": not events, "events": events[:100]}, open(sys.argv[2], "w"), indent=2)
PY
  rm -f "$OUT.tgz"
  tar -czf "$OUT.tgz" -C "$ROOT/out/$X" "$SHA"
  log "packed $(du -h "$OUT.tgz" | cut -f1)"
}

case "$STEP" in
  build) step_build ;;
  unit) step_unit ;;
  test) step_test ;;
  capture) step_capture ;;
  motion) step_motion ;;
  pack) step_pack ;;
  *) echo "unknown step $STEP" >&2; exit 2 ;;
esac
