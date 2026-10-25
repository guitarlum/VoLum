#!/usr/bin/env bash
# Runs inside one portability container. The host mounts the tarball and
# verify-package.sh at /in and a writable log directory at /out.
# CASE is ubuntu2204, ubuntu2404, fedora, or nogtk.
set -euo pipefail

CASE="${CASE:?CASE is required}"
export DEBIAN_FRONTEND=noninteractive

log() { printf '[%s] %s\n' "$CASE" "$*"; }

apt_install() {
  apt-get update -qq
  apt-get install -y --no-install-recommends "$@"
}

case "$CASE" in
  ubuntu2204)
    apt_install \
      libgtk-3-0 libasound2 libgl1 libgl1-mesa-dri libx11-6 libxi6 \
      libfontconfig1 libfreetype6 libatomic1 libstdc++6 \
      xvfb xauth xdotool x11-utils patchelf binutils ca-certificates
    MODE=full
    RED_NEEDED=1
    RED_UNINSTALL=1
    ;;
  ubuntu2404)
    apt_install \
      libgtk-3-0t64 libasound2t64 libgl1 libgl1-mesa-dri libx11-6 libxi6 \
      libfontconfig1 libfreetype6 libatomic1 libstdc++6 \
      xvfb xauth xdotool x11-utils binutils ca-certificates
    MODE=full
    RED_NEEDED=0
    RED_UNINSTALL=0
    ;;
  fedora)
    dnf install -y --setopt=install_weak_deps=False \
      gtk3 alsa-lib mesa-libGL mesa-dri-drivers libX11 libXi fontconfig freetype \
      libatomic libstdc++ \
      xorg-x11-server-Xvfb xdotool xwininfo \
      patchelf binutils ca-certificates
    MODE=full
    RED_NEEDED=0
    RED_UNINSTALL=0
    ;;
  nogtk)
    apt_install \
      libasound2t64 libgl1 libgl1-mesa-dri libx11-6 libxi6 \
      libfontconfig1 libfreetype6 libatomic1 libstdc++6 \
      binutils ca-certificates
    MODE=nogtk
    RED_NEEDED=0
    RED_UNINSTALL=0
    ;;
  *)
    echo "unknown CASE=$CASE" >&2
    exit 2
    ;;
esac

mkdir -p /tmp/pkg /out
tar -xzf /in/pkg.tar.gz -C /tmp/pkg
TOP="$(find /tmp/pkg -mindepth 1 -maxdepth 1 -type d | head -n 1)"
SO="$TOP/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
APP="$TOP/VoLum"
[[ -f "$SO" && -f "$APP" && -x "$TOP/install.sh" ]] || { echo "bad tarball layout" >&2; exit 1; }
cp "$TOP/install.sh" /out/install.sh.orig

log "NEEDED"
bash /in/verify-package.sh --needed "$SO" "$APP" | tee /out/needed.txt

if [[ "$RED_NEEDED" -eq 1 ]]; then
  log "red proof: GDK NEEDED on the VST3"
  cp "$SO" /tmp/VoLum-gdk.so
  patchelf --add-needed libgdk-3.so.0 /tmp/VoLum-gdk.so
  set +e
  bash /in/verify-package.sh --needed /tmp/VoLum-gdk.so "$APP" > /out/red-needed.log 2>&1
  red_rc=$?
  set -e
  cat /out/red-needed.log
  if [[ "$red_rc" -eq 0 ]]; then
    echo "RED PROOF FAILED: GDK on the VST3 was accepted" >&2
    exit 1
  fi
  grep -q 'unexpected NEEDED on VST3: libgdk-3.so.0' /out/red-needed.log
  bash /in/verify-package.sh --needed "$SO" "$APP" >/dev/null
  cmp -s "$SO" "$TOP/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
  echo "restored: VST3 NEEDED check passed on the unmutated plug-in"
fi

