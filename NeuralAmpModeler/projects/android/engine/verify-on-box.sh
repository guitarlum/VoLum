#!/usr/bin/env bash
set -euo pipefail

SHA="${1:?commit sha}"
ROOT="$HOME/volum-android"
# shellcheck disable=SC1091
source "$ROOT/env.sh"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../../../.." && pwd)"
PROJECT="$REPO/NeuralAmpModeler/projects/android"
CMAKE="$HOME/android-sdk/cmake/3.22.1/bin/cmake"
OUT="$ROOT/out/spike/$SHA"
LOGS="$OUT/logs"
mkdir -p "$LOGS"

exec 7>"$ROOT/locks/git.lock"
flock -w 7200 7
git -C "$REPO" submodule update --init --depth 1 NeuralAmpModelerCore AudioDSPTools eigen \
  >"$LOGS/submodules.txt" 2>&1
git -C "$REPO" submodule status NeuralAmpModelerCore AudioDSPTools eigen \
  >"$LOGS/submodule-status.txt"
cat "$ROOT/ndk-version.txt" >"$LOGS/ndk-version.txt"
exec 7>&-

exec 8>"$ROOT/locks/build1.lock"
if ! flock -n 8; then
  exec 8>"$ROOT/locks/build2.lock"
  if ! flock -n 8; then
    exec 8>"$ROOT/locks/build1.lock"
    flock -w 7200 8
  fi
fi

rm -rf "$OUT/host"
"$CMAKE" -S "$HERE" -B "$OUT/host" -DCMAKE_BUILD_TYPE=Release \
  >"$LOGS/host-configure.txt" 2>&1
"$CMAKE" --build "$OUT/host" --target volum_engine_render --parallel 8 \
  >"$LOGS/host-build.txt" 2>&1
"$OUT/host/volum_engine_render" \
  "$REPO/NeuralAmpModeler/tests/golden_rigs/_input.wav" \
  "$REPO/rigs/Ampete One/AMP-Ampt-1.nam" \
  "$OUT/host.wav" >"$LOGS/host-render.json" 2>"$LOGS/host-render-error.txt"

(cd "$PROJECT" && ./gradlew --console=plain \
  :engine:assembleDebug :engine:assembleDebugAndroidTest) \
  >"$LOGS/ndk-build.txt" 2>&1 8>&-

ARM_SO="$(find "$PROJECT/engine/build" -path '*/arm64-v8a/libvolum_engine_jni.so' -print -quit)"
X86_SO="$(find "$PROJECT/engine/build" -path '*/x86_64/libvolum_engine_jni.so' -print -quit)"
test -n "$ARM_SO" && test -s "$ARM_SO"
test -n "$X86_SO" && test -s "$X86_SO"
printf 'arm64-v8a %s\nx86_64 %s\n' "$ARM_SO" "$X86_SO" >"$LOGS/ndk-artifacts.txt"
exec 8>&-

SERIAL="emulator-5554"
adbx() { adb -s "$SERIAL" "$@" < /dev/null 9>&-; }
boot_completed() {
  [ "$(adbx shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]
}
adb start-server >/dev/null 2>&1 9>&- || true
exec 9>"$ROOT/locks/emu.lock"
flock -w 7200 9
if ! { adbx get-state 2>/dev/null | grep -q device && boot_completed; }; then
  setsid nohup emulator -avd volum_phone -no-window -no-audio -no-boot-anim -no-snapshot \
    -gpu swiftshader_indirect -accel on -port 5554 >"$ROOT/logs/emulator.log" 2>&1 < /dev/null 9>&- &
  adbx wait-for-device
  elapsed=0
  until boot_completed; do
    sleep 3
    elapsed=$((elapsed + 3))
    [ "$elapsed" -lt 300 ]
  done
fi

APK="$(find "$PROJECT/engine/build/outputs/apk/androidTest" -name '*androidTest.apk' -print -quit)"
test -n "$APK" && test -s "$APK"
adbx install -r -t "$APK" >"$LOGS/install.txt" 2>&1
adbx logcat -c || true
set +e
timeout 1800 adb -s "$SERIAL" shell am instrument -w -r \
  -e class com.volum.engine.EngineParityInstrumentedTest \
  com.volum.engine.test/androidx.test.runner.AndroidJUnitRunner \
  < /dev/null >"$LOGS/instrumented-test.txt" 2>&1 9>&-
TEST_EXIT=$?
set -e
grep -q 'OK (1 test)' "$LOGS/instrumented-test.txt"
test "$TEST_EXIT" -eq 0
adbx shell run-as com.volum.engine.test cat files/android.wav >"$OUT/android.wav"
adbx shell run-as com.volum.engine.test cat files/engine-result.json >"$LOGS/emulator-render.json"
exec 9>&-

python3 - "$OUT/host.wav" "$OUT/android.wav" "$LOGS/null-test.json" <<'PY'
import json, math, struct, sys

def samples(path):
    data = open(path, "rb").read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise RuntimeError(path + " is not RIFF/WAVE")
    pos = 12
    audio = None
    while pos + 8 <= len(data):
        tag, size = data[pos:pos+4], struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos+8:pos+8+size]
        if tag == b"data":
            audio = body
            break
        pos += 8 + size + (size & 1)
    if audio is None or len(audio) % 4:
        raise RuntimeError(path + " has no float data")
    return struct.unpack("<%df" % (len(audio) // 4), audio)

a, b = samples(sys.argv[1]), samples(sys.argv[2])
if len(a) != len(b):
    raise RuntimeError("frame count differs: %d != %d" % (len(a), len(b)))
diff = [x - y for x, y in zip(a, b)]
peak = max(map(abs, diff), default=0.0)
rms = math.sqrt(sum(x*x for x in diff) / max(1, len(diff)))
db = lambda x: 20.0 * math.log10(x) if x > 0.0 else -300.0
result = {
    "frames": len(diff),
    "peak_difference_dbfs": db(peak),
    "rms_difference_dbfs": db(rms),
    "threshold_dbfs_rms": -90.0,
    "pass": db(rms) <= -90.0,
}
json.dump(result, open(sys.argv[3], "w"), indent=2)
print(json.dumps(result))
if not result["pass"]:
    raise SystemExit(1)
PY

python3 - "$SHA" "$OUT/spike.json" "$LOGS/null-test.json" "$LOGS/emulator-render.json" <<'PY'
import json, sys
sha, out, null_path, emulator_path = sys.argv[1:]
null = json.load(open(null_path))
emulator = json.load(open(emulator_path))
result = {
    "commit": sha,
    "ndk_build": True,
    "host_build": True,
    "null_test_db_rms": null["rms_difference_dbfs"],
    "pass": bool(null["pass"]),
    "null_test_db_peak": null["peak_difference_dbfs"],
    "emulator_rtf": emulator["rtf"],
    "emulator_rtf_label": "x86 emulator, not a phone",
}
json.dump(result, open(out, "w"), indent=2)
PY

tar -czf "$ROOT/out/spike/$SHA.tgz" -C "$ROOT/out/spike" "$SHA"
cat "$OUT/spike.json"
