#!/usr/bin/env bash
# Measure line/region coverage of the VoLum doctest suite with Clang
# source-based coverage on macOS, scoped to VoLum-authored sources +
# AudioDSPTools (vendored deps and the test files themselves are excluded).
#
# Outputs:
#   - a text summary printed to stdout (and saved to coverage-html/summary.txt)
#   - an HTML report under NeuralAmpModeler/build-tests-mac/coverage-html/
#
# Uses a dedicated build dir so it never collides with the normal or
# sanitizer test builds.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/.." && pwd)"

BUILD_DIR="$PROJECT_DIR/build-tests-mac/tests-coverage"
HTML_DIR="$PROJECT_DIR/build-tests-mac/coverage-html"
BINARY="$BUILD_DIR/NeuralAmpModeler-Tests"
PROFRAW="$BUILD_DIR/volum.profraw"
PROFDATA="$BUILD_DIR/volum.profdata"

# Keep only VoLum-owned product code (NeuralAmpModeler/VoLum*, ToneStack) and
# AudioDSPTools DSP in the report. Drop vendored deps, the test sources, and
# system/toolchain headers so the percentage reflects code we actually own.
IGNORE_REGEX='(NeuralAmpModelerCore|/eigen/|iPlug2|tests/third_party|tests/test_|tests/golden_helpers|/usr/|/Applications/Xcode|/Library/Developer)'

echo "==> Configuring coverage build ($BUILD_DIR)"
cmake -S "$PROJECT_DIR/tests" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVOLUM_ENABLE_COVERAGE=1

echo "==> Building NeuralAmpModeler-Tests"
cmake --build "$BUILD_DIR" --config Debug -j"$(sysctl -n hw.ncpu)"

echo "==> Running suite (instrumented)"
rm -f "$PROFRAW" "$PROFDATA"
# Run from the repo root so golden / NAM-load tests resolve rigs/ correctly.
cd "$REPO_ROOT"
# Coverage is a measurement tool, not a test gate: the blocking unit + ASan/UBSan
# jobs already gate correctness. The instrumented binary is built at -O0 without
# fast-math, so the bit-exact golden-DSP comparisons can differ from the
# optimized build by a few ULPs and report a "failure"; that must not abort the
# coverage run (which would also skip the report below). doctest still executes
# every case and writes the profile, so swallow its exit code and keep going.
TEST_EXIT=0
LLVM_PROFILE_FILE="$PROFRAW" "$BINARY" || TEST_EXIT=$?
if [[ "$TEST_EXIT" -ne 0 ]]; then
  echo "==> NOTE: doctest returned $TEST_EXIT under the -O0 coverage build (expected for"
  echo "    bit-exact golden comparisons); coverage was still measured. See the blocking"
  echo "    unit + sanitizer jobs for the authoritative pass/fail."
fi

echo "==> Merging profile data"
xcrun llvm-profdata merge -sparse "$PROFRAW" -o "$PROFDATA"

echo "==> Generating HTML report ($HTML_DIR)"
rm -rf "$HTML_DIR"
mkdir -p "$HTML_DIR"
xcrun llvm-cov show "$BINARY" \
  -instr-profile="$PROFDATA" \
  -ignore-filename-regex="$IGNORE_REGEX" \
  -format=html \
  -output-dir="$HTML_DIR"

echo "==> Coverage summary (VoLum + AudioDSPTools)"
xcrun llvm-cov report "$BINARY" \
  -instr-profile="$PROFDATA" \
  -ignore-filename-regex="$IGNORE_REGEX" \
  | tee "$HTML_DIR/summary.txt"

echo "==> HTML report: $HTML_DIR/index.html"
