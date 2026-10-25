#!/usr/bin/env bash
# Install VoLum for the current user, or under /usr with --system.
#
#   ./install.sh                 # ~/.vst3, ~/.local/bin, $XDG_DATA_HOME/VoLum
#   ./install.sh --uninstall
#   ./install.sh --system        # /usr/lib/vst3, /usr/local/bin, /usr/share/VoLum
#   ./install.sh --system --uninstall
#
# This script never elevates and never writes a system path unless --system
# is set. --system when not root prints how to re-run and exits.
# A missing GTK 3 (the standalone's GDK libraries) prints the distro package
# name and still installs the VST3.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"

usage() {
  cat <<'EOF'
Usage: install.sh [--system] [--uninstall]

Default paths (current user):
  ~/.vst3/VoLum.vst3
  ~/.local/bin/VoLum
  $XDG_DATA_HOME/VoLum          (default ~/.local/share/VoLum)
  $XDG_DATA_HOME/applications/VoLum.desktop

--system paths (root only; this script does not elevate):
  /usr/lib/vst3/VoLum.vst3
  /usr/local/bin/VoLum
  /usr/share/VoLum
  /usr/share/applications/VoLum.desktop
EOF
}

SYSTEM=0
UNINSTALL=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --system) SYSTEM=1 ;;
    --uninstall) UNINSTALL=1 ;;
    -h|--help) usage; exit 0 ;;
    *)
      echo "install.sh: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ "${HOME:-}" != /* || "$HOME" == "/" ]]; then
  echo "install.sh: HOME must be an absolute directory other than /" >&2
  exit 1
fi

if [[ "$SYSTEM" -eq 1 && "$(id -u)" -ne 0 ]]; then
  echo "install.sh: --system writes under /usr and /usr/local. Re-run as root." >&2
  exit 1
fi

