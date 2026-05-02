#!/usr/bin/env bash
# Build and run the VoLum doctest suite with clang/CMake on macOS.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

SANITIZE=0
for arg in "$@"; do
  case "$arg" in
    --sanitize)
      SANITIZE=1
      ;;
    *)
      echo "Usage: $0 [--sanitize]" >&2
      exit 1
      ;;
  esac
done

BUILD_DIR="$PROJECT_DIR/build-mac/tests"
BUILD_TYPE=Release
if [[ "$SANITIZE" == "1" ]]; then
  BUILD_DIR="$PROJECT_DIR/build-mac/tests-sanitized"
  BUILD_TYPE=Debug
fi

cmake -S "$PROJECT_DIR/tests" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DVOLUM_ENABLE_SANITIZERS="$SANITIZE"

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j"$(sysctl -n hw.ncpu)"

cd "$REPO_ROOT"
"$BUILD_DIR/NeuralAmpModeler-Tests"