if [[ "$CASE" == "fedora" ]]; then
  log "ldd"
  bash /in/verify-package.sh --ldd "$SO" "$APP" | tee /out/ldd.txt
  log "red proof: unexpected ldd library"
  cp "$APP" /tmp/VoLum-curl
  chmod +x /tmp/VoLum-curl
  patchelf --add-needed libcurl.so.4 /tmp/VoLum-curl
  set +e
  bash /in/verify-package.sh --ldd "$SO" /tmp/VoLum-curl > /out/red-ldd.log 2>&1
  red_rc=$?
  set -e
  cat /out/red-ldd.log
  if [[ "$red_rc" -eq 0 ]]; then
    echo "RED PROOF FAILED: unexpected library was accepted" >&2
    exit 1
  fi
  grep -q 'ldd: unexpected library libcurl.so.4' /out/red-ldd.log
  bash /in/verify-package.sh --ldd "$SO" "$APP" >/dev/null
  echo "restored: ldd check passed on the unmutated standalone"
fi

setup_home() {
  local home="$1"
  mkdir -p "$home/.config" "$home/.cache" "$home/.local/share"
  printf '%s\n' 'pcm.!default { type null }' 'ctl.!default { type null }' > "$home/.asoundrc"
}

user_paths() {
  HOME_DIR="$1"
  DATA="$HOME_DIR/.local/share/VoLum"
  VST3="$HOME_DIR/.vst3/VoLum.vst3"
  BIN="$HOME_DIR/.local/bin/VoLum"
  DESKTOP="$HOME_DIR/.local/share/applications/VoLum.desktop"
  ICON="$HOME_DIR/.local/share/icons/hicolor/256x256/apps/volum.png"
}

assert_user_present() {
  user_paths "$1"
  local path
  for path in "$VST3/Contents/x86_64-linux/VoLum.so" "$BIN" "$DATA/VoLum" \
    "$DATA/resources/ttf/Roboto-Regular.ttf" \
    "$DATA/VoLumRigs/factory-presets.json" \
    "$DESKTOP" "$ICON"; do
    if [[ ! -e "$path" ]]; then
      echo "install missing: $path" >&2
      return 1
    fi
  done
  if ! find "$DATA/VoLumRigs/PrePedals" -name '*.nam' -print -quit | grep -q .; then
    echo "install missing PrePedals" >&2
    return 1
  fi
}

assert_user_gone() {
  user_paths "$1"
  local path left=0
  for path in "$VST3" "$BIN" "$DATA/VoLum" "$DATA/resources" "$DATA/VoLumRigs" "$DESKTOP" "$ICON"; do
    if [[ -e "$path" ]]; then
      echo "left behind: $path"
      left=1
    fi
  done
  [[ "$left" -eq 0 ]]
}

assert_no_system() {
  local path
  for path in /usr/lib/vst3/VoLum.vst3 /usr/local/bin/VoLum /usr/share/VoLum \
    /usr/share/applications/VoLum.desktop \
    /usr/share/icons/hicolor/256x256/apps/volum.png; do
    if [[ -e "$path" ]]; then
      echo "system path written without --system: $path" >&2
      return 1
    fi
  done
}

assert_system_present() {
  local path
  for path in /usr/lib/vst3/VoLum.vst3/Contents/x86_64-linux/VoLum.so \
    /usr/local/bin/VoLum /usr/share/VoLum/VoLum \
    /usr/share/VoLum/VoLumRigs/factory-presets.json \
    /usr/share/applications/VoLum.desktop \
    /usr/share/icons/hicolor/256x256/apps/volum.png; do
    if [[ ! -e "$path" ]]; then
      echo "system install missing: $path" >&2
      return 1
    fi
  done
}

run_install() {
  local home="$1"
  shift
  setup_home "$home"
  HOME="$home" XDG_DATA_HOME="$home/.local/share" "$TOP/install.sh" "$@"
}

assert_nogtk() {
  local log="$1" home="$2" pkg="$3"
  user_paths "$home"
  if [[ ! -f "$VST3/Contents/x86_64-linux/VoLum.so" ]]; then
    echo "VST3 was not installed"
    return 1
  fi
  if ! grep -q "$pkg" "$log"; then
    echo "missing package name ${pkg}"
    return 1
  fi
  echo "nogtk: package ${pkg}"
  echo "nogtk: VST3 installed"
}

