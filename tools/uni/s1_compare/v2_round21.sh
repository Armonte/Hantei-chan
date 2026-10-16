#!/bin/bash
# tools/uni/s1_compare/v2_round21.sh [BWDIR]: render-match21. Same rows as v2_round20.sh (editor zoom 1.5 vs REAL game zoom
# 1.5, _z15sa, plate_b0, subpix 0,0) on editor_bw shots re-shot with the render-match21 build (Render::BeginPass puts the world origin
# on the whole pixel floor(origin) at every zoom: 631.5 -> 632 at 1.5x), plus the zoom-1 _z1 vs _sa (plate_b0) rows,
# which must not move.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2/nohud_st
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" --game "$GD" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2; do
  echo "-- $c (editor zoom 1.5) vs REAL zoom 1.5 _z15sa, plate_b0, subpix 0,0"
  run $c --game-name ${c}_z15sa --plate $GD/${c}_z15sa_game_plate_b0.png "${L0[@]}" --grey --subpix 0,0
done
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2 mbtl_flipV_chr001_p157f2_solo_s8; do
  echo "-- ${c}_z1 vs _sa (plate_b0)"
  run ${c}_z1 --game-name ${c}_sa --plate $GD/${c}_sa_game_plate_b0.png "${L0[@]}" --grey --subpix 0,0
done
