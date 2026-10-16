#!/bin/bash
# run_cases.sh: the showcase metric rows (same game capture, plate, light model and registration for every tool).
# Flags are the render-match rounds' own (tools/uni/s1_compare/v2_round14.sh, v2_round21.sh, v2_round24_cgfilter.sh) minus
# --layers / --glow-sil cg, which need per-layer shots that only our editor can make.
set -u
P=/home/teo/dev/hantei-chan-wt/parity; S=$P/docs/showcase_2026-10-16; M="python3 $S/scripts/showcase_metric.py"
R=$P/docs/audit_2026_10_14/evidence/compare/v2/round13/reg; GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2; GS=$GD/nohud_st
BK="--bokashi 0.0784"; L0=(--light "1,0;1,0;1,0"); O=$S/work/m; mkdir -p $O
row() { local id=$1; shift; [ -n "${ONLY:-}" ] && ! [[ $id =~ $ONLY ]] && return; echo "== $id"; $M "$@" 2>&1 | grep -v -i "warn\|^  " | tail -1; }
row uni2_set991 pras_chr020_p5f2_z1 $O/uni2_set991 --game-name pras_chr020_p5f2_r3 --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3 --glow sq --grey $BK --subpix $R/chr020_r3.json
row uni2_p435 afhk_cg_chr020_p435_t27 $O/uni2_p435 --game-name afhk_cg_chr020_p435_t27_r3 --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3 --glow sq --grey $BK --subpix $R/chr020_r3.json
row uni2_pups pups_chr027_p449f4 $O/uni2_pups --game-name pups_chr027_p449f4_r5 "${L0[@]}" --grey --grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5 $BK --subpix $R/chr027_r5.json
row uni2_flipHV flipHV_chr019_p153_z1 $O/uni2_flipHV --game-name flipHV_chr019_p153_r5 "${L0[@]}" --grey --subpix $R/chr019_r5_z1.json
for c in mbtl_patctrl_chr001_p238f10 mbtl_flipV_chr001_p157f2; do
  row ${c} $c $O/$c --game $GS --game-name ${c}_z15sa --plate $GS/${c}_z15sa_game_plate_b0.png "${L0[@]}" --grey --subpix 0,0
done
row mbtl_cg_z15 mbtl_ctrl_chr001_p0f0_z15 $O/mbtl_cg_z15 --game $GS --game-name mbtl_ctrl_chr001_p0f0_z15sabb --plate $GS/mbtl_ctrl_chr001_p0f0_z15sabb_game_plate_b0.png --mbtl-light bg021,330 --grey --subpix 0,0 --exclude 30,170,145,210
