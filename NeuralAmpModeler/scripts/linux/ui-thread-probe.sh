#!/usr/bin/env bash
# Build the VST3 the UI-thread gate links. It exports the six harness symbols.
# The shipped VoLum.vst3 does not. The .so is at
# build-linux-ui-probe/VoLum.vst3/Contents/x86_64-linux/VoLum.so.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=host-env.sh
source "$HERE/host-env.sh"
linux_gate_root
linux_prefer_ci_compiler
cd "$REPO_ROOT"

BUILD="${VOLUM_UI_PROBE_BUILD:-$REPO_ROOT/build-linux-ui-probe}"
cmake_args=(
  -S NeuralAmpModeler/projects/linux
  -B "$BUILD"
  -DCMAKE_BUILD_TYPE=Release
  -DVOLUM_UI_THREAD_PROBE=ON
  -DVOLUM_BUILD_VALIDATOR=OFF
)
if [[ -n "${CC:-}" ]]; then
  cmake_args+=(-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX")
fi
cmake "${cmake_args[@]}"
cmake --build "$BUILD" -j "${JOBS:-10}" --target VoLum-vst3
