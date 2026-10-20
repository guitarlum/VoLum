#!/usr/bin/env bash
# Provisions the box (Ubuntu under WSL2, KVM) as the VoLum Android build and
# emulator host. Idempotent, so it is safe to rerun after a partial failure.
#
#   bash provision.sh            # toolchain, SDK packages, AVD, bare repo, locks
#   bash provision.sh --smoke    # also boot the AVD headless and write smoke.png
#
# Never touches ~/localai-platform.
set -euo pipefail

ROOT="$HOME/volum-android"
SDK="$HOME/android-sdk"
CMDLINE_ZIP="commandlinetools-linux-16111833_latest.zip"
AVD_NAME="volum_phone"
SYS_IMAGE="system-images;android-35;google_apis;x86_64"
# cmdline-tools 23 replaced sdkmanager with the `android sdk` CLI, which names
# packages with '/' separators.
PACKAGES=(
  "platform-tools"
  "emulator"
  "platforms/android-35"
  "build-tools/35.0.0"
  "cmake/3.22.1"
  "system-images/android-35/google_apis/x86_64"
)
NDK_MAJOR="29"

log() { printf '[provision %s] %s\n' "$(date +%H:%M:%S)" "$*"; }

install_apt() {
  local want=(unzip curl git libpulse0 libnss3 libxcomposite1 libxcursor1 libxi6 libxtst6 libxdamage1 libxkbfile1 libgl1 libegl1 libc++1)
  local java_pkg=""
  for p in openjdk-17-jdk-headless openjdk-21-jdk-headless; do
    if apt-cache show "$p" >/dev/null 2>&1; then java_pkg="$p"; break; fi
  done
  [ -n "$java_pkg" ] || { log "no openjdk 17/21 package found"; exit 1; }
  local missing=()
  for p in "$java_pkg" "${want[@]}"; do
    dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p")
  done
  if [ "${#missing[@]}" -gt 0 ]; then
    log "apt install ${missing[*]}"
    sudo apt-get update -qq
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${missing[@]}" || {
      log "retrying without optional libs that do not exist on this release"
      for p in "${missing[@]}"; do
        sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "$p" || log "skip $p"
      done
    }
  fi
  java -version 2>&1 | head -n 1
}

install_kvm_access() {
  if ! id -nG "$USER" | grep -qw kvm; then
    log "adding $USER to kvm group"
    sudo usermod -aG kvm "$USER"
  fi
  [ -w /dev/kvm ] || sudo chmod 666 /dev/kvm
}

install_cmdline_tools() {
  if [ ! -x "$SDK/cmdline-tools/latest/bin/sdkmanager" ]; then
    log "downloading $CMDLINE_ZIP"
    mkdir -p "$SDK/cmdline-tools"
    local tmp
    tmp="$(mktemp -d)"
    curl -fsSL "https://dl.google.com/android/repository/$CMDLINE_ZIP" -o "$tmp/c.zip"
    unzip -q "$tmp/c.zip" -d "$tmp"
    rm -rf "$SDK/cmdline-tools/latest"
    mv "$tmp/cmdline-tools" "$SDK/cmdline-tools/latest"
    rm -rf "$tmp"
  fi
}

asdk() { "$SDK/cmdline-tools/latest/bin/android" --sdk="$SDK" sdk "$@"; }

install_packages() {
  local ndk
  ndk="$(asdk list --all --all-versions 2>/dev/null | grep -oE "^  ndk/${NDK_MAJOR}\.[0-9.]+" | tr -d ' ' | sort -V | tail -n 1 || true)"
  [ -n "$ndk" ] || { log "no ndk/${NDK_MAJOR}.x in android sdk list"; exit 1; }
  for p in "${PACKAGES[@]}" "$ndk"; do
    if [ -e "$SDK/$p/package.xml" ] || [ -e "$SDK/$p/source.properties" ]; then continue; fi
    log "installing $p"
    asdk install "$p" < /dev/null > "$ROOT/logs/sdk-install.log" 2>&1 || { log "install $p failed"; tail -n 20 "$ROOT/logs/sdk-install.log"; exit 1; }
  done
  echo "${ndk#ndk/}" > "$ROOT/ndk-version.txt"
}

