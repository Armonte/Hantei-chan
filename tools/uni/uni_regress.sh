#!/bin/bash
# UNI2 / MBTL regression, plus the MBAACC / MBAC suites they share code with.
#
#   tools/uni/uni_regress.sh            (build first: ./build.sh)
#
# 1. HA6: all 262 UNI2/MBTL files byte-identical after load -> save
#    (roundtrip.exe --bytes), and field-identical with the loaded encoding
#    dropped (--fresh; MBTL chr016's inverted boxes then count as "edited" and
#    get normalised, so it is the one expected rc5 there).
# 2. PAT: all 135 UNI2/MBTL .pat byte-identical (+ --fresh field check),
#    MBAACC .pat byte-identical.
# 3. PAL: every UNI2/MBTL (130-palette) and MBAACC .pal byte-identical.
# 4. #71 saved edits: load a project .txt stack, edit one pattern, save: only
#    that pattern may differ from the original file (base and MBTL variant
#    projects; the variant saves into chrNNN_N.ha6, #46).
# 5. tools/ha6_regress.sh: MBAACC / UNIB single and stacked saves, UNI2/MBTL stacks.
# 6. ha4tool roundtrip (MBAC .DAT), tools/bg_regress.sh, cmdfile and unit tests.
# Game data: GAMEDATA (default /mnt/c/dev/hantei-chan/gamedata), read only.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="$ROOT/build"
G="${GAMEDATA:-/mnt/c/dev/hantei-chan/gamedata}"
MBAACC="${HA6_MBAACC_DIR:-/mnt/c/games/mbaacc_tag/data}"
MBAC_DAT="${MBAC_DAT_DIR:-/mnt/c/games/MB/AC/install/MBACPC/02_extracted}"
wp() { wslpath -w "$1"; }
fail=0
note() { echo "== $*"; }
check() { # label cmd... ; fails on nonzero exit
	local label="$1"; shift
	if "$@"; then :; else echo "FAIL: $label"; fail=1; fi
}

note "1. UNI2/MBTL HA6"
out=$("$ROOT/tools/uni/rt_all.sh" "$G"); echo "$out"
echo "$out" | head -1 | grep -q "rc0=262$" || { echo "FAIL: HA6 bytes"; fail=1; }
out=$("$ROOT/tools/uni/rt_all.sh" --fresh "$G"); echo "$out" | head -1
echo "$out" | grep "^rc=" | grep -v "^rc=7\|chr016/chr016.ha6" && { echo "FAIL: HA6 fresh"; fail=1; }

note "2. PAT"
out=$("$ROOT/tools/uni/pat_rt_all.sh" "$G"); echo "$out"
echo "$out" | head -1 | grep -q "rc0=135$" || { echo "FAIL: PAT bytes"; fail=1; }
out=$("$ROOT/tools/uni/pat_rt_all.sh" --fresh "$G"); echo "$out"
echo "$out" | head -1 | grep -q "rc0=135$" || { echo "FAIL: PAT fresh"; fail=1; }
n=0; bad=0
for f in "$MBAACC"/*.pat; do [ -f "$f" ] || continue; n=$((n+1)); "$B/pat_roundtrip.exe" --bytes "$(wp "$f")" "$(wp "$B/mbaacc_rt.pat")" </dev/null >/dev/null 2>&1 || bad=$((bad+1)); done
echo "MBAACC .pat byte round trip: $n files, $bad differ"; [ $bad -eq 0 ] || fail=1

note "3. PAL"
out=$("$ROOT/tools/uni/pal_rt_all.sh" "$G"); echo "$out"
echo "$out" | grep -q ", 0 differ" || { echo "FAIL: PAL"; fail=1; }

note "4. #71 saved edits (only the edited pattern changes)"
edit_case() { # dir txt pattern
	local dir="$1" txt="$2" pat="$3" files=() names=() num own
	num=$(grep -a -m1 '^FileNum=' "$dir/$txt" | tr -d ' \r' | cut -d= -f2)
	for ((i=0;i<num;i++)); do local n; n=$(grep -a -m1 "^$(printf 'File%02d' $i)=" "$dir/$txt" | tr -d ' \r' | cut -d= -f2); names+=("$n"); files+=("$(wp "$dir/$n")"); done
	own=$((num-1)); while [ $own -gt 0 ] && { echo "${names[$own]}" | grep -qi basedata || [ "${names[$own]:0:3}" = "../" ]; }; do own=$((own-1)); done
	"$B/uni_edit_test.exe" $own $pat "$(wp "$B/edit71.ha6")" "${files[@]}" </dev/null >/dev/null 2>&1 || { echo "FAIL: edit $txt"; fail=1; return; }
	local r; r=$(python3 "$ROOT/tools/uni/ha6patdiff.py" "$dir/${names[$own]}" "$B/edit71.ha6")
	echo "  $txt -> ${names[$own]}, edit p$pat: $r"
	[ "$r" = "1 pattern(s) differ: $pat" ] || { echo "FAIL: edit $txt"; fail=1; }
}
edit_case "$G/uni2/data/chr000" chr000_0.txt 100
edit_case "$G/uni2/data/chr027" chr027_0.txt 128
edit_case "$G/mbtl/data/chr000" chr000_0.txt 0
edit_case "$G/mbtl/data/chr001" chr001_7.txt 17
edit_case "$G/mbtl/data/chr016" chr016_0.txt 6

note "5. HA6 stacks and MBAACC/UNIB round trips"
out=$(RT_EXE="${RT_EXE:-$B/roundtrip.exe}" "$ROOT/tools/ha6_regress.sh" 2>&1); echo "$out" | tail -8
echo "$out" | grep -q HA6_REGRESS_PASS || { echo "FAIL: ha6_regress"; fail=1; }

note "6. MBAC, stages, command files, unit tests"
if [ -d "$MBAC_DAT" ]; then
	out=$(cd "$MBAC_DAT" && "$B/ha4tool.exe" roundtrip *.DAT 2>&1 | tail -3); echo "$out"
fi
if [ -x "$ROOT/tools/bg_regress.sh" ]; then
	out=$("$ROOT/tools/bg_regress.sh" 600 2>&1 | tail -4); echo "$out"
	echo "$out" | grep -qE "(^|[^0-9])[1-9][0-9]* failed|failures: [1-9]" && { echo "FAIL: bg_regress"; fail=1; }
fi
(cd "$ROOT" && check cmdfile_test "$B/cmdfile_test.exe" 'tests\fixtures\cmdfile' >/dev/null)
for t in undo_manager_test shortcut_router_test refs_test pattern_search_test shared_clipboard_test; do
	[ -x "$B/$t.exe" ] && (cd "$ROOT" && check $t "$B/$t.exe" >/dev/null 2>&1)
done
echo "unit tests done"

[ $fail -eq 0 ] && echo UNI_REGRESS_PASS || echo UNI_REGRESS_FAIL
exit $fail
