#!/bin/bash
# tools/uni/s1_compare/v2_round12_mbtl_z1.sh [BWDIR]: render-match12. MBTL chr001 at editor zoom 1 (cases_control.txt
# *_z1 rows): no editor-shot resample, camera geometry registration (--subpix 0,0; zoom-1 controls fit to 0,0, round 11).
# Also the per-prop split of p157 at zoom 1.5 (v2_split_by_shot.py, solo shots s8 / s456).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2; L0="1,0;1,0;1,0"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2; do
  run ${c}_z1 --game-name ${c}_nb --light "$L0" --grey --subpix 0,0
done
for k in s8 s456; do
  run mbtl_flipV_chr001_p157f2_solo_${k}_z1 --game-name mbtl_flipV_chr001_p157f2_solo_${k} --light "$L0" --grey \
    --plate $GD/mbtl_flipV_chr001_p157f2_solo_${k}_game_plate_bb.png --subpix 0,0
done
echo "-- p157 (zoom 1.5, chr001 raw-fitted offset) split by the solo shots' coverage"
python3 "$T/v2_split_by_shot.py" mbtl_flipV_chr001_p157f2_solo_s456 mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb \
  --light "$L0" --grey --subpix "$D/docs/audit_2026_10_14/evidence/compare/v2/round12/reg/mbtl_chr001_nb.json" --bw "$BW" 2>&1 | tail -3
echo "-- p157 zoom 1 split"
python3 "$T/v2_split_by_shot.py" mbtl_flipV_chr001_p157f2_solo_s456_z1 mbtl_flipV_chr001_p157f2_z1 --game-name mbtl_flipV_chr001_p157f2_nb \
  --light "$L0" --grey --subpix 0,0 --bw "$BW" 2>&1 | tail -3