create_avd() {
  local avdmanager="$SDK/cmdline-tools/latest/bin/avdmanager"
  if ! "$avdmanager" list avd 2>/dev/null | grep -q "Name: $AVD_NAME"; then
    log "creating AVD $AVD_NAME"
    echo no | "$avdmanager" create avd -n "$AVD_NAME" -k "$SYS_IMAGE" -d pixel_7 --force >/dev/null
  fi
  local cfg="$HOME/.android/avd/$AVD_NAME.avd/config.ini"
  set_cfg() {
    if grep -q "^$1=" "$cfg"; then sed -i "s|^$1=.*|$1=$2|" "$cfg"; else echo "$1=$2" >> "$cfg"; fi
  }
  set_cfg hw.ramSize 2048
  set_cfg disk.dataPartition.size 6G
  set_cfg hw.keyboard yes
  set_cfg hw.gpu.enabled yes
  set_cfg hw.gpu.mode swiftshader_indirect
  set_cfg hw.initialOrientation landscape
  set_cfg showDeviceFrame no
}

create_layout() {
  mkdir -p "$ROOT/wt" "$ROOT/locks" "$ROOT/out" "$ROOT/logs"
  touch "$ROOT/locks/emu.lock" "$ROOT/locks/build1.lock" "$ROOT/locks/build2.lock"
  if [ ! -d "$ROOT/VoLum.git" ]; then
    log "creating bare repo $ROOT/VoLum.git"
    git init -q --bare "$ROOT/VoLum.git"
  fi
  cat > "$ROOT/env.sh" <<EOF
export ANDROID_HOME="$SDK"
export ANDROID_SDK_ROOT="$SDK"
export PATH="$SDK/platform-tools:$SDK/emulator:$SDK/cmdline-tools/latest/bin:\$PATH"
export GRADLE_OPTS="-Xmx2g -Dorg.gradle.daemon.idletimeout=600000"
EOF
}

smoke() {
  # shellcheck disable=SC1091
  source "$ROOT/env.sh"
  log "booting $AVD_NAME headless"
  # The adb server and the emulator outlive this script; neither may inherit the lock fd.
  adb start-server >/dev/null
  exec 9>"$ROOT/locks/emu.lock"
  flock -w 600 9
  emulator -avd "$AVD_NAME" -no-window -no-audio -no-boot-anim -no-snapshot -gpu swiftshader_indirect -accel on -port 5554 \
    > "$ROOT/logs/emu-smoke.log" 2>&1 9>&- &
  local pid=$!
  adb -s emulator-5554 wait-for-device
  local t=0
  until [ "$(adb -s emulator-5554 shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]; do
    sleep 3; t=$((t + 3))
    [ "$t" -lt 300 ] || { log "boot timeout"; kill "$pid"; exit 1; }
  done
  log "booted in ${t}s"
  adb -s emulator-5554 shell settings put system user_rotation 1 || true
  sleep 3
  adb -s emulator-5554 exec-out screencap -p > "$ROOT/out/smoke.png"
  log "smoke.png $(stat -c %s "$ROOT/out/smoke.png") bytes"
  adb -s emulator-5554 emu kill >/dev/null 2>&1 || kill "$pid"
  wait "$pid" 2>/dev/null || true
}

mkdir -p "$ROOT"
install_apt
install_kvm_access
install_cmdline_tools
install_packages
create_avd
create_layout
log "done: $(cat "$ROOT/ndk-version.txt"), AVD $AVD_NAME, repo $ROOT/VoLum.git"
if [ "${1:-}" = "--smoke" ]; then smoke; fi
