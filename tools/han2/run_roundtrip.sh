#!/bin/bash
# Full han2 verification over every shipped RBO / GOF2 / GOF1 archive (needs the game installs; read-only, nothing is copied).
# Usage: tools/han2/run_roundtrip.sh [path/to/han2tool.exe]
# Every byte round trip section must end with "skipped 0"; "n/a" counts are files that belong to ANOTHER section (see docs/formats/frenchbread_rbo_gof.md section 7).
T=${1:-$(dirname "$0")/../../build/han2tool.exe}
RBO='C:\games\rbo\DATA'
RARCS=("$RBO\\DATA01.PAC" "$RBO\\DATA02.PAC" "$RBO\\Update01.PAC" "$RBO\\Ex1Disc.PAC" "$RBO\\Ex2Disc.PAC" "$RBO\\Ex3Disc.pac" "$RBO\\BG01.PAC" "$RBO\\BG02.PAC" "$RBO\\BGM.PAC" "$RBO\\SE.PAC" "$RBO\\ETC.PAC" "$RBO\\CG.PAC")
GARCS=(C:/games/gof/Data/data0{0,1,2,3,4,5}.dat)
G1ARCS=(C:/games/gof1/run/gof_0{0,1,2,3}.p)
rc=0
run() { # run <title> <args...>: show the per-section result lines, keep every FAIL line, remember a failing exit code
	local title=$1; shift
	echo "== $title"
	out=$("$T" "$@" 2>&1); r=$?
	echo "$out" | grep -E "^(FAIL|DIFF|SECTION|OK  |animtest|NOTE|    )"
	[ $r -ne 0 ] && { echo "!! $title exited $r"; rc=1; }
	if echo "$out" | grep -E "^SECTION" | grep -qv "skipped 0 "; then echo "!! $title reports skipped files"; rc=1; fi
}
run "PAC entry counts"                         count "${RARCS[@]}" "${GARCS[@]}"
run "PAC rebuild byte-identical"               pacrt "${RARCS[@]}" "${GARCS[@]}"
run "container: HAN2RBO parse -> serialize"    roundtrip "${RARCS[@]}" "${GARCS[@]}"
run "model: load into Hantei-chan, save"       modelrt "${RARCS[@]}" "${GARCS[@]}"
run "pat: PAT parts reader + writer"           patrt "${RARCS[@]}" "${GARCS[@]}"
run "img: IMG parse -> serialize"              imgrt "${RARCS[@]}" "${GARCS[@]}"
run "cg: CG bank load / re-import"             cgrt "${RARCS[@]}" "${GARCS[@]}"
run "opaque: entries without a reader"         opaquert "${RARCS[@]}" "${GARCS[@]}"
run "gof1: GOF1 archives + character .DAT"     gof1rt "${G1ARCS[@]}"
run "animtest (behaviour check, not bytes)"    animtest "${RARCS[@]}" "${GARCS[@]}"
exit $rc
