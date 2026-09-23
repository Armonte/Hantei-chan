#!/bin/bash
# HA6 save regression (issues #71/#76/#68 family).
#
#   tools/ha6_regress.sh
#
# 1. Single-file round trip (roundtrip.exe) of every MBAACC and UNIB HA6:
#    prints the exit-code histogram (0 = no field diffs, 5 = the known
#    inverted/degenerate box cleanup on 31 MBAACC files).
# 2. Stacked save: every .txt with several HA6 files is loaded the way the
#    editor loads it (later files overlay earlier ones; in a UNI stack the files
#    after the character's own file only fill empty slots) and saved with the
#    editor's target file. The result must be byte-identical to a single-file
#    round trip of that target file: nothing from the other files of the stack
#    is baked into it.
#
# Data folders are read-only; outputs go to a temp dir. Override with
#   HA6_MBAACC_DIR (default /mnt/c/games/mbaacc_tag/data)
#   HA6_UNI_DIR    (default /mnt/c/games/unib/data)
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RT="${RT_EXE:-$ROOT/build/roundtrip.exe}"
MBAACC_DIR="${HA6_MBAACC_DIR:-/mnt/c/games/mbaacc_tag/data}"
UNI_DIR="${HA6_UNI_DIR:-/mnt/c/games/unib/data}"
[ -x "$RT" ] || { echo "build first: $RT missing"; exit 2; }
# roundtrip.exe is a Windows binary: give it Windows paths, and keep outputs on
# a Windows drive (build/) so the paths stay short.
TMP="$ROOT/build/ha6_regress.tmp"
rm -rf "$TMP"; mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
wp() { if command -v wslpath >/dev/null; then wslpath -w "$1"; else echo "$1"; fi; }
fail=0

single() { # dir label
	local dir="$1" label="$2"; declare -A rc=()
	while IFS= read -r f; do
		local key; key=$(echo "${f#$dir/}" | tr '/' '_')
		"$RT" "$(wp "$f")" "$(wp "$TMP/$key")" </dev/null >/dev/null 2>&1
		local c=$?; rc[$c]=$(( ${rc[$c]:-0} + 1 ))
		[ "$c" -ne 0 ] && [ "$c" -ne 5 ] && { echo "  ROUNDTRIP FAIL rc=$c $f"; fail=1; }
	done < <(find "$dir" -iname '*.ha6' | sort)
	echo -n "$label single round trip:"; for k in "${!rc[@]}"; do echo -n " rc$k=${rc[$k]}"; done; echo
}

txtval() { grep -a -m1 "^$2=" "$1" | tr -d '\r' | cut -d= -f2 | tr -d ' '; }

stacks() { # dir label
	local dir="$1" label="$2" ok=0 bad=0 inh=0
	while IFS= read -r t; do
		local num; num=$(txtval "$t" FileNum); [ -z "$num" ] && continue; [ "$num" -lt 2 ] && continue
		local tdir; tdir=$(dirname "$t"); local files=() names=()
		for ((i=0;i<num;i++)); do local n; n=$(txtval "$t" "$(printf 'File%02d' $i)"); names+=("$n"); files+=("$(wp "$tdir/$n")"); done
		# Save target = FrameData::StackSaveTarget: the highest-indexed file
		# that is not shared data (a ../ path or BaseData).
		local own=$((num-1))
		while [ $own -gt 0 ] && { echo "${names[$own]}" | grep -qi basedata || [ "${names[$own]:0:3}" = "../" ]; }; do own=$((own-1)); done
		[ -f "$tdir/${names[$own]}" ] || continue
		local base; base=$(basename "$t" .txt)
		local out; out=$("$RT" --stack $own "$(wp "$TMP/stack_$base.ha6")" "${files[@]}" </dev/null 2>/dev/null)
		inh=$(( inh + $(echo "$out" | grep -o 'left out: [0-9]*' | awk '{print $3}') ))
		"$RT" "$(wp "$tdir/${names[$own]}")" "$(wp "$TMP/own_$base.ha6")" </dev/null >/dev/null 2>&1
		if cmp -s "$TMP/stack_$base.ha6" "$TMP/own_$base.ha6"; then ok=$((ok+1)); else bad=$((bad+1)); fail=1; echo "  STACK DIFF $t"; fi
	done < <(find "$dir" -iname '*.txt' | sort)
	echo "$label stacked save == own-file save: ok=$ok bad=$bad (inherited patterns kept out: $inh)"
}

[ -d "$MBAACC_DIR" ] && { single "$MBAACC_DIR" MBAACC; stacks "$MBAACC_DIR" MBAACC; }
[ -d "$UNI_DIR" ] && { single "$UNI_DIR" UNI; stacks "$UNI_DIR" UNI; }
# UNI2 / MBTL project stacks (base _0 projects and MBTL 4-file variants).
GAMEDATA="${HA6_GAMEDATA_DIR:-/mnt/c/dev/hantei-chan/gamedata}"
[ -d "$GAMEDATA/uni2/data" ] && stacks "$GAMEDATA/uni2/data" UNI2
[ -d "$GAMEDATA/mbtl/data" ] && stacks "$GAMEDATA/mbtl/data" MBTL
[ $fail -eq 0 ] && echo HA6_REGRESS_PASS || echo HA6_REGRESS_FAIL
exit $fail
