#!/usr/bin/env bash
# Idempotent VoLum Linux host setup for Ubuntu 22.04, 24.04 and 26.04.
#
#   bash provision-linux.sh        # apt, then pluginval, REAPER and Carla
#   bash provision-linux.sh --ci   # apt build/test packages only
#
# --ci is for GitHub ubuntu-22.04/24.04 runners and cloud VMs. It installs the
# apt toolchain and test packages (including xvfb, xdotool and jackd2) and does
# not download pluginval, REAPER or Carla.
#
# ubuntu-22.04 CI uses the archive compilers g++-12 and clang-15. gcc-12 could
# not compile ResamplingContainer.h until the LanczosResampler alias was
# qualified; that fix is in AudioDSPTools. The toolchain PPA and apt.llvm.org
# are not used. If a previous run added those source files, this script
# removes only those files.
#
# Never stops, restarts or reconfigures ~/localai-platform, llama-server,
# Ollama or docker. Package installs only.
set -euo pipefail

CI=0
case "${1:-}" in
  "") ;;
  --ci) CI=1 ;;
  *)
    echo "usage: provision-linux.sh [--ci]" >&2
    exit 1
    ;;
esac

ROOT="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}"
URL="https://github.com/guitarlum/VoLum.git"

# pluginval 1.0.4 Linux zip (Tracktion/pluginval releases).
PLUGINVAL_VER="1.0.4"
PLUGINVAL_URL="https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Linux.zip"
PLUGINVAL_SHA="c01c49d8063965c4c2dea8324468336768f5c9139e0b1caebde14c2400b55352"

# REAPER 7.82 portable Linux x86_64 tarball (www.reaper.fm/download.php).
REAPER_VER="7.82"
REAPER_URL="https://www.reaper.fm/files/7.x/reaper782_linux_x86_64.tar.xz"
REAPER_SHA="a4aa961534f22ae5ae910e4ec0c200dcb4476c9f7556806fe09a9743cd582563"

log() { printf '[provision-linux %s] %s\n' "$(date +%H:%M:%S)" "$*"; }

run_apt() {
  if [ "$(id -u)" -eq 0 ]; then
    DEBIAN_FRONTEND=noninteractive apt-get "$@"
  else
    sudo DEBIAN_FRONTEND=noninteractive apt-get "$@"
  fi
}

run_root() {
  if [ "$(id -u)" -eq 0 ]; then
    "$@"
  else
    sudo "$@"
  fi
}

have_pkg() {
  dpkg-query -W -f '${Status}' "$1" 2>/dev/null | grep -q "install ok installed"
}

# First name that exists in the apt cache. Distros rename a few virtual -dev packages.
choose_pkg() {
  local name
  for name in "$@"; do
    if apt-cache show "$name" >/dev/null 2>&1; then
      printf '%s\n' "$name"
      return 0
    fi
  done
  log "none of these packages exist: $*"
  return 1
}

fetch_verified() {
  local url=$1 sha=$2 dest=$3
  if [ -f "$dest" ] && echo "$sha  $dest" | sha256sum -c --status; then
    return 0
  fi
  log "download $(basename "$dest")"
  curl -fL --retry 5 --retry-delay 2 -o "$dest.partial" "$url"
  if ! echo "$sha  $dest.partial" | sha256sum -c --status; then
    log "checksum mismatch for $url"
    sha256sum "$dest.partial" >&2
    rm -f "$dest.partial"
    exit 1
  fi
  mv "$dest.partial" "$dest"
}

apt_retry() {
  local n=0
  while [ "$n" -lt 5 ]; do
    if "$@"; then
      return 0
    fi
    n=$((n + 1))
    log "apt failed; retry $n/5"
    sleep 15
  done
  return 1
}

link_if_needed() {
  local target=$1 link=$2
  if [ -L "$link" ] && [ "$(readlink "$link")" = "$target" ]; then
    return 0
  fi
  ln -sfn "$target" "$link"
}

write_if_changed() {
  local dest=$1 tmp
  tmp=$(mktemp)
  cat >"$tmp"
  if [ -f "$dest" ] && cmp -s "$tmp" "$dest"; then
    rm -f "$tmp"
  else
    mv "$tmp" "$dest"
  fi
}

# Drop only the compiler sources this script used to add. Nothing else under
# /etc/apt is touched.
drop_added_compiler_sources() {
  local removed=0
  local f
  shopt -s nullglob
  for f in /etc/apt/sources.list.d/ubuntu-toolchain-r-ubuntu-test-*.list \
           /etc/apt/sources.list.d/llvm-toolchain-*-21.list \
           /etc/apt/trusted.gpg.d/apt.llvm.org.asc \
           /etc/apt/trusted.gpg.d/apt.llvm.org.gpg; do
    log "removing previously added compiler source $f"
    run_root rm -f "$f"
    removed=1
  done
  shopt -u nullglob
  if [ "$removed" -eq 1 ]; then
    log "apt-get update after removing compiler sources"
    apt_retry run_apt update -qq
  fi
}

