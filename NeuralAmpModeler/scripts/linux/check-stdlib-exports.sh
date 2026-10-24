#!/usr/bin/env bash
# Fail if a built VoLum.so exports libstdc++ / __gnu_cxx or any GNU-unique
# binding. Static libstdc++ facet ids are STB_GNU_UNIQUE; once they are in
# the dynamic table, a host locale binds them and num_put crashes in codecvt.
#
#   check-stdlib-exports.sh path/to/VoLum.so

set -euo pipefail

SO="${1:-}"
if [[ -z "$SO" || ! -f "$SO" ]]; then
  echo "check-stdlib-exports: usage: check-stdlib-exports.sh VoLum.so" >&2
  exit 2
fi

tmp="$(mktemp)"
readelf -W --dyn-syms "$SO" | awk '
  NF < 8 { next }
  $7 == "UND" { next }
  $5 == "UNIQUE" { print; next }
  {
    name = $NF
    if (name ~ /^_Z(N|NK|NV|NVK|GVN|TIN|TSN|TVN|TTN|Thn)?St/ ||
        name ~ /^_Z(N|NK|TVN|TIN|TSN|GVN)?9__gnu_cxx/ ||
        name ~ /^_ZSt/)
      print
  }
' > "$tmp"

if [[ -s "$tmp" ]]; then
  echo "check-stdlib-exports: $SO exports std::/__gnu_cxx or GNU-unique symbols" >&2
  head -n 40 "$tmp" >&2
  echo "check-stdlib-exports: $(wc -l < "$tmp") bad symbols" >&2
  rm -f "$tmp"
  exit 1
fi
rm -f "$tmp"
echo "check-stdlib-exports: PASS ($SO)"
