#!/bin/bash
# Compile the Linux iPlug2 leaf units with GCC and Clang (-Wall -Wextra -Werror).
set -euo pipefail
cd "$(dirname "$0")"

build_one() {
  local cc="$1"
  local cxx="$2"
  local dir="$3"
  rm -rf "$dir"
  cmake -S . -B "$dir" -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$dir" --parallel 14
}

echo "=== gcc ==="
gcc --version | head -n 1
build_one gcc g++ build-gcc
echo "=== clang ==="
clang --version | head -n 1
build_one clang clang++ build-clang
echo "compile-check: gcc and clang ok"
