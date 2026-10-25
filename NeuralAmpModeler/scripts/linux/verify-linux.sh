#!/usr/bin/env bash
# Linux half of the support-loop predicate. Run from the repo root on the box.
#
#   bash NeuralAmpModeler/scripts/linux/verify-linux.sh
#   bash NeuralAmpModeler/scripts/linux/verify-linux.sh --final
#
# Runs the GCC suite, then the Clang ASan/UBSan suite, then each later gate
# that exists. --final turns a missing gate into a failure. Stops at the first
# failure. Prints "linux gates: MET" only when every required step passed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

FINAL=0
for arg in "$@"; do
  case "$arg" in
    --final) FINAL=1 ;;
    *)
      echo "Usage: $0 [--final]" >&2
      exit 1
      ;;
  esac
done

cd "$REPO_ROOT"

bash NeuralAmpModeler/scripts/run-tests-linux.sh
bash NeuralAmpModeler/scripts/run-tests-linux.sh --clang --sanitize

gates=(
  NeuralAmpModeler/scripts/validate-vst3-linux.sh
  NeuralAmpModeler/scripts/smoke-reaper-linux.sh
  NeuralAmpModeler/scripts/linux/smoke-hosts-sanitize.sh
  NeuralAmpModeler/scripts/smoke-standalone-audio-linux.sh
  NeuralAmpModeler/scripts/e2e-standalone-linux.sh
  NeuralAmpModeler/scripts/linux/real-device-gate.sh
  NeuralAmpModeler/scripts/linux/ui-scenes.sh
)

# The Babyface route needs the owner's TotalMix loopback and Windows defaults, so
# the repeatable run nulls the same app-to-Pulse path on a private null sink.
# VOLUM_BABYFACE=1 runs it on the real device instead (ticket 12 report).
gate_args() {
  if [[ "$1" == */real-device-gate.sh && "${VOLUM_BABYFACE:-0}" != "1" ]]; then
    echo "--private"
  fi
}

for gate in "${gates[@]}"; do
  if [[ -f "$REPO_ROOT/$gate" ]]; then
    args="$(gate_args "$gate")"
    echo "linux gate: $gate $args"
    # shellcheck disable=SC2086
    bash "$REPO_ROOT/$gate" $args
  elif [[ "$FINAL" == "1" ]]; then
    echo "missing gate: $gate" >&2
    exit 1
  else
    echo "skip missing gate: $gate"
  fi
done

echo "linux gates: MET"
