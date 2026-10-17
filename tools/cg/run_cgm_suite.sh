#!/bin/bash
# CG manager suite (docs/cg/cg_manager_design.md section 4), every French-Bread game with BMP Cutter banks.
# Read-only on the game installs and on gamedata/; scratch folders live under build/ and C:\dev\hantei-chan\work and are removed.
# tools/cg/run_cgm_suite.sh [cgmtool.exe] [--quick] [--every] [--jobs N] [--games "MBAACC UNIB UNI2 MBTL MBAC"]
#   --every   round trip + struct on EVERY bank (default: 6 per game), project-refs + folder on every bank that a .txt project draws
#   --jobs N  parallel workers for the non-quick sections (default 4: 12 workers next to other streams aborted 8 big-bank jobs with exit 3, all pass at 4); each job has its own scratch folder
# Sections per game:
#   cgm-bank           parse -> serialize byte-exact, decode == CG::draw_texture (every bank; MBAC/ReAct: the CG inside each .DAT)
#   cgm-palette(-uni)  .pal raw exact + .act/.gpl/.png round trips; UNI2/MBTL pair moves, layout conversion, import of any index
#   cgm-roundtrip      export all -> import unchanged identical; edit locality              (not with --quick)
#   cgm-struct         add/insert/delete/move/permute/clear/unshare/duplicate proofs         (not with --quick)
#   cgm-project-refs   shared bank: insert/move/delete rewrite EVERY project that draws it, shared files untouched, inverse byte-identical
#   cgm-folder         cgtool export -> plain import / sync identical; folder add/remove/reorder; build from folder; resized replace
#   cgm-cc-palettes    PovertyCaster <charID>_<palNum>.png + .pal grown past 36 (MBAACC)
#   texsrc-pat         cgmtex (src/cgm/cgm_texsrc.h, the CG + .pat browser's layer) on EVERY .pat of the game: byte-exact load -> save, every texture
#                      decoded, PNG export -> import of each texture (first 8 per file; --every: all) changes no byte, a texture replaced ->
#                      saved -> reloaded decodes as edited and the parts sampling it follow (MBAACC, UNIB, UNI2, MBTL; ActCadenza has no .pat)
#   texsrc-cg          the same operations on the sampled CG banks through the same layer (cgmtool covers the banks themselves)
#   texsrc-sheet       RBO / GOF2 part sheets (cgmtex sheetcheck, SheetTexSource) in every character archive: PAT block byte-exact on read ->
#                      write, sheet PNG export -> import changes no byte, replace -> undo -> redo byte-exact, the replaced sheet reloads as edited
HERE="$(cd "$(dirname "$0")" && pwd)"; ROOT="$(cd "$HERE/../.." && pwd)"
T="$ROOT/build/cgmtool.exe"; QUICK=0; EVERY=0; JOBS=4; GAMES="MBAACC UNIB UNI2 MBTL MBAC RBO GOF2"
while [ $# -gt 0 ]; do case "$1" in --quick) QUICK=1;; --every) EVERY=1;; --jobs) JOBS="$2"; shift;; --games) GAMES="$2"; shift;; *.exe) T="$1";; esac; shift; done
GD="${CGM_GAMEDATA:-/mnt/c/dev/hantei-chan/gamedata}"
SCR="${CGM_TMP:-C:\\dev\\hantei-chan\\work\\cgm_suite}\\run$$_$RANDOM"   # per process: concurrent suites must not share job folders
w() { while read -r f; do wslpath -w "$f"; done; }
TX="$(dirname "$T")/cgmtex.exe"; TXMAX=8
rc=0; NPAL=40; [ $EVERY -eq 1 ] && { NPAL=100000; TXMAX=0; }
# texsrc <pat list file> <cg list file>: the unified browser's operations (cgmtex) on .pat textures and CG images
texsrc() {
	[ -x "$TX" ] || { echo "SECTION texsrc: skipped (no cgmtex.exe)"; return; }
	local out r; mapfile -t X < <(cat "$1" | w)
	if [ ${#X[@]} -gt 0 ]; then out=$(cd "$ROOT/build" && nice -n 10 "$TX" check --max $TXMAX "${X[@]}" 2>&1); r=$?; echo "$out" | grep -E "^(FAIL|SECTION)"; [ $r -ne 0 ] && { echo "!! cgmtex check (.pat) exited $r"; rc=1; }; fi
	mapfile -t X < <(cat "$2" | w); local S=() n=0 sp=$(( (${#X[@]} + 5) / 6 )); [ $EVERY -eq 1 ] && sp=1
	for b in "${X[@]}"; do [ $((n % sp)) -eq 0 ] && S+=("$b"); n=$((n + 1)); done
	if [ ${#S[@]} -gt 0 ]; then out=$(cd "$ROOT/build" && nice -n 10 "$TX" check --max 16 "${S[@]}" 2>&1); r=$?; echo "$out" | grep -E "^(FAIL|SECTION)"; [ $r -ne 0 ] && { echo "!! cgmtex check (.cg) exited $r"; rc=1; }; fi
}
run() { local out r; out=$(nice -n 10 "$T" "$@" 2>&1); r=$?; echo "$out" | grep -E "^(FAIL|SECTION)"; [ $r -ne 0 ] && { echo "!! $1 $2 exited $r"; rc=1; }; }
# non-quick sections are queued (one tab-separated command per line) and run in parallel at the end; {TMP} becomes a per-job scratch folder
queue() { local IFS=$'\t'; echo "$*" >> "$L/jobs"; }
has_project() { ls "$(dirname "$1")"/*.txt >/dev/null 2>&1; }
game() {   # game <name> <banks (WSL glob list file)> <pals> <project banks...>
	local name="$1"; shift
	echo "=== $name"
	local BL="$1"; mapfile -t B < <(cat "$1" | w); mapfile -t P < <(cat "$2" | w); shift 2
	[ ${#B[@]} -eq 0 ] && { echo "SECTION cgm-bank: skipped ($name data not found)"; return; }
	run check "${B[@]}"
	if [ ${#P[@]} -gt 0 ]; then (cd "$ROOT/build" && run pal-check "${P[@]}"); fi
	if [ $QUICK -eq 0 ]; then
		local n=0 sp=$(( (${#B[@]} + 5) / 6 ))   # struct/roundtrip on every sp-th bank (6 per game) unless --every
		[ $EVERY -eq 1 ] && sp=1
		for b in "${B[@]}"; do
			if [ $((n % sp)) -eq 0 ]; then queue "$name" roundtrip "$b" --tmp "{TMP}"; queue "$name" struct-check "$b"; fi
			n=$((n + 1))
		done
		local PB=("$@")
		if [ $EVERY -eq 1 ] && [ "$name" != MBAACC ]; then   # every bank with a .txt project beside it (character banks; _csel/_talk/_coloredit have none). MBAACC keeps its listed shared banks (all of data/ is one folder)
			PB=(); while read -r b; do case "$b" in *.cg|*.CG) has_project "$b" && PB+=("$b");; esac; done < "$BL"
		fi
		for b in "${PB[@]}"; do queue "$name" project-refs-check "$(wslpath -w "$b")" --tmp "{TMP}"; done
		if [ $EVERY -eq 1 ]; then for b in "${PB[@]}"; do queue "$name" folder-check "$(wslpath -w "$b")" --tmp "{TMP}"; done
		elif [ ${#PB[@]} -gt 0 ]; then queue "$name" folder-check "$(wslpath -w "${PB[0]}")" --tmp "{TMP}"; fi
	fi
}
L=$(mktemp -d); : > "$L/jobs"
for g in $GAMES; do
	case $g in
	MBAACC)
		ls /mnt/c/games/mbaacc/data/*.cg > $L/b 2>/dev/null; ls /mnt/c/games/mbaacc/data/*.pal > $L/p 2>/dev/null
		game MBAACC $L/b $L/p /mnt/c/games/mbaacc/data/akiha.cg /mnt/c/games/mbaacc/data/neco.cg
		ls /mnt/c/games/mbaacc/data/*.pat > $L/t 2>/dev/null; texsrc $L/t $L/b
		run cc-check "$(wslpath -w /mnt/c/games/mbaacc/data/arc.pal)" "$(wslpath -w /mnt/c/games/mbaacc/data/akiha.pal)"
		;;
	UNIB)
		ls /mnt/c/games/unib/data/*/*.cg > $L/b 2>/dev/null; ls /mnt/c/games/unib/data/*/*.pal > $L/p 2>/dev/null
		game UNIB $L/b $L/p /mnt/c/games/unib/data/Hyd_0/Hyd.cg
		find /mnt/c/games/unib/data -iname '*.pat' 2>/dev/null | sort > $L/t; texsrc $L/t $L/b
		;;
	UNI2)
		find "$GD/uni2/data" -iname '*.cg' 2>/dev/null | sort > $L/b; find "$GD/uni2/data" -iname '*.pal' 2>/dev/null | sort > $L/p
		game UNI2 $L/b $L/p "$GD/uni2/data/chr000/chr000.cg" "$GD/uni2/data/chr006/chr006.cg"
		find "$GD/uni2/data" -iname '*.pat' 2>/dev/null | sort > $L/t; texsrc $L/t $L/b
		[ -s $L/p ] && { mapfile -t P < <(grep -v _coloredit $L/p | head -$NPAL | w); run pal-check-uni "${P[@]}"; }
		;;
	MBTL)
		find "$GD/mbtl/data" -iname '*.cg' 2>/dev/null | sort > $L/b; find "$GD/mbtl/data" -iname '*.pal' 2>/dev/null | sort > $L/p
		game MBTL $L/b $L/p "$GD/mbtl/data/chr000/chr000.cg" "$GD/mbtl/data/chr011/chr011.cg"
		find "$GD/mbtl/data" -iname '*.pat' 2>/dev/null | sort > $L/t; texsrc $L/t $L/b
		[ -s $L/p ] && { mapfile -t P < <(grep -v _coloredit $L/p | head -$NPAL | w); run pal-check-uni "${P[@]}"; }
		;;
	MBAC)
		find "$GD/mbac/02" -iname "*.DAT" 2>/dev/null | sort > $L/b; : > $L/p   # character DATs; the 05/BG*.DAT stage files hold a different CG layout (not a character bank)
		game MBAC $L/b $L/p
		: > $L/t; texsrc $L/t $L/b   # no .pat: the CG inside the DATs through the same layer
		[ $QUICK -eq 0 ] && [ $EVERY -eq 0 ] && [ -f "$GD/mbac/02/GAKIHA.DAT" ] && { G=$(wslpath -w "$GD/mbac/02/GAKIHA.DAT"); queue MBAC struct-check "$G"; queue MBAC roundtrip "$G" --tmp "{TMP}"; }
		[ $QUICK -eq 0 ] && [ -f "$GD/mbac/02/GAKIHA.DAT" ] && queue MBAC folder-check "$(wslpath -w "$GD/mbac/02/GAKIHA.DAT")" --tmp "{TMP}"
		;;
	RBO|GOF2)   # part sheets inside the character archives (the CG banks of these games are covered by tools/han2/run_roundtrip.sh "cgm")
		[ -x "$TX" ] || { echo "SECTION texsrc-sheet: skipped (no cgmtex.exe)"; continue; }
		if [ $g = RBO ]; then R=/mnt/c/games/rbo/DATA; A=("$R/DATA01.PAC" "$R/DATA02.PAC" "$R/Update01.PAC" "$R/Ex1Disc.PAC" "$R/Ex2Disc.PAC" "$R/Ex3Disc.pac"); else A=(/mnt/c/games/gof/Data/data0{0,1,2,3,4,5}.dat); fi
		X=(); for a in "${A[@]}"; do [ -f "$a" ] && X+=("$(wslpath -w "$a")"); done
		[ ${#X[@]} -eq 0 ] && continue
		out=$(cd "$ROOT/build" && nice -n 10 "$TX" sheetcheck --max $TXMAX "${X[@]}" 2>&1); r=$?
		echo "$out" | grep -E "^(FAIL|SECTION)" | sed "s|^|[$g] |"; [ $r -ne 0 ] && { echo "!! [$g] cgmtex sheetcheck exited $r"; rc=1; }
		;;
	esac
done
if [ -s "$L/jobs" ]; then
	echo "=== parallel sections: $(wc -l < "$L/jobs") jobs, $JOBS workers"
	export T SCR L
	worker() {   # worker <job number> <tab-separated line>
		local i="$1" a out r; IFS=$'\t' read -r -a a <<< "$2"
		local tmp="$SCR\\j$i"; a=("${a[@]//\{TMP\}/$tmp}")
		out=$(nice -n 10 "$T" "${a[@]:1}" 2>&1); r=$?
		{ echo "$out" | grep -E "^(FAIL|SECTION)" | sed "s|^|[${a[0]}] |"; [ $r -ne 0 ] && echo "!! [${a[0]}] ${a[1]} ${a[2]} exited $r"; } > "$L/out.$i"
		rm -rf "$(wslpath -u "$tmp")" 2>/dev/null; return 0
	}
	export -f worker
	nl -ba -w1 -s$'\t' "$L/jobs" | xargs -d '\n' -P "$JOBS" -I{} bash -c 'l="$1"; worker "${l%%$'"'"'\t'"'"'*}" "${l#*$'"'"'\t'"'"'}"' _ {}
	for i in $(seq 1 "$(wc -l < "$L/jobs")"); do cat "$L/out.$i"; grep -q '^!!\|FAIL' "$L/out.$i" && rc=1; done
fi
rm -rf "$L"
FBC="$ROOT/build/fbchartool.exe"
if [ -x "$FBC" ] && [ $QUICK -eq 0 ] && [ -f /mnt/c/games/MB/MeltyBlood/data03.p ]; then   # foreign (MB strip) bank through the same batch export/import (BankIO over the CG object)
	(nice -n 10 "$FBC" cgio 'C:/games/MB/MeltyBlood/data03.p' AKIHA.DAT "$SCR" | grep -E "^(FAIL|SECTION)") || rc=1
fi
exit $rc
