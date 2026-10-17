#!/bin/bash
# tools/uni/s1_compare/v2_round4.sh: re-measure all 15 v2 cases and their own controls (render-match3) from the
# shoot_batch.sh shots. Every v2_model run uses the 64-grey backdrop (--grey: additive parts no longer clip on white).
# Output: one line per run in $OUT/round4.log, JSON per run in $OUT/r4_json/.
set -u
BWDIR="${BWDIR:-}"; D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare
OUT="${OUT:-$D/docs/audit_2026_10_14/evidence/compare/v2}"; J=$OUT/r4_json; mkdir -p "$J"
V2=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2; S1=/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare
LOG=$OUT/round4.log; : > "$LOG"
run() {  # tag, v2_model args...
	local tag=$1; shift
	echo "== $tag: v2_model.py $*" >> "$LOG"
	python3 "$T/v2_model.py" "$@" ${BWDIR:+--bw $BWDIR} --grey --json "$J/$tag.json" 2>&1 | grep -E "^[a-z].*: raw|ring|gradient|per-layer" >> "$LOG"
}
GR="--grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3"
# controls (floors)
run ctrl_chr020_r3 ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR
G24="--grad-ctrl ctrl_chr024_p0,ctrl_chr024_p0_unlit"; G19="--grad-ctrl ctrl_chr019_p0,ctrl_chr019_p0_unlit"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_unlit"
# UNI2 CG sprites carry the stage chara gradient: each control is calibrated on itself, its cases use it unchanged
run ctrl_chr024_unlit ctrl_chr024_p0 --game-name ctrl_chr024_p0_unlit $G24
run ctrl_chr019_unlit ctrl_chr019_p0 --game-name ctrl_chr019_p0_unlit --plate $V2/flipHV_chr019_p153_plate.png $G19 --light "1,0;1,0;1,0"
run ctrl_chr027_unlit ctrl_chr027_p0 --game-name ctrl_chr027_p0_unlit --plate $S1/game/pups_chr027_p449_game_plate.png $G27 --light "1,0;1,0;1,0"
run ctrl_chr000 ctrl_chr000_p0f0 --game $S1/control --aura-fit
run ctrl_chr025 ctrl_chr025_p0f0 --game $S1/control --aura-fit
run mbtl_ctrl mbtl_ctrl_chr001_p0f0 --game-name mbtl_ctrl_chr001_p0f0_nb --mbtl-light bg007,330
# UNI2 cases
run flipHV flipHV_chr019_p153 --game-name flipHV_chr019_p153_unlit --plate $V2/flipHV_chr019_p153_plate.png --light "1,0;1,0;1,0"
run pups_chr027 pups_chr027_p449 --game-name pups_chr027_p449_unlit --plate $S1/game/pups_chr027_p449_game_plate.png $G27
for c in pral2_chr025_p133f1 flipH_chr000_p96f1 flipV_chr000_p383f1 afhk_pat_chr000_p72_t2 afhk_pat_chr019_p62_t7; do
	run $c $c --plate $S1/game/${c}_game_plate.png --aura-fit
done
run afhk_cg_p435 afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_r3 $GR --glow sq
run afhk_cg_p437 afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 $GR --glow sq
run pras pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq
run pras_layers pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --layers pras_chr020_p5f2_cg,pras_chr020_p5f2_pat
run rect_px rect_chr024_p76 --game-name rect_chr024_p76_unlit
echo "== rect thin" >> "$LOG"; python3 "$T/v2_thin.py" rect_chr024_p76_z1 --game-name rect_chr024_p76_unlit 2>&1 | tail -5 >> "$LOG"
# MBTL: .pat-only frames (AFGX type 1 on every drawn layer): no CG BgColor shading, raw PatPart model
run mbtl_flipV mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb --light "1,0;1,0;1,0"
run mbtl_pups mbtl_pups_chr011_p309 --game-name mbtl_pups_chr011_p309_nb --light "1,0;1,0;1,0"
echo "== mbtl_pral2 full frame" >> "$LOG"
python3 "$T/v2_fullframe.py" mbtl_pral2_chr004_p420f4 ${PRAL2_ARGS:---rect 200,100,1080,600} 2>&1 | tail -3 >> "$LOG"
cat "$LOG"
