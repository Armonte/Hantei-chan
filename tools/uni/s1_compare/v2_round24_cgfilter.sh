#!/bin/bash
# tools/uni/s1_compare/v2_round24_cgfilter.sh: MBTL CG sprite (chr001 p0 f0) at zoom 1 and at a REAL zoom 1.5, editor vs game, with the
# MBTL BgColor light model (bg021, StdH 330), plate_b0, --subpix 0,0. Game shots: the backbuffer (_bb) captures of
# v2r19_mbtl_chr001_stagehold.gcs (_sa) and v2r24_mbtl_chr001_cg_zoom15.gcs (_z15sa), copied (same folder) as *_sabb / *_z15sabb
# (the z15 window shot has a foreign overlay on its left half). Editor shots: cgfilter/editor_on (Filter_V on, the default) and
# cgfilter/editor_off (MbtlCgZoomFilter=0). The training frame counter (game px 30,170-145,210) is left out at 1.5x.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; E=$D/docs/audit_2026_10_14/evidence/compare/v2/cgfilter; G=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2/nohud_st
m() { python3 "$T/v2_model.py" "$@" --game "$G" --mbtl-light bg021,330 --grey --subpix 0,0 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
X="--exclude 30,170,145,210"
echo "-- zoom 1 (Filter_V not used)"; m mbtl_ctrl_chr001_p0f0 --bw "$E/editor_on" --game-name mbtl_ctrl_chr001_p0f0_sabb --plate "$G/mbtl_ctrl_chr001_p0f0_sabb_game_plate_b0.png"
for v in on off; do
  echo "-- zoom 1.5, editor Filter_V $v"; m mbtl_ctrl_chr001_p0f0_z15 --bw "$E/editor_$v" --game-name mbtl_ctrl_chr001_p0f0_z15sabb --plate "$G/mbtl_ctrl_chr001_p0f0_z15sabb_game_plate_b0.png" $X ${SHEET:+--out $E/p0f0_z15_filter_${v}_sheet.png}
done
