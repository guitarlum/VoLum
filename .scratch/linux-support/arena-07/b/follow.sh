#!/usr/bin/env bash
set -u
ROOT="$(pwd)"
OUT="$HOME/volum-linux/out/07b"
SO="$ROOT/build-linux-asan/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
echo "=== asan embed open/close ==="
rm -f /tmp/volum-embed-ready /tmp/volum-embed-done
export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:abort_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"
xvfb-run -a -s "-screen 0 1920x1080x24" bash -c '
  "$1" "$2" 1 >"$3" 2>&1 &
  pid=$!
  for i in $(seq 1 150); do
    [ -f /tmp/volum-embed-ready ] && break
    sleep 0.1
  done
  echo done > /tmp/volum-embed-done
  wait "$pid"
  echo embed_exit:$?
' _ "$OUT/embed" "$SO" "$OUT/asan-embed.log"
echo "embed wrapper exit $?"
echo "--- asan embed log ---"
cat "$OUT/asan-embed.log" || true
if grep -E -q "ERROR: AddressSanitizer|runtime error:|IPlugLinux X error" "$OUT/asan-embed.log"; then
  echo "asan embed reported a sanitizer or X error"
fi

echo "=== red test ==="
python3 - <<'PY'
from pathlib import Path
p = Path("iPlug2/IPlug/Linux/IPlugLinuxRunLoop.cpp")
text = p.read_text()
needle = "bool IPlugLinuxReleaseRunLoop(IPlugLinuxRunLoop* runLoop)\n{\n  TouchUiThread();\n  if (!runLoop)\n    return false;\n"
insert = needle + """
  for (auto& binding : State().bindings)
  {
    if (binding.registered && binding.loop)
      binding.loop->UnregisterTimer(binding.hostId);
    binding.registered = false;
    binding.loop = nullptr;
    binding.hostId = 0;
  }
"""
if needle not in text:
    raise SystemExit("needle missing")
p.write_text(text.replace(needle, insert, 1))
print("patched")
PY
cmake --build "$ROOT/NeuralAmpModeler/build-tests-linux" -j 10 --target NeuralAmpModeler-Tests
echo "red build $?"
"$ROOT/NeuralAmpModeler/build-tests-linux/NeuralAmpModeler-Tests" --test-case="linux run loop: editor B timers survive editor A closing" >"$OUT/red-test.txt" 2>&1
echo "red test exit $?"
tail -40 "$OUT/red-test.txt"
git -C "$ROOT/iPlug2" checkout -- IPlug/Linux/IPlugLinuxRunLoop.cpp
echo "restored $(git -C "$ROOT/iPlug2" status --short -- IPlug/Linux/IPlugLinuxRunLoop.cpp)"
