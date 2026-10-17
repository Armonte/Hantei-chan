#!/usr/bin/env python3
"""Compare a game capture with a Hantei-chan render (tools/stage_capture).

    stage_diff.py <game.bmp|png> <hantei.png> <out_prefix> [--mask x0,y0,x1,y1 ...] [--label TEXT]

Writes <out_prefix>_triple.png (game | hantei | amplified |diff|) and prints metrics:
  mae      mean absolute error per channel (0..255)
  exact    % of pixels identical in all channels
  close    % of pixels within 8/255 in every channel (the "tiny filtering differences" budget)
  psnr     dB
Masked rectangles (e.g. the FPS counter) are excluded from every metric.
"""
import sys, json
from PIL import Image, ImageChops, ImageDraw
import numpy as np


def load(p):
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.int16)


def main():
    a, b, out = sys.argv[1], sys.argv[2], sys.argv[3]
    masks, label = [], ""
    i = 4
    while i < len(sys.argv):
        if sys.argv[i] == "--mask":
            masks.append(tuple(int(v) for v in sys.argv[i + 1].split(","))); i += 2
        elif sys.argv[i] == "--label":
            label = sys.argv[i + 1]; i += 2
        else:
            i += 1
    A, B = load(a), load(b)
    if A.shape != B.shape:
        print(json.dumps({"error": f"size {A.shape} vs {B.shape}"})); return 2
    valid = np.ones(A.shape[:2], bool)
    for x0, y0, x1, y1 in masks:
        valid[y0:y1, x0:x1] = False
    D = np.abs(A - B)
    Dv = D[valid]
    mae = float(Dv.mean())
    exact = float((Dv.max(axis=1) == 0).mean() * 100)
    close = float((Dv.max(axis=1) <= 8).mean() * 100)
    mse = float((Dv.astype(np.float64) ** 2).mean())
    psnr = 99.0 if mse == 0 else float(10 * np.log10(255 * 255 / mse))
    amp = np.clip(D * 4, 0, 255).astype(np.uint8)
    amp[~valid] = (40, 0, 40)
    h, w = A.shape[:2]
    tri = Image.new("RGB", (w * 3, h + 18), (0, 0, 0))
    tri.paste(Image.fromarray(A.astype(np.uint8)), (0, 18))
    tri.paste(Image.fromarray(B.astype(np.uint8)), (w, 18))
    tri.paste(Image.fromarray(amp), (w * 2, 18))
    d = ImageDraw.Draw(tri)
    d.text((4, 3), "GAME " + label, fill=(255, 255, 0))
    d.text((w + 4, 3), "HANTEI-CHAN", fill=(0, 255, 255))
    d.text((2 * w + 4, 3), f"|diff|x4  mae={mae:.2f} exact={exact:.1f}% close={close:.1f}% psnr={psnr:.1f}", fill=(255, 128, 128))
    tri.save(out + "_triple.png")
    print(json.dumps({"mae": round(mae, 3), "exact": round(exact, 2), "close": round(close, 2), "psnr": round(psnr, 2)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
