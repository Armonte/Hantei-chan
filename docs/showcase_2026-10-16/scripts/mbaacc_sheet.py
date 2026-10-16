#!/usr/bin/env python3
"""mbaacc_sheet.py: the MBAACC / ActCadenza editor-only sheet (no game capture: gamecap drives UNI2 and MBTL only).
Same view (1264x861, zoom 2, 64-grey clear colour) from each tool, cropped to the union of what any tool drew."""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont
S = '/home/teo/dev/hantei-chan-wt/parity/docs/showcase_2026-10-16'; Wk = S + '/work'
F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15); FB = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
case = 'ac_akiha3_p238f4'
cols = [('Hantei-chan (ours)', f'{Wk}/bw_ours/{case}_new_g.png', 'effect spawns previewed (effect.ha6)'),
        ('ours, spawns off', None, 'own frame only (for reference)'),
        ("sosfiro's fork", f'{Wk}/bw_sosfiro/{case}_new_g.png', ''),
        ('gonp upstream', f'{Wk}/bw_gonp/{case}_new_g.png', 'one sprite, no layers / effects')]
ims = []
for name, p, sub in cols:
    if p and os.path.exists(p): ims.append(np.asarray(Image.open(p).convert('RGB')).astype(int))
    else: ims.append(None)
# spawns-off reference: our k shot without spawns, recoloured onto the grey clear colour where it is black
k = np.asarray(Image.open(f'{Wk}/bw_ours/{case}_nospawn_k.png').convert('RGB')).astype(int)
ims[1] = np.where((k.sum(2) == 0)[..., None], 64, k)
m = np.zeros(ims[0].shape[:2], bool)
for a in ims:
    if a is not None: m |= np.abs(a - 64).sum(2) > 6
m[:, 631:634] = False; m[[i for i in range(m.shape[0]) if m[i].mean() > 0.8]] = False
ys, xs = np.nonzero(m); y0, y1, x0, x1 = max(0, ys.min() - 20), ys.max() + 20, max(0, xs.min() - 20), xs.max() + 20
W = 396; out = []
for (name, p, sub), a in zip(cols, ims):
    c = Image.new('RGB', (W, 520), (22, 22, 26)); d = ImageDraw.Draw(c)
    if a is None:
        d.rectangle((0, 46, W, 520), fill=(60, 20, 20))
        for i, ln in enumerate(["CAN'T SHOW: capture build crashed", 'loading MBAACC akiha_0 / akiha_3', '(.txt and .HA6, exit 5, minidump).', 'Not checked in sosfiro\'s own UI,', 'so not counted as a sosfiro failure.']):
            d.text((10, 60 + 22 * i), ln, font=F, fill=(255, 220, 220))
    else:
        cr = Image.fromarray(np.clip(a[y0:y1, x0:x1], 0, 255).astype(np.uint8)); s = min(W / cr.width, 470 / cr.height)
        cr = cr.resize((int(cr.width * s), int(cr.height * s)), Image.LANCZOS); c.paste(cr, ((W - cr.width) // 2, 48))
    d.text((6, 3), name, font=FB, fill=(235, 235, 235)); d.text((6, 25), sub, font=F, fill=(200, 200, 200)); out.append(c)
sheet = Image.new('RGB', (4 * (W + 4) - 4, 520 + 58), (10, 10, 10)); d = ImageDraw.Draw(sheet)
d.text((8, 4), 'ActCadenza moon-3 Akiha p238 frame 4 (MBAACC data, effect spawns)  [MBAACC, editor-only]', font=FB, fill=(255, 255, 255))
d.text((8, 32), 'No game column: gamecap has no MBAACC driver, so there is no pixel metric for this case. Qualitative only.', font=F, fill=(190, 190, 190))
for i, c in enumerate(out): sheet.paste(c, (i * (W + 4), 58))
os.makedirs(S + '/sheets', exist_ok=True); sheet.save(S + '/sheets/mbaacc_actcadenza_p238.jpg', quality=88); print(sheet.size)
