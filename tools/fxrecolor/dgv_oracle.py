#!/usr/bin/env python3
"""Regression oracle: DGV's indexed Akiha sprites ("Better Akiha" by DGV) vs the runtime shader maths (cgmtool fxr shade, i.e. the vendored
FxRecolor.hpp) applied to his ORIGINAL pixels with the shipped dgv_akiha.ini.  Samples N sprites per group.
  dgv_oracle.py <'Better Akiha v2' dir> <cgmtool.exe> <dgv_akiha.ini> [per-group=5]
Exit 1 when a group's mean abs error (0..255) exceeds its tolerance (80 and 90 are known weaker: multi-hue / single sprite)."""
import sys, os, subprocess, numpy as np
from PIL import Image
TOL = {'80': 20.0, '90': 30.0}
def shade(tool, ini, rule, rgba):
    h, w = rgba.shape[:2]
    p = subprocess.run([tool, 'fxr', 'shade', ini, rule], input=b'%d %d\n' % (w, h) + rgba.tobytes(), capture_output=True)
    return np.frombuffer(p.stdout, np.uint8).reshape(h, w, 4)
root, tool, ini = sys.argv[1:4]; per = int(sys.argv[4]) if len(sys.argv) > 4 else 5
winini = subprocess.run(['wslpath', '-w', ini], capture_output=True, text=True).stdout.strip()
bad = 0
for g in ['00', '01', '02', '10', '20', '21', '30', '80', '90']:
    ds, dm = [os.path.join(root, x, g) for x in ('01-3 Partitioned Display Sprites', '02-1 modded display sprites')]
    files = [f for f in sorted(os.listdir(ds)) if f.endswith('.png') and os.path.exists(os.path.join(dm, f))]
    files = files[::max(1, len(files) // per)][:per]
    errs, within = [], []
    for f in files:
        a = np.array(Image.open(os.path.join(ds, f)).convert('RGBA')); b = np.array(Image.open(os.path.join(dm, f)).convert('RGBA'))
        o = shade(tool, winini, 'dgv_' + g, a)
        m = (a[..., 3] > 127) & (b[..., 3] > 127)
        if not m.any(): continue
        e = np.abs(o[..., :3].astype(int) - b[..., :3].astype(int)).max(2)[m]
        errs.append(e); within.append((e <= 24).mean())
    e = np.concatenate(errs); ok = e.mean() <= TOL.get(g, 6.0)
    print('group %s: %d sprites, mean abs err %.2f, %.1f%% within 24/255 %s' % (g, len(files), e.mean(), 100 * np.mean(within), 'ok' if ok else 'FAIL'))
    bad += not ok
print('SECTION fxr-dgv-oracle: %s' % ('FAIL' if bad else 'pass'))
sys.exit(1 if bad else 0)
