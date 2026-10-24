#!/usr/bin/env bash
# Throwaway ticket 07a gates on the box (ticket 08 turns them into product
# gates). Run from the worktree root:
#
#   bash .scratch/linux-support/arena-07/a/run-gates.sh [step...]
#
# Steps (default: all, in this order): build validator pluginval reaper shots
# asan-build asan-reaper asan-pluginval. Output goes to $OUT (default
# ~/volum-linux/out/07a/<sha>). Every host runs with a throwaway HOME.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(pwd)" # the worktree root; the scripts may be a copy under /tmp
cd "$ROOT"
SHA="$(git rev-parse --short HEAD)"
OUT="${OUT:-$HOME/volum-linux/out/07a/$SHA}"
PLUGINVAL="${PLUGINVAL:-$HOME/volum-linux/tools/pluginval/pluginval}"
JOBS="${JOBS:-10}"
SCREEN=1920x1080x24
mkdir -p "$OUT"

STEPS=("$@")
[[ ${#STEPS[@]} -gt 0 ]] || STEPS=(build validator pluginval reaper shots asan-build asan-reaper asan-pluginval)

throwaway_home() {
  local home
  home="$(mktemp -d "$OUT/home.XXXXXX")"
  mkdir -p "$home/.config" "$home/.local/share"
  echo "$home"
}

# No X error, no off-thread registry call, no sanitizer report in a log.
clean_log() {
  local log="$1" bad
  bad=$(grep -cE "IGraphicsLinux: X error|off the UI thread|ERROR: AddressSanitizer:|runtime error:" "$log" || true)
  echo "  $(basename "$log"): $bad X-error/off-thread/sanitizer lines"
  [[ "$bad" == 0 ]]
}

run_pluginval() {
  local bundle="$1" log="$2" preload=() home
  home="$(throwaway_home)"
  # ASAN_PRELOAD: none | asan (default) | asan+stdc++. See the 07 report for
  # which one each host needs.
  local lib=""
  case "${ASAN_PRELOAD:-asan}" in
    none) ;;
    asan) lib="$(gcc -print-file-name=libasan.so)" ;;
    asan+stdc++) lib="$(gcc -print-file-name=libasan.so):$(gcc -print-file-name=libstdc++.so.6)" ;;
  esac
  [[ "${3:-}" == asan ]] && preload=(env "LD_PRELOAD=$lib"
    "ASAN_OPTIONS=detect_leaks=0" "UBSAN_OPTIONS=print_stacktrace=1:suppressions=$HERE/ubsan.supp")
  set +e
  HOME="$home" XDG_CONFIG_HOME="$home/.config" XDG_DATA_HOME="$home/.local/share" IGRAPHICS_LINUX_TRACE=1 \
    xvfb-run -a -s "-screen 0 $SCREEN" "${preload[@]}" "$PLUGINVAL" --strictness-level 10 --validate-in-process \
    --timeout-ms 1800000 --output-dir "$OUT/pluginval-logs" --validate "$bundle" > "$log" 2>&1
  local rc=$?
  set -e
  echo "pluginval exit $rc ($log), editor opens/closes: $(grep -c 'IGraphicsLinux: editor .* open in' "$log")/$(grep -c 'IGraphicsLinux: editor .* closed' "$log")"
  grep -E "^(SUCCESS|FAILURE|ALL TESTS PASSED)|Starting test: pluginval / Editor|tests? (passed|failed)" "$log" | tail -12 || true
  clean_log "$log" && [[ "$rc" == 0 ]]
}

