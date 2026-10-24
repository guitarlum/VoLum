#!/usr/bin/env bash
set -u
ROOT="$(pwd)"
SO="$ROOT/build-linux/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
SDK="$ROOT/iPlug2/Dependencies/IPlug/VST3_SDK"
OUT="${HOME}/volum-linux/out/07b"
mkdir -p "$OUT"
if ! command -v import >/dev/null 2>&1; then
  sudo apt-get install -y imagemagick
fi
g++ -std=c++20 -O2 -I"$SDK" \
  /tmp/volum-embed.cpp \
  "$SDK/pluginterfaces/base/coreiids.cpp" \
  "$SDK/pluginterfaces/base/funknown.cpp" \
  "$SDK/pluginterfaces/base/ustring.cpp" \
  "$SDK/public.sdk/source/common/commoniids.cpp" \
  "$SDK/public.sdk/source/vst/vstinitiids.cpp" \
  -o "$OUT/embed" -lX11 -ldl -pthread
if [[ $? -ne 0 ]]; then
  echo "embed compile failed"
  exit 1
fi
shot() {
  local scale="$1" png="$2"
  rm -f /tmp/volum-embed-ready /tmp/volum-embed-done
  xvfb-run -a -s "-screen 0 1920x1080x24" bash -c '
    "$1" "$2" "$3" >"$4" 2>&1 &
    pid=$!
    for i in $(seq 1 150); do
      [ -f /tmp/volum-embed-ready ] && break
      sleep 0.1
    done
    if [ ! -f /tmp/volum-embed-ready ]; then
      echo "not ready" >&2
      wait "$pid" || true
      exit 2
    fi
    import -window root "$5"
    echo done > /tmp/volum-embed-done
    wait "$pid" || true
  ' _ "$OUT/embed" "$SO" "$scale" "$OUT/embed-$scale.log" "$png"
  if [[ $? -ne 0 ]]; then
    echo "scale $scale failed"
    cat "$OUT/embed-$scale.log"
    return 1
  fi
  if grep -q "IPlugLinux X error" "$OUT/embed-$scale.log"; then
    echo "scale $scale X error"
    return 1
  fi
  echo "wrote $png"
  return 0
}
fail=0
shot 1 "$OUT/scale1.png" || fail=1
shot 2 "$OUT/scale2.png" || fail=1
ls -l "$OUT"/scale1.png "$OUT"/scale2.png || true
exit $fail
