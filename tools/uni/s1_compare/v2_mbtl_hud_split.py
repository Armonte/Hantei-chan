#!/usr/bin/env python3
"""v2_mbtl_hud_split.py: render-match14. mbtl_patctrl_chr001_p238f10_z1: split the residual by the opaque tray prop (slot 5
solo editor shot), test +-1 px shifts on it, and find the rows where the MAGIC CIRCUIT HUD gauge (in the plate) is drawn
OVER the opaque tray in the game (game == plate inside an opaque editor footprint). Prints the residual on / off those rows."""
import sys, os, numpy as np
from PIL import Image
sys.path.insert(0, '/home/teo/dev/hantei-chan-wt/render14/tools/uni/s1_compare')
import v2_model as vm; sh = vm.sh
sys.argv = ['v2_model.py','mbtl_patctrl_chr001_p238f10_z1','--game-name','mbtl_patctrl_chr001_p238f10_nb','--light','1,0;1,0;1,0','--grey','--subpix','0,0']
vm.main()
G, B, P, Q, m = sh.game_vs_model.last_arrays
lp = np.asarray(sh.game_vs_model.last_light[3]).astype(np.float64)
xa, ya, xb, yb, gorg = sh.game_vs_model.last_window
bw = os.path.join(vm.D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw')
n='mbtl_patctrl_chr001_p238f10_solo_s5_z1'
K = np.asarray(Image.open(os.path.join(bw, n + '_new_k.png')).convert('RGB')).astype(np.float64).max(2)
o = sh.editor_origin(Image.open(os.path.join(bw, n + '_new_k.png')))
yy, xx = np.mgrid[ya:yb, xa:xb]
ex = np.round(xx - gorg[0] + o[0]).astype(int); ey = np.round(yy - gorg[1] + o[1]).astype(int)
ok = (ex >= 0) & (ey >= 0) & (ex < K.shape[1]) & (ey < K.shape[0])
a = np.zeros(ex.shape); a[ok] = K[ey[ok], ex[ok]]
s5 = (a > 4) & m
ys, xs = np.nonzero(s5); print('s5 bbox', xs.min(), xs.max(), ys.min(), ys.max(), s5.sum())
for dy in (-1,0,1):
    print(' '.join('%d,%d:%.2f' % (dx,dy, np.abs(G - np.roll(np.roll(lp,dy,0),dx,1)).mean(2)[s5].mean()) for dx in (-1,0,1)))
d = (G - lp).mean(2); print('signed', d[s5].mean())
Bp = np.asarray(B).astype(np.float64)
occ = s5 & (np.abs(G - Bp).max(2) < 8) & (Q.mean(2) < 0.02 if Q.ndim == 3 else Q < 0.02)
ys2, xs2 = np.nonzero(occ); print('HUD-occluded opaque-tray px', occ.sum(), 'rows', ys2.min() if len(ys2) else None, ys2.max() if len(ys2) else None)
ad = np.abs(G - lp).mean(2)
hud = np.zeros_like(m)
if len(ys2): hud[ys2.min():, :] = True
for nm, s in (('footprint', m), ('HUD rows (y >= first occluded row)', m & hud), ('off HUD rows', m & ~hud), ('s5 off HUD', s5 & ~hud)):
    print('%-36s px %6d mean %.2f share %.0f%%' % (nm, s.sum(), ad[s].mean(), 100 * ad[s].sum() / ad[m].sum()))
