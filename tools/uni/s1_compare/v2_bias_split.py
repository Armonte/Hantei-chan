#!/usr/bin/env python3
"""v2_bias_split.py <v2_model args...>: render-match15. Signed lit residual (game - prediction) per channel of a case,
split into interior px (Q-coverage eroded by 2 px) and edge px, plus the lit mean after removing the best per-channel
constant (the size of a pure uniform bias) and with the |d| > 5 px left out. Used for afhk_cg_chr020_p435_t27."""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
sys.argv = ['v2_model.py'] + sys.argv[1:]
vm.main()
G, B, P, Q, m = sh.game_vs_model.last_arrays
lp = np.asarray(sh.game_vs_model.last_lpred, dtype=np.float64)
d = G - lp; ad = np.abs(d).mean(2)
er = m.copy()
for dy in (-2, -1, 0, 1, 2):
    for dx in (-2, -1, 0, 1, 2):
        er &= np.roll(np.roll(m, dy, 0), dx, 1)
for nm, s in (('footprint', m), ('interior (eroded 2)', er), ('edge', m & ~er)):
    print('%-20s px %5d  lit %.2f  signed R/G/B %+.2f %+.2f %+.2f' % (nm, s.sum(), ad[s].mean(), *[d[..., c][s].mean() for c in range(3)]))
c = np.array([d[..., k][m].mean() for k in range(3)])
print('best constant per channel %+.2f %+.2f %+.2f -> lit %.2f' % (*c, np.abs(d - c).mean(2)[m].mean()))
big = ad > 5
print('without the %d |d|>5 px: lit %.2f; with the constant removed too: %.2f' % ((big & m).sum(), ad[m & ~big].mean(),
      np.abs(d - c).mean(2)[m & ~big].mean()))
