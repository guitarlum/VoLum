#!/usr/bin/env bash
# Show each plugin gate failing on purpose, then restore the tree.
# Run from the repo root after a Release build. Exits 0 only when every gate
# failed. Does not commit the broken plug-in or the removed UI-thread checks.
#
#   redproof-plugin-gates.sh

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=host-env.sh
source "$HERE/host-env.sh"
linux_gate_root
cd "$REPO_ROOT"

BUNDLE="$REPO_ROOT/build-linux/VoLum.vst3"
[[ -d "$BUNDLE" ]] || { echo "redproof: build the VST3 first" >&2; exit 2; }
SHA="$(git rev-parse --short HEAD)"
OUT="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}/out/plugin-gates/$SHA/redproof"
mkdir -p "$OUT"
CPP="iPlug2/IGraphics/Platforms/IGraphicsLinux.cpp"
ok=0

expect_fail() {
  local name="$1"
  shift
  echo "### red $name"
  set +e
  "$@" > "$OUT/$name.log" 2>&1
  local rc=$?
  set -e
  tail -30 "$OUT/$name.log" || true
  if [[ "$rc" == 0 ]]; then
    echo "RED PROOF FAILED: $name exited 0"
    ok=1
  else
    echo "red $name exit $rc"
  fi
}

broken="$OUT/broken.vst3"
rm -rf "$broken"
mkdir -p "$broken/Contents/x86_64-linux"
cp -a "$BUNDLE/Contents/." "$broken/Contents/"
echo 'void plug(void) {}' | gcc -shared -fPIC -x c - -o "$broken/Contents/x86_64-linux/VoLum.so"

expect_fail validator bash "$REPO_ROOT/NeuralAmpModeler/scripts/validate-vst3-linux.sh" \
  --no-build --bundle "$broken" --skip-ui-thread --skip-pluginval --skip-carla
expect_fail pluginval bash "$REPO_ROOT/NeuralAmpModeler/scripts/validate-vst3-linux.sh" \
  --no-build --bundle "$broken" --skip-ui-thread --skip-validator --skip-carla
expect_fail carla bash "$REPO_ROOT/NeuralAmpModeler/scripts/validate-vst3-linux.sh" \
  --no-build --bundle "$broken" --skip-ui-thread --skip-validator --skip-pluginval

python3 - "$OUT/silence.wav" <<'PY'
import struct, sys
n = 48000 * 10
data = struct.pack("<" + "f" * n, *([0.0] * n))
header = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(data), b"WAVE", b"fmt ", 16, 3, 1, 48000, 192000, 4, 32, b"data", len(data))
open(sys.argv[1], "wb").write(header + data)
PY
expect_fail reaper bash "$REPO_ROOT/NeuralAmpModeler/scripts/smoke-reaper-linux.sh" \
  --bundle "$BUNDLE" --reference "$OUT/silence.wav" --out "$OUT/reaper"

restore_cpp() {
  git -C "$REPO_ROOT/iPlug2" checkout -- "IGraphics/Platforms/IGraphicsLinux.cpp"
  cmake --build "$REPO_ROOT/build-linux" -j "${JOBS:-10}" --target VoLum-vst3 || true
}
trap restore_cpp EXIT
perl -0pi -e 's/IPlugLinuxCheckUIThread\("IGraphicsLinux::(OpenWindow|CloseWindow|ProcessX11Events|OnDisplayTimer)"\);/\/\* REVERTED: no IPlugLinuxCheckUIThread $1 *\//g' "$REPO_ROOT/$CPP"
if ! grep -q "REVERTED: no IPlugLinuxCheckUIThread" "$REPO_ROOT/$CPP"; then
  echo "redproof: the UI-thread revert did not apply" >&2
  exit 1
fi
cmake --build "$REPO_ROOT/build-linux" -j "${JOBS:-10}" --target VoLum-vst3
expect_fail ui-thread bash "$HERE/ui-thread-sites.sh" --so "$BUNDLE/Contents/x86_64-linux/VoLum.so"
trap - EXIT
git -C "$REPO_ROOT/iPlug2" checkout -- "IGraphics/Platforms/IGraphicsLinux.cpp"
cmake --build "$REPO_ROOT/build-linux" -j "${JOBS:-10}" --target VoLum-vst3

if [[ "$ok" != 0 ]]; then
  echo "RED PROOFS FAILED"
  exit 1
fi
echo "RED PROOFS PASSED (each gate exited non-zero)"
