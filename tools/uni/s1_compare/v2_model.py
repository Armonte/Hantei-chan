#!/usr/bin/env python3
"""v2 model check: game frame vs the editor's black/white-backdrop shots composited over the game's own backdrop
(the fighters-hidden plate), raw and with the stage light of the draw object.

  v2_model.py <case> [--bw DIR] [--game DIR] [--plate PNG] [--light k,c;k,c;k,c | --light-json F] [--out PNG] [--zoom Z] [--fit | --aura-fit]

The light model is the game's own vertex path (uni2 1.42 PatPart_SetupWorldMatrixAndShader 0x415230):
diffuse = PRCL x DrawObj+0x0C (PatPart_ModulateColorARGB 0x4ACB70) and specular = PRSP + DrawObj+0x10
(PatPart_ComputeSpecularWithObjAdd 0x413BA0, skipped for PRAL 1 parts with no PRSP); PS_CharaPtDrawV:
out = tex*diffuse*palette[PPPA] + specular. Over a backdrop that is k*P + c*(1-Q) + Q*plate.
Prints mean |d| and % px > 24 on the editor footprint; the controls' floor is ~5.5 / 18%."""
import importlib.util, json, os, sys
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('sheet', os.path.join(HERE, 'sheet.py'))
sh = importlib.util.module_from_spec(spec); spec.loader.exec_module(sh)
D = os.path.abspath(os.path.join(HERE, '../../..'))


def arg(a, k, d=None):
    if k in a: i = a.index(k); v = a[i + 1]; del a[i:i + 2]; return v
    return d


def aura_add(meta):
    """The specular add colour of P1's .pat parts from the sidecar (CharaObj_DrawWithStageLight 0x5CE3C0): the chara
    aura colour (0..1 floats) as int(int(c*255) * strength) when the aura is active (type >= 0), 0 otherwise."""
    if meta.get('aura_type', -1) < 0: return (0, 0, 0)
    return tuple(int(int(c * 255.0) * meta['aura_strength']) for c in meta['aura_rgb'])


def aura_fit(game, pl, ek, ew, zoom, meta, org):
    """c per channel for game = P + c*(1-Q) + Q*plate (k = 1) on the footprint, L1-optimal (weighted median of
    (G - P - Q*plate) / (1-Q), weights 1-Q): the aura add of a capture whose sidecar predates the aura fields."""
    sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    G, B, P, Q, m = sh.game_vs_model.last_arrays
    c = []
    for ch in range(3):
        w = (1 - Q[..., ch])[m]; r = (G[..., ch] - P[..., ch] - Q[..., ch] * B[..., ch])[m]
        k = w > 0.05
        if k.sum() == 0: c.append(0.0); continue
        x = r[k] / w[k]; ww = w[k]; o = np.argsort(x); cw = np.cumsum(ww[o])
        c.append(float(x[o][np.searchsorted(cw, cw[-1] / 2)]))
    return c


def main():
    a = sys.argv[1:]
    bw = arg(a, '--bw', os.path.join(D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw'))
    gd = arg(a, '--game', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2')
    plate = arg(a, '--plate')
    lj = arg(a, '--light-json', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare/light_stage1.json')
    ls = arg(a, '--light')
    out = arg(a, '--out')
    gname = arg(a, '--game-name')
    zarg = arg(a, '--zoom')
    fitl = '--fit' in a
    if fitl: a.remove('--fit')
    afit = '--aura-fit' in a
    if afit: a.remove('--aura-fit')
    case = a[0]
    zoom = float(zarg) if zarg else next((float(z) for n, _, z in sh.cases() if n == case), 1.0)
    gn = gname or case
    game = Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')
    meta = json.load(open(os.path.join(gd, gn + '_game.json')))
    plate = plate or os.path.join(gd, gn + '_game_plate.png')
    ek = Image.open(os.path.join(bw, case + '_new_k.png')); ew = Image.open(os.path.join(bw, case + '_new_w.png'))
    org = sh.editor_origin(ek)
    if ls: light = [tuple(map(float, x.split(','))) for x in ls.split(';')]
    elif 'aura_rgb' in meta: light = [(1.0, float(c)) for c in aura_add(meta)]
    else: light = json.load(open(lj))
    pl = Image.open(plate).convert('RGB')
    sh.LIGHT = None
    r = sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    if r is None: print(case, 'no footprint'); return
    fit = sh.game_vs_model.last_fit
    if afit:  # the game's own .pat part light with an unrecorded aura: k = 1, c = the aura add (>= 0), least squares
        sh.LIGHT = [(1.0, 0.0)] * 3
        sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
        light = [(1.0, max(0.0, f)) for f in aura_fit(game, pl, ek, ew, zoom, meta, org)]
    if fitl: light = fit  # free least-squares (k, c): the floor for CG sprites, whose shader adds its own stage colour
    sh.LIGHT = light
    r = sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    _, lmean, lch, lpred, ldiff = sh.game_vs_model.last_light
    print('light (k, c) per RGB:', light)
    print('%s: raw %.2f / %.1f%%  lit %.2f / %.1f%%  (%d px, refine %+d,%+d)  free fit k,c = %s'
          % (case, r[3], r[4], lmean, lch, r[6], r[5][0], r[5][1], ', '.join('(%.2f,%.0f)' % f for f in fit)))
    if out:
        g = r[0]; w = g.width
        s = Image.new('RGB', (w * 3 + 16, g.height), (30, 30, 30))
        s.paste(g, (0, 0)); s.paste(lpred.resize(g.size), (w + 8, 0)); s.paste(ldiff.resize(g.size), (2 * w + 16, 0))
        s.save(out)


if __name__ == '__main__':
    main()
