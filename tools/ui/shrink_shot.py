#!/usr/bin/env python3
# tools/ui/shrink_shot.py <in.png> <out.png> [x0 y0 x1 y1] [--max W] : crop an app capture to the relevant panel, optionally
# downscale to at most W px wide, and palette-quantize (256 colours) so evidence PNGs stay small in git. ImGui UIs quantize well.
import sys
from PIL import Image

args = [a for a in sys.argv[1:]]
maxw = 0
if '--max' in args:
    i = args.index('--max'); maxw = int(args[i + 1]); del args[i:i + 2]
src, dst = args[0], args[1]
im = Image.open(src).convert('RGB')
if len(args) >= 6:
    x0, y0, x1, y1 = map(int, args[2:6])
    im = im.crop((x0, y0, x1, y1))
if maxw and im.width > maxw:
    im = im.resize((maxw, round(im.height * maxw / im.width)), Image.LANCZOS)
im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).save(dst, optimize=True)
print(f"{dst}: {im.width}x{im.height}")
