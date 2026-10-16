#!/bin/bash
# tools/uni/s1_compare/v2_round18.sh [BWDIR]: render-match18. The p157_z1 split with additive coverage (v2_split_by_shot
# now counts PRAL 1 parts), the s456 solo re-measured on the round-18 stage-21 capture (window pair and backbuffer pair),
# the s8 solo with MBTL's stage bloom held off (_nbl), and the stage-21 lamp-flicker check (v2_lamp_match.py).
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
N=mbtl_flipV_chr001_p157f2
echo "-- ${N}_z1 vs _st split by the s8 / s456 solo shots (additive coverage)"
SPLIT_EXCL=1 python3 "$T/v2_split_by_shot.py" ${N}_solo_s8_z1,${N}_solo_s456_z1 ${N}_z1 --game-name ${N}_st "${L0[@]}" --grey --subpix 0,0 --bw "$BW" 2>&1 | grep -v "Warning\|ret = \|print(" | tail -11
for g in st stbb; do echo "-- ${N}_solo_s456_z1 vs _$g"; run ${N}_solo_s456_z1 --game-name ${N}_solo_s456_$g "${L0[@]}" --grey --subpix 0,0; done
for g in st stbb nbl; do echo "-- ${N}_solo_s8_z1 vs _$g"; run ${N}_solo_s8_z1 --game-name ${N}_solo_s8_$g "${L0[@]}" --grey --subpix 0,0; done
for c in solo_s8_st solo_s8_nbl solo_s456_st st; do
  z=${N}_z1; [ "$c" = st ] || z=${N}_${c%_*}_z1
  echo "-- lamp check $z vs _$c (alt plate = plate_b0)"
  python3 "$T/v2_lamp_match.py" $GD/${N}_${c}_game_plate_b0.png $z --game-name ${N}_$c "${L0[@]}" --grey --subpix 0,0 --bw "$BW" 2>&1 | grep -v Warn | tail -2
done
