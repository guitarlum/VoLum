#!/usr/bin/env bash
# Fail unless the release VoLum.so exports only the VST3 entry points.
# Anything else in the dynamic table (libstdc++ facet ids, UI-thread gate
# hooks) can be interposed by the host or another plug-in.
#
#   check-stdlib-exports.sh path/to/VoLum.so

set -euo pipefail

SO="${1:-}"
if [[ -z "$SO" || ! -f "$SO" ]]; then
  echo "check-stdlib-exports: usage: check-stdlib-exports.sh VoLum.so" >&2
  exit 2
fi

tmp="$(mktemp)"
nm -D --defined-only "$SO" | awk '
  NF < 3 { next }
  {
    name = $NF
    sub(/@.*/, "", name)
    if (name != "GetPluginFactory" && name != "ModuleEntry" && name != "ModuleExit")
      print
  }
' > "$tmp"

if [[ -s "$tmp" ]]; then
  echo "check-stdlib-exports: $SO exports a symbol other than GetPluginFactory, ModuleEntry, ModuleExit" >&2
  head -n 40 "$tmp" >&2
  echo "check-stdlib-exports: $(wc -l < "$tmp") extra symbols" >&2
  rm -f "$tmp"
  exit 1
fi
rm -f "$tmp"
echo "check-stdlib-exports: PASS ($SO)"