if [ ! -r /etc/os-release ]; then
  log "missing /etc/os-release"
  exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release
if [ "${ID:-}" != "ubuntu" ]; then
  log "Ubuntu is required (found ${ID:-unknown})"
  exit 1
fi
case "$VERSION_ID" in
  22.04|24.04|26.04) ;;
  *)
    log "Ubuntu $VERSION_ID is not supported (need 22.04, 24.04 or 26.04)"
    exit 1
    ;;
esac

if [ "$CI" -eq 0 ]; then
  avail=$(df -B1 --output=avail "$HOME" | tail -n 1 | tr -d ' ')
  if [ "$avail" -lt 53687091200 ]; then
    log "refusing: under 50 GiB free on $HOME ($avail bytes)"
    exit 1
  fi
fi

log "ubuntu $VERSION_ID ${VERSION_CODENAME:-} ci=$CI"
drop_added_compiler_sources

gl_pkg=$(choose_pkg libgl-dev libgl1-mesa-dev)
alsa_pkg=$(choose_pkg libasound2-dev libasound-dev)
log "packages: gl=$gl_pkg alsa=$alsa_pkg"

pkgs=(
  gcc
  g++
  clang
  lld
  cmake
  ninja-build
  pkg-config
  libx11-dev
  libxcursor-dev
  libxrandr-dev
  libxext-dev
  libxi-dev
  "$gl_pkg"
  "$alsa_pkg"
  mesa-utils
  libjack-jackd2-dev
  jackd2
  libpulse-dev
  pulseaudio-utils
  libfontconfig-dev
  libfreetype-dev
  libgtk-3-dev
  libcurl4-openssl-dev
  xvfb
  xdotool
  x11-apps
  x11-utils
  imagemagick
  alsa-utils
  zenity
  python3-pip
  git
  curl
  ca-certificates
  unzip
  xz-utils
)
if [ "$VERSION_ID" = "22.04" ]; then
  pkgs+=(gcc-12 g++-12 clang-15)
fi

missing=()
for pkg in "${pkgs[@]}"; do
  have_pkg "$pkg" || missing+=("$pkg")
done

carla_ver=""
carla_sha=""
carla_needed=0
if [ "$CI" -eq 0 ]; then
  case "$VERSION_ID" in
    22.04)
      carla_ver="2.4.2-0ubuntu2"
      carla_sha="9d69f9bfad29a6a69392bee43df1dd313b0bdb93301aacac35beb3e3ae233eed"
      ;;
    24.04)
      carla_ver="2.5.8-0ubuntu5"
      carla_sha="68cf6eae0af54a8953f46f6fa01fc2c946eaf141ca36397b933f6c38628e532d"
      ;;
    26.04)
      carla_ver="2.5.10-0ubuntu4"
      carla_sha="e0cb6b11558412e11f774e00f6416ce08d7c4688122322a0cb60042fd56e5582"
      ;;
  esac
  if ! have_pkg carla || [ "$(dpkg-query -W -f '${Version}' carla)" != "$carla_ver" ]; then
    carla_needed=1
  fi
fi

if [ "${#missing[@]}" -gt 0 ] || [ "$carla_needed" -eq 1 ]; then
  log "apt-get update"
  apt_retry run_apt update -qq
fi

if [ "${#missing[@]}" -gt 0 ]; then
  if printf '%s\n' "${missing[@]}" | grep -qx jackd2; then
    printf '%s\n' 'jackd2 jackd/tweak_rt_limits boolean true' | run_root debconf-set-selections
  fi
  log "apt install ${missing[*]}"
  apt_retry run_apt install -y --no-install-recommends "${missing[@]}"
else
  log "apt: already installed"
fi

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { log "missing command after install: $1"; exit 1; }
}
need_cmd gcc
need_cmd g++
need_cmd clang
need_cmd ld.lld
if [ "$VERSION_ID" = "22.04" ]; then
  need_cmd g++-12
  need_cmd clang++-15
fi

# Clang 18+ ships the ASan runtime as libclang-rt-<major>-dev; older releases
# bundle it with clang itself, so the package may not exist.
install_clang_rt() {
  local major=$1
  local pkg="libclang-rt-${major}-dev"
  if apt-cache show "$pkg" >/dev/null 2>&1 && ! have_pkg "$pkg"; then
    log "apt install $pkg"
    apt_retry run_apt install -y --no-install-recommends "$pkg"
  fi
}
install_clang_rt "$(clang -dumpversion | cut -d. -f1)"
if command -v clang++-15 >/dev/null 2>&1; then
  install_clang_rt "$(clang++-15 -dumpversion | cut -d. -f1)"
fi
need_cmd cmake
need_cmd ninja
need_cmd pkg-config
need_cmd xvfb-run
need_cmd xdotool
need_cmd xdpyinfo
need_cmd jackd