if [[ "$MODE" == "nogtk" ]]; then
  set +e
  ldd_out="$(ldd "$APP" 2>&1)"
  set -e
  if printf '%s\n' "$ldd_out" | grep -F 'libgdk-3.so.0' | grep -q 'not found'; then
    log "libgdk-3 is absent, as this case requires"
  else
    echo "nogtk container still resolves libgdk-3" >&2
    printf '%s\n' "$ldd_out" >&2
    exit 1
  fi
  pkg="${EXPECT_GTK:-libgtk-3-0t64}"
  home=/tmp/volum-home
  run_install "$home" | tee /out/install.log
  assert_nogtk /out/install.log "$home" "$pkg" | tee /out/nogtk.txt
  assert_no_system
  user_paths "$home"
  log "validator without GTK 3"
  vhome="$(mktemp -d)"
  HOME="$vhome" XDG_DATA_HOME="$vhome/.local/share" /in/validator "$VST3" | tee /out/validator.log
  log "red proof: GTK package name omitted"
  red_top="$(mktemp -d)"
  cp -a "$TOP/." "$red_top/"
  sed -i \
    -e 's/libgtk-3-0t64/not-the-gtk-package/g' \
    -e 's/libgtk-3-0/not-the-gtk-package/g' \
    -e 's/gtk3/not-the-gtk-package/g' \
    "$red_top/install.sh"
  red_home=/tmp/volum-home-red
  HOME="$red_home" XDG_DATA_HOME="$red_home/.local/share" "$red_top/install.sh" > /out/red-nogtk.log 2>&1 || true
  cat /out/red-nogtk.log
  set +e
  assert_nogtk /out/red-nogtk.log "$red_home" "$pkg" > /out/red-nogtk-gate.txt 2>&1
  red_rc=$?
  set -e
  cat /out/red-nogtk-gate.txt
  if [[ "$red_rc" -eq 0 ]]; then
    echo "RED PROOF FAILED: gate accepted a missing GTK package name" >&2
    exit 1
  fi
  grep -q "missing package name ${pkg}" /out/red-nogtk-gate.txt
  user_paths "$red_home"
  [[ -f "$VST3/Contents/x86_64-linux/VoLum.so" ]]
  cmp -s "$TOP/install.sh" /out/install.sh.orig
  echo "restored: install.sh byte-identical; bad copy was discarded"
  HOME="$home" XDG_DATA_HOME="$home/.local/share" "$TOP/install.sh" --uninstall | tee /out/uninstall.log
  assert_user_gone "$home" | tee /out/uninstall-left.txt
  log "PASS"
  chmod -R a+rwX /out || true
  exit 0
fi

home=/tmp/volum-home
run_install "$home" | tee /out/install.log
assert_user_present "$home"
assert_no_system
user_paths "$home"

log "validator"
vhome="$(mktemp -d)"
set +e
HOME="$vhome" XDG_DATA_HOME="$vhome/.local/share" /in/validator "$VST3" > /out/validator.log 2>&1
vrc=$?
set -e
tail -20 /out/validator.log || true
if [[ "$vrc" -ne 0 ]]; then
  echo "validator exit $vrc" >&2
  exit 1
fi
log "validator exit 0"

log "standalone launch"
export HOME XDG_DATA_HOME="$home/.local/share" XDG_CONFIG_HOME="$home/.config" XDG_CACHE_HOME="$home/.cache"
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
cat > /tmp/launch-volum.sh << 'EOF'
#!/bin/bash
set -euo pipefail
app="$1"
"$app" > /tmp/app.log 2>&1 &
pid=$!
opened=0
i=0
for i in $(seq 1 90); do
  if xdotool search --name VoLum >/dev/null 2>&1; then
    opened=1
    break
  fi
  if xwininfo -root -tree 2>/dev/null | grep -q VoLum; then
    opened=1
    break
  fi
  if ! kill -0 "$pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if [[ "$opened" -ne 1 ]]; then
  echo "launch: window did not open" >&2
  xwininfo -root -tree >&2 || true
  tail -80 /tmp/app.log >&2 || true
  kill -KILL "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  exit 1
