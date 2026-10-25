# Shared paths for the Linux plugin gates. Source this; do not execute it.
# shellcheck shell=bash

linux_gate_root() {
  local here
  here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  REPO_ROOT="$(cd "$here/../../.." && pwd)"
  LINUX_DIR="$here"
  UBSAN_SUPP="$here/ubsan.supp"
  REAPER_REFERENCE="$here/fixtures/reaper-default-10s.wav"
  local root="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}"
  if [[ -d "$root/tools/bin" ]]; then
    case ":$PATH:" in
      *":$root/tools/bin:"*) ;;
      *) export PATH="$root/tools/bin:$PATH" ;;
    esac
  fi
}

linux_throwaway_home() {
  local base="$1" home
  home="$(mktemp -d "$base/home.XXXXXX")"
  mkdir -p "$home/.config" "$home/.local/share"
  printf '%s\n' "$home"
}

linux_tool() {
  local name="$1" fallback="$2"
  if [[ -n "${!name:-}" && -x "${!name}" ]]; then
    printf '%s\n' "${!name}"
    return 0
  fi
  local cmd="${3:-}"
  if [[ -n "$cmd" ]] && command -v "$cmd" >/dev/null 2>&1; then
    command -v "$cmd"
    return 0
  fi
  local root="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}"
  if [[ -n "$fallback" && -x "$root/$fallback" ]]; then
    printf '%s\n' "$root/$fallback"
    return 0
  fi
  echo "linux gate: $cmd not found (provision-linux.sh)" >&2
  return 1
}

# Host processes are uninstrumented. LeakSanitizer would report their
# still-live allocations at exit, so detect_leaks stays off here. The Clang
# ASan doctest does not set that flag and still catches VoLum leaks.
linux_asan_lib() {
  local cc="${CC:-gcc}"
  "$cc" -print-file-name=libasan.so
}

linux_clean_log() {
  local log="$1" bad
  bad=$(grep -a -cE "IGraphicsLinux: X error|off the UI thread|ERROR: AddressSanitizer:|runtime error:" "$log" || true)
  echo "  $(basename "$log"): $bad X-error/off-thread/sanitizer lines"
  [[ "$bad" == 0 ]]
}

linux_gate_out() {
  local sha="$1"
  if [[ -n "${GITHUB_ACTIONS:-}" || -n "${VOLUM_GATE_OUT:-}" ]]; then
    printf '%s\n' "${VOLUM_GATE_OUT:-$REPO_ROOT/build-linux/plugin-gates}"
    return
  fi
  local root="${VOLUM_LINUX_ROOT:-$HOME/volum-linux}"
  if [[ -d "$root/out" ]]; then
    printf '%s\n' "$root/out/plugin-gates/$sha"
    return
  fi
  printf '%s\n' "$REPO_ROOT/build-linux/plugin-gates"
}

linux_prefer_ci_compiler() {
  if [[ -z "${CXX:-}" && -z "${CC:-}" ]] && command -v g++-12 >/dev/null 2>&1 && command -v gcc-12 >/dev/null 2>&1; then
    export CC=gcc-12
    export CXX=g++-12
  fi
}
