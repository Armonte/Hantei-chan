#!/usr/bin/env python3
"""v2_split_additive.py SHOT1,SHOT2 <v2_model args...>: render-match13. Split a zoom-1 case's lit residual by where
each of two additive-layer editor shots draws (black-backdrop value > 4; Q-based coverage misses additive parts), e.g.
pras_chr020_p5f2_z1 by its set-991 solo .pat layer shots (s12 / slin). Zoom 1, --subpix 0,0 only (integer mapping)."""
import sys, os, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
shots = sys.argv[1].split(','); sys.argv = ['v2_model.py'] + sys.argv[2:]
orig = sh.game_vs_model; cap = {}
def wrapped(game, plate, ek, ew, zoom, meta, org):
    r = orig(game, plate, ek, ew, zoom, meta, org)
    if 'win' not in cap or True:
        cap['args'] = (game, plate, zoom, meta, org)
    return r
sh.game_vs_model = wrapped
vm.main()
G, B, P, Q, m = wrapped.last_arrays
lp = np.asarray(wrapped.last_light[3]).astype(np.float64)
ad = np.abs(G - lp).mean(2)
xa, ya, xb, yb, gorg = wrapped.last_window
game, plate, zoom, meta, org = cap['args']
bw = os.path.join(vm.D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw')
gt, gs = sh.game_transform(meta)
masks = {}
for n in shots:
    K = np.asarray(Image.open(os.path.join(bw, n + '_new_k.png')).convert('RGB')).astype(np.float64).max(2)
    o = sh.editor_origin(Image.open(os.path.join(bw, n + '_new_k.png')))
    # zoom 1, scale 1: game px (x, y) <- editor px (x - gorg + o)
    yy, xx = np.mgrid[ya:yb, xa:xb]
    ex = np.round(xx - gorg[0] + o[0]).astype(int); ey = np.round(yy - gorg[1] + o[1]).astype(int)
    ok = (ex >= 0) & (ey >= 0) & (ex < K.shape[1]) & (ey < K.shape[0])
    a = np.zeros(ex.shape); a[ok] = K[ey[ok], ex[ok]]
    masks[n] = a > 4
s12, sl = masks[shots[0]], masks[shots[1]]
tot = ad[m].sum()
for nm, s in (('%s only' % shots[0], s12 & ~sl), ('%s only' % shots[1], sl & ~s12), ('both', s12 & sl), ('neither', ~s12 & ~sl)):
    s = s & m
    print('%-40s px %6d mean %.2f share %.0f%%' % (nm, s.sum(), ad[s].mean() if s.sum() else 0, 100 * ad[s].sum() / tot))