log "g++ $(g++ --version | head -n 1)"
log "clang $(clang --version | head -n 1)"
if command -v g++-12 >/dev/null 2>&1; then
  log "g++-12 $(g++-12 --version | head -n 1)"
fi
if command -v clang++-15 >/dev/null 2>&1; then
  log "clang++-15 $(clang++-15 --version | head -n 1)"
fi
if [ "$CI" -eq 1 ]; then
  log "ci mode: skipped pluginval, REAPER and Carla"
  log "done"
  exit 0
fi

mkdir -p "$ROOT/wt" "$ROOT/locks" "$ROOT/out" "$ROOT/logs" "$ROOT/tools/bin" "$ROOT/tools/cache"
for lock in build1.lock build2.lock git.lock gui.lock; do
  [ -e "$ROOT/locks/$lock" ] || : >"$ROOT/locks/$lock"
done

if [ ! -e "$ROOT/VoLum" ]; then
  log "cloning $URL"
  git clone "$URL" "$ROOT/VoLum"
elif [ ! -d "$ROOT/VoLum/.git" ]; then
  log "$ROOT/VoLum exists and is not a git clone"
  exit 1
else
  log "clone: already present"
fi

pluginval_home="$ROOT/tools/pluginval"
pluginval_bin="$pluginval_home/pluginval"
pluginval_zip="$ROOT/tools/cache/pluginval_Linux.zip"
if [ -x "$pluginval_bin" ] && [ "$(cat "$pluginval_home/.stamp" 2>/dev/null || true)" = "$PLUGINVAL_SHA" ] \
  && echo "$PLUGINVAL_SHA  $pluginval_zip" | sha256sum -c --status; then
  log "pluginval: already installed ($PLUGINVAL_VER)"
else
  fetch_verified "$PLUGINVAL_URL" "$PLUGINVAL_SHA" "$pluginval_zip"
  rm -rf "$pluginval_home"
  mkdir -p "$pluginval_home"
  unzip -q "$pluginval_zip" -d "$pluginval_home"
  [ -f "$pluginval_bin" ] || { log "pluginval zip has no pluginval binary"; exit 1; }
  chmod +x "$pluginval_bin"
  printf '%s\n' "$PLUGINVAL_SHA" >"$pluginval_home/.stamp"
  log "pluginval: installed $PLUGINVAL_VER"
fi
link_if_needed "$pluginval_bin" "$ROOT/tools/bin/pluginval"

reaper_home="$ROOT/tools/reaper"
reaper_bin="$reaper_home/reaper_linux_x86_64/REAPER/reaper"
reaper_tar="$ROOT/tools/cache/reaper${REAPER_VER}_linux_x86_64.tar.xz"
if [ -x "$reaper_bin" ] && [ "$(cat "$reaper_home/.stamp" 2>/dev/null || true)" = "$REAPER_SHA" ] \
  && echo "$REAPER_SHA  $reaper_tar" | sha256sum -c --status; then
  log "reaper: already installed ($REAPER_VER)"
else
  fetch_verified "$REAPER_URL" "$REAPER_SHA" "$reaper_tar"
  rm -rf "$reaper_home"
  mkdir -p "$reaper_home"
  tar -xJf "$reaper_tar" -C "$reaper_home"
  [ -x "$reaper_bin" ] || { log "REAPER tarball has no $reaper_bin"; exit 1; }
  printf '%s\n' "$REAPER_SHA" >"$reaper_home/.stamp"
  log "reaper: installed $REAPER_VER"
fi
link_if_needed "$reaper_bin" "$ROOT/tools/bin/reaper"

carla_deb="$ROOT/tools/cache/carla_${carla_ver}_amd64.deb"
carla_url="https://archive.ubuntu.com/ubuntu/pool/universe/c/carla/carla_${carla_ver}_amd64.deb"
if [ "$carla_needed" -eq 0 ]; then
  log "carla: already installed ($carla_ver)"
else
  fetch_verified "$carla_url" "$carla_sha" "$carla_deb"
  log "apt install carla $carla_ver"
  apt_retry run_apt install -y --no-install-recommends "$carla_deb"
fi
need_cmd carla-single

write_if_changed "$ROOT/tools/versions.txt" <<EOF
ubuntu=$VERSION_ID
pluginval=$PLUGINVAL_VER
pluginval_sha256=$PLUGINVAL_SHA
reaper=$REAPER_VER
reaper_sha256=$REAPER_SHA
carla=$carla_ver
carla_sha256=$carla_sha
EOF

log "gcc $(gcc --version | head -n 1)"
log "clang $(clang --version | head -n 1)"
log "cmake $(cmake --version | head -n 1)"
log "ninja $(ninja --version)"
log "carla $(dpkg-query -W -f '${Version}' carla) $(command -v carla-single)"
log "pluginval $pluginval_bin"
log "reaper $reaper_bin"
log "done"
exit 0
