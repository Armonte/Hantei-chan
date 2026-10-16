#!/usr/bin/env python3
"""v2_lamp_match.py ALT_PLATE <v2_model args...>: render-match18. Stage 21 (night alley) has a flickering door lamp that
lights the wall / floor around x 560-840, y 360-680 by up to +8 levels (plate_b0 - plate of one session). The game shot and
its plate are different frames, so they can sit at different lamp levels, and that difference lands on the prediction
as a smooth, bluish "brighter / darker part". This fits the lamp light of the case's game frame from the backdrop
pixels around the footprint and measures the case against it:
  L = ALT_PLATE - plate (a lamp-light map from two plates of the session with different lamp states)
  B' = plate + lam_c * L, lam_c per channel = least squares of (G - plate) on L over the backdrop px (outside the
       footprint dilated by 8 px, |L| > 0.5) of the window
  raw' = P + Q * B'  (float), printed next to the plain raw.
Zoom-1 windows only (the plate crop is direct pixels)."""
import sys, os, numpy as np
from PIL import Image
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import v2_model as vm; sh = vm.sh
alt = sys.argv[1]
sys.argv = ['v2_model.py'] + sys.argv[2:]
vm.main()
G, B, P, Q, m = sh.game_vs_model.last_arrays
xa, ya, xb, yb, _ = sh.game_vs_model.last_window
A = np.asarray(Image.open(alt).convert('RGB')).astype(np.float64)[ya:yb, xa:xb]
L = A - B
bg = ~ndimage.binary_dilation(m, iterations=8) & (np.abs(L).mean(2) > 0.5)
lam = []
for c in range(3):
    l = L[..., c][bg]; r = (G - B)[..., c][bg]
    lam.append(float((l * r).sum() / max((l * l).sum(), 1e-9)) if bg.sum() > 50 else 0.0)
lam = np.array(lam)
B2 = B + lam * L
raw = np.clip(P + Q * B, 0, 255); raw2 = np.clip(P + Q * B2, 0, 255)
d1 = np.abs(G - raw).mean(2); d2 = np.abs(G - raw2).mean(2)
print('lamp fit: %d backdrop px, lam R/G/B %+.3f %+.3f %+.3f; backdrop |G-B| %.2f -> %.2f' % (
    bg.sum(), *lam, np.abs(G - B).mean(2)[bg].mean(), np.abs(G - B2).mean(2)[bg].mean()))
print('footprint %d px: raw %.2f -> lamp-matched %.2f (signed %+.2f -> %+.2f)' % (
    m.sum(), d1[m].mean(), d2[m].mean(), (G - raw).mean(2)[m].mean(), (G - raw2).mean(2)[m].mean()))