run_step() {
  local step="$1" home rc
  case "$step" in
    build)
      JOBS="$JOBS" bash NeuralAmpModeler/scripts/build-linux.sh Release > "$OUT/build.log" 2>&1 || { tail -40 "$OUT/build.log"; exit 1; }
      tail -3 "$OUT/build.log"
      ;;
    tests)
      # run-tests-linux.sh builds with -j 14; the arena caps the box at 10, so
      # the same configuration is built here first and the script finds it current.
      # Same compiler choice as run-tests-linux.sh, so it keeps this cache.
      local cc cxx
      cc="$(command -v gcc-12 || command -v gcc-11 || command -v gcc)"
      cxx="$(command -v g++-12 || command -v g++-11 || command -v g++)"
      cmake -S NeuralAmpModeler/tests -B NeuralAmpModeler/build-tests-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" -DVOLUM_ENABLE_SANITIZERS=0 > "$OUT/tests-build.log" 2>&1
      cmake --build NeuralAmpModeler/build-tests-linux -j "$JOBS" >> "$OUT/tests-build.log" 2>&1 || { tail -40 "$OUT/tests-build.log"; return 1; }
      set +e
      bash NeuralAmpModeler/scripts/run-tests-linux.sh > "$OUT/tests.log" 2>&1
      rc=$?
      set -e
      grep -a -E "^\[doctest\] (test cases|assertions|Status)" "$OUT/tests.log" | tail -3
      NeuralAmpModeler/build-tests-linux/NeuralAmpModeler-Tests --test-case="*IPlugLinux*,*IGraphicsLinuxGLNesting*" --success=false | tail -4
      echo "run-tests-linux.sh exit $rc"
      [[ "$rc" == 0 ]]
      ;;
    validator)
      home="$(throwaway_home)"
      set +e
      HOME="$home" XDG_DATA_HOME="$home/.local/share" build-linux/validator build-linux/VoLum.vst3 > "$OUT/validator.log" 2>&1
      rc=$?
      set -e
      grep -E "Result:" "$OUT/validator.log" | tail -2
      echo "validator exit $rc"
      [[ "$rc" == 0 ]]
      ;;
    pluginval) run_pluginval build-linux/VoLum.vst3 "$OUT/pluginval.log" ;;
    reaper) bash "$HERE/reaper-smoke.sh" --bundle build-linux/VoLum.vst3 --out "$OUT" --cycles 20 --instances 2 ;;
    input) bash "$HERE/reaper-smoke.sh" --bundle build-linux/VoLum.vst3 --out "$OUT" --mode input --shot "$OUT/input-start.png" ;;
    xft-shot)
      # Scale from Xft.dpi alone (no GDK_SCALE): 192 dpi is scale 2.
      bash "$HERE/reaper-smoke.sh" --bundle build-linux/VoLum.vst3 --out "$OUT" --mode shot --xft-dpi 192 --screen 3840x2160x24 --shot "$OUT/07a-xft192.png"
      ;;
    shots)
      bash "$HERE/reaper-smoke.sh" --bundle build-linux/VoLum.vst3 --out "$OUT" --mode shot --shot "$OUT/07a-scale1.png"
      # 1800x1200 at scale 2 does not fit 1920x1080, so this one screen is larger.
      bash "$HERE/reaper-smoke.sh" --bundle build-linux/VoLum.vst3 --out "$OUT" --mode shot --scale 2 --screen 3840x2160x24 --shot "$OUT/07a-scale2.png"
      ;;
    asan-build)
      cmake -S NeuralAmpModeler/projects/linux -B build-linux-asan -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVOLUM_SANITIZE=ON -DVOLUM_BUILD_VALIDATOR=OFF > "$OUT/asan-build.log" 2>&1
      cmake --build build-linux-asan -j "$JOBS" --target VoLum-vst3 >> "$OUT/asan-build.log" 2>&1 || { tail -40 "$OUT/asan-build.log"; exit 1; }
      readelf -d build-linux-asan/VoLum.vst3/Contents/x86_64-linux/VoLum.so | grep -E "libasan|libubsan"
      ;;
    asan-reaper) bash "$HERE/reaper-smoke.sh" --bundle build-linux-asan/VoLum.vst3 --out "$OUT" --cycles 20 --instances 2 --asan ;;
    asan-pluginval) run_pluginval build-linux-asan/VoLum.vst3 "$OUT/pluginval-asan.log" asan ;;
    *) echo "unknown step $step" >&2; return 2 ;;
  esac
}

failed=()
for step in "${STEPS[@]}"; do
  echo "=== $step ==="
  run_step "$step" || failed+=("$step")
done
echo "out: $OUT"
if [[ ${#failed[@]} -gt 0 ]]; then
  echo "GATES FAILED: ${failed[*]}"
  exit 1
fi
echo "GATES PASSED: ${STEPS[*]}"
