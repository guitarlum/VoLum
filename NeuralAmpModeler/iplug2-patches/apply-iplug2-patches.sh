#!/usr/bin/env bash
# Apply VoLum's iPlug2 patches to the iPlug2 submodule working tree.
# Idempotent: a second invocation is a no-op.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$here/../.." && pwd)"
iplug_dir="$repo_root/iPlug2"

if [ ! -e "$iplug_dir/.git" ]; then
  if [ ! -d "$iplug_dir" ]; then
    echo "iPlug2 submodule not found at $iplug_dir. Run 'git submodule update --init --recursive' first." >&2
    exit 1
  fi
fi

shopt -s nullglob
patches=("$here"/*.patch)
shopt -u nullglob

if [ ${#patches[@]} -eq 0 ]; then
  echo "No patches in $here; nothing to apply."
  exit 0
fi

for patch in "${patches[@]}"; do
  name="$(basename "$patch")"
  echo "Checking $name..."

  if git -C "$iplug_dir" apply --reverse --check "$patch" >/dev/null 2>&1; then
    echo "  already applied, skipping."
    continue
  fi

  if ! git -C "$iplug_dir" apply --check "$patch" >/dev/null 2>&1; then
    echo "Patch $name does not apply cleanly to iPlug2 (and is not already applied). Aborting." >&2
    exit 1
  fi

  git -C "$iplug_dir" apply --whitespace=nowarn "$patch"
  echo "  applied."
done
