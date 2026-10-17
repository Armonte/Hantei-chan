#!/usr/bin/env python3
"""render-match23: 8-bit blend quantization model for the MBTL zoom-1 rows, next to the float model.

The float model (v2_model / sheet.game_vs_model) predicts G = P + Q*B with P the editor's black-backdrop shot and
Q = (W - K) / 255 (W extrapolated from the 64-grey shot, --grey). The game instead blends every part into the
X8R8G8B8 backbuffer one after another, and each blend output is quantized to 8 bits.

Quantized model: start from the plate B (integer) and blend each layer in draw order, quantizing after every layer:
    dst = Qz(x_i + q_i * dst)          x_i = layer i on black (editor solo shot K_i), q_i = (E_i - K_i) / 64
with Qz = round-to-nearest ('rn') or truncate ('tr'). For additive layers (q_i = 1) both give dst + x_i whenever x_i
is a whole level, so for an all-additive stack the quantized model equals the float one exactly (except at 255).

Usage: v2_quant.py CASE GAME_NAME PLATE LAYER1[,LAYER2...]   (layers in draw order; editor solo shot names, or '-'
       to use the case's own shot as a single layer). Prints float / quantized residuals over the footprint."""
import importlib.util, os, sys
import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('v2_model', os.path.join(HERE, 'v2_model.py'))
vm = importlib.util.module_from_spec(spec); sys.argv = sys.argv[:1] + sys.argv[1:]; spec.loader.exec_module(vm)
sh = vm.sh
BW = os.path.join(HERE, '../../../docs/audit_2026_10_14/evidence/compare/v2/editor_bw')


def shot(name, k):
    return np.asarray(Image.open(os.path.join(BW, '%s_new_%s.png' % (name, k))).convert('RGB')).astype(np.float64)


def main():
    case, gname, plate, layers = sys.argv[1:5]
    sh.SUBPIX = (0.0, 0.0)
    vm.measure([case, '--game-name', gname, '--plate', plate, '--light', '1,0;1,0;1,0', '--grey',
                '--exact-reg', '--refine', '0'], quiet=True)
    Gs, Bs, Ps, Qs, m = sh.game_vs_model.last_arrays
    xa, ya, xb, yb, gorg = sh.game_vs_model.last_window
    K = shot(case, 'k'); org = sh.editor_origin(Image.open(os.path.join(BW, case + '_new_k.png')))
    # zoom 1, exact registration, subpix 0: the editor -> game map is a whole-pixel translation; check it on P
    ox = int(round(org[0] - gorg[0])); oy = int(round(org[1] - gorg[1]))
    win = (slice(ya + oy, yb + oy), slice(xa + ox, xb + ox))
    assert np.abs(K[win] - Ps).max() < 1e-3, 'editor/game translation mismatch (%.3f)' % np.abs(K[win] - Ps).max()
    names = [case] if layers == '-' else layers.split(',')
    L = []
    for n in names:
        k = shot(n, 'k'); g = shot(n, 'g')
        L.append((k[win], (g[win] - k[win]) / 64.0))
    if layers != '-':  # the solo shots composite back to the case shot on black (sanity, also in 8 bits)
        dst = np.zeros_like(Ps)
        for x, q in L: dst = np.floor(x + q * dst + 0.5)
        print('layer stack on black vs case shot: mean |d| %.3f' % np.abs(dst - Ps)[m].mean())
        # the solo shots may not hold every part of the frame: keep the pixels where the stack reproduces the case
        # shot on black and on grey (so the listed layers are the whole frame there)
        dg = np.full_like(Ps, 64.0)
        for x, q in L: dg = np.floor(x + q * dg + 0.5)
        Kg = shot(case, 'g')[win]
        cov = (np.abs(dst - Ps).max(2) < 0.5) & (np.abs(dg - Kg).max(2) < 0.5)
        m = m & cov[..., None] if m.ndim == 3 else m & cov
        print('pixels where the layers are the whole frame: %d' % int(m.sum()))
    fl = np.clip(Ps + Qs * Bs, 0, 255)
    res = {'float': fl}
    for mode, f in (('quant rn', lambda v: np.floor(v + 0.5)), ('quant tr', np.floor)):
        dst = Bs.copy()
        for x, q in L: dst = np.clip(f(x + q * dst), 0, 255)
        res[mode] = dst
    n = int(m.sum())
    print('%s vs %s (plate %s): footprint %d px' % (case, gname, os.path.basename(plate), n))
    for k, pr in res.items():
        d = Gs - pr
        print('  %-9s residual %.3f  signed %+.3f  |d|<=1 %.1f%%  |d|>=2 %.2f%%' % (
            k, np.abs(d)[m].mean(), d[m].mean(), 100 * (np.abs(d)[m] <= 1.0001).mean(), 100 * (np.abs(d)[m] >= 1.9999).mean()))
    d = (Gs - res['quant rn'])[m]
    h = np.round(d).astype(int); u, c = np.unique(np.clip(h, -4, 4), return_counts=True)
    print('  quant rn integer residual histogram:', ' '.join('%+d:%.1f%%' % (a, 100 * b / h.size) for a, b in zip(u, c)))
    # where the quantized residual sits: flat interior vs the filtered edges of a part (|grad P| over 4 levels / px)
    gy, gx = np.gradient(Ps.mean(2)); edge = np.hypot(gx, gy) > 4
    dq = np.abs(Gs - res['quant rn']).mean(2)
    for nm, w in (('flat', m & ~edge), ('edge', m & edge)):
        if w.sum(): print('  quant rn on %s px (%d): residual %.3f' % (nm, int(w.sum()), dq[w].mean()))
    print('  float vs quant rn prediction: mean |d| %.3f' % np.abs(res['float'] - res['quant rn'])[m].mean())


if __name__ == '__main__':
    main()
