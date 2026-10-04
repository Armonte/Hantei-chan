#!/bin/bash
# CG manager suite (docs/cg/cg_manager_design.md section 4). Read-only on the game installs; scratch export folders live under build/ and are removed.
# Usage: tools/cg/run_cgm_suite.sh [path/to/cgmtool.exe] [--quick]      (--quick: skip the slow export/import round trip)
# Sections: cgm-bank (parse -> serialize byte-exact, decode == CG::draw_texture), cgm-roundtrip (export all -> import unchanged -> identical bytes; edit locality).
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
T="${1:-$ROOT/build/cgmtool.exe}"; QUICK=0; [ "$2" = "--quick" ] && QUICK=1
DATA="${CGM_DATA:-/mnt/c/games/mbaacc/data}"
SCR="$(wslpath -w "$ROOT/build")\\cgm_rt"
mapfile -t BANKS < <(ls "$DATA"/*.cg | while read -r f; do wslpath -w "$f"; done)
rc=0
nice -n 10 "$T" check "${BANKS[@]}" | grep -E "^(FAIL|SECTION)" || rc=1
[ ${PIPESTATUS[0]} -ne 0 ] && rc=1
if [ $QUICK -eq 0 ]; then
	for b in "${BANKS[@]}"; do
		nice -n 10 "$T" roundtrip "$b" --tmp "$SCR" | grep -E "^FAIL" ; [ ${PIPESTATUS[0]} -ne 0 ] && rc=1
	done
	echo "SECTION cgm-roundtrip: ${#BANKS[@]} banks processed (any FAIL lines above)"
fi
exit $rc
