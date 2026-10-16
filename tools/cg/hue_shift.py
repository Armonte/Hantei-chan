#!/usr/bin/env python3
"""Recolour for the in-game showcase: rotate the hue of the coloured pixels in one hue band, keep everything else.

  hue_shift.py png  <in.png>  <out.png>  --band LO HI --shift DEG [--smin S] [--satmul M] [--valmul V]
  hue_shift.py act  <in.act>  <out.act>  (same options)   a 768-byte .act palette (the CG manager's "Export colours")
  hue_shift.py hist <in.png|in.act>                        hue histogram of the coloured pixels (to pick the band)
  --idx 8-12,31-40   act only: shift only these palette entries (what a modder selects in the swatch grid), band still applies
  --colorize H S     png only: grey pixels (saturation < smin, alpha > 0) get hue H and saturation S, value kept (a grey
                     texture that the game tints with a palette slot)
  --rect X Y W H     png only, repeatable: change only pixels inside these rectangles (a .pat cutout's texel area)

A pixel / colour is shifted when its saturation >= smin and its hue (degrees) lies in [LO, HI] (LO > HI wraps through 0).
Shifted: H += shift, S *= satmul (clamped), V *= valmul (clamped). Value (shading, line art) and alpha are never touched
otherwise; greys, black line art and out-of-band colours (skin) come out byte-identical.
"""
import argparse, colorsys, sys
from PIL import Image

def in_band(h, lo, hi): return lo <= h <= hi if lo <= hi else (h >= lo or h <= hi)

def shift_rgb(r, g, b, a):
    h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
    if a.colorize and s < a.smin:
        return tuple(int(round(c * 255)) for c in colorsys.hsv_to_rgb(a.colorize[0] / 360, a.colorize[1], v))
    if s < a.smin or not in_band(h * 360, *a.band): return None
    h = ((h * 360 + a.shift) % 360) / 360; s = min(1, s * a.satmul); v = min(1, v * a.valmul)
    return tuple(int(round(c * 255)) for c in colorsys.hsv_to_rgb(h, s, v))

def main():
    p = argparse.ArgumentParser(); p.add_argument("mode"); p.add_argument("inp"); p.add_argument("out", nargs="?")
    p.add_argument("--band", nargs=2, type=float, default=[0, 360]); p.add_argument("--shift", type=float, default=0)
    p.add_argument("--smin", type=float, default=0.2); p.add_argument("--satmul", type=float, default=1); p.add_argument("--valmul", type=float, default=1)
    p.add_argument("--idx", default=""); p.add_argument("--colorize", nargs=2, type=float)
    p.add_argument("--rect", nargs=4, type=int, action="append", default=[])
    a = p.parse_args()
    if a.mode == "hist":
        if a.inp.endswith(".act"):
            d = open(a.inp, "rb").read()[:768]; cols = [(d[i], d[i + 1], d[i + 2], 255) for i in range(0, 768, 3)]
        else: cols = list(Image.open(a.inp).convert("RGBA").getdata())
        hist = [0] * 36
        for r, g, b, al in cols:
            if not al: continue
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            if s >= a.smin and v > 0.08: hist[int(h * 36) % 36] += 1
        for i, n in enumerate(hist):
            if n: print("%3d-%3d deg %7d" % (i * 10, i * 10 + 9, n))
        return
    n = 0
    if a.mode == "act":
        d = bytearray(open(a.inp, "rb").read())
        sel = set()
        for part in filter(None, a.idx.split(",")):
            lo, _, hi = part.partition("-"); sel.update(range(int(lo), int(hi or lo) + 1))
        for i in range(0, 768, 3):
            if sel and i // 3 not in sel: continue
            o = shift_rgb(d[i], d[i + 1], d[i + 2], a)
            if o: d[i:i + 3] = bytes(o); n += 1
        open(a.out, "wb").write(d)
    else:
        im = Image.open(a.inp).convert("RGBA"); px = list(im.getdata()); cache = {}; W = im.width
        inside = lambda k: not a.rect or any(x <= k % W < x + w and y <= k // W < y + h for x, y, w, h in a.rect)
        for k, (r, g, b, al) in enumerate(px):
            if not al or not inside(k): continue
            key = (r, g, b)
            if key not in cache: cache[key] = shift_rgb(r, g, b, a)
            o = cache[key]
            if o: px[k] = (*o, al); n += 1
        im.putdata(px); im.save(a.out)
    print("%s: %d %s shifted (band %g..%g, shift %+g deg, smin %g, sat x%g, val x%g)" % (a.out, n, "colours" if a.mode == "act" else "pixels", *a.band, a.shift, a.smin, a.satmul, a.valmul))

if __name__ == "__main__": main()
