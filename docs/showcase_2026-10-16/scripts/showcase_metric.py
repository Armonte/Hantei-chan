#!/usr/bin/env python3
"""showcase_metric.py <case> <out_prefix> <v2_model args...> -- per-tool game-vs-editor metric on ONE shared pixel set.

Runs tools/uni/s1_compare/v2_model.py's measure() unchanged for every tool whose shots exist in
work/bw_<tool>/ (ours, sosfiro, gonp), with the SAME game capture, plate, light model and registration flags.
v2_model scores a tool only on that tool's own footprint, which would reward a tool that draws less (a missing
effect leaves no footprint). So each tool's lit prediction is pasted into the full game frame (outside its window the
tool predicts the plate, since it drew nothing there), and every tool is scored on the UNION of all tools' footprints:
mean |d| (0-255, per channel) and the share of pixels whose |d| summed over RGB is > 24.
Writes <out_prefix>.json and a 5-column crop sheet (<out_prefix>_cols.png: game | ours | sosfiro | gonp predictions)."""
import importlib.util, json, os, sys
import numpy as np
from PIL import Image, ImageDraw
P = '/home/teo/dev/hantei-chan-wt/parity'; T = P + '/tools/uni/s1_compare'; S = P + '/docs/showcase_2026-10-16'
spec = importlib.util.spec_from_file_location('v2m', T + '/v2_model.py'); vm = importlib.util.module_from_spec(spec)
ARGV = list(sys.argv); sys.argv = ['v2_model.py']; spec.loader.exec_module(vm); sh = vm.sh; sys.argv = ARGV
TOOLS = ['ours', 'sosfiro', 'gonp']


def run_tool(tool, args):
    a = list(args) + ['--bw', f'{S}/work/bw_{tool}']
    sp = vm.arg(a, '--subpix')
    sh.SUBPIX = (0.0, 0.0)
    if sp:
        vm.arg(a, '--refine'); a = [x for x in a if x != '--exact-reg'] + ['--exact-reg', '--refine', '0']
        sh.SUBPIX = tuple(json.load(open(sp))['subpix']) if os.path.exists(sp) else tuple(float(v) for v in sp.split(','))
    case = a[0]
    if not all(os.path.exists(f'{S}/work/bw_{tool}/{case}_new_{b}.png') for b in 'kwg'): return None
    r = vm.measure(a, quiet=True)
    if r is None: return {'native': None}
    G, B, Ps, Qs, m = sh.game_vs_model.last_arrays
    xa, ya, xb, yb, _ = sh.game_vs_model.last_window
    return {'native': r, 'lpred': sh.game_vs_model.last_lpred.copy(), 'win': (xa, ya, xb, yb), 'm': m.copy()}


def main():
    case, outp = sys.argv[1], sys.argv[2]; args = [case] + sys.argv[3:]
    gd = vm.arg(list(args), '--game', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2')
    gn = vm.arg(list(args), '--game-name') or case
    plate = vm.arg(list(args), '--plate') or os.path.join(gd, gn + '_game_plate.png')
    G = np.asarray(Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')).astype(np.float64)
    B = np.asarray(Image.open(plate).convert('RGB')).astype(np.float64)
    res = {t: run_tool(t, args) for t in TOOLS}
    U = np.zeros(G.shape[:2], bool)
    for t, r in res.items():
        if r and r.get('m') is not None:
            xa, ya, xb, yb = r['win']; U[ya:yb, xa:xb] |= r['m']
    exc = vm.arg(list(args), '--exclude')
    if exc:
        for b in exc.split(';'):
            x0, y0, x1, y1 = map(int, b.split(',')); U[y0:y1, x0:x1] = False
    out = {'case': case, 'game': gn, 'plate': os.path.basename(plate), 'union_px': int(U.sum()), 'args': sys.argv[3:], 'tools': {}}
    full = {}
    for t, r in res.items():
        if r is None: out['tools'][t] = None; continue
        if r.get('m') is None:
            pf = B.copy()   # drew nothing: predicts the plate
            out['tools'][t] = {'native_footprint': None}
        else:
            pf = B.copy(); xa, ya, xb, yb = r['win']; pf[ya:yb, xa:xb] = r['lpred']
            out['tools'][t] = {'native_footprint': {'lit': round(r['native']['lit'], 2), 'lit_pct': round(r['native']['lit_pct'], 1), 'px': int(r['m'].sum())}}
        d = np.abs(G - pf)
        out['tools'][t].update({'mean_abs_d': round(float(d[U].mean()), 2), 'pct_px_gt24': round(float((d.sum(2)[U] > 24).mean() * 100), 1)})
        full[t] = pf
    json.dump(out, open(outp + '.json', 'w'), indent=1)
    ys, xs = np.nonzero(U); pad = 24
    y0, y1, x0, x1 = max(0, ys.min() - pad), min(G.shape[0], ys.max() + pad), max(0, xs.min() - pad), min(G.shape[1], xs.max() + pad)
    cols = [('game', G)] + [(t, full[t]) for t in TOOLS if t in full]
    tiles = []
    for name, im in cols:
        crop = Image.fromarray(np.clip(im[y0:y1, x0:x1], 0, 255).astype(np.uint8))
        tiles.append((name, crop))
    np.save(outp + '_crop.npy', np.array([y0, y1, x0, x1]))
    for name, crop in tiles: crop.save(f'{outp}_{name}.png')
    for t in full:
        dd = np.abs(G - full[t])[y0:y1, x0:x1] * U[y0:y1, x0:x1, None]
        Image.fromarray(np.clip(dd * 4, 0, 255).astype(np.uint8)).save(f'{outp}_{t}_diff.png')
    print(json.dumps({t: (v and {k: v[k] for k in ('mean_abs_d', 'pct_px_gt24')}) for t, v in out['tools'].items()}), 'union', out['union_px'])


if __name__ == '__main__':
    main()
