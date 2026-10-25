#!/usr/bin/env bash
# Prove the four IPlugLinuxCheckUIThread sites in IGraphicsLinux.cpp.
#   ui-thread-sites.sh [--so PATH/VoLum.so]
# Builds a small harness and links the plug-in .so, which already contains the
# sites. Run after build-linux.sh. Red with those four calls removed: the log
# lines disappear and this script exits 1.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=host-env.sh
source "$HERE/host-env.sh"
linux_gate_root

SO=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --so) SO="$2"; shift 2 ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done
if [[ -z "$SO" ]]; then
  SO="$REPO_ROOT/build-linux/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
fi
[[ -f "$SO" ]] || { echo "ui-thread sites: missing $SO" >&2; exit 2; }
SO="$(cd "$(dirname "$SO")" && pwd)/$(basename "$SO")"

IPLUG="$REPO_ROOT/iPlug2"
NAM="$REPO_ROOT/NeuralAmpModeler"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/volum-ui-thread.XXXXXX")"
LOG="$WORK/sites.log"

g++ -std=c++20 -O0 -g -pthread \
  -DIGRAPHICS_NANOVG -DIGRAPHICS_GL2 \
  -I"$NAM" -I"$IPLUG/IPlug" -I"$IPLUG/IPlug/APP" -I"$IPLUG/IPlug/Extras" \
  -I"$IPLUG/IGraphics" -I"$IPLUG/IGraphics/Platforms" -I"$IPLUG/IGraphics/Drawing" \
  -I"$IPLUG/IGraphics/Controls" -I"$IPLUG/WDL" \
  -I"$IPLUG/Dependencies/IGraphics/NanoVG/src" \
  -I"$IPLUG/Dependencies/IGraphics/NanoSVG/src" \
  -I"$IPLUG/Dependencies/IGraphics/glad_GL2/include" \
  -I"$IPLUG/Dependencies/IGraphics/STB" \
  "$HERE/ui-thread-sites.cpp" "$SO" -Wl,-rpath,"$(dirname "$SO")" -o "$WORK/ui-thread-sites"

set +e
xvfb-run -a -s "-screen 0 1920x1080x24" "$WORK/ui-thread-sites" "$LOG"
rc=$?
set -e
cat "$LOG" || true
echo "ui-thread sites exit $rc"
exit "$rc"
