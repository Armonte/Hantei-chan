#!/bin/bash
# tools/uni/s1_compare/v2_round10.sh [BWDIR]: render-match10. Every round-9 case with the footprint-independent
# registration (--exact-reg), next to the round-9 command; the MBTL p157 solo-arc cases (plate: the backbuffer one,
# the window plate of those runs came out black); pras_chr020_p5f2 with the per-layer bokashi (--layers).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
BK="--bokashi 0.0784"; L0="--light 1,0;1,0;1,0"
GR="--grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for x in "" "--exact-reg"; do
  echo "== ${x:-round-9 registration}"
  run mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb "$L0" --grey $x
  run mbtl_patctrl_chr001_p238f10 --game-name mbtl_patctrl_chr001_p238f10_nb "$L0" --grey $x
  run mbtl_patctrl_chr001_p277f0 --game-name mbtl_patctrl_chr001_p277f0_nb "$L0" --grey $x
  for k in s8 s456; do
    run mbtl_flipV_chr001_p157f2_solo_$k "$L0" --grey --plate $GD/mbtl_flipV_chr001_p157f2_solo_${k}_game_plate_bb.png $x
  done
  run mbtl_pups_chr011_p309 --game-name mbtl_pups_chr011_p309_nb "$L0" --grey $x
  run ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $BK $x
  run afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_r3 $GR --glow sq --grey $BK $x
  run afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 $GR --glow sq --grey $BK $x
  run pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --grey $BK $x
  run pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --grey $BK --layers pras_chr020_p5f2_cg,pras_chr020_p5f2_pat $x
  run ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 "$L0" --grey $G27 $BK $x
  run pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 "$L0" --grey $G27 $BK $x
  run flipHV_chr019_p153 --game-name flipHV_chr019_p153_r5 "$L0" --grey $x
  run flipHV_chr019_p153_z1 --game-name flipHV_chr019_p153_r5 "$L0" --grey $x
  run ctrl_chr019_p0_z1 --game-name ctrl_chr019_p0_r5 "$L0" --grey $x
  run flipHV_chr019_p153_solo8_z1 --game-name flipHV_chr019_p153_solo8 "$L0" --grey $x
  run rect_chr024_p76 --game-name rect_chr024_p76_unlit --grey $x
done
