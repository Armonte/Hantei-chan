#!/bin/bash
# tools/uni/s1_compare/v2_round9.sh [BWDIR]: render-match9. The UNI2 CG cases with and without the chara Bokashi
# pass model (--bokashi 0.0784 = training stage bg001 stage_bokashi_alpha.img 20/255), then the round-8 list.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
BK="--bokashi 0.0784"
GR="--grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3"; G27="--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5"
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for b in "" "$BK"; do
  echo "== ${b:-no bokashi}"
  run ctrl_chr020_p0 --game-name ctrl_chr020_p0_r3 $GR --grey $b
  run afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_r3 $GR --glow sq --grey $b
  run afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_unlit --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_unlit --grey $b
  run afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 $GR --glow sq --grey $b
  run pras_chr020_p5f2 --game-name pras_chr020_p5f2_r3 $GR --glow sq --grey $b
  run ctrl_chr027_p0 --game-name ctrl_chr027_p0_r5 --light "1,0;1,0;1,0" --grey $G27 $b
  run pups_chr027_p449f4 --game-name pups_chr027_p449f4_r5 --light "1,0;1,0;1,0" --grey $G27 $b
done
echo "== round 8 list"
bash "$T/v2_round8.sh" "$BW"
