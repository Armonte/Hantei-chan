#!/bin/bash
# tools/uni/s1_compare/v2_round20.sh [BWDIR]: render-match20. MBTL stage-21 rows at a REAL camera zoom of 1.5 (gamecap
# v2r20_mbtl_chr001_zoom15.gcs: g_CamElem0_RenderZoom 0xCABAA4 / g_CamElem1_Zoom 0xCABABC / g_CamElem2_ZoomTarget
# 0xCABAD4 held at 1.5, stage anime held; suffix _z15sa) against the editor's zoom-1.5 shots (cases.txt /
# cases_control.txt rows without _z1), plate_b0 backdrop (stage shadow kept). Editor 1.5 -> game 1.5 is sc = 1: no
# resample. Before: round 19's fake rows (editor 1.5 box-downsampled to the zoom-1 game shot).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2/nohud_st
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" --game "$GD" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2; do
  python3 -c "import json,sys;m=json.load(open(sys.argv[1]));print('-- $c sidecar zoom elem0 %.4f elem1 %.4f cam x %d y %d' % (m['cam_f32'][5], m['cam_f32'][11], m['cam_i32'][8], m['cam_i32'][9]))" "$GD/${c}_z15sa_game.json"
  echo "-- $c (editor zoom 1.5) vs REAL zoom 1.5 _z15sa, plate_b0, subpix 0,0"
  run $c --game-name ${c}_z15sa --plate $GD/${c}_z15sa_game_plate_b0.png "${L0[@]}" --grey --subpix 0,0
  echo "-- $c (editor zoom 1.5) vs zoom-1 _sa (round 19 fake row), plate_b0"
  run $c --game-name ${c}_sa --plate $GD/${c}_sa_game_plate_b0.png "${L0[@]}" --grey --subpix "$D/docs/audit_2026_10_14/evidence/compare/v2/round13/reg/mbtl_chr001_nb.json"
done
