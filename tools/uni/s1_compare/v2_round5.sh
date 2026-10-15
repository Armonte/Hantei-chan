#!/bin/bash
# tools/uni/s1_compare/v2_round5.sh: render-match4 round 5 for the four open v2 cases (flipHV, rect, MBTL flipV,
# pups chr027) and their controls. Captures: tools/gamecap/cases/v2r5_*.gcs (same-session plates).
# Output: $OUT/round5.log, JSON per run in $OUT/r5_json/.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare
OUT="${OUT:-$D/docs/audit_2026_10_14/evidence/compare/v2}"; J=$OUT/r5_json; mkdir -p "$J"
LOG=$OUT/round5.log; : > "$LOG"
run() {
	local tag=$1; shift
	echo "== $tag: v2_model.py $*" >> "$LOG"
	python3 "$T/v2_model.py" "$@" --grey --json "$J/$tag.json" 2>&1 | grep -E "^[a-z].*: raw|gradient|no footprint" >> "$LOG"
}
L0="--light 1,0;1,0;1,0"
G19="--grad-ctrl ctrl_chr019_p0,ctrl_chr019_p0_r5"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5"
# 1. flipHV: same-session plate (r5); zoom 1.5 shot and zoom 1 shot (no 1.5x resample of the thin streak edges)
run ctrl_chr019_r5 ctrl_chr019_p0 --game-name ctrl_chr019_p0_r5 $G19 $L0
run flipHV_r5 flipHV_chr019_p153 --game-name flipHV_chr019_p153_r5 $L0
run ctrl_chr019_r5_z1 ctrl_chr019_p0_z1 --game-name ctrl_chr019_p0_r5 --grad-ctrl ctrl_chr019_p0_z1,ctrl_chr019_p0_r5 $L0
run flipHV_r5_z1 flipHV_chr019_p153_z1 --game-name flipHV_chr019_p153_r5 $L0
# 2. rect (unchanged capture)
echo "== rect thin" >> "$LOG"; python3 "$T/v2_thin.py" rect_chr024_p76_z1 --game-name rect_chr024_p76_unlit 2>&1 | tail -5 >> "$LOG"
# 3. MBTL .pat-only controls, then the flipV case
run mbtl_patctrl_p238 mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nb $L0
run mbtl_patctrl_p277 mbtl_patctrl_chr001_p277f0 --game-name mbtl_patctrl_chr001_p277f0_nb $L0
run mbtl_flipV mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb $L0
# 4. pups chr027 on frames where the PUPS-bank part is on screen
run ctrl_chr027_r5 ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 $G27 $L0
run pups_f4 pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 $G27 $L0
run pups_f6 pups_chr027_p449f6 --game-name pups_chr027_p449f6_r5 $G27 $L0
run pups_f4_nograd pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 $L0
cat "$LOG"
