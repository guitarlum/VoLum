#!/usr/bin/env bash
# Boot an Ubuntu generic-kernel guest and run the Linux MIDI scenario.
#
#   bash run.sh bake    # download the cloud image, install the generic kernel, cache the disk
#   bash run.sh test    # boot that disk and run e2e-standalone-linux.sh --scenario midi --require-midi
#
# KVM when /dev/kvm is usable, TCG otherwise. The guest image is independent of
# the VoLum binary: bake once, then each test copies the just-built standalone
# onto a second disk.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
MODE="${1:-}"
IMAGE="${VOLUM_MIDI_VM_IMAGE:-$ROOT/midi-vm-cache/volum-midi-vm.qcow2}"
WORK="${VOLUM_MIDI_VM_WORK:-${RUNNER_TEMP:-/tmp}/volum-midi-vm}"
BASE_URL="https://cloud-images.ubuntu.com/jammy/current"
IMG_NAME="jammy-server-cloudimg-amd64.img"
ACCEL=tcg
CPU=qemu64
OUTIMG=""

usage() {
  echo "usage: $0 bake|test" >&2
  exit 2
}

[[ "$MODE" == bake || "$MODE" == test ]] || usage

log() { printf '%s\n' "$*"; }

prepare_kvm() {
  ACCEL=tcg
  CPU=qemu64
  if [[ -e /dev/kvm ]]; then
    printf '%s\n' 'KERNEL=="kvm", GROUP="kvm", MODE="0666", OPTIONS+="static_node=kvm"' \
      | sudo tee /etc/udev/rules.d/99-kvm4all.rules >/dev/null
    sudo udevadm control --reload-rules || true
    sudo udevadm trigger --name-match=kvm || true
    sudo chmod 666 /dev/kvm || true
  fi
  if [[ -r /dev/kvm && -w /dev/kvm ]]; then
    ACCEL=kvm
    CPU=host
  fi
  log "VOLUM_MIDI_VM_ACCEL=$ACCEL cpu=$CPU"
  if [[ "$ACCEL" == tcg ]]; then
    log "VOLUM_MIDI_VM_ACCEL warning: /dev/kvm is not usable; booting with TCG"
    ls -l /dev/kvm || true
  fi
}

install_host_packages() {
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends qemu-system-x86 qemu-utils cloud-image-utils e2fsprogs
}

download_base() {
  local dest="$1"
  local dir hash sums
  dir="$(dirname "$dest")"
  sums="$dir/SHA256SUMS"
  mkdir -p "$dir"
  if [[ -f "$dest" ]]; then
    return 0
  fi
  log "downloading $BASE_URL/$IMG_NAME"
  curl -fL --retry 5 --retry-delay 2 -o "$dest.partial" "$BASE_URL/$IMG_NAME"
  curl -fL --retry 5 --retry-delay 2 -o "$sums" "$BASE_URL/SHA256SUMS"
  hash="$(awk -v name="$IMG_NAME" '$NF == name || $NF == "*" name { print $1; exit }' "$sums")"
  [[ -n "$hash" ]] || { log "no checksum for $IMG_NAME"; exit 1; }
  mv "$dest.partial" "$dest"
  echo "$hash  $IMG_NAME" | (cd "$dir" && sha256sum -c -)
}

write_bake_seed() {
  local user_data="$1" meta="$2" seed="$3"
  {
    echo '#cloud-config'
    echo 'write_files:'
    echo '  - path: /var/lib/cloud/scripts/per-boot/volum-midi-vm-bake'
    echo '    permissions: "0755"'
    echo '    content: |'
    sed 's/^/      /' "$SCRIPT_DIR/guest-bake.sh"
  } > "$user_data"
  printf '%s\n' 'instance-id: volum-midi-bake' 'local-hostname: volum-midi' > "$meta"
  cloud-localds "$seed" "$user_data" "$meta"
}

