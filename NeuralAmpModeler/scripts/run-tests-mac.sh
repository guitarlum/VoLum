#!/usr/bin/env bash
# Build and run the VoLum doctest suite with clang/CMake on macOS.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

# Mirrors run-tests-win.ps1: the tests compile plugin headers that depend on the
# local iPlug2 patches, so apply them before configuring. Idempotent.
# Invoked through `bash` rather than directly because the executable bit is not
# set consistently across the repo's shell scripts.
bash "$PROJECT_DIR/iplug2-patches/apply-iplug2-patches.sh"

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

BUILD_DIR="$PROJECT_DIR/build-tests-mac/tests"
BUILD_TYPE=Release
if [[ "$SANITIZE" == "1" ]]; then
  BUILD_DIR="$PROJECT_DIR/build-tests-mac/tests-sanitized"
  BUILD_TYPE=Debug
fi

cmake -S "$PROJECT_DIR/tests" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DVOLUM_ENABLE_SANITIZERS="$SANITIZE"

cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j"$(sysctl -n hw.ncpu)"

cd "$REPO_ROOT"
if [[ "$SANITIZE" == "1" ]]; then
  # doctest's parseOptionImpl keeps only the last repeated --test-case-exclude.
  # Comma-separated filters in ONE flag accumulate. Repeating the flag does not.
  "$BUILD_DIR/NeuralAmpModeler-Tests" \
    --test-case-exclude="Golden *,Load *NAM*,Cached NAM dspData can construct multiple models when copied,Core slimmable NAM example loads and processes,Process one block through every bundled main NAM file,A2 core load and prewarm timing is visible in test logs,A2 container can lazily activate the Lite submodel after load"
else
  # --duration prints each case's name as it finishes. doctest cannot report which
  # case it was in when the process dies, so without this a crash in CI is just
  # "Bus error" with no way to narrow it from a machine we do not have.
  set +e
  "$BUILD_DIR/NeuralAmpModeler-Tests" --duration=true
  rc=$?
  set -e
  if [[ "$rc" -ne 0 ]]; then
    # A signal death prints one line and no backtrace, and CI runners keep no crash
    # reports. Re-running under lldb turns "Bus error" into a symbolicated stack,
    # which is the whole difference when the machine is one we cannot reproduce on.
    echo "=== tests exited $rc; re-running under lldb for a backtrace ==="
    lldb --batch -o run -k "bt all" -k "register read" -k "quit 1" \
      -- "$BUILD_DIR/NeuralAmpModeler-Tests" --duration=true 2>&1 | tail -n 120 || true
    exit "$rc"
  fi
fi