# Absolute XDG_DATA_HOME, or the spec's ~/.local/share fallback. A relative
# value is ignored, matching VoLumLinuxAppDataDir.
data_home="${XDG_DATA_HOME:-}"
if [[ -z "$data_home" || "$data_home" != /* ]]; then
  data_home="$HOME/.local/share"
fi

user_vst3="$HOME/.vst3/VoLum.vst3"
user_bin="$HOME/.local/bin/VoLum"
user_data="$data_home/VoLum"
user_desktop="$data_home/applications/VoLum.desktop"
user_icon="$data_home/icons/hicolor/256x256/apps/volum.png"

sys_vst3="/usr/lib/vst3/VoLum.vst3"
sys_bin="/usr/local/bin/VoLum"
sys_data="/usr/share/VoLum"
sys_desktop="/usr/share/applications/VoLum.desktop"
sys_icon="/usr/share/icons/hicolor/256x256/apps/volum.png"

refuse_outside() {
  local path="$1" prefix="$2"
  case "$path" in
    "$prefix"|"$prefix"/*) ;;
    *)
      echo "install.sh: refusing to write $path (outside $prefix)" >&2
      exit 1
      ;;
  esac
}

gtk_package_name() {
  local id="" ver="" blob=""
  if [[ -r /etc/os-release ]]; then
    blob="$(cat /etc/os-release)"
    id="$(sed -n 's/^ID=//p' /etc/os-release | head -n 1 | tr -d '"')"
    ver="$(sed -n 's/^VERSION_ID=//p' /etc/os-release | head -n 1 | tr -d '"')"
  fi
  case "$blob" in
    *'ID=fedora'*|*'ID="fedora"'*|*'ID=rhel'*|*'ID="rhel"'*|*'ID=centos'*|*'ID_LIKE="fedora"'*|*'ID_LIKE=fedora'*)
      printf '%s\n' gtk3
      return
      ;;
  esac
  case "$id" in
    arch|manjaro|endeavouros)
      printf '%s\n' gtk3
      return
      ;;
  esac
  case "$ver" in
    22.04|20.04|12|11)
      printf '%s\n' libgtk-3-0
      ;;
    *)
      printf '%s\n' libgtk-3-0t64
      ;;
  esac
}

warn_missing_standalone_libs() {
  local bin="$1" ldd_out="" soname="" missing="" pkg=""
  if [[ ! -x "$bin" && ! -f "$bin" ]]; then
    echo "install.sh: standalone binary is missing: $bin" >&2
    exit 1
  fi
  set +e
  ldd_out="$(ldd "$bin" 2>&1)"
  set -e
  for soname in libgdk-3.so.0 libgdk_pixbuf-2.0.so.0 libgobject-2.0.so.0 libglib-2.0.so.0 libcairo.so.2 libXi.so.6; do
    if printf '%s\n' "$ldd_out" | grep -F "$soname" | grep -q "not found"; then
      missing="${missing}${missing:+ }$soname"
    fi
  done
  if [[ -n "$missing" ]]; then
    pkg="$(gtk_package_name)"
    echo "WARNING: the standalone is missing libraries: $missing"
    echo "WARNING: install package ${pkg} and the standalone can start. The VST3 does not need it."
    echo "The VST3 was installed."
  fi
}

require_payload() {
  local missing=0 path
  for path in "$HERE/VoLum.vst3" "$HERE/VoLum" "$HERE/resources" "$HERE/VoLumRigs" "$HERE/VoLum.desktop" "$HERE/volum.png"; do
    if [[ ! -e "$path" ]]; then
      echo "install.sh: missing $path (run from the extracted tarball)" >&2
      missing=1
    fi
  done
  [[ "$missing" -eq 0 ]]
}

install_user() {
  require_payload
  refuse_outside "$user_vst3" "$HOME/.vst3"
  refuse_outside "$user_bin" "$HOME/.local"
  refuse_outside "$user_data/VoLum" "$data_home"
  refuse_outside "$user_data/resources" "$data_home"
  refuse_outside "$user_data/VoLumRigs" "$data_home"
  refuse_outside "$user_desktop" "$data_home"
  refuse_outside "$user_icon" "$data_home"

  mkdir -p "$HOME/.vst3" "$HOME/.local/bin" "$user_data" \
    "$(dirname "$user_desktop")" "$(dirname "$user_icon")"
  rm -rf -- "$user_vst3"
  cp -a "$HERE/VoLum.vst3" "$user_vst3"
  rm -rf -- "$user_data/resources" "$user_data/VoLumRigs"
  rm -f -- "$user_data/VoLum"
  cp -a "$HERE/VoLum" "$user_data/VoLum"
  cp -a "$HERE/resources" "$user_data/resources"
  cp -a "$HERE/VoLumRigs" "$user_data/VoLumRigs"
  chmod 755 "$user_data/VoLum"
  cat > "$user_bin" <<EOF
#!/bin/sh
exec $(printf '%q' "$user_data/VoLum") "\$@"
EOF
  chmod 755 "$user_bin"
  awk -v exec="$user_bin" -v icon="$user_icon" '
    /^Exec=/ { print "Exec=" exec; next }
    /^Icon=/ { print "Icon=" icon; next }
    { print }
  ' "$HERE/VoLum.desktop" > "$user_desktop"
  cp -a "$HERE/volum.png" "$user_icon"
  cat > "$user_data/.install-manifest" <<EOF
$user_vst3
$user_bin
$user_data/VoLum
$user_data/resources
$user_data/VoLumRigs
$user_desktop
$user_icon
EOF
  echo "Installed VoLum for ${USER:-$(id -un)}"
  echo "  VST3        $user_vst3"
  echo "  Standalone  $user_bin"
  echo "  Data        $user_data"
  warn_missing_standalone_libs "$user_data/VoLum"
}

install_system() {
  require_payload
  refuse_outside "$sys_vst3" "/usr/lib/vst3"
  refuse_outside "$sys_bin" "/usr/local/bin"
  refuse_outside "$sys_data/VoLum" "/usr/share/VoLum"
  refuse_outside "$sys_desktop" "/usr/share/applications"
  refuse_outside "$sys_icon" "/usr/share/icons"

  mkdir -p /usr/lib/vst3 /usr/local/bin "$sys_data" \
    "$(dirname "$sys_desktop")" "$(dirname "$sys_icon")"
  rm -rf -- "$sys_vst3"
  cp -a "$HERE/VoLum.vst3" "$sys_vst3"
  rm -rf -- "$sys_data/resources" "$sys_data/VoLumRigs"
  rm -f -- "$sys_data/VoLum"
  cp -a "$HERE/VoLum" "$sys_data/VoLum"
  cp -a "$HERE/resources" "$sys_data/resources"
  cp -a "$HERE/VoLumRigs" "$sys_data/VoLumRigs"
  chmod 755 "$sys_data/VoLum"
  cat > "$sys_bin" <<EOF
#!/bin/sh
exec $(printf '%q' "$sys_data/VoLum") "\$@"
EOF
  chmod 755 "$sys_bin"
  awk -v exec="$sys_bin" -v icon="$sys_icon" '
    /^Exec=/ { print "Exec=" exec; next }
    /^Icon=/ { print "Icon=" icon; next }
    { print }
  ' "$HERE/VoLum.desktop" > "$sys_desktop"
  cp -a "$HERE/volum.png" "$sys_icon"
  cat > "$sys_data/.install-manifest" <<EOF
$sys_vst3
$sys_bin
$sys_data/VoLum
$sys_data/resources
$sys_data/VoLumRigs
$sys_desktop
$sys_icon
EOF
  echo "Installed VoLum under /usr"
  echo "  VST3        $sys_vst3"
  echo "  Standalone  $sys_bin"
  echo "  Data        $sys_data"
  warn_missing_standalone_libs "$sys_data/VoLum"
}

uninstall_user() {
  refuse_outside "$user_vst3" "$HOME/.vst3"
  refuse_outside "$user_bin" "$HOME/.local"
  refuse_outside "$user_data/VoLum" "$data_home"
  refuse_outside "$user_desktop" "$data_home"
  refuse_outside "$user_icon" "$data_home"
  # User VST3. The package red proof deletes this line and expects it left behind.
  rm -rf -- "$user_vst3" # VOLUM_UNINSTALL_VST3
  rm -f -- "$user_bin"
  rm -rf -- "$user_data/VoLum" "$user_data/resources" "$user_data/VoLumRigs"
  rm -f -- "$user_data/.install-manifest" "$user_desktop" "$user_icon"
  rmdir "$HOME/.vst3" "$HOME/.local/bin" "$(dirname "$user_desktop")" "$(dirname "$user_icon")" 2>/dev/null || true
  echo "Uninstalled user VoLum"
}

uninstall_system() {
  refuse_outside "$sys_vst3" "/usr/lib/vst3"
  refuse_outside "$sys_bin" "/usr/local/bin"
  refuse_outside "$sys_data" "/usr/share/VoLum"
  refuse_outside "$sys_desktop" "/usr/share/applications"
  refuse_outside "$sys_icon" "/usr/share/icons"
  rm -rf -- "$sys_vst3"
  rm -f -- "$sys_bin"
  rm -rf -- "$sys_data"
  rm -f -- "$sys_desktop" "$sys_icon"
  rmdir /usr/lib/vst3 /usr/local/bin "$(dirname "$sys_desktop")" "$(dirname "$sys_icon")" 2>/dev/null || true
  echo "Uninstalled system VoLum"
}

if [[ "$UNINSTALL" -eq 1 && "$SYSTEM" -eq 1 ]]; then
  uninstall_system
elif [[ "$UNINSTALL" -eq 1 ]]; then
  uninstall_user
elif [[ "$SYSTEM" -eq 1 ]]; then
  install_system
else
  install_user
fi
