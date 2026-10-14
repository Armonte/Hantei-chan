#!/bin/bash
# ctest wrapper for ha4_golden.py: convert every MBAC .DAT, then run the golden + behaviour checks.
#   ha4_golden_ctest.sh <ha4tool.exe> <mbac .DAT dir> <mbaacc data dir> <out dir>
set -eu
TOOL="$1"; DAT="$2"; CC="$3"; OUT="$4"
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$OUT"
win() { if command -v wslpath >/dev/null 2>&1; then wslpath -w "$1"; else echo "$1"; fi; }
(cd "$DAT" && "$TOOL" convert *.DAT -o "$(win "$OUT")" > "$OUT/convert_log.txt" 2>&1) || { tail -20 "$OUT/convert_log.txt"; exit 1; }
# ha4tool names outputs after the lower-case stem; the golden script expects <name>.ha6 directly in OUT
python3 "$HERE/ha4_golden.py" "$OUT" "$CC" > "$OUT/golden.txt" 2>&1; rc=$?
grep -E "^(golden|behaviour):|^BAD" "$OUT/golden.txt" || true
exit $rc
