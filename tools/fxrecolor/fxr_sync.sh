#!/usr/bin/env bash
# The runtime recolour grammar + shader maths live in PovertyCaster (pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp).
# Hantei-chan vendors that header BYTE-IDENTICAL as src/cgm/fxr_spec.hpp so the editor previews with the very same code.
#   fxr_sync.sh check [runtime-checkout]   exit 1 when they differ
#   fxr_sync.sh pull  [runtime-checkout]   copy the runtime's header over ours
RT="${2:-/home/teo/dev/PovertyCaster}"
SRC="$RT/pc-adapters/mbaacc/include/mbaacc/FxRecolor.hpp"; DST="$(cd "$(dirname "$0")/../.." && pwd)/src/cgm/fxr_spec.hpp"
[ -f "$SRC" ] || { echo "no $SRC"; exit 2; }
case "${1:-check}" in
  check) cmp -s "$SRC" "$DST" && echo "fxr_spec.hpp in sync" || { echo "fxr_spec.hpp DIFFERS from the runtime header"; exit 1; } ;;
  pull) cp "$SRC" "$DST" && echo copied ;;
esac
