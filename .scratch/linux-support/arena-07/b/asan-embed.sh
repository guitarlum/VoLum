#!/usr/bin/env bash
set -u
ROOT="$(pwd)"
OUT="$HOME/volum-linux/out/07b"
SO="$ROOT/build-linux-asan/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
export LD_PRELOAD="$(gcc -print-file-name=libasan.so)"
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
rm -f /tmp/volum-embed-ready /tmp/volum-embed-done
echo "preload $LD_PRELOAD"
xvfb-run -a -s "-screen 0 1920x1080x24" bash -c '
  "$1" "$2" 1 >"$3" 2>&1 &
  pid=$!
  for i in $(seq 1 150); do
    [ -f /tmp/volum-embed-ready ] && break
    sleep 0.1
  done
  if [ ! -f /tmp/volum-embed-ready ]; then
    echo "not ready"
    wait "$pid" || true
    exit 2
  fi
  echo done > /tmp/volum-embed-done
  wait "$pid"
  echo embed_exit:$?
' _ "$OUT/embed" "$SO" "$OUT/asan-embed.log"
echo "wrapper $?"
echo "--- log ---"
cat "$OUT/asan-embed.log" || true
