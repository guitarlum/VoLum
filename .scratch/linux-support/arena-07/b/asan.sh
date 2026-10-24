#!/usr/bin/env bash
set -u
ROOT="$(pwd)"
REAL_HOME="$HOME"
cmake -S NeuralAmpModeler/projects/linux -B build-linux-asan -DCMAKE_BUILD_TYPE=Release -DVOLUM_SANITIZE=ON
if [[ $? -ne 0 ]]; then echo "asan configure failed"; exit 1; fi
cmake --build build-linux-asan -j 10 --target VoLum-vst3
if [[ $? -ne 0 ]]; then echo "asan build failed"; exit 1; fi
VST="$ROOT/build-linux-asan/VoLum.vst3"
REAPER="${HOME}/volum-linux/tools/bin/reaper"
if [[ ! -x "$REAPER" ]]; then
  REAPER="${HOME}/volum-linux/tools/reaper/reaper_linux_x86_64/REAPER/reaper"
fi
CFG=$(mktemp -d)
export HOME="$CFG/home"
mkdir -p "$HOME/.config/REAPER/Scripts"
cat > "$HOME/.config/REAPER/reaper.ini" <<EOF
[reaper]
loadlastproj=0
splashfeedback=0
vstpath=$ROOT/build-linux-asan
EOF
cp /tmp/volum-07b-reaper.lua "$HOME/.config/REAPER/Scripts/__startup.lua"
OUT="$REAL_HOME/volum-linux/out/07b"
mkdir -p "$OUT"
export VOLUM_SMOKE_RESULT="$OUT/reaper-asan-result.txt"
rm -f "$VOLUM_SMOKE_RESULT"
ASAN_LIB=$(gcc -print-file-name=libasan.so)
echo "asan lib $ASAN_LIB"
export LD_PRELOAD="$ASAN_LIB"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
xvfb-run -a -s "-screen 0 1920x1080x24" "$REAPER" -nosplash >"$OUT/reaper-asan.txt" 2>&1 &
rpid=$!
for i in $(seq 1 180); do
  if ! kill -0 "$rpid" 2>/dev/null; then break; fi
  if [[ -f "$VOLUM_SMOKE_RESULT" ]] && grep -q '^PASS$' "$VOLUM_SMOKE_RESULT"; then
    sleep 1
    kill "$rpid" 2>/dev/null || true
    break
  fi
  sleep 1
done
kill "$rpid" 2>/dev/null || true
wait "$rpid" 2>/dev/null || true
echo "asan reaper result:"
cat "$VOLUM_SMOKE_RESULT" 2>/dev/null || echo "(none)"
fail=0
if ! grep -q '^PASS$' "$VOLUM_SMOKE_RESULT" 2>/dev/null; then fail=1; tail -50 "$OUT/reaper-asan.txt"; fi
if grep -E -q "ERROR: AddressSanitizer|runtime error:" "$OUT/reaper-asan.txt"; then
  echo "sanitizer report"
  fail=1
fi
if grep -q "IPlugLinux X error" "$OUT/reaper-asan.txt"; then
  echo "asan reaper X error"
  fail=1
fi
exit $fail
