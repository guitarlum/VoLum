#!/usr/bin/env bash
# Build the glibc 2.35 tarball in a throwaway Ubuntu 22.04 container, then run
# the portability matrix. The host only needs docker. Nothing here reconfigures
# the daemon or touches localai-platform.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
JOBS="${JOBS:-14}"
name="volum-14-build-$$"

docker_cmd() {
  if docker info >/dev/null 2>&1; then
    docker "$@"
  else
    sudo -n docker "$@"
  fi
}

echo "package-box: build container $name"
docker_cmd run --rm --name "$name" \
  -v "$ROOT:/src" \
  -w /src \
  -e "JOBS=$JOBS" \
  ubuntu:22.04 \
  bash NeuralAmpModeler/scripts/linux/package-build-inner.sh

tarball="$(echo build-linux/out/VoLum-*-linux-x86_64.tar.gz)"
[[ -f "$tarball" ]] || { echo "package-box: no tarball at $tarball" >&2; exit 1; }
bash NeuralAmpModeler/scripts/linux/package-matrix.sh \
  --tarball "$tarball" \
  --validator build-linux/validator
