#!/usr/bin/env python3
"""Side-by-side sheet: game capture | Hantei-chan 3D stage render | abs difference (x2), one row per camera.
usage: sheet.py out.png "label" game.png hantei.png ["label" game.png hantei.png ...]"""
import sys
from PIL import Image, ImageChops, ImageDraw

def main(a):
    out, rows = a[0], [a[i:i + 3] for i in range(1, len(a), 3)]
    W, H = 640, 360
    sheet = Image.new("RGB", (W * 3, (H + 24) * len(rows) + 24), (20, 20, 20))
    d = ImageDraw.Draw(sheet)
    for c, t in enumerate(["game (UNI2 1.42, gamecap)", "Hantei-chan 3D Stage view", "|difference| x2"]):
        d.text((c * W + 6, 4), t, fill=(230, 230, 230))
    for r, (label, g, h) in enumerate(rows):
        gi = Image.open(g).convert("RGB").resize((W, H), Image.LANCZOS)
        hi = Image.open(h).convert("RGB").resize((W, H), Image.LANCZOS)
        df = ImageChops.difference(gi, hi).point(lambda v: min(255, v * 2))
        y = 24 + r * (H + 24)
        d.text((6, y + 4), label, fill=(255, 220, 120))
        for c, im in enumerate([gi, hi, df]):
            sheet.paste(im, (c * W, y + 20))
        mean = sum(ImageChops.difference(gi, hi).convert("L").getdata()) / (W * H)
        d.text((2 * W + 6, y + 4), "mean |diff| %.1f / 255" % mean, fill=(255, 220, 120))
    sheet.save(out)

if __name__ == "__main__":
    main(sys.argv[1:])
