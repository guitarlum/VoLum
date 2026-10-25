#!/bin/bash
# MIDI scenario inside the guest. The host has mounted the payload on the
# VOLUM_OUT disk and started this script with its stdout teed to the serial
# console. --require-midi stays fail-closed: a missing sequencer is a failure.
set -u
set -o pipefail

log() { printf '%s\n' "$*"; }

log "VOLUM_MIDI_VM_GUEST kernel=$(uname -r)"
modprobe snd-seq 2>/dev/null || true
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
  if [[ -e /dev/snd/seq ]]; then
    break
  fi
  sleep 0.1
done
if [[ -e /dev/snd/seq ]]; then
  chmod a+rw /dev/snd/seq || true
fi
log "VOLUM_MIDI_VM_SEQ"
ls -l /dev/snd/seq || log "VOLUM_MIDI_VM_SEQ missing"
find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*' || true

PAYLOAD="$(cd "$(dirname "$0")" && pwd)"
APP="$PAYLOAD/build-linux/standalone/VoLum"
E2E="$PAYLOAD/NeuralAmpModeler/scripts/e2e-standalone-linux.sh"
OUT="${VOLUM_MIDI_VM_OUT:-/mnt/out/e2e}"
mkdir -p "$OUT"

if [[ ! -x "$APP" ]]; then
  log "VOLUM_MIDI_VM_GUEST_FAIL no app at $APP"
  exit 1
fi
if [[ ! -f "$E2E" ]]; then
  log "VOLUM_MIDI_VM_GUEST_FAIL no e2e script at $E2E"
  exit 1
fi
if ldd "$APP" | grep -q 'not found'; then
  log "VOLUM_MIDI_VM_GUEST_FAIL missing libraries"
  ldd "$APP" | grep 'not found' || true
  exit 1
fi
log "VOLUM_MIDI_VM_LDD ok"

# No GPU in the guest. The scenario checks MIDI log lines, not pixels.
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
export HOME="${HOME:-/root}"
mkdir -p /tmp/volum-runtime
chmod 700 /tmp/volum-runtime
export XDG_RUNTIME_DIR=/tmp/volum-runtime

bash "$E2E" --app "$APP" --out "$OUT" --scenario midi --require-midi
rc=$?
log "VOLUM_MIDI_VM_E2E rc=$rc"
exit "$rc"
