#!/usr/bin/env bash
# Build the Linux VST3 and standalone. Run from anywhere; the script finds the repo root.
#   bash NeuralAmpModeler/scripts/build-linux.sh [Release|Debug]
# JOBS overrides the parallel jobs (default 14, the box build-lock cap).
set -euo pipefail

CONFIG="${1:-Release}"
case "$CONFIG" in
  Release|Debug) ;;
  *)
    echo "usage: build-linux.sh [Release|Debug]" >&2
    exit 2
    ;;
esac

JOBS="${JOBS:-14}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

# Prefer the Ubuntu 22.04 CI compiler when it is installed. The box (Ubuntu 26.04)
# has no g++-12 and keeps the default GCC 15. An explicit CC/CXX wins.
if [[ -z "${CXX:-}" && -z "${CC:-}" ]] && command -v g++-12 >/dev/null 2>&1 && command -v gcc-12 >/dev/null 2>&1; then
  export CC=gcc-12
  export CXX=g++-12
fi

SDK="iPlug2/Dependencies/IPlug/VST3_SDK"
if [[ ! -f "$SDK/public.sdk/source/main/linuxmain.cpp" || ! -f "$SDK/public.sdk/samples/vst-hosting/validator/source/validator.cpp" ]]; then
  echo "build-linux: fetching the VST3 SDK (linuxmain.cpp and the validator sample)"
  rm -rf "$SDK"
  git clone --depth 1 --branch master https://github.com/steinbergmedia/vst3sdk.git "$SDK"
  git -C "$SDK" submodule update --init --depth 1 base pluginterfaces public.sdk
fi

BUILD="${BUILD_DIR:-build-linux}"
cmake -S NeuralAmpModeler/projects/linux -B "$BUILD" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILD" -j "$JOBS" --target VoLum-vst3 VoLum-app validator

echo "VST3:      $ROOT/$BUILD/VoLum.vst3"
echo "APP:       $ROOT/$BUILD/standalone/VoLum"
echo "validator: $ROOT/$BUILD/validator"
