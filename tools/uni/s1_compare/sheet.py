#!/usr/bin/env python3
"""Contact sheets for the S1 render comparison.

  sheet.py <compare dir>

For every case in cases.txt: <name>_old.png | <name>_new.png | (|old-new| x4) from <dir>/editor, plus
<dir>/game/<name>_game.png when present (then also |game - new| after aligning on the brightest-content box).
Writes <dir>/sheets/<name>.png and prints the mean / changed-pixel stats per pair."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))


def cases():
    for ln in open(os.path.join(HERE, 'cases.txt'), encoding='utf-8'):
        if ln.startswith('#') or not ln.strip(): continue
        f = [x.strip() for x in ln.split('|')]
        yield f[0], f[-1]


def crop_box(imgs, pad=24):
    """Common box around everything that differs from the corner (clear) colour in any image."""
    box = None
    for im in imgs:
        a = np.asarray(im.convert('RGB')).astype(int)
        bg = a[2, 2]
        m = np.abs(a - bg).sum(2) > 24
        # drop the editor's full-length grid lines
        m[m.mean(1) > 0.8, :] = False
        m[:, m.mean(0) > 0.8] = False
        ys, xs = np.nonzero(m)
        if len(xs) == 0: continue
        b = (xs.min(), ys.min(), xs.max(), ys.max())
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    if box is None: return None
    w, h = imgs[0].size
    return (max(0, box[0] - pad), max(0, box[1] - pad), min(w, box[2] + pad), min(h, box[3] + pad))


def diff(a, b):
    x = np.asarray(a.convert('RGB')).astype(int); y = np.asarray(b.convert('RGB')).astype(int)
    d = np.abs(x - y)
    changed = (d.sum(2) > 24).mean() * 100.0
    return Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)), d.mean(), changed


def label(im, text):
    out = Image.new('RGB', (im.width, im.height + 22), (20, 20, 20))
    out.paste(im, (0, 22))
    ImageDraw.Draw(out).text((6, 4), text, fill=(240, 240, 240))
    return out


def main():
    root = sys.argv[1]
    ed = os.path.join(root, 'editor'); gd = os.path.join(root, 'game'); od = os.path.join(root, 'sheets')
    os.makedirs(od, exist_ok=True)
    for name, note in cases():
        po, pn = os.path.join(ed, name + '_old.png'), os.path.join(ed, name + '_new.png')
        if not (os.path.exists(po) and os.path.exists(pn)):
            print(name, 'missing editor shots'); continue
        old, new = Image.open(po).convert('RGB'), Image.open(pn).convert('RGB')
        box = crop_box([old, new]) or (0, 0, old.width, old.height)
        old_c, new_c = old.crop(box), new.crop(box)
        d, mean, changed = diff(old_c, new_c)
        tiles = [label(old_c, 'old build (395a56b)'), label(new_c, 'new build (S1)'), label(d, '|old - new| x4')]
        pg = os.path.join(gd, name + '_game.png')
        if os.path.exists(pg):
            tiles.insert(0, label(Image.open(pg).convert('RGB'), 'game (UNI2)'))
        W = sum(t.width for t in tiles) + 8 * (len(tiles) - 1); H = max(t.height for t in tiles)
        sheet = Image.new('RGB', (W, H + 24), (32, 32, 32))
        x = 0
        for t in tiles:
            sheet.paste(t, (x, 24)); x += t.width + 8
        ImageDraw.Draw(sheet).text((6, 6), '%s: %s' % (name, note), fill=(255, 220, 120))
        sheet.save(os.path.join(od, name + '.png'))
        print('%-28s old->new mean |d| %.2f, %.2f%% pixels changed' % (name, mean, changed))


if __name__ == '__main__':
    main()
