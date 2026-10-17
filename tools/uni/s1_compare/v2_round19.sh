#!/bin/bash
# tools/uni/s1_compare/v2_round19.sh [BWDIR]: render-match19. The MBTL stage-21 rows recaptured with the stage FBX anime
# held (gamecap v2r19_*_stagehold.gcs: hold g_StageAnimFrame 0xA300A0, MBTL IDB; suffix _sa), so the game shot, the
# window / backbuffer shots and every plate show one stage state. Each row is measured against the _sa window plate
# (taken after one F6 tick with P1/P2 alpha 0) and against plate_b0 (alpha 0, no tick), which keeps the stage-21
# character shadow (BgList ShadowLightType 1) that the game shot has under the fighters. Zoom-1.5 rows: the game is
# at camera zoom 1.0 (sidecar cam_f32[11]); they compare a 1.5x editor raster box-downsampled to 1x.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; BW=${1:-$D/docs/audit_2026_10_14/evidence/compare/v2/editor_bw}
R13=$D/docs/audit_2026_10_14/evidence/compare/v2/round13/reg; GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
L0=(--light "1,0;1,0;1,0")
run() { python3 "$T/v2_model.py" "$@" --bw "$BW" 2>&1 | grep -E "^[a-z].*: raw" | cut -c1-110; }
for c in mbtl_flipV_chr001_p157f2_solo_s8 mbtl_flipV_chr001_p157f2_solo_s456 mbtl_flipV_chr001_p157f2 mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0; do
  for s in st sa; do
    [ -f "$GD/${c}_${s}_game.png" ] || continue
    echo "-- ${c}_z1 vs _$s (window plate)"; run ${c}_z1 --game-name ${c}_$s "${L0[@]}" --grey --subpix 0,0
  done
  echo "-- ${c}_z1 vs _sa (plate_b0: shadow kept)"; run ${c}_z1 --game-name ${c}_sa --plate $GD/${c}_sa_game_plate_b0.png "${L0[@]}" --grey --subpix 0,0
done
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2; do
  echo "-- $c (editor zoom 1.5 -> game zoom 1) vs _sa plate_b0, reg mbtl_chr001_nb"
  run $c --game-name ${c}_sa --plate $GD/${c}_sa_game_plate_b0.png "${L0[@]}" --grey --subpix "$R13/mbtl_chr001_nb.json"
done
python3 - "$GD" <<'PY'
import sys, numpy as np
from PIL import Image
gd = sys.argv[1]
L = lambda f: np.asarray(Image.open(f'{gd}/{f}.png').convert('RGB')).astype(float)
print('-- lamp probe (x 690-750, y 295-325) per shot')
for c in ['mbtl_flipV_chr001_p157f2_st', 'mbtl_flipV_chr001_p157f2_sa', 'mbtl_flipV_chr001_p157f2_solo_s8_st', 'mbtl_flipV_chr001_p157f2_solo_s8_sa',
          'mbtl_flipV_chr001_p157f2_solo_s456_st', 'mbtl_flipV_chr001_p157f2_solo_s456_sa', 'mbtl_patctrl_chr001_p238f10_sa', 'mbtl_patctrl_chr001_p277f0_sa']:
    print(c, ' '.join('%s %.1f' % (s, L(f'{c}_{s}')[295:325, 690:750].mean()) for s in ['game', 'game_bb', 'game_plate', 'game_plate_bb', 'game_plate_b0']))
c = 'mbtl_flipV_chr001_p157f2_sa'
d = L(c + '_game_plate_b0') - L(c + '_game_plate')
print('-- %s plate_b0 - plate, shadow ellipse core (x 700-780, y 630-650): mean RGB %s' % (c, np.round(d[630:650, 700:780].reshape(-1, 3).mean(0), 2)))
PY
