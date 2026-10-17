#!/bin/bash
# tools/uni/s1_compare/v2_round8.sh [BWDIR]: render-match8 round 8, every case whose editor shot the segment-split
# change touched, plus the flipHV solo-prop cases. Prints one "raw" line per run (pass --bw dir to compare shot sets).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
P=/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare/game
L0="--light 1,0;1,0;1,0"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-100; }
run flipHV_chr019_p153 --game-name flipHV_chr019_p153_r5 $L0 --grey
run flipHV_chr019_p153_z1 --game-name flipHV_chr019_p153_r5 $L0 --grey
for k in 1 2 3 4 6 8; do run flipHV_chr019_p153_solo${k}_z1 --game-name flipHV_chr019_p153_solo$k $L0 --grey; done
run mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb $L0 --grey
run mbtl_pups_chr011_p309 --game-name mbtl_pups_chr011_p309_nb $L0 --grey
run flipV_chr000_p383f1 --plate $P/flipV_chr000_p383f1_game_plate.png --aura-fit
run afhk_pat_chr019_p62_t7 --plate $P/afhk_pat_chr019_p62_t7_game_plate.png --aura-fit
run pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3 --glow sq --grey
run rect_chr024_p76 --game-name rect_chr024_p76_unlit --grey
run pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 $L0 --grey --grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5
run afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_unlit --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_unlit --grey
run ctrl_chr019_p0 --game-name ctrl_chr019_p0_r5 $L0 --grey
