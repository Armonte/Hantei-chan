#!/bin/bash
# tools/uni/s1_compare/shoot_batch.sh [new.exe]: every editor view of every case and control (cases.txt +
# cases_control.txt), on a black and a white clear colour (<name>_new_k.png / <name>_new_w.png), plus a 64-grey one (<name>_new_g.png), in ONE hidden
# (--headless) editor process driven by a --batch file. No window is ever shown.
# Extra per-layer passes (LAYER_SHOTS, "name|base case|layers" entries) render only some AF layers of a case
# (--layer): pras_chr020_p5f2 mixes a CG layer (stage gradient) and a .pat layer (none); v2_model.py --layers.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; H=/mnt/c/dev/hantei-chan
NEW="${1:-$D/build/gonptechan.exe}"
W=$H/work/s1batch${WORK_SUFFIX:-}; OUT="${OUTDIR:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}"; mkdir -p "$W" "$OUT"
cp "$NEW" "$W/gonptechan.exe"
G='C:\dev\hantei-chan\gamedata'
WO="$(wslpath -w "$W")"
LAYER_SHOTS="${LAYER_SHOTS:-pras_chr020_p5f2_cg|pras_chr020_p5f2|1
pras_chr020_p5f2_pat|pras_chr020_p5f2|0}"
printf "[Other settings][]\nposX=-2000\nposY=10\nsizeX=1280\nsizeY=900\nColor=0.000000,0.000000,0.000000\n" > "$W/hanteichan.ini"
B="$W/shots.batch"; : > "$B"
rows() { cat "$(dirname "$0")/cases.txt" "$(dirname "$0")/cases_control.txt" | grep -v "^#" | grep -E "${CASES:-.}"; }
line() {  # name txt pat fr tick zoom layers
	local name=$1 txt=$2 pat=$3 fr=$4 tick=$5 zoom=$6 lay=$7 bg
	for bg in k w g; do   # g: a 64-grey backdrop, so additive parts do not clip (v2_model.py --grey)
		local c=$(case $bg in k) echo 0,0,0;; w) echo 255,255,255;; g) echo 64,64,64;; esac)
		local t=""; [ "$tick" != "-1" ] && t="; tick $tick"
		echo "open $G\\$txt; size 1264x861; boxes 0; layer $lay; bg $c; zoom $zoom; pattern $pat; frame $fr$t; export $WO\\${name}_new_$bg.png" >> "$B"
	done
}
while IFS='|' read -r name txt pat fr tick zoom note; do
	name=$(echo $name); [ -z "$name" ] && continue
	line "$name" "$(echo $txt | tr / '\\')" $(echo $pat) $(echo $fr) $(echo $tick) $(echo $zoom) all
done < <(rows)
while IFS='|' read -r name base lay; do
	[ -z "$name" ] && continue
	r=$(cat "$(dirname "$0")/cases.txt" "$(dirname "$0")/cases_control.txt" | grep -v "^#" | grep "^$base ")
	[ -z "$r" ] && continue
	IFS='|' read -r _ txt pat fr tick zoom note <<< "$r"
	line "$name" "$(echo $txt | tr / '\\')" $(echo $pat) $(echo $fr) $(echo $tick) $(echo $zoom) "$lay"
done <<< "$LAYER_SHOTS"
rm -f "$W"/*_new_[kwg].png
(cd "$W" && timeout 900 ./gonptechan.exe --headless --batch "$WO\\shots.batch" </dev/null >/dev/null 2>&1); rc=$?
n=$(ls "$W"/*_new_[kwg].png 2>/dev/null | wc -l)
cp "$W"/*_new_[kwg].png "$OUT/" 2>/dev/null
echo "batch rc=$rc, $n shots ($(wc -l < "$B") lines) -> $OUT"
