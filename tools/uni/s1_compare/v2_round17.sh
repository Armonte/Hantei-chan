#!/bin/bash
# tools/uni/s1_compare/v2_round17.sh [BWDIR]: render-match17. MBTL rows with the draw origin floored to whole sprite px
# (sheet.MBTL_SNAP_ORIGIN, MBTL Han6Object_Draw 0x597070 sar 7) against the round-16 HUD-free captures (*_nh) and the
# round-17 HUD-free captures on the pinned stage 21 (*_st: gamecap v2r17_mbtl_*_stage.gcs, g_BattleStageSel 0xAFC04C),
# plus the p238 split by prop (exclusive regions, edge / inner).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
R13=$D/docs/audit_2026_10_14/evidence/compare/v2/round13/reg; GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2 mbtl_pups_chr011_p309; do
  for s in nb nh st; do
    [ -f "$GD/${c}_${s}_game.png" ] || continue
    echo "-- ${c}_z1 vs _$s (reg 0,0)"; run ${c}_z1 --game-name ${c}_$s "${L0[@]}" --grey --subpix 0,0
    echo "-- $c (zoom 1.5) vs _$s (reg mbtl_chr001_nb)"; run $c --game-name ${c}_$s "${L0[@]}" --grey --subpix "$R13/mbtl_chr001_nb.json"
  done
done
for k in s8 s456; do
  echo "-- mbtl_flipV_chr001_p157f2_solo_${k}_z1 vs round-10 (bb plate)"
  run mbtl_flipV_chr001_p157f2_solo_${k}_z1 --game-name mbtl_flipV_chr001_p157f2_solo_${k} "${L0[@]}" --grey --plate $GD/mbtl_flipV_chr001_p157f2_solo_${k}_game_plate_bb.png --subpix 0,0
  echo "-- mbtl_flipV_chr001_p157f2_solo_${k}_z1 vs _st"
  run mbtl_flipV_chr001_p157f2_solo_${k}_z1 --game-name mbtl_flipV_chr001_p157f2_solo_${k}_st "${L0[@]}" --grey --subpix 0,0
done
for s in nh st; do
  echo "-- mbtl_patctrl_chr001_p238f10_z1 vs _$s split by the s4 / s5 / s18 editor solo shots"
  SPLIT_EXCL=1 python3 "$T/v2_split_by_shot.py" mbtl_patctrl_chr001_p238f10_solo_s4_z1,mbtl_patctrl_chr001_p238f10_solo_s5_z1,mbtl_patctrl_chr001_p238f10_solo_s18_z1 \
    mbtl_patctrl_chr001_p238f10_z1 --game-name mbtl_patctrl_chr001_p238f10_$s "${L0[@]}" --grey --subpix 0,0 --bw "$BW" 2>&1 | grep -v "Warning\|ret = " | tail -10
done
