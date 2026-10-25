#!/usr/bin/env bash
# Build VoLum-<version>-linux-x86_64.tar.gz from a Linux Release build.
#
#   bash NeuralAmpModeler/scripts/makedist-linux.sh [--build]
#
# Without --build the script packages build-linux/ (see build-linux.sh).
# The archive's top directory is VoLum-<version>-linux-x86_64/ and holds
# VoLum.vst3/, VoLum, resources/, VoLumRigs/, install.sh, the desktop file,
# the icon, LICENSE and THIRD_PARTY_LICENSES.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DO_BUILD=0
for arg in "$@"; do
  case "$arg" in
    --build) DO_BUILD=1 ;;
    *)
      echo "usage: makedist-linux.sh [--build]" >&2
      exit 2
      ;;
  esac
done

if [[ "$DO_BUILD" -eq 1 ]]; then
  bash NeuralAmpModeler/scripts/build-linux.sh Release
fi

BUILD="${BUILD_DIR:-build-linux}"
SO="$BUILD/VoLum.vst3/Contents/x86_64-linux/VoLum.so"
APP="$BUILD/standalone/VoLum"
RES="$BUILD/standalone/resources"
[[ -f "$SO" && -x "$APP" && -d "$RES" ]] || {
  echo "makedist-linux: missing $SO or $APP (build with build-linux.sh or pass --build)" >&2
  exit 1
}

VER="$(sed -n 's/^#define PLUG_VERSION_STR "\([^"]*\)"/\1/p' NeuralAmpModeler/config.h | head -n 1)"
[[ -n "$VER" ]] || { echo "makedist-linux: PLUG_VERSION_STR missing" >&2; exit 1; }
NAME="VoLum-${VER}-linux-x86_64"

bash NeuralAmpModeler/scripts/linux/verify-package.sh --audit-installer \
  NeuralAmpModeler/installer/linux/install.sh

STAGE="$(mktemp -d)"
DEST="$STAGE/$NAME"
mkdir -p "$DEST"
cp -a "$BUILD/VoLum.vst3" "$DEST/VoLum.vst3"
cp -a "$APP" "$DEST/VoLum"
cp -a "$RES" "$DEST/resources"
rm -rf "$DEST/VoLumRigs"
cp -a rigs "$DEST/VoLumRigs"
find "$DEST" -name '.DS_Store' -delete
cp NeuralAmpModeler/installer/linux/install.sh "$DEST/install.sh"
cp NeuralAmpModeler/installer/linux/VoLum.desktop "$DEST/VoLum.desktop"
cp NeuralAmpModeler/installer/linux/volum.png "$DEST/volum.png"
cp LICENSE "$DEST/LICENSE"
cp THIRD_PARTY_LICENSES.md "$DEST/THIRD_PARTY_LICENSES.md"
chmod 755 "$DEST/install.sh" "$DEST/VoLum"
strip --strip-unneeded "$DEST/VoLum" "$DEST/VoLum.vst3/Contents/x86_64-linux/VoLum.so"

bash NeuralAmpModeler/scripts/linux/verify-package.sh --needed \
  "$DEST/VoLum.vst3/Contents/x86_64-linux/VoLum.so" \
  "$DEST/VoLum"

OUT="$BUILD/out"
mkdir -p "$OUT"
tar -C "$STAGE" -czf "$OUT/$NAME.tar.gz" "$NAME"
rm -rf "$STAGE"

bash NeuralAmpModeler/scripts/linux/verify-package.sh --tarball "$OUT/$NAME.tar.gz"
echo "TARBALL $ROOT/$OUT/$NAME.tar.gz"
