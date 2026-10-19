#!/bin/bash
# Fetch the DXVK native release into deps/dxvk-native (not committed: prebuilt binaries stay out of git).
# Pinned asset and SHA-256; deps/dxvk-native/README.md says what is taken from the tarball and why.
#   tools/linux/fetch_dxvk.sh          fetch when deps/dxvk-native/lib is missing
#   tools/linux/fetch_dxvk.sh --force  fetch again
set -euo pipefail
cd "$(dirname "$0")/../.."
VER=3.1.1
ASSET="dxvk-native-${VER}-steamrt-sniper.tar.gz"
URL="https://github.com/doitsujin/dxvk/releases/download/v${VER}/${ASSET}"
SHA256=506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7
DST=deps/dxvk-native
if [ -f "$DST/lib/libdxvk_d3d9.so" ] && [ "${1:-}" != "--force" ]; then exit 0; fi
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/hantei-chan"
mkdir -p "$CACHE"
TGZ="$CACHE/$ASSET"
if ! echo "$SHA256  $TGZ" | sha256sum -c --quiet 2>/dev/null; then
	echo "fetching $URL"
	curl -fL --retry 3 -o "$TGZ.part" "$URL"
	mv "$TGZ.part" "$TGZ"
	echo "$SHA256  $TGZ" | sha256sum -c --quiet || { echo "checksum mismatch for $ASSET"; exit 1; }
fi
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
tar xzf "$TGZ" -C "$TMP"
rm -rf "$DST/lib" "$DST/include"; mkdir -p "$DST/lib" "$DST/include"
cp -a "$TMP"/usr/lib/libdxvk_d3d9.so* "$TMP"/usr/lib/libdxvk_dxgi.so* "$DST/lib/"
cp -a "$TMP"/usr/include/dxvk/. "$DST/include/"
cp -a "$TMP"/usr/lib/pkgconfig/dxvk-d3d9.pc "$TMP"/usr/lib/pkgconfig/dxvk-dxgi.pc "$DST/"
echo "DXVK $VER in $DST"
