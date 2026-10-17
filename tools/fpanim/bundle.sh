#!/bin/bash
# Fill res/fpan-im/ with the fpan-im binaries Hantei-chan installs into UNI2 / MBTL (Game > fpan-im window).
#   tools/fpanim/bundle.sh                 copies from /mnt/c/dev/fbmod/build/bin (fpan-im's ./build.sh output)
#   FPANIM_BIN=/path/to/bin tools/fpanim/bundle.sh
#   tools/fpanim/bundle.sh --clean         removes the bundle again
# Layout written:  res/fpan-im/winmm.dll   res/fpan-im/fpan-im/fpan-im.dll   res/fpan-im/VERSION
# The copies are stripped (i686-w64-mingw32-strip; the version resource survives). The binaries are git-ignored:
# fpan-im is a private repo, so the bundle is optional and the build passes without it. docs/FPANIM_BUNDLE.md.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="$ROOT/res/fpan-im"
if [ "${1:-}" = "--clean" ]; then rm -f "$OUT/winmm.dll" "$OUT/fpan-im/fpan-im.dll" "$OUT/VERSION"; rmdir "$OUT/fpan-im" "$OUT" 2>/dev/null || true; echo "bundle removed"; exit 0; fi
BIN="${FPANIM_BIN:-/mnt/c/dev/fbmod/build/bin}"
for f in winmm.dll fpan-im.dll; do
	[ -f "$BIN/$f" ] || { echo "missing $BIN/$f (build fpan-im first: cd /mnt/c/dev/fbmod && ./build.sh)" >&2; exit 1; }
done
STRIP="$(command -v i686-w64-mingw32-strip || true)"
mkdir -p "$OUT/fpan-im"
cp "$BIN/winmm.dll" "$OUT/winmm.dll"
cp "$BIN/fpan-im.dll" "$OUT/fpan-im/fpan-im.dll"
if [ -n "$STRIP" ]; then "$STRIP" --strip-unneeded "$OUT/winmm.dll" "$OUT/fpan-im/fpan-im.dll"; else echo "warning: i686-w64-mingw32-strip not found, copies not stripped" >&2; fi
# VERSION: fpan-im's git describe when the bin folder sits in its repo, else the CMake project version.
SRC="$(cd "$BIN/../.." 2>/dev/null && pwd || true)"
VER=""
if [ -n "$SRC" ] && git -C "$SRC" rev-parse --git-dir >/dev/null 2>&1; then
	VER="$(git -C "$SRC" describe --tags --always --dirty 2>/dev/null || true)"
	PV="$(sed -n 's/^project(fpan-im VERSION \([0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt" 2>/dev/null | head -1)"
	[ -n "$PV" ] && VER="$PV ($VER)"
fi
echo "${VER:-unknown}" > "$OUT/VERSION"
echo "bundled fpan-im ${VER:-unknown} from $BIN into $OUT"
ls -l "$OUT/winmm.dll" "$OUT/fpan-im/fpan-im.dll"
