#!/bin/bash
# Wave 2 regression pass (docs/HANTEI_WAVE2.md §7). Runs every existing
# regression plus the wave 2 unit tests and prints one summary line each.
#
#   tools/wave2_regress.sh [baseline_roundtrip.exe]
#
# The HA6 round trip saves every MBAACC .HA6 with this build's roundtrip.exe
# and with a baseline roundtrip.exe (default: the update/ex-mbac build in the
# main checkout) and compares the written bytes.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$ROOT/build"
BASE_RT="${1:-/mnt/c/dev/hantei-chan/Hantei-chan/build/roundtrip.exe}"
HA6_DIR="${HA6_DIR:-/mnt/c/games/mbaacc/data}"
DAT_DIR="${DAT_DIR:-/mnt/c/games/MB/AC/install/MBACPC/02_extracted}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
wp() { wslpath -w "$1"; }
fail=0
line() { printf '%-28s %s\n' "$1" "$2"; }

for t in shortcut_router_test undo_manager_test workspace_session_test package_tools_test; do
	if [ -x "$B/$t.exe" ]; then
		out="$("$B/$t.exe" 2>&1 | tail -1)"; rc=$?
		line "$t" "$out"
		echo "$out" | grep -q PASS || fail=1
	fi
done

out="$(cd "$B" && ./cmdfile_test.exe '..\tests\fixtures\cmdfile' 2>&1 | tail -1)"
line cmdfile_test "$out"; echo "$out" | grep -qE ' [1-9][0-9]* failed|FAIL' && fail=1

out="$("$B/preview_sim_tool.exe" --selftest 2>&1 | tail -1)"
line "preview_sim_tool --selftest" "$out"; echo "$out" | grep -qE ' [1-9][0-9]* failed|FAIL' && fail=1

# HA6 round trip: new vs baseline bytes.
same=0; diff=0; total=0
if [ -x "$BASE_RT" ]; then
	mkdir -p "$TMP/ha6"
	for f in "$HA6_DIR"/*.HA6; do
		n="$(basename "$f")"
		cp "$f" "$TMP/ha6/$n"
		"$B/roundtrip.exe" "$(wp "$TMP/ha6/$n")" "$(wp "$TMP/ha6/$n.new")" >/dev/null 2>&1
		"$BASE_RT" "$(wp "$TMP/ha6/$n")" "$(wp "$TMP/ha6/$n.base")" >/dev/null 2>&1
		total=$((total+1))
		if cmp -s "$TMP/ha6/$n.new" "$TMP/ha6/$n.base"; then same=$((same+1)); else diff=$((diff+1)); echo "  HA6 differs: $n"; fi
	done
	line "HA6 roundtrip vs baseline" "$same/$total identical"
	[ "$diff" -eq 0 ] || fail=1
else
	line "HA6 roundtrip" "SKIPPED (no baseline $BASE_RT)"
fi

# MBAC .DAT byte round trip.
out="$(cd "$DAT_DIR" && "$B/ha4tool.exe" roundtrip *.DAT 2>&1 | tail -1)"
line "ha4tool roundtrip" "$out"

# Stage files.
out="$("$ROOT/tools/bg_regress.sh" 600 2>&1 | tail -2 | tr '\n' ' ')"
line "bg_regress.sh 600" "$out"
echo "$out" | grep -qi 'fail [1-9]\|FAIL' && fail=1

exit $fail
