#!/usr/bin/env bash
# Box side of the Android shootout pipeline. Runs from the box worktree of the
# commit under test (android-box.ps1 Sync checks it out), so the steps always
# match the project they build.
#
#   box.sh build   <flavor> <x> <sha>   assemble app + test APKs, dump registries   (build lock)
#   box.sh unit    <flavor> <x> <sha>   :core unit tests                             (build lock)
#   box.sh test    <flavor> <x> <sha>   coverage, reachability, touch-target tests   (emu lock)
#   box.sh capture <flavor> <x> <sha>   every screen x fixture at font scale 1.0/1.3 (emu lock)
#   box.sh motion  <flavor> <x> <sha>   screenrecord of motion scripts -> sheets     (emu lock)
#   box.sh pack    <flavor> <x> <sha>   tar the results for Fetch
#
# Results land in ~/volum-android/out/<x>/<sha>/{results,apk,captures,motion,logs,registry}.
# At most two Gradle builds (locks/build1|2.lock) and one emulator user
# (locks/emu.lock). Never touches ~/localai-platform.
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
ADB=(adb -s "$SERIAL")
AVD="volum_phone"
PKG="com.volum.android.$FLAVOR"
TEST_PKG="$PKG.test"
CAPTURE_ACTIVITY="com.volum.harness.CaptureActivity"
FFMPEG="$HOME/hubris/bin/ffmpeg"
SCOPE="candidate"
[ "$FLAVOR" = "design" ] && SCOPE="design"
FLAVOR_CAP="$(tr '[:lower:]' '[:upper:]' <<< "${FLAVOR:0:1}")${FLAVOR:1}"

log() { printf '[box %s %s/%s] %s\n' "$(date +%H:%M:%S)" "$STEP" "$X" "$*"; }

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

take_emu_lock() {
  # The adb server daemonizes and would inherit (and keep) the lock fd.
  adb start-server >/dev/null 2>&1 8>&- 9>&- || true
  exec 9>"$ROOT/locks/emu.lock"
  log "waiting for the emulator lock"
  flock -w 7200 9
}

