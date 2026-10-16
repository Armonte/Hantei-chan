#!/usr/bin/env python3
# make_sheets.py <out_dir> [before_tag after_tag [dir_suffix]] : before | after sheets (shrunk, palette PNG) from run_layout.sh's before / after folders
import sys, os
from PIL import Image, ImageDraw
W = '/mnt/c/dev/hantei-chan/work'; out = sys.argv[1]; os.makedirs(out, exist_ok=True)
BT, AT = (sys.argv[2], sys.argv[3]) if len(sys.argv) > 3 else ('before', 'after')   # run_layout.sh tags
SUF = sys.argv[4] if len(sys.argv) > 4 else ''   # e.g. _L_r3 for run_layout.sh's non-L_all scripts
for sz in ['1280x720', '1900x1000', '1920x1080']:
    b, a = f'{W}/ab_layout_{BT}_{sz}{SUF}', f'{W}/ab_layout_{AT}_{sz}{SUF}'
    if not os.path.isdir(a): continue
    for n in sorted(os.listdir(a)):
        if n[0] not in 'LR' or not n.endswith('.png') or not os.path.exists(f'{b}/{n}'): continue
        if n.startswith('L02') or n.startswith('L04') or n.startswith('L06') or n.startswith('L08'):
            if sz != '1280x720': continue   # panes without a layout change: one size is enough
        ib, ia = Image.open(f'{b}/{n}').convert('RGB'), Image.open(f'{a}/{n}').convert('RGB')
        h = max(ib.height, ia.height) + 28; s = Image.new('RGB', (ib.width + ia.width + 12, h), (40, 40, 40))
        s.paste(ib, (0, 28)); s.paste(ia, (ib.width + 12, 28)); d = ImageDraw.Draw(s)
        d.text((8, 6), f'BEFORE  {n[:-4]}  {sz}', fill=(255, 200, 120)); d.text((ib.width + 20, 6), 'AFTER', fill=(140, 255, 140))
        s = s.resize((1600, round(s.height * 1600 / s.width)), Image.LANCZOS)
        p = f'{out}/{n[:-4]}_{sz}.png'
        s.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(p, optimize=True); print(p)
