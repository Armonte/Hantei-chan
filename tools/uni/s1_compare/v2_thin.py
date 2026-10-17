#!/usr/bin/env python3
"""Thin-geometry residual (render-match2): a vertical additive line compared by its geometry and colour instead of a
per-pixel mean, which is dominated by sub-pixel placement on a 1-6 px wide line.

  v2_thin.py <editor case (zoom 1 shot)> --game-name N [--rows y0,y1] [--x0 X --x1 X]

Game: (frame - same-session plate) = the line's additive contribution (saturated channels flagged). Editor: the zoom-1
black-backdrop shot, the editor's additive y-axis (64 grey) taken out. Both are averaged over the rows, then:
centre (intensity-weighted x, game px), width (px with R > 5), top endpoint (first row with R > 20 on the core),
and the per-column R profile (R is the only unsaturated channel). Prints game vs editor and the differences."""
import json, os, sys
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.abspath(os.path.join(HERE, '../../..'))
sys.path.insert(0, HERE)


def arg(a, k, d=None):
    if k in a: i = a.index(k); v = a[i + 1]; del a[i:i + 2]; return v
    return d


def main():
    a = sys.argv[1:]
    gd = arg(a, '--game', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2')
    bw = arg(a, '--bw', os.path.join(D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw'))
    gn = arg(a, '--game-name'); rows = tuple(int(v) for v in arg(a, '--rows', '660,716').split(','))
    case = a[0]
    import importlib.util
    spec = importlib.util.spec_from_file_location('sheet', os.path.join(HERE, 'sheet.py'))
    sh = importlib.util.module_from_spec(spec); spec.loader.exec_module(sh)
    meta = json.load(open(os.path.join(gd, gn + '_game.json')))
    gorg, gsc = sh.game_transform(meta)
    G = np.asarray(Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')).astype(float)
    B = np.asarray(Image.open(os.path.join(gd, gn + '_game_plate.png')).convert('RGB')).astype(float)
    K = np.asarray(Image.open(os.path.join(bw, case + '_new_k.png')).convert('RGB')).astype(float)
    eo = sh.editor_origin(Image.open(os.path.join(bw, case + '_new_k.png')))
    ox, oy = int(round(eo[0])), int(round(eo[1]))
    K[:, ox] = np.maximum(K[:, ox] - K[2, ox], 0)  # the editor's additive y axis
    gx = int(round(gorg[0])); gy = int(round(gorg[1]))
    xs = np.arange(-8, 9)
    gp = (G - B)[rows[0]:rows[1], gx - 8:gx + 9].mean(0)
    sat = (G[rows[0]:rows[1], gx - 8:gx + 9] >= 254).mean((0)).max(1)
    ey = (oy + (rows[0] - gy), oy + (rows[1] - gy))
    ep = K[ey[0]:ey[1], ox - 8:ox + 9].mean(0)
    def geo(p):
        r = np.clip(p[:, 0], 0, None)
        return float((xs * r).sum() / max(r.sum(), 1e-9)), int((r > 5).sum()), float(r.sum())
    def top(img, x, y0, y1):
        col = img[y0:y1, x, 0]
        hit = np.nonzero(col > 20)[0]
        return int(hit[0]) if len(hit) else -1
    gt = top(G - B, gx, gy - 20, gy + 20) - 20
    et = top(K, ox, oy - 20, oy + 20) - 20
    gc, gw, gs = geo(gp); ec, ew, es = geo(ep)
    print('%s vs %s rows %s' % (case, gn, rows))
    print('  x   game R,G,B (sat)      editor R,G,B')
    for i, x in enumerate(xs):
        if gp[i].max() > 0.5 or ep[i].max() > 0.5:
            print('%+3d  %5.1f %5.1f %5.1f (%s)   %5.1f %5.1f %5.1f' % (x, *gp[i], 'S' if sat[i] > 0.5 else '-', *ep[i]))
    print('centre (R-weighted, px from origin): game %+.2f editor %+.2f  d %+.2f' % (gc, ec, ec - gc))
    print('width (px with R > 5): game %d editor %d  d %+d' % (gw, ew, ew - gw))
    print('top endpoint (row from origin): game %+d editor %+d  d %+d' % (gt, et, et - gt))
    print('R integral across the line: game %.1f editor %.1f  ratio %.3f' % (gs, es, es / max(gs, 1e-9)))
    peak = np.abs(np.sort(gp[:, 0])[::-1][:3] - np.sort(ep[:, 0])[::-1][:3])
    print('R profile mean |d| over the line columns: %.2f' % np.abs(gp[:, 0] - ep[:, 0])[(gp[:, 0] > 0.5) | (ep[:, 0] > 0.5)].mean())


if __name__ == '__main__':
    main()
