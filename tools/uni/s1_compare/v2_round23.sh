#!/bin/bash
# tools/uni/s1_compare/v2_round23.sh: render-match23. Every MBTL zoom-1 row (_sa game shot, plate_b0) with the float
# model (P + Q*B, as v2_round19.sh) and the 8-bit quantized model (v2_quant.py: blend into the integer plate, round
# to nearest / truncate after the layer). p238 also with its s4/s5/s18 editor solo shots as separate layers.
set -u
D="$(cd "$(dirname "$0")/../../.." && pwd)"; T=$D/tools/uni/s1_compare; GD=/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2
for c in mbtl_patctrl_chr001_p238f10 mbtl_patctrl_chr001_p277f0 mbtl_flipV_chr001_p157f2 mbtl_flipV_chr001_p157f2_solo_s8 mbtl_flipV_chr001_p157f2_solo_s456; do
  echo "== ${c}_z1 (one layer)"; python3 "$T/v2_quant.py" ${c}_z1 ${c}_sa $GD/${c}_sa_game_plate_b0.png -
done
C=mbtl_patctrl_chr001_p238f10
for L in ${C}_solo_s18_z1 ${C}_solo_s18_z1,${C}_solo_s4_z1,${C}_solo_s5_z1 ${C}_solo_s4_z1,${C}_solo_s5_z1,${C}_solo_s18_z1; do
  echo "== ${C}_z1 layers $L"; python3 "$T/v2_quant.py" ${C}_z1 ${C}_sa $GD/${C}_sa_game_plate_b0.png $L
done
python3 - "$GD" <<'PY'
import sys, numpy as np
from PIL import Image
gd = sys.argv[1]; L = lambda f: np.asarray(Image.open(f'{gd}/{f}.png').convert('RGB')).astype(float)
ref = L('mbtl_patctrl_chr001_p277f0_sa_game_plate_b0')
print('-- p157 floor strip (y 660-705, x 500-900) minus p277 plate_b0; r23 = the effect-peek session')
for s in ['game_bb', 'game_plate_b0', 'game_plate_bb']:
    for c in ['mbtl_flipV_chr001_p157f2_solo_s456_sa', 'r23/mbtl_flipV_chr001_p157f2_solo_s456_sa23']:
        print(c, s, np.round((L(f'{c}_{s}') - ref)[660:705, 500:900].reshape(-1, 3).mean(0), 2))
for s in ['b0', 'bb']:
    b = (L(f'r23/mbtl_flipV_chr001_p157f2_solo_s456_sa23_game_plate_{s}') - ref)[600:720, ..., 2]
    print('p158 puddle, plate_%s: blue sum over y 600-720 (B > 0) %d' % (s, b[b > 0].sum()))
PY
