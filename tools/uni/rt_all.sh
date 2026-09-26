#!/bin/bash
# Round-trip every UNI2/MBTL HA6 through roundtrip.exe --bytes and summarise.
#   tools/uni/rt_all.sh [--fresh] [gamedata dir]
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
RT="$ROOT/build/roundtrip.exe"
OPT=""; [ "$1" = "--fresh" ] && { OPT="--fresh"; shift; }
DATA="${1:-/mnt/c/dev/hantei-chan/gamedata}"
TMP="$ROOT/build/rt_all.tmp"; mkdir -p "$TMP"
declare -A rc=(); fails=()
while IFS= read -r f; do
	key=$(echo "${f#$DATA/}" | tr '/' '_')
	out=$("$RT" --bytes $OPT "$(wslpath -w "$f")" "$(wslpath -w "$TMP/$key")" </dev/null 2>&1)
	c=$?; rc[$c]=$(( ${rc[$c]:-0} + 1 ))
	[ $c -ne 0 ] && fails+=("rc=$c ${f#$DATA/} :: $(echo "$out" | tail -1)")
done < <(find "$DATA/uni2" "$DATA/mbtl" -iname '*.ha6' | sort)
echo -n "HA6 round trip ($OPT):"; for k in "${!rc[@]}"; do echo -n " rc$k=${rc[$k]}"; done; echo
printf "%s\n" "${fails[@]}" | sort | grep -v "^rc=7" | head -40; [ -n "$SHOW7" ] && printf "%s\n" "${fails[@]}" | head -20
