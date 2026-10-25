#!/usr/bin/env bash
# Portability matrix for a VoLum Linux tarball.
#
#   package-matrix.sh --tarball build-linux/out/VoLum-*-linux-x86_64.tar.gz \
#                     --validator build-linux/validator [--only ubuntu2204]
#
# Containers are throwaway --rm runs: Ubuntu 22.04, Ubuntu 24.04, Fedora
# latest, and Ubuntu 24.04 without GTK 3. Each one installs, and the three
# desktop images also launch and uninstall. Fedora also checks ldd.
set -euo pipefail

# box.ps1 holds the build flock on fds 7-9. The matrix does not compile.
for fd in 7 8 9; do
  eval "exec ${fd}>&-" 2>/dev/null || true
done

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
TARBALL=""
VALIDATOR=""
ONLY=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --tarball) TARBALL="$2"; shift 2 ;;
    --validator) VALIDATOR="$2"; shift 2 ;;
    --only) ONLY+=("$2"); shift 2 ;;
    *)
      echo "usage: package-matrix.sh --tarball TAR --validator BIN [--only CASE]" >&2
      exit 2
      ;;
  esac
done

[[ -f "$TARBALL" && -x "$VALIDATOR" ]] || {
  echo "package-matrix: need a tarball and a validator binary" >&2
  exit 1
}

docker_cmd() {
  if docker info >/dev/null 2>&1; then
    docker "$@"
  else
    sudo -n docker "$@"
  fi
}

stage="$(mktemp -d)"
cp "$TARBALL" "$stage/pkg.tar.gz"
cp "$VALIDATOR" "$stage/validator"
chmod 755 "$stage/validator"
cp "$HERE/verify-package.sh" "$stage/verify-package.sh"
cp "$HERE/package-inside.sh" "$stage/package-inside.sh"
chmod 755 "$stage/verify-package.sh" "$stage/package-inside.sh"

run_case() {
  local case_name="$1" image="$2" expect_gtk="${3:-}"
  local name="volum-pkg14-${case_name}-$$"
  local out="$ROOT/build-linux/package-matrix/$case_name"
  rm -rf "$out"
  mkdir -p "$out"
  echo "=== $case_name ($image) container $name ==="
  docker_cmd run --rm --name "$name" \
    -e "CASE=$case_name" \
    -e "EXPECT_GTK=$expect_gtk" \
    -v "$stage:/in:ro" \
    -v "$out:/out" \
    "$image" \
    bash /in/package-inside.sh
  echo "=== $case_name PASS ==="
}

wanted() {
  local name="$1"
  [[ "${#ONLY[@]}" -eq 0 ]] && return 0
  local item
  for item in "${ONLY[@]}"; do
    [[ "$item" == "$name" ]] && return 0
  done
  return 1
}

if wanted ubuntu2204; then
  run_case ubuntu2204 ubuntu:22.04
fi
if wanted ubuntu2404; then
  run_case ubuntu2404 ubuntu:24.04
fi
if wanted fedora; then
  run_case fedora fedora:latest
fi
if wanted nogtk; then
  run_case nogtk ubuntu:24.04 libgtk-3-0t64
fi

rm -rf "$stage"
echo "package matrix: all requested cases passed"
