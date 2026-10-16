#!/usr/bin/env python3
"""v2_split_by_shot.py SHOT1[,SHOT2...] <v2_model args...>: render-match12. Run v2_model's measurement and split the
lit and raw residual of the footprint by the coverage (1 - Q > 0.02) of other editor shots with the same framing
(e.g. the single-prop solo shots of a case), mapped into the game window the same way as the case.
Used for mbtl_flipV_chr001_p157f2: which props carry the residual above the .pat-only control."""
import sys, os, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
shots = sys.argv[1].split(','); args = sys.argv[2:]
bw = args[args.index('--bw') + 1] if '--bw' in args else os.path.join(vm.D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw')
def pq(n):
    K = np.asarray(Image.open(os.path.join(bw, n + '_new_k.png')).convert('RGB')).astype(np.float64)
    w = vm.load_w(bw, n); W = w if isinstance(w, np.ndarray) else np.asarray(w.convert('RGB')).astype(np.float64)
    return K, (W - K) / 255.0
orig = sh.game_vs_model
covs = {}
def wrapped(*a, **k):
    out = []
    for n in shots:  # map each shot through the case's own editor -> game window (POST_ADD_SRC path, zero add)
        if n not in covs: covs[n] = pq(n)
        keep = sh.POST_ADD, sh.POST_ADD_SRC
        sh.POST_ADD = lambda P, Q: np.zeros_like(P); sh.POST_ADD_SRC = (covs[n][0], covs[n][1], np.ones_like(covs[n][1]))
        orig(*a, **k); out.append(np.clip(1 - wrapped.last_cg[1].mean(2), 0, 1))  # sheet sets attrs on the module name (= wrapped)
        sh.POST_ADD, sh.POST_ADD_SRC = keep
    r = orig(*a, **k); wrapped.cov = out
    return r
sh.game_vs_model = wrapped
sys.argv = ['v2_model.py'] + args
vm.main()
G, B, P, Q, m = wrapped.last_arrays
lp = np.asarray(wrapped.last_light[3]).astype(np.float64)
raw = np.clip(P + Q * B, 0, 255)
dl = np.abs(G - lp).mean(2); dr = np.abs(G - raw).mean(2)
tl, tr = dl[m].sum(), dr[m].sum()
print('footprint px %d  lit %.2f  raw %.2f' % (m.sum(), dl[m].mean(), dr[m].mean()))
cov_any = np.zeros(m.shape, bool)
for n, c in zip(shots, wrapped.cov):
    s = m & (c > 0.02); cov_any |= s
    print('%-44s px %6d  lit %.2f (share %4.1f%%)  raw %.2f (share %4.1f%%)' % (n, s.sum(), dl[s].mean(), dl[s].sum() * 100 / tl, dr[s].mean(), dr[s].sum() * 100 / tr))
s = m & ~cov_any
print('%-44s px %6d  lit %.2f (share %4.1f%%)  raw %.2f (share %4.1f%%)' % ('(none of these shots)', s.sum(), dl[s].mean(), dl[s].sum() * 100 / tl, dr[s].mean(), dr[s].sum() * 100 / tr))
# render-match17: exclusive coverage combinations, each split into edge (within 1 px of a coverage boundary of any
# shot or the footprint) and interior, with the signed residual (game - prediction) per region. Uses raw (float
# P + Q*B): the lit column above reads last_light[3], which is truncated to uint8 (a fake +0.48, round 15).
if os.environ.get('SPLIT_EXCL'):
    from scipy import ndimage
    cs = [(c > 0.02) & m for c in wrapped.cov]
    edge = np.zeros(m.shape, bool)
    for c in cs + [m]:
        edge |= c ^ ndimage.binary_erosion(c, iterations=1, border_value=0)
    edge = ndimage.binary_dilation(edge, iterations=1) & m
    sl = (G - raw).mean(2)
    print('-- exclusive regions (edge = within 1 px of a coverage boundary)')
    for code in range(0, 1 << len(cs)):
        r = m.copy()
        for i, c in enumerate(cs): r &= c if code >> i & 1 else ~c
        if not r.any(): continue
        nm = '+'.join(shots[i].split('_solo_')[-1] for i in range(len(cs)) if code >> i & 1) or '(none)'
        for tag, rr in (('edge', r & edge), ('inner', r & ~edge)):
            if rr.any():
                print('%-16s %-5s px %6d  raw %.2f signed %+.2f (share %4.1f%%)  |d|>8 px %d' % (nm, tag, rr.sum(), dr[rr].mean(), sl[rr].mean(), dr[rr].sum() * 100 / tr, (dr[rr] > 8).sum()))
    # coverage disagreement on the big-error edge px: which side matches the plate (backdrop + other props)?
    big = edge & (dr > 8)
    if big.any():
        gb = np.abs(G - B).mean(2); pb = np.abs(raw - B).mean(2)
        print('-- |d|>8 edge px %d: game==plate(<3) %d, model==plate(<3) %d, neither %d; signed mean %+.2f' % (
            big.sum(), (big & (gb < 3)).sum(), (big & (pb < 3)).sum(), (big & (gb >= 3) & (pb >= 3)).sum(), sl[big].mean()))
        ys, xs = np.nonzero(big); print('   rows %d-%d cols %d-%d' % (ys.min(), ys.max(), xs.min(), xs.max()))