write_test_seed() {
  local user_data="$1" meta="$2" seed="$3"
  cat > "$user_data" <<'EOF'
#cloud-config
write_files:
  - path: /usr/local/sbin/volum-midi-vm-run.sh
    permissions: "0755"
    content: |
      #!/bin/bash
      set -u
      log() { printf '%s\n' "$*" | tee -a /dev/ttyS0 || printf '%s\n' "$*"; }
      log "VOLUM_MIDI_VM_RUN_BOOT kernel=$(uname -r)"
      modprobe snd-seq 2>/dev/null || true
      found=0
      for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30; do
        if blkid -L VOLUM_OUT >/dev/null 2>&1; then
          found=1
          break
        fi
        sleep 0.2
      done
      if [[ "$found" != 1 ]]; then
        log "VOLUM_MIDI_VM_GUEST_FAIL VOLUM_OUT disk not found"
        blkid || true
        lsblk || true
        sync
        systemctl poweroff || poweroff || true
        exit 1
      fi
      mkdir -p /mnt/out
      mount "$(blkid -L VOLUM_OUT)" /mnt/out
      rc=1
      if [[ -f /mnt/out/payload/guest-run.sh ]]; then
        bash /mnt/out/payload/guest-run.sh 2>&1 | tee /mnt/out/guest.log
        rc=${PIPESTATUS[0]}
      else
        log "VOLUM_MIDI_VM_GUEST_FAIL guest-run.sh missing"
        ls -la /mnt/out /mnt/out/payload || true
      fi
      printf '%s\n' "$rc" > /mnt/out/result
      chmod -R a+rX /mnt/out || true
      sync
      log "VOLUM_MIDI_VM_GUEST_DONE rc=$rc"
      sleep 1
      systemctl poweroff || poweroff || true
      exit "$rc"
runcmd:
  - [bash, /usr/local/sbin/volum-midi-vm-run.sh]
EOF
  printf '%s\n' 'instance-id: volum-midi-run' 'local-hostname: volum-midi' > "$meta"
  cloud-localds "$seed" "$user_data" "$meta"
}

boot_qemu() {
  local disk="$1" seed="$2" serial="$3" secs="$4" no_reboot="$5"
  local -a cmd=(
    qemu-system-x86_64
    -machine "type=pc,accel=${ACCEL}"
    -cpu "$CPU"
    -m 3072
    -smp 2
    -display none
    -serial stdio
    -monitor none
    -vga none
    -device virtio-net-pci,netdev=net0
    -netdev user,id=net0
    -device virtio-rng-pci
    -drive "file=${disk},format=qcow2,if=virtio,cache=writeback"
    -drive "file=${seed},format=raw,if=ide,media=cdrom,readonly=on,index=0"
  )
  if [[ "$no_reboot" == 1 ]]; then
    cmd+=(-no-reboot)
  fi
  if [[ -n "$OUTIMG" && -f "$OUTIMG" ]]; then
    cmd+=(-drive "file=${OUTIMG},format=raw,if=virtio,cache=writeback")
  fi
  local rc=0
  log "VOLUM_MIDI_VM_QEMU seconds=$secs reboot_ok=$([[ "$no_reboot" == 1 ]] && echo no || echo yes)"
  set +e
  timeout --foreground --kill-after=30s "$secs" "${cmd[@]}" 2>&1 | stdbuf -oL tee "$serial"
  rc=${PIPESTATUS[0]}
  set -e
  log "VOLUM_MIDI_VM_QEMU_EXIT rc=$rc"
  return 0
}

do_bake() {
  mkdir -p "$WORK" "$(dirname "$IMAGE")"
  local base="$WORK/$IMG_NAME"
  local overlay="$WORK/bake-overlay.qcow2"
  local seed="$WORK/bake-seed.iso"
  local serial="$WORK/bake-serial.log"
  download_base "$base"
  rm -f "$overlay" "$seed" "$serial"
  qemu-img create -f qcow2 -b "$base" -F qcow2 "$overlay" 12G
  write_bake_seed "$WORK/bake-user-data" "$WORK/bake-meta-data" "$seed"
  OUTIMG=""
  boot_qemu "$overlay" "$seed" "$serial" 40m 0
  if grep -q 'VOLUM_MIDI_VM_BAKE_FAIL' "$serial"; then
    log "bake failed"
    tail -n 80 "$serial" >&2 || true
    exit 1
  fi
  if ! grep -E -q 'VOLUM_MIDI_VM_READY kernel=.*-generic' "$serial"; then
    log "bake did not reach a generic kernel with the sequencer"
    tail -n 120 "$serial" >&2 || true
    exit 1
  fi
  if ! grep -F -q '/dev/snd/seq' "$serial"; then
    log "bake log has no /dev/snd/seq"
    exit 1
  fi
  if ! grep -F -q 'snd-seq.ko' "$serial"; then
    log "bake log has no snd-seq.ko"
    exit 1
  fi
  local partial="$IMAGE.partial"
  rm -f "$partial"
  qemu-img convert -c -O qcow2 "$overlay" "$partial"
  mv "$partial" "$IMAGE"
  qemu-img info "$IMAGE"
  log "VOLUM_MIDI_VM_BAKED $IMAGE"
  rm -f "$overlay"
}

