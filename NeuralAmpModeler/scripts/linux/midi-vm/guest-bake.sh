#!/bin/bash
# First boots of the Ubuntu cloud image. Installs the generic kernel (the cloud
# image often boots linux-kvm, which has no ALSA sequencer) and the packages the
# MIDI scenario needs, then powers off so the host can cache the disk.
#
# A second boot is this same script: the non-generic path reboots once, and the
# marker /var/lib/volum-midi-vm-kernel-switched stops a reboot loop.
set -u
set -o pipefail

log() { printf '%s\n' "$*" | tee -a /dev/ttyS0 || printf '%s\n' "$*"; }

fail() {
  trap - ERR
  log "VOLUM_MIDI_VM_BAKE_FAIL $*"
  sync
  systemctl poweroff || true
  exit 1
}

trap 'fail "line $LINENO"' ERR
set -e

export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a
printf '%s\n' 'Acquire::ForceIPv4 "true";' 'Dpkg::Options { "--force-confdef"; "--force-confold"; };' \
  > /etc/apt/apt.conf.d/99volum-midi-vm

retry() {
  local i
  for i in 1 2 3 4 5; do
    if "$@"; then
      return 0
    fi
    log "retry $i failed: $*"
    sleep $((i * 5))
  done
  return 1
}

if [[ -f /var/lib/volum-midi-vm-ready ]]; then
  log "VOLUM_MIDI_VM_BAKE_SKIP already ready kernel=$(uname -r)"
  exit 0
fi

log "VOLUM_MIDI_VM_BAKE_BOOT kernel=$(uname -r)"

if [[ "$(uname -r)" != *-generic ]]; then
  if [[ -f /var/lib/volum-midi-vm-kernel-switched ]]; then
    fail "still not the generic kernel after reboot ($(uname -r))"
  fi
  touch /var/lib/volum-midi-vm-kernel-switched
  retry apt-get update
  retry apt-get install -y linux-image-generic
  mapfile -t others < <(dpkg-query -W -f '${Package}\n' 'linux-image-[0-9]*' | grep -v -- '-generic' || true)
  if [[ ${#others[@]} -gt 0 ]]; then
    apt-get remove -y --purge "${others[@]}" || true
  fi
  apt-get remove -y --purge linux-image-kvm linux-kvm linux-virtual linux-image-virtual 2>/dev/null || true
  compgen -G '/boot/vmlinuz-*-generic' >/dev/null || fail "linux-image-generic did not install a kernel"
  update-grub
  log "VOLUM_MIDI_VM_BAKE_REBOOT to generic"
  grep -E '^menuentry|^submenu' /boot/grub/grub.cfg | head -n 20 | tee -a /dev/ttyS0 || true
  sync
  systemctl reboot || reboot
  exit 0
fi

if ! find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*' | grep -q .; then
  retry apt-get update
  retry apt-get install -y "linux-modules-$(uname -r)" || true
  if ! find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*' | grep -q .; then
    retry apt-get install -y "linux-modules-extra-$(uname -r)"
  fi
fi
find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*' | grep -q . || fail "generic kernel has no snd-seq.ko"

printf '%s\n' 'jackd2 jackd/tweak_rt_limits boolean true' | debconf-set-selections
retry apt-get update
retry apt-get install -y --no-install-recommends \
  alsa-utils \
  libasound2 \
  libasound2-dev \
  gcc \
  libc6-dev \
  make \
  xvfb \
  xdotool \
  x11-utils \
  x11-apps \
  imagemagick \
  python3 \
  tesseract-ocr \
  jackd2 \
  libjack-jackd2-dev \
  libgl1 \
  libgl1-mesa-dri \
  libgtk-3-0 \
  libx11-6 \
  libxext6 \
  libxcursor1 \
  libxrandr2 \
  libxi6 \
  libfontconfig1 \
  libfreetype6 \
  libglib2.0-0 \
  libatomic1 \
  sudo \
  kmod \
  udev \
  util-linux \
  ca-certificates

if [[ ! -x /usr/bin/cc ]]; then
  ln -sf gcc /usr/bin/cc
fi

modprobe snd-seq
for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
  if [[ -e /dev/snd/seq ]]; then
    break
  fi
  sleep 0.1
done
[[ -e /dev/snd/seq ]] || fail "modprobe snd-seq did not create /dev/snd/seq"
chmod a+rw /dev/snd/seq

log "VOLUM_MIDI_VM_READY kernel=$(uname -r)"
log "$(ls -l /dev/snd/seq)"
log "$(find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*')"
{
  echo "kernel=$(uname -r)"
  ls -l /dev/snd/seq
  find "/lib/modules/$(uname -r)" -name 'snd-seq.ko*'
} > /var/lib/volum-midi-vm-proof.txt
touch /var/lib/volum-midi-vm-ready

apt-get clean
rm -rf /var/lib/apt/lists/*
sync
sleep 2
log "VOLUM_MIDI_VM_BAKE_POWEROFF"
systemctl poweroff || poweroff
