#!/bin/bash
# Lossless .pal round trip (PalFile) over every UNI2/MBTL palette (+ MBAACC's).
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$ROOT/build/pal_roundtrip.exe"
DATA="${1:-/mnt/c/dev/hantei-chan/gamedata}"
MBAACC="${HA6_MBAACC_DIR:-/mnt/c/games/mbaacc_tag/data}"
n=0; bad=0; declare -A kinds=()
while IFS= read -r f; do
	out=$("$RT" "$(wslpath -w "$f")" "$(wslpath -w "$ROOT/build/pal_rt.tmp.pal")" </dev/null 2>&1) || { bad=$((bad+1)); echo "DIFF $f: $out"; }
	k=$(echo "$out" | awk '{print $1, $2}'); kinds[$k]=$(( ${kinds[$k]:-0} + 1 ))
	n=$((n+1))
done < <(find "$DATA/uni2" "$DATA/mbtl" "$MBAACC" -iname '*.pal' 2>/dev/null | sort)
echo "PAL round trip: $n files, $bad differ"; for k in "${!kinds[@]}"; do echo "  $k: ${kinds[$k]}"; done
