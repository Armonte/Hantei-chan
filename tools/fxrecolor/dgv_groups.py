#!/usr/bin/env python3
"""DGV "Better Akiha v2" -> runtime recolour ruleset (credit: DGV, community modder).
Derives, per DGV colour group (folders 00,01,02,10,20,21,30,80,90 of '01-3 Partitioned Display Sprites'):
  * the sprite ids (matched by name against akiha.cg),
  * a ramp: the group's used palette colours (from the indexed '02-1 modded display sprites') ordered by brightness, fitted as 16 evenly spaced stops (the runtime's Oklab ramp, kRampN),
  * the by/vrange that best reproduces his indexing from the ORIGINAL pixels (the oracle).
Outputs  docs/cg/effect_recolor_data/dgv_akiha.ini  and  dgv_akiha_oracle.csv.
  dgv_groups.py <'Better Akiha v2' dir> <akiha.cg> <out dir>
"""
import sys, os, numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from cgparse import Bank

GROUPS = ['00', '01', '02', '10', '20', '21', '30', '80', '90']
DESC = {'00': 'Red (white-red-black ramp)', '01': 'Inverted red, grayscale source', '02': 'Grayscale red, grayscale source',
        '10': 'Red to pink', '20': 'multi-colour (rainbow-ified) red/pink', '21': 'multi-colour second set', '30': 'crimson',
        '80': 'multi-colour last-arc palette', '90': 'last arc (single sprite)'}

def load_pairs(root, g):
    src, mod = {}, {}
    ds, dm = os.path.join(root, '01-3 Partitioned Display Sprites', g), os.path.join(root, '02-1 modded display sprites', g)
    for f in sorted(os.listdir(ds)):
        if not f.endswith('.png') or not os.path.exists(os.path.join(dm, f)): continue
        a = np.array(Image.open(os.path.join(ds, f)).convert('RGBA')).astype(np.float32) / 255
        b = np.array(Image.open(os.path.join(dm, f)).convert('RGBA')).astype(np.float32) / 255
        if a.shape != b.shape: continue
        src[f[:-4]] = a; mod[f[:-4]] = b
    return src, mod

def s2l(x): return np.where(x >= 0.04045, ((x + 0.055) / 1.055) ** 2.4, x / 12.92)
def l2s(x): return np.where(x >= 0.0031308, 1.055 * np.maximum(x, 0) ** (1 / 2.4) - 0.055, 12.92 * x)
M1 = np.array([[0.4122214708, 0.5363325363, 0.0514459929], [0.2119034982, 0.6806995451, 0.1073969566], [0.0883024619, 0.2817188376, 0.6299787005]])
M2 = np.array([[0.2104542553, 0.7936177850, -0.0040720468], [1.9779984951, -2.4285922050, 0.4505937099], [0.0259040371, 0.7827717662, -0.8086757660]])
def to_lab(rgb): return np.cbrt(s2l(rgb) @ M1.T) @ M2.T
def from_lab(lab): return l2s(((lab @ np.linalg.inv(M2).T) ** 3) @ np.linalg.inv(M1).T)

def lum_stats(src, mod, by):
    S, T = [], []
    for k in src:
        a, b = src[k], mod[k]
        m = (a[..., 3] > 0.5) & (b[..., 3] > 0.5)
        rgb = a[..., :3][m]
        v = rgb.max(1) if by == 'max' else to_lab(rgb)[:, 0] if by == 'lightness' else rgb @ np.array([.299, .587, .114], np.float32)
        S.append(v); T.append(to_lab(b[..., :3][m]))   # targets in Oklab: the runtime interpolates the ramp there
    return np.concatenate(S), np.concatenate(T)

def fit_ramp(v, t, n=16):
    """ramp entry k = mean target colour of source values near k/(n-1) (linear interpolation = what the shader does)."""
    lo, hi = np.percentile(v, 1), np.percentile(v, 99.5)
    if hi - lo < 0.05: lo, hi = 0.0, 1.0
    u = np.clip((v - lo) / (hi - lo), 0, 1)
    x = u * (n - 1)
    # least squares on the hat basis
    A = np.zeros((len(x), n), np.float32)
    k0 = np.floor(x).astype(int).clip(0, n - 2); f = x - k0
    A[np.arange(len(x)), k0] = 1 - f; A[np.arange(len(x)), k0 + 1] = f
    R = np.linalg.lstsq(A, t, rcond=None)[0]   # Oklab entries (rows with no samples stay at the lstsq minimum-norm value)
    return lo, hi, R

def apply_ramp(v, lo, hi, R):
    n = len(R); x = np.clip((v - lo) / max(hi - lo, 1e-4), 0, 1) * (n - 1)
    k0 = np.floor(x).astype(int).clip(0, n - 2); f = (x - k0)[:, None]
    return R[k0] * (1 - f) + R[k0 + 1] * f

def hexc(c): return '#%02x%02x%02x' % tuple(int(round(float(x) * 255)) for x in np.clip(from_lab(np.asarray(c)), 0, 1))

if __name__ == '__main__':
    root, cg, out = sys.argv[1:4]
    bank = Bank(cg)
    byname = {m.name.rsplit('.', 1)[0].lower(): i for i, m in bank.images.items()}
    ini = ['; "Better Akiha" by DGV -- the colour groups of his manual Photoshop pipeline, re-derived as runtime recolour rules.',
           '; Sprites are grouped exactly as he sorted them (folders 00..90); each ramp is fitted to HIS indexed output from the ORIGINAL pixels',
           '; (see tools/fxrecolor/dgv_groups.py and dgv_akiha_oracle.csv). Starting point only: the ramps are his reds, the accent tokens are not used.',
           '; Credit: DGV, "Better Akiha v2".']
    rows = ['group,sprites,matched_ids,by,pixels,mean_abs_err,pct_within_16,pct_within_32']
    for g in GROUPS:
        src, mod = load_pairs(root, g)
        ids = sorted({byname[k.lower()] for k in src if k.lower() in byname})
        if not src: continue
        best = None
        for by in ('lightness', 'max', 'luma'):
            v, t = lum_stats(src, mod, by)
            if len(v) < 50: continue
            lo, hi, R = fit_ramp(v, t)
            e = np.abs(np.clip(from_lab(apply_ramp(v, lo, hi, R)), 0, 1) - np.clip(from_lab(t), 0, 1)).max(1)
            sc = float(e.mean())
            if best is None or sc < best[0]: best = (sc, by, lo, hi, R, e, len(v))
        if best is None: continue
        sc, by, lo, hi, R, e, n = best
        rows.append('%s,%d,%d,%s,%d,%.4f,%.1f,%.1f' % (g, len(src), len(ids), by, n, sc * 255, 100 * (e <= 16 / 255).mean(), 100 * (e <= 32 / 255).mean()))
        ini += ['', '[rule dgv_%s]' % g, '; %s: %d sprites (%d of them found in akiha.cg)' % (DESC[g], len(src), len(ids)),
                'sprites = ' + ','.join(map(str, ids)), 'bank = char', 'kind = lumramp', 'by = ' + by, 'vrange = %.3f,%.3f' % (lo, hi),
                'ramp = ' + ' '.join(hexc(c) for c in R)]
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, 'dgv_akiha.ini'), 'w').write('\n'.join(ini) + '\n')
    open(os.path.join(out, 'dgv_akiha_oracle.csv'), 'w').write('\n'.join(rows) + '\n')
    print('\n'.join(rows))
