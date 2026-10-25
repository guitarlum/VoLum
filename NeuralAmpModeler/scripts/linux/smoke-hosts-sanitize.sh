#!/usr/bin/env bash
# Sanitizer host smoke: REAPER, two instances, 20 open/close cycles, then
# pluginval strictness 10, both against the VOLUM_SANITIZE build.
# LD_PRELOAD=libasan.so is applied by the gate scripts. detect_leaks=0 because
# the hosts are uninstrumented. The only suppression is shift-base:stb_image.h.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=host-env.sh
source "$HERE/host-env.sh"
linux_gate_root
linux_prefer_ci_compiler
cd "$REPO_ROOT"

JOBS="${JOBS:-10}"
cmake_args=(
  -S NeuralAmpModeler/projects/linux
  -B build-linux-asan
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
  -DVOLUM_SANITIZE=ON
  -DVOLUM_BUILD_VALIDATOR=OFF
)
if [[ -n "${CC:-}" ]]; then
  cmake_args+=(-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX")
fi
cmake "${cmake_args[@]}"
cmake --build build-linux-asan -j "$JOBS" --target VoLum-vst3
so="build-linux-asan/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
readelf -d "$so" | grep -E "libasan|libubsan"
echo "NEEDED libstdc++: $(readelf -d "$so" | grep -c 'libstdc++' || true) (shared runtime; static libstdc++ is off for VOLUM_SANITIZE)"

bash "$REPO_ROOT/NeuralAmpModeler/scripts/smoke-reaper-linux.sh" \
  --bundle "$REPO_ROOT/build-linux-asan/VoLum.vst3" \
  --mode cycles --cycles 20 --instances 2 --asan
bash "$REPO_ROOT/NeuralAmpModeler/scripts/validate-vst3-linux.sh" \
  --bundle "$REPO_ROOT/build-linux-asan/VoLum.vst3" --asan --pluginval-only --no-build
echo "smoke-hosts-sanitize: PASS"
