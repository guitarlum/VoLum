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
cp "$ROOT/.scratch/linux-support/arena-07/b/reaper.lua" "$HOME/.config/REAPER/Scripts/__startup.lua"
OUT="$REAL_HOME/volum-linux/out/07b"
mkdir -p "$OUT"
export VOLUM_SMOKE_RESULT="$OUT/reaper-asan-result.txt"
rm -f "$VOLUM_SMOKE_RESULT"
SUPP="$ROOT/.scratch/linux-support/arena-07/b/ubsan.supp"
if [[ ! -f "$SUPP" ]]; then echo "missing $SUPP"; exit 1; fi
ASAN_LIB=$(gcc -print-file-name=libasan.so)
echo "asan lib $ASAN_LIB"
echo "ubsan supp $SUPP"
export LD_PRELOAD="$ASAN_LIB"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:suppressions=$SUPP"
setsid xvfb-run -a -s "-screen 0 1920x1080x24" "$REAPER" -nosplash >"$OUT/reaper-asan.txt" 2>&1 &
rpid=$!
for i in $(seq 1 360); do
  if ! kill -0 "$rpid" 2>/dev/null; then break; fi
  if [[ -f "$VOLUM_SMOKE_RESULT" ]] && grep -q '^PASS$' "$VOLUM_SMOKE_RESULT"; then
    sleep 1
    kill -- -"$rpid" 2>/dev/null || kill "$rpid" 2>/dev/null || true
    break
  fi
  sleep 1
done
kill -- -"$rpid" 2>/dev/null || kill "$rpid" 2>/dev/null || true
wait "$rpid" 2>/dev/null || true
echo "asan reaper result:"
cat "$VOLUM_SMOKE_RESULT" 2>/dev/null || echo "(none)"
xcount=$(grep -c "IPlugLinux X error" "$OUT/reaper-asan.txt" || true)
offcount=$(grep -c "off the UI thread" "$OUT/reaper-asan.txt" || true)
sancount=$(grep -E -c "ERROR: AddressSanitizer|runtime error:" "$OUT/reaper-asan.txt" || true)
echo "X errors: ${xcount:-0}"
echo "off-thread: ${offcount:-0}"
echo "sanitizer reports: ${sancount:-0}"
fail=0
if ! grep -q '^PASS$' "$VOLUM_SMOKE_RESULT" 2>/dev/null; then fail=1; tail -80 "$OUT/reaper-asan.txt"; fi
if [[ "${sancount:-0}" != "0" ]]; then
  echo "sanitizer report"
  fail=1
  awk '
    /ERROR: AddressSanitizer|runtime error:/ { show=1 }
    show { print }
    show && /^$/ { show=0 }
  ' "$OUT/reaper-asan.txt"
fi
if [[ "${xcount:-0}" != "0" ]]; then
  echo "asan reaper X error"
  fail=1
fi
exit $fail