boot_completed() {
  [ "$("${ADB[@]}" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]
}

ensure_emulator() {
  if "${ADB[@]}" get-state 2>/dev/null | grep -q device && boot_completed; then return; fi
  log "booting $AVD"
  setsid nohup emulator -avd "$AVD" -no-window -no-audio -no-boot-anim -no-snapshot -gpu swiftshader_indirect -accel on -port 5554 \
    > "$ROOT/logs/emulator.log" 2>&1 < /dev/null 8>&- 9>&- &
  "${ADB[@]}" wait-for-device
  local t=0
  until boot_completed; do
    sleep 3; t=$((t + 3))
    [ "$t" -lt 300 ] || { log "emulator boot timeout"; return 1; }
  done
  "${ADB[@]}" shell svc power stayon true >/dev/null 2>&1 || true
  "${ADB[@]}" shell input keyevent 82 >/dev/null 2>&1 || true
  "${ADB[@]}" shell settings put system font_scale 1.0 >/dev/null
  log "emulator up after ${t}s"
}

install_apks() {
  "${ADB[@]}" install -r -t "$OUT/apk/app.apk" > "$LOGS/install-app.txt" 2>&1
  if [ "$1" = "with-tests" ]; then
    "${ADB[@]}" install -r -t "$OUT/apk/test.apk" > "$LOGS/install-test.txt" 2>&1
  fi
}

scan_crashes() {
  local tag="$1"
  { "${ADB[@]}" logcat -d -b crash -v time; "${ADB[@]}" logcat -d -b main,system -v time; } 2>/dev/null \
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

run_instrumented() {
  local cls="$1" report="$2"
  log "instrumented $cls"
  "${ADB[@]}" shell run-as "$PKG" rm -f "files/harness/$report" >/dev/null 2>&1 || true
  timeout 1800 "${ADB[@]}" shell am instrument -w -r -e class "com.volum.harness.tests.$cls" \
    "$TEST_PKG/androidx.test.runner.AndroidJUnitRunner" > "$LOGS/instrument-$cls.txt" 2>&1 || true
  if ! "${ADB[@]}" shell run-as "$PKG" cat "files/harness/$report" > "$RES/$report" 2>/dev/null || [ ! -s "$RES/$report" ]; then
    python3 - "$RES/$report" "$LOGS/instrument-$cls.txt" <<'PY'
import json, sys
tail = open(sys.argv[2], errors="replace").read().splitlines()[-30:]
json.dump({"pass": False, "error": "no report written (test crashed or did not run)", "instrument_tail": tail}, open(sys.argv[1], "w"), indent=2)
PY
  fi
}

step_test() {
  take_emu_lock
  ensure_emulator
  install_apks with-tests
  "${ADB[@]}" logcat -c || true
  rm -f "$RES/crash.txt"
  run_instrumented CoverageTest coverage.json
  run_instrumented ReachabilityTest reachability.json
  run_instrumented TouchTargetTest touch.json
  scan_crashes test
}

wait_log() {
  local pattern="$1" limit="$2" t=0
  while ! "${ADB[@]}" logcat -d -s VoLumCapture:I 2>/dev/null | grep -q -- "$pattern"; do
    sleep 0.25; t=$((t + 1))
    [ "$t" -lt $((limit * 4)) ] || return 1
  done
}

step_capture() {
  take_emu_lock
  ensure_emulator
  install_apks no-tests
  rm -rf "$OUT/captures" && mkdir -p "$OUT/captures"
  local list="$OUT/registry/captures-$SCOPE.tsv"
  local seq=0 bad=() count=0 expected=0
  trap 'adb -s "$SERIAL" shell settings put system font_scale 1.0 >/dev/null 2>&1 || true' EXIT
  for scale in 1.0 1.3; do
    "${ADB[@]}" shell settings put system font_scale "$scale" >/dev/null
    "${ADB[@]}" logcat -c || true
    local first=true
    while IFS=$'\t' read -r screen fixture; do
      [ -n "$screen" ] || continue
      expected=$((expected + 1))
      seq=$((seq + 1))
      local stop=()
      if $first; then stop=(-S); first=false; fi
      "${ADB[@]}" shell am start "${stop[@]}" -W -n "$PKG/$CAPTURE_ACTIVITY" --es screen "$screen" --es fixture "$fixture" --ei seq "$seq" >/dev/null 2>&1 || true
      local name="${screen}__${fixture}__fs${scale}.png"
      if ! wait_log "ready seq=$seq " 20; then
        bad+=("$name: not ready within 20 s")
        continue
      fi
      "${ADB[@]}" exec-out screencap -p > "$OUT/captures/$name"
      local dims
      dims="$(python3 -c 'import struct,sys; d=open(sys.argv[1],"rb").read(24); print("%dx%d" % struct.unpack(">II", d[16:24]) if d[:8]==b"\x89PNG\r\n\x1a\n" else "not-png")' "$OUT/captures/$name")"
      if [ "$dims" != "2400x1080" ]; then
        bad+=("$name: $dims, expected 2400x1080")
      else
        count=$((count + 1))
      fi
    done < "$list"
  done
  scan_crashes capture
  printf '%s\n' "${bad[@]:-}" | python3 - "$RES/captures.json" "$count" "$expected" <<'PY'
import json, sys
bad = [l for l in sys.stdin.read().splitlines() if l.strip()]
count, expected = int(sys.argv[2]), int(sys.argv[3])
json.dump({"pass": not bad and count == expected and expected > 0, "count": count, "expected": expected, "bad": bad}, open(sys.argv[1], "w"), indent=2)
PY
}

step_motion() {
  take_emu_lock
  ensure_emulator
  install_apks no-tests
  "${ADB[@]}" shell settings put system font_scale 1.0 >/dev/null
  rm -rf "$OUT/motion" && mkdir -p "$OUT/motion"
  local list="$OUT/registry/motion-$SCOPE.tsv"
  local seq=1000 bad=() sheets=() expected=0
  while IFS=$'\t' read -r id duration_ms; do
    [ -n "$id" ] || continue
    expected=$((expected + 1))
    seq=$((seq + 1))
    "${ADB[@]}" logcat -c || true
    "${ADB[@]}" shell rm -f /sdcard/volum-motion.mp4 >/dev/null 2>&1 || true
    local limit=$(( (duration_ms + 999) / 1000 + 6 ))
    local t0
    t0="$(date +%s.%N)"
    "${ADB[@]}" shell screenrecord --time-limit "$limit" --size 1200x540 --bit-rate 6000000 /sdcard/volum-motion.mp4 > "$LOGS/screenrecord-$id.txt" 2>&1 &
    local rec=$!
    sleep 0.5
    "${ADB[@]}" shell am start -S -W -n "$PKG/$CAPTURE_ACTIVITY" --es motion "$id" --ei seq "$seq" >/dev/null 2>&1 || true
    if ! wait_log "motion-start seq=$seq " 20; then
      bad+=("$id: motion did not start")
      wait "$rec" || true
      continue
    fi
    local t1
    t1="$(date +%s.%N)"
    wait_log "done seq=$seq " $((limit + 5)) || bad+=("$id: motion did not finish")
    wait "$rec" || true
    "${ADB[@]}" pull /sdcard/volum-motion.mp4 "$OUT/motion/$id.mp4" > /dev/null 2>&1 || { bad+=("$id: no recording"); continue; }
    local skip dur fps
    skip="$(python3 -c 'import sys; print(max(0.0, float(sys.argv[2]) - float(sys.argv[1]) - 0.2))' "$t0" "$t1")"
    dur="$(python3 -c 'import sys; print(int(sys.argv[1]) / 1000.0)' "$duration_ms")"
    fps="$(python3 -c 'import sys; print(16.0 / float(sys.argv[1]))' "$dur")"
    if "$FFMPEG" -loglevel error -y -ss "$skip" -t "$dur" -i "$OUT/motion/$id.mp4" \
        -vf "fps=$fps,scale=480:-1,tile=4x4:padding=4:margin=4:color=0x202020" -frames:v 1 "$OUT/motion/$id.png" \
        > "$LOGS/ffmpeg-$id.txt" 2>&1 && [ -s "$OUT/motion/$id.png" ]; then
      sheets+=("$id")
    else
      bad+=("$id: contact sheet failed")
    fi
  done < "$list"
  scan_crashes motion
  printf '%s\n' "${bad[@]:-}" | python3 - "$RES/motion.json" "$expected" "${sheets[@]:-}" <<'PY'
import json, sys
bad = [l for l in sys.stdin.read().splitlines() if l.strip()]
expected = int(sys.argv[2])
sheets = [s for s in sys.argv[3:] if s]
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
