#!/bin/bash
# French-Bread format suite: every shipped archive on this machine through the generalized archive layer (src/fbarc).
# Usage: tools/fb/run_fb_suite.sh [path/to/fbarctool.exe]    (read-only; nothing is copied; large temp files go to %TEMP% and are removed)
# Sections must end with "skipped 0". n/a (justified): game files named *.dat / *.p that are NOT archives are listed explicitly below.
T=${1:-$(dirname "$0")/../../build/fbarctool.exe}
rc=0
G=/mnt/c/games; F=/mnt/c/dev/frenchbread
gather() { # print Windows paths of the shipped archives (explicit globs only, no tree walks)
	ls $G/MB/AC/*.p $G/MB/MeltyBlood/*.p $G/MB/R/*.p $G/mbaacc/0*.p $G/mbaacc_dev/0*.p $G/mbaacc_tag/0*.p \
	   $G/gof/Data/data0*.dat $G/gof/*.PAC $G/gof2_run/Data/data0*.dat $G/gof1/run/gof_0*.p \
	   $G/pb/0*.dat $G/rbo/DATA/*.PAC $G/rbo/DATA/*.pac $G/rbo_run/Data/*.PAC \
	   $F/dmp_1020/DATA/*.PAC $F/drill_milky_punch/files/dMp/dMp/Data/*.PAC $F/aquat1c_dmp/*/DATA/*.PAC "$F/aquat1c_dmp/Drill Milky Punch/DATA"/*.PAC \
	   "$F/benibara_rendan/files/Rosa Chinensis Four hand/Data"/*.PAC "$F/yamayuri_rendan/files/omake/Rosa Chinensis Four hand/Data"/*.PAC \
	   $F/yamayuri_rendan/files/data/*.p 2>/dev/null | sed 's|^/mnt/\(.\)/|\U\1\E:/|'
}
mapfile -t ARCS < <(gather)
echo "== archives found: ${#ARCS[@]}"
echo "== census"; "$T" count "${ARCS[@]}" | grep -E "^(FAIL)" ; [ ${PIPESTATUS[0]} -ne 0 ] && rc=1
echo "== byte-exact rebuild (no edits)";   out=$("$T" rt "${ARCS[@]}" 2>&1) || rc=1; echo "$out" | grep -E "^(DIFF|FAIL|SECTION)"
echo "$out" | grep -E "^SECTION" | grep -qv "skipped 0$" && { echo "!! skipped files"; rc=1; }
# edited rebuild on one archive of every kind (replace + remove + add, reopen, compare every entry)
EDITS=("$G/MB/AC/03.p" "$G/MB/AC/10.p" "$G/mbaacc/0008.p" "$G/mbaacc/0004.p" "$G/MB/MeltyBlood/data00.p" "$G/MB/R/04.p" "$G/pb/04.dat" "$G/gof1/run/gof_00.p" "$G/rbo/DATA/Update01.PAC")
EDITS=("${EDITS[@]/#\/mnt\/c/C:}")
echo "== edited rebuild"; out=$("$T" edittest "${EDITS[@]}" 2>&1) || rc=1; echo "$out" | grep -E "^(FAIL|SECTION)"
echo "n/a: $G/gof/Data/{System00,uninst}.dat, $G/pb/pbex.dat are installer / trainer files, not archives (headers carry no archive magic)"
exit $rc
