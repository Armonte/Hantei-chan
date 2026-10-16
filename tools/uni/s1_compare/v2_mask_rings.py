#!/usr/bin/env python3
"""v2_mask_rings.py EDSHOT GAMEREF <v2_model args...>: render-match14. Report a zoom-1 case's lit residual with the
3D ring/arc props of a part set masked out. The mask is the union of
  - where the editor draws them: EDSHOT (an editor_bw solo shot of just those props, black backdrop value > 4), and
  - where the game draws them: |game - GAMEREF| > 12 (any channel), GAMEREF = a capture of the same case from a .pat with
    those props at PRCL alpha 0 (same harness, same camera),
dilated by 2 px. Used for pras_chr020_p5f2 (set-991 slots 41/44/45: their game arcs change from run to run with
byte-identical data, see INDEX round 14). Zoom 1, --subpix 0,0 only (integer mapping)."""
import sys, os, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
edshot, gref = sys.argv[1], sys.argv[2]; sys.argv = ['v2_model.py'] + sys.argv[3:]
cap = {}; orig = sh.game_vs_model
def wrapped(game, plate, ek, ew, zoom, meta, org):
    cap['args'] = (game, plate, zoom, meta, org); return orig(game, plate, ek, ew, zoom, meta, org)
sh.game_vs_model = wrapped
vm.main()
G, B, P, Q, m = wrapped.last_arrays
lp = np.asarray(wrapped.last_light[3]).astype(np.float64)
ad = np.abs(G - lp).mean(2)
xa, ya, xb, yb, gorg = wrapped.last_window
game, plate, zoom, meta, org = cap['args']
bw = os.path.join(vm.D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw')
K = np.asarray(Image.open(os.path.join(bw, edshot + '_new_k.png')).convert('RGB')).astype(np.float64).max(2)
o = sh.editor_origin(Image.open(os.path.join(bw, edshot + '_new_k.png')))
yy, xx = np.mgrid[ya:yb, xa:xb]
ex = np.round(xx - gorg[0] + o[0]).astype(int); ey = np.round(yy - gorg[1] + o[1]).astype(int)
ok = (ex >= 0) & (ey >= 0) & (ex < K.shape[1]) & (ey < K.shape[0])
a = np.zeros(ex.shape); a[ok] = K[ey[ok], ex[ok]]
med = a > 4
g0 = np.asarray((game if isinstance(game, Image.Image) else Image.open(game)).convert('RGB')).astype(np.float64)
g1 = np.asarray(Image.open(gref).convert('RGB')).astype(np.float64)
mg = np.abs(g0 - g1).max(2)[ya:yb, xa:xb] > 12
def dil(b, r=2):
    out = b.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            out |= np.roll(np.roll(b, dy, 0), dx, 1)
    return out
mask = dil(med | mg)
tot = ad[m].sum()
for nm, s in (('all footprint', m), ('ring mask (editor | game)', m & mask), ('  editor rings only', m & med & ~mg),
              ('  game rings only', m & mg & ~med), ('masked (off rings)', m & ~mask)):
    print('%-28s px %6d mean %.2f share %.0f%%' % (nm, s.sum(), ad[s].mean() if s.sum() else 0, 100 * ad[s].sum() / tot))