stage_payload() {
  local app="$ROOT/build-linux/standalone/VoLum"
  [[ -x "$app" ]] || { log "no standalone at $app"; exit 1; }
  [[ -d "$ROOT/build-linux/standalone/VoLumRigs" ]] || { log "no VoLumRigs beside the standalone"; exit 1; }
  [[ -d "$ROOT/build-linux/standalone/resources" ]] || { log "no standalone resources"; exit 1; }
  [[ -f "$ROOT/NeuralAmpModeler/scripts/linux/smoke/volum-jack-probe.c" ]] || { log "no jack probe source"; exit 1; }
  OUTIMG="$WORK/out.img"
  rm -f "$OUTIMG"
  truncate -s 2G "$OUTIMG"
  mkfs.ext4 -F -L VOLUM_OUT "$OUTIMG" >/dev/null
  local mnt="$WORK/mnt"
  mkdir -p "$mnt"
  sudo mount -o loop "$OUTIMG" "$mnt"
  sudo mkdir -p "$mnt/payload/NeuralAmpModeler/scripts/linux/smoke" "$mnt/payload/build-linux/standalone"
  sudo cp -a "$SCRIPT_DIR/guest-run.sh" "$mnt/payload/guest-run.sh"
  sudo cp -a "$ROOT/NeuralAmpModeler/scripts/e2e-standalone-linux.sh" "$mnt/payload/NeuralAmpModeler/scripts/"
  sudo cp -a "$ROOT/NeuralAmpModeler/scripts/linux/smoke/volum-jack-probe.c" "$mnt/payload/NeuralAmpModeler/scripts/linux/smoke/"
  sudo cp -a "$app" "$mnt/payload/build-linux/standalone/VoLum"
  sudo cp -a "$ROOT/build-linux/standalone/resources" "$mnt/payload/build-linux/standalone/resources"
  sudo cp -a "$ROOT/build-linux/standalone/VoLumRigs" "$mnt/payload/build-linux/standalone/VoLumRigs"
  sudo chmod 755 "$mnt/payload/guest-run.sh" "$mnt/payload/build-linux/standalone/VoLum" \
    "$mnt/payload/NeuralAmpModeler/scripts/e2e-standalone-linux.sh"
  sync
  sudo umount "$mnt"
}

do_test() {
  local art="$WORK/artifact"
  [[ -f "$IMAGE" ]] || { log "no guest image at $IMAGE (bake first)"; exit 1; }
  mkdir -p "$WORK" "$art"
  local overlay="$WORK/test-overlay.qcow2"
  local seed="$WORK/test-seed.iso"
  local serial="$WORK/test-serial.log"
  rm -f "$overlay" "$seed" "$serial"
  qemu-img create -f qcow2 -b "$IMAGE" -F qcow2 "$overlay"
  stage_payload
  write_test_seed "$WORK/test-user-data" "$WORK/test-meta-data" "$seed"
  boot_qemu "$overlay" "$seed" "$serial" 20m 1
  local mnt="$WORK/mnt"
  mkdir -p "$mnt"
  sudo mount -o loop "$OUTIMG" "$mnt"
  local rc=1
  if [[ -f "$mnt/result" ]]; then
    rc="$(tr -d '[:space:]' < "$mnt/result")"
  else
    log "guest wrote no result file"
  fi
  if [[ -f "$mnt/guest.log" ]]; then
    log "----- guest log -----"
    cat "$mnt/guest.log"
    log "----- end guest log -----"
    cp -a "$mnt/guest.log" "$art/guest.log"
  fi
  if [[ -d "$mnt/e2e" ]]; then
    cp -a "$mnt/e2e" "$art/e2e"
  fi
  cp -a "$serial" "$art/serial.log" || true
  sudo umount "$mnt"
  if [[ ! -f "$art/guest.log" ]]; then
    log "no guest log"
    tail -n 120 "$serial" >&2 || true
    exit 1
  fi
  if grep -q 'SKIP  \[midi\]' "$art/guest.log"; then
    log "midi scenario skipped; --require-midi must fail"
    exit 1
  fi
  if ! grep -F -q '/dev/snd/seq' "$art/guest.log"; then
    log "guest log does not show /dev/snd/seq"
    exit 1
  fi
  if ! grep -q 'ALL PASS' "$art/guest.log"; then
    log "midi scenario did not pass (result=$rc)"
    exit 1
  fi
  if [[ "$rc" != 0 ]]; then
    log "guest result was $rc"
    exit 1
  fi
  log "VOLUM_MIDI_VM_PASS"
}

cleanup() {
  if [[ -d "$WORK/mnt" ]] && mountpoint -q "$WORK/mnt"; then
    sudo umount "$WORK/mnt" || true
  fi
}
trap cleanup EXIT

df -h
prepare_kvm
install_host_packages
case "$MODE" in
  bake) do_bake ;;
  test) do_test ;;
esac