fi
echo "launch: window open after ${i}s"
kill -TERM "$pid" 2>/dev/null || true
for _ in $(seq 1 20); do
  if ! kill -0 "$pid" 2>/dev/null; then
    break
  fi
  sleep 1
done
if kill -0 "$pid" 2>/dev/null; then
  echo "launch: did not quit on SIGTERM" >&2
  kill -KILL "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
  exit 1
fi
rc=0
wait "$pid" || rc=$?
diag="${HOME}/.local/share/VoLum/volum.log"
cp /tmp/app.log /out/app.log 2>/dev/null || true
cp "$diag" /out/volum.log 2>/dev/null || true
if [[ "$rc" -ne 0 ]]; then
  echo "launch: exit $rc" >&2
  tail -40 /tmp/app.log >&2 || true
  tail -40 "$diag" >&2 || true
  exit 1
fi
if [[ ! -f "$diag" ]] || ! grep -q 'audio teardown complete' "$diag"; then
  echo "launch: missing audio teardown" >&2
  tail -40 /tmp/app.log >&2 || true
  tail -40 "$diag" >&2 || true
  exit 1
fi
echo "launch: quit 0"
EOF
chmod 755 /tmp/launch-volum.sh
if command -v xvfb-run >/dev/null 2>&1; then
  xvfb-run -a -s '-screen 0 1280x800x24' /tmp/launch-volum.sh "$BIN"
else
  Xvfb :99 -screen 0 1280x800x24 >/tmp/xvfb.log 2>&1 &
  xvfb_pid=$!
  sleep 0.5
  DISPLAY=:99 /tmp/launch-volum.sh "$BIN"
  kill "$xvfb_pid" 2>/dev/null || true
  wait "$xvfb_pid" 2>/dev/null || true
fi

log "uninstall"
HOME="$home" XDG_DATA_HOME="$home/.local/share" "$TOP/install.sh" --uninstall | tee /out/uninstall.log
assert_user_gone "$home" | tee /out/uninstall-left.txt

if [[ "$RED_UNINSTALL" -eq 1 ]]; then
  log "red proof: uninstall leaves the VST3"
  red_top="$(mktemp -d)"
  cp -a "$TOP/." "$red_top/"
  sed -i '/VOLUM_UNINSTALL_VST3/d' "$red_top/install.sh"
  red_home=/tmp/volum-home-red
  HOME="$red_home" XDG_DATA_HOME="$red_home/.local/share" "$red_top/install.sh" >/tmp/red-install.log
  HOME="$red_home" XDG_DATA_HOME="$red_home/.local/share" "$red_top/install.sh" --uninstall >/tmp/red-uninstall.log
  set +e
  assert_user_gone "$red_home" > /out/red-uninstall.txt 2>&1
  red_rc=$?
  set -e
  cat /out/red-uninstall.txt
  if [[ "$red_rc" -eq 0 ]]; then
    echo "RED PROOF FAILED: uninstall still removed the VST3" >&2
    exit 1
  fi
  grep -q 'left behind: .*/\.vst3/VoLum\.vst3' /out/red-uninstall.txt
  user_paths "$red_home"
  [[ ! -e "$BIN" ]]
  cmp -s "$TOP/install.sh" /out/install.sh.orig
  echo "restored: install.sh byte-identical; bad copy was discarded"
fi

log "system install"
HOME="$home" XDG_DATA_HOME="$home/.local/share" "$TOP/install.sh" --system | tee /out/system-install.log
assert_system_present
log "system uninstall"
HOME="$home" XDG_DATA_HOME="$home/.local/share" "$TOP/install.sh" --system --uninstall | tee /out/system-uninstall.log
assert_no_system
cmp -s "$TOP/install.sh" /out/install.sh.orig
log "PASS"
chmod -R a+rwX /out || true
