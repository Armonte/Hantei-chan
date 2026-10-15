#!/bin/bash
# Pixel check of part set renumbering (Hantei_Docs/pat_renumber.md): copies a UNI2 csel project (txt, ha6, cg, pat, pal)
# twice, renumbers the second copy with pattool, shoots every frame of pattern 0 of both with the editor
# (--headless --batch) and requires identical pixels (identical PNG bytes from the same encoder).
#   tools/pat/pat_renumber_shots.sh [build dir]     (GAMEDATA default /mnt/c/dev/hantei-chan/gamedata)
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
B="${1:-$ROOT/build}"
G="${GAMEDATA:-/mnt/c/dev/hantei-chan/gamedata}/uni2/data/_csel"
T="$B/pat_rn_shots"
wp() { wslpath -w "$1"; }
fail=0
# chr  frames  ops
CASES=(
	"007 11 --move 1-11:40 --insert 5:3"
	"018 20 --move 20-25:0 --insert 30:2 --place 2-4:60 --swap 7:9"
	"023 15 --insert 3:10 --swap 13:15 --move 100-120:3"
)
for c in "${CASES[@]}"; do
	read -r chr frames ops <<<"$c"
	rm -rf "$T/$chr"; mkdir -p "$T/$chr/a" "$T/$chr/b"
	for d in a b; do cp "$G"/csel_chr$chr.* "$G"/chr$chr.pal "$T/$chr/$d/"; done
	"$B/pattool.exe" renumber "$(wp "$T/$chr/b/csel_chr$chr.pat")" $ops --no-backup > "$T/$chr/renumber.txt" || { echo "FAIL $chr: pattool renumber"; cat "$T/$chr/renumber.txt"; fail=1; continue; }
	cmp -s "$T/$chr/a/csel_chr$chr.ha6" "$T/$chr/b/csel_chr$chr.ha6" && { echo "FAIL $chr: the HA6 was not rewritten"; fail=1; }
	for d in a b; do
		: > "$T/$chr/$d.batch"
		for ((f = 0; f < frames; f++)); do
			echo "open $(wp "$T/$chr/$d/csel_chr$chr.txt"); pattern 0; frame $f; size 768x768; zoom 0.3; export $(wp "$T/$chr/$d/shot_$f.png")" >> "$T/$chr/$d.batch"
		done
		timeout 600 "$B/gonptechan.exe" --headless --batch "$(wp "$T/$chr/$d.batch")" > /dev/null 2>&1 || { echo "FAIL $chr/$d: batch rc=$?"; fail=1; }
	done
	# negative control: the renumbered .pat with the ORIGINAL HA6 must draw something else in some frame
	mkdir -p "$T/$chr/n"; cp "$T/$chr/a/"csel_chr$chr.* "$T/$chr/a/"chr$chr.pal "$T/$chr/n/"; cp "$T/$chr/b/csel_chr$chr.pat" "$T/$chr/n/"
	: > "$T/$chr/n.batch"
	for ((f = 0; f < frames; f++)); do echo "open $(wp "$T/$chr/n/csel_chr$chr.txt"); pattern 0; frame $f; size 768x768; zoom 0.3; export $(wp "$T/$chr/n/shot_$f.png")" >> "$T/$chr/n.batch"; done
	timeout 600 "$B/gonptechan.exe" --headless --batch "$(wp "$T/$chr/n.batch")" > /dev/null 2>&1
	neg=0; for ((f = 0; f < frames; f++)); do cmp -s "$T/$chr/a/shot_$f.png" "$T/$chr/n/shot_$f.png" || neg=$((neg + 1)); done
	[ $neg -gt 0 ] || { echo "FAIL $chr: negative control (old HA6 + new .pat) draws the same in every frame"; fail=1; }
	same=0
	for ((f = 0; f < frames; f++)); do
		if [ -s "$T/$chr/a/shot_$f.png" ] && cmp -s "$T/$chr/a/shot_$f.png" "$T/$chr/b/shot_$f.png"; then same=$((same + 1)); else echo "FAIL $chr frame $f differs (or missing)"; fail=1; fi
	done
	sz=$(stat -c %s "$T/$chr/a/shot_0.png" 2>/dev/null || echo 0)
	echo "csel_chr$chr ($ops): $same/$frames frames identical, negative control differs in $neg, shot 0 = $sz bytes, $(grep -c . "$T/$chr/renumber.txt") report lines"
done
[ $fail = 0 ] && echo "PASS pat_renumber_shots" || echo "FAIL pat_renumber_shots"
exit $fail
