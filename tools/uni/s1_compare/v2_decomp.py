#!/usr/bin/env python3
"""v2_decomp.py <v2_model args...>: run v2_model's measurement and split the lit residual |G - pred| of the footprint
by editor coverage (interior / edge) and by distance to the silhouette edge, plus the game-minus-prediction sign.
Round 9 (p435 glow / edge study)."""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
sys.argv = ['v2_model.py'] + sys.argv[1:]
vm.main()
G, B, Pp, Q, m = sh.game_vs_model.last_arrays
lp = np.asarray(sh.game_vs_model.last_light[3]).astype(np.float64)
cov = np.clip(1 - Q.mean(2), 0, 1)
d = G - lp; ad = np.abs(d)
inner = cov > 0.98
from scipy import ndimage
dist_in = ndimage.distance_transform_edt(inner)       # px from the nearest non-interior px
tot = ad[m].sum()
print('footprint px %d, mean %.2f' % (m.sum(), ad[m].mean()))
for name, sel in [('interior d>=3', m & (dist_in >= 3)), ('interior d=2', m & (dist_in == 2)), ('interior d=1', m & (dist_in == 1)),
                  ('edge cov .02-.98', m & ~inner & (cov > 0.02)), ('outside cov<.02', m & (cov <= 0.02))]:
    if sel.sum() == 0: continue
    s = ad[sel]; sg = d[sel].mean(0)
    print('%-18s px %5d  mean %.2f  share %4.1f%%  signed G-pred RGB %+.1f %+.1f %+.1f' % (name, sel.sum(), s.mean(), s.sum() * 100 / tot, *sg))
if os.environ.get('DECOMP_OUT'): np.savez_compressed(os.environ['DECOMP_OUT'], G=G, B=B, P=Pp, Q=Q, m=m, lp=lp)
