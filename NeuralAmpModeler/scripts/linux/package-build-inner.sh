#!/usr/bin/env bash
# Runs as root inside the throwaway Ubuntu 22.04 build container.
# The repo is mounted at /src. Files created here are given back to the host
# user on the way out so the next checkout can update the worktree.
set -euo pipefail

cd /src
host_id="$(stat -c %u:%g /src)"
cleanup() {
  chown -R "$host_id" /src/build-linux /src/iPlug2/Dependencies/IPlug/VST3_SDK 2>/dev/null || true
}
trap cleanup EXIT

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y --no-install-recommends make
rm -rf /src/build-linux
bash NeuralAmpModeler/scripts/linux/provision-linux.sh --ci
bash NeuralAmpModeler/scripts/makedist-linux.sh --build
