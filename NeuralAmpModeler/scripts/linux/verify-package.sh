#!/usr/bin/env bash
# Checks for the Linux tarball. Used by makedist-linux.sh and the container matrix.
#
#   verify-package.sh --needed <VoLum.so> <VoLum>
#   verify-package.sh --ldd <VoLum.so> <VoLum>
#   verify-package.sh --tarball <VoLum-*-linux-x86_64.tar.gz>
#   verify-package.sh --audit-installer <install.sh>
set -euo pipefail

# Direct DT_NEEDED. ld-linux may also show up as NEEDED; it is optional.
# Anything else, including GDK on the VST3, is a failure.
VST3_REQUIRED=(
  libX11.so.6
  libGL.so.1
  libfontconfig.so.1
  libfreetype.so.6
  libatomic.so.1
  libm.so.6
  libc.so.6
)
APP_REQUIRED=(
  "${VST3_REQUIRED[@]}"
  libasound.so.2
  libgdk-3.so.0
  libgdk_pixbuf-2.0.so.0
  libgobject-2.0.so.0
  libglib-2.0.so.0
  libcairo.so.2
  libXi.so.6
)
NEEDED_OPTIONAL=(
  ld-linux-x86-64.so.2
)

usage() {
  echo "usage: verify-package.sh --needed <so> <app> | --ldd <so> <app> | --tarball <tar.gz> | --audit-installer <install.sh>" >&2
  exit 2
}

in_list() {
  local needle="$1"
  shift
  local item
  for item in "$@"; do
    [[ "$item" == "$needle" ]] && return 0
  done
  return 1
}

read_needed() {
  readelf -d "$1" | awk -F'[][]' '/\(NEEDED\)/ { print $2 }' | sort -u
}

check_needed_one() {
  local label="$1" bin="$2"
  shift 2
  local -a required=("$@")
  local name extra=0 missing=0
  local -A have=()
  [[ -f "$bin" ]] || { echo "NEEDED check: missing $bin" >&2; return 1; }
  while IFS= read -r name; do
    [[ -n "$name" ]] || continue
    have["$name"]=1
  done < <(read_needed "$bin")

  for name in "${required[@]}"; do
    if [[ -z "${have[$name]:-}" ]]; then
      echo "missing NEEDED on ${label}: ${name}"
      missing=1
    fi
  done
  if [[ "${#have[@]}" -gt 0 ]]; then
    for name in "${!have[@]}"; do
      if in_list "$name" "${required[@]}" || in_list "$name" "${NEEDED_OPTIONAL[@]}"; then
        continue
      fi
      echo "unexpected NEEDED on ${label}: ${name}"
      extra=1
    done
  fi
  if [[ "$missing" -ne 0 || "$extra" -ne 0 ]]; then
    echo "NEEDED check failed for ${label} ($bin)"
    return 1
  fi
  echo "${label} NEEDED: $(read_needed "$bin" | tr '\n' ' ')"
}

check_needed() {
  local so="$1" app="$2" rc=0
  check_needed_one VST3 "$so" "${VST3_REQUIRED[@]}" || rc=1
  check_needed_one standalone "$app" "${APP_REQUIRED[@]}" || rc=1
  return "$rc"
}

ldd_sonames() {
  # First field of each ldd line. Skip the loader path (/lib.../ld-linux) and
  # keep "libfoo.so.1 => not found" so an unexpected soname is still visible.
  ldd "$1" 2>/dev/null | awk '{ print $1 }' | grep -E '\.so' | grep -v '^/' || true
}

