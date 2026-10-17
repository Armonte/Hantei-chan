#!/usr/bin/env python3
"""Full-frame residual for cases whose parts fill the view (no backdrop shows through, so the blend model has no
footprint): the editor shot placed on the game frame by geometry (v2_sheet.place) and compared per pixel inside a
window of game pixels, optionally with an added colour c (the chara aura of the game's draw object).

  v2_fullframe.py <case> --rect x0,y0,x1,y1 [--game-name N] [--ours PNG] [--add r,g,b | --aura]
Prints mean |d| and % px > 24 in the window."""
import importlib.util, json, os, sys
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('v2s', os.path.join(HERE, 'v2_sheet.py'))
v2s = importlib.util.module_from_spec(spec); spec.loader.exec_module(v2s)
sh = v2s.sh


def arg(a, k, d=None):
    if k in a: i = a.index(k); v = a[i + 1]; del a[i:i + 2]; return v
    return d


def main():
    a = sys.argv[1:]
    gd = arg(a, '--game', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2')
    v2 = '/mnt/c/dev/hantei-chan/docs/audit_2026_10_14/evidence/compare/v2'
    rect = tuple(int(x) for x in arg(a, '--rect').split(','))
    add = np.array([float(x) for x in arg(a, '--add', '0,0,0').split(',')])
    case = a[0]; gn = arg(a, '--game-name', case)
    ours = arg(a, '--ours', os.path.join(v2, 'ours', case + '_ours.png'))
    meta = json.load(open(os.path.join(gd, gn + '_game.json')))
    if '--aura' in a:  # the game's specular add of P1's parts from the sidecar (v2_model.aura_add)
        a.remove('--aura')
        if meta.get('aura_type', -1) >= 0 and 'aura_rgb' in meta:
            add = np.array([int(int(c * 255.0) * meta['aura_strength']) for c in meta['aura_rgb']], float)
    gorg, gsc = sh.game_transform(meta)
    ed = np.asarray(v2s.place(Image.open(ours).convert('RGB'), gorg, abs(gsc[0]))).astype(float)
    g = np.asarray(Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')).astype(float)
    x0, y0, x1, y1 = rect
    e = np.clip(ed[y0:y1, x0:x1] + add, 0, 255); gg = g[y0:y1, x0:x1]
    d = np.abs(e - gg)
    print('%s vs %s rect %s add %s: mean |d| %.2f, %.1f%% px > 24 (game mean %s, editor mean %s)'
          % (case, gn, rect, add.tolist(), d.mean(), (d.sum(2) > 24).mean() * 100,
             np.round(gg.reshape(-1, 3).mean(0), 1).tolist(), np.round(e.reshape(-1, 3).mean(0), 1).tolist()))


if __name__ == '__main__':
    main()
