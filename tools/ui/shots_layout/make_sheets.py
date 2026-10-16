#!/usr/bin/env python3
# make_sheets.py <out_dir> : before | after sheets (shrunk, palette PNG) from run_layout.sh's before / after folders
import sys, os
from PIL import Image, ImageDraw
W = '/mnt/c/dev/hantei-chan/work'; out = sys.argv[1]; os.makedirs(out, exist_ok=True)
for sz in ['1280x720', '1900x1000', '1920x1080']:
    b, a = f'{W}/ab_layout_before_{sz}', f'{W}/ab_layout_after_{sz}'
    for n in sorted(os.listdir(a)):
        if not n.startswith('L') or not n.endswith('.png') or not os.path.exists(f'{b}/{n}'): continue
        if n.startswith('L02') or n.startswith('L04') or n.startswith('L06') or n.startswith('L08'):
            if sz != '1280x720': continue   # panes without a layout change: one size is enough
        ib, ia = Image.open(f'{b}/{n}').convert('RGB'), Image.open(f'{a}/{n}').convert('RGB')
        h = max(ib.height, ia.height) + 28; s = Image.new('RGB', (ib.width + ia.width + 12, h), (40, 40, 40))
        s.paste(ib, (0, 28)); s.paste(ia, (ib.width + 12, 28)); d = ImageDraw.Draw(s)
        d.text((8, 6), f'BEFORE  {n[:-4]}  {sz}', fill=(255, 200, 120)); d.text((ib.width + 20, 6), 'AFTER', fill=(140, 255, 140))
        s = s.resize((1600, round(s.height * 1600 / s.width)), Image.LANCZOS)
        p = f'{out}/{n[:-4]}_{sz}.png'
        s.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(p, optimize=True); print(p)