check_ldd_one() {
  local label="$1" bin="$2"
  shift 2
  local -a expected=("$@")
  local -A allowed=() path_of=()
  local line name path dep fail=0
  [[ -f "$bin" ]] || { echo "ldd: missing $bin" >&2; return 1; }

  while IFS= read -r line; do
    name="$(printf '%s\n' "$line" | awk '{ print $1 }')"
    path="$(printf '%s\n' "$line" | awk '{ print $3 }')"
    [[ -n "$name" ]] || continue
    if [[ "$path" == /* ]]; then
      path_of["$name"]="$path"
    fi
  done < <(ldd "$bin" 2>&1 || true)

  for name in "${expected[@]}" "${NEEDED_OPTIONAL[@]}" linux-vdso.so.1; do
    allowed["$name"]=1
    path="${path_of[$name]:-}"
    [[ -n "$path" && -e "$path" ]] || continue
    while IFS= read -r dep; do
      [[ -n "$dep" ]] || continue
      allowed["$dep"]=1
    done < <(ldd "$path" 2>/dev/null | awk '{ print $1 }' | grep -E '\.so' || true)
  done

  for name in "${expected[@]}"; do
    if [[ -z "${path_of[$name]:-}" ]]; then
      echo "ldd: ${name} not found on ${label}"
      fail=1
    fi
  done
  while IFS= read -r name; do
    [[ -n "$name" ]] || continue
    if [[ -z "${allowed[$name]:-}" ]]; then
      echo "ldd: unexpected library ${name}"
      fail=1
    fi
  done < <(ldd_sonames "$bin")
  if [[ "$fail" -ne 0 ]]; then
    echo "ldd check failed for ${label} ($bin)"
    return 1
  fi
  echo "${label} ldd: $(ldd_sonames "$bin" | tr '\n' ' ')"
}

check_ldd() {
  local so="$1" app="$2" rc=0
  check_ldd_one VST3 "$so" "${VST3_REQUIRED[@]}" || rc=1
  check_ldd_one standalone "$app" "${APP_REQUIRED[@]}" || rc=1
  return "$rc"
}

audit_installer() {
  local script="$1" line
  [[ -f "$script" ]] || { echo "audit: missing $script" >&2; return 1; }
  # A line that invokes sudo. Mentions inside comments would count too; install.sh has none.
  if grep -nE '(^|[^A-Za-z0-9_])sudo([^A-Za-z0-9_]|$)' "$script"; then
    echo "audit: install.sh invokes sudo"
    return 1
  fi
  if ! grep -q 'VOLUM_UNINSTALL_VST3' "$script"; then
    echo "audit: install.sh is missing the user VST3 uninstall line"
    return 1
  fi
  if ! grep -q 'gtk_package_name' "$script"; then
    echo "audit: install.sh does not name a GTK package"
    return 1
  fi
  echo "audit: install.sh does not invoke sudo"
}

check_tarball() {
  local tarball="$1" stage="" top="" so="" app=""
  [[ -f "$tarball" ]] || { echo "tarball missing: $tarball" >&2; return 1; }
  stage="$(mktemp -d)"
  tar -xzf "$tarball" -C "$stage"
  top="$(find "$stage" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
  [[ -n "$top" ]] || { echo "tarball has no top directory"; rm -rf "$stage"; return 1; }
  local rel missing=0 path
  for rel in \
    VoLum.vst3/Contents/x86_64-linux/VoLum.so \
    VoLum \
    resources/ttf/Roboto-Regular.ttf \
    VoLumRigs/factory-presets.json \
    "VoLumRigs/Diezel Herbert Mk1/V30-Herb-4.nam" \
    install.sh \
    LICENSE \
    THIRD_PARTY_LICENSES.md \
    VoLum.desktop \
    volum.png
  do
    if [[ ! -e "$top/$rel" ]]; then
      echo "tarball missing $rel"
      missing=1
    fi
  done
  if ! find "$top/VoLumRigs/PrePedals" -name '*.nam' -print -quit | grep -q .; then
    echo "tarball missing VoLumRigs/PrePedals/*.nam"
    missing=1
  fi
  so="$top/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
  app="$top/VoLum"
  echo "tarball: $tarball"
  echo "size: $(du -h "$tarball" | awk '{ print $1 }') ($(wc -c < "$tarball") bytes)"
  echo "sha256: $(sha256sum "$tarball" | awk '{ print $1 }')"
  audit_installer "$top/install.sh" || missing=1
  if [[ -f "$so" && -f "$app" ]]; then
    check_needed "$so" "$app" || missing=1
  fi
  # shellcheck disable=SC2064
  rm -rf "$stage"
  [[ "$missing" -eq 0 ]]
}

[[ $# -ge 1 ]] || usage
case "$1" in
  --needed)
    [[ $# -eq 3 ]] || usage
    check_needed "$2" "$3"
    ;;
  --ldd)
    [[ $# -eq 3 ]] || usage
    check_ldd "$2" "$3"
    ;;
  --tarball)
    [[ $# -eq 2 ]] || usage
    check_tarball "$2"
    ;;
  --audit-installer)
    [[ $# -eq 2 ]] || usage
    audit_installer "$2"
    ;;
  *)
    usage
    ;;
esac
