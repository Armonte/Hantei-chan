#!/bin/bash
# Round-trip every UNI2/MBTL .pat through pat_roundtrip.exe (field fingerprints
# + byte identity); with --fresh every record is re-encoded (field check only).
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$ROOT/build/pat_roundtrip.exe"
MODE="--bytes"; [ "$1" = "--fresh" ] && { MODE="--fresh"; shift; }
DATA="${1:-/mnt/c/dev/hantei-chan/gamedata}"
TMP="$ROOT/build/pat_rt.tmp"; mkdir -p "$TMP"
declare -A rc=(); fails=()
while IFS= read -r f; do
	out=$("$RT" $MODE "$(wslpath -w "$f")" "$(wslpath -w "$TMP/x.pat")" </dev/null 2>&1)
	c=$?; rc[$c]=$(( ${rc[$c]:-0} + 1 ))
	[ $c -ne 0 ] && fails+=("rc=$c ${f#$DATA/} :: $(echo "$out" | grep -v '^\[' | head -2 | tr '\n' ' ')")
done < <(find "$DATA/uni2" "$DATA/mbtl" -iname '*.pat' | sort)
echo -n "PAT round trip ($MODE):"; for k in "${!rc[@]}"; do echo -n " rc$k=${rc[$k]}"; done; echo
printf '%s\n' "${fails[@]}" | head -20
