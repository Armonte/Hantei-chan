#!/usr/bin/env python3
"""Contact sheets for the S1 render comparison.

  sheet.py <compare dir>

For every case in cases.txt: <name>_old.png | <name>_new.png | (|old-new| x4) from <dir>/editor, plus
<dir>/game/<name>_game.png when present (tools/gamecap writes those). The game shot is a whole game frame
(stage behind the character, unknown camera scale), so it is registered to the new editor shot first:
a scale x offset x mirror search maximising normalised cross-correlation of edge maps (FFT), then the game
region under the editor crop is resampled to the editor crop and |game - new| is measured only on the
editor's content pixels (where the editor shows sprite, not its clear colour/grid).
Writes <dir>/sheets/<name>.png and prints the mean / changed-pixel stats per pair; with a game shot it
also writes <dir>/sheets/game_vs_new.tsv."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageOps

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


def content_mask(im):
    """Pixels that differ from the clear colour (corner pixel), editor grid lines removed."""
    a = np.asarray(im.convert('RGB')).astype(int)
    m = np.abs(a - a[2, 2]).sum(2) > 24
    m[m.mean(1) > 0.8, :] = False
    m[:, m.mean(0) > 0.8] = False
    return m


def _corr(img, k, fs):
    """Valid-mode cross-correlation of 2-D k over img via FFT."""
    H, W = img.shape; h, w = k.shape
    return np.fft.irfft2(np.fft.rfft2(img, fs) * np.conj(np.fft.rfft2(k, fs)), fs)[:H - h + 1, :W - w + 1]


def masked_ncc(img, tpl, mask):
    """Zero-mean NCC of an RGB template over an RGB image, counting only template pixels where mask is set
    (so the stage behind the game sprite and the editor clear colour do not take part). Returns a map."""
    H, W, _ = img.shape; h, w, _ = tpl.shape
    if h > H or w > W or mask.sum() < 16: return None
    fs = (H, W); M = mask.astype(np.float64); n = M.sum()
    num = 0.0; ivar = 0.0; tvar = 0.0
    sM = None
    for c in range(3):
        I = img[:, :, c]; T = tpl[:, :, c]
        tz = (T - (T * M).sum() / n) * M
        tvar += (tz * tz).sum()
        num = num + _corr(I, tz, fs)
        s1 = _corr(I, M, fs); s2 = _corr(I * I, M, fs)
        ivar = ivar + np.maximum(s2 - s1 * s1 / n, 0)
    return num / np.sqrt(np.maximum(ivar * tvar, 1e-9))


def register(game, new_c, scales=np.arange(0.40, 1.61, 0.02)):
    """Find scale (game px per editor px), mirror and top-left offset of new_c inside game."""
    G = np.asarray(game.convert('RGB')).astype(np.float64) / 255.0
    D = 4
    G2 = G[::D, ::D]
    mask0 = Image.fromarray((content_mask(new_c) * 255).astype(np.uint8))
    best = (-2, None)
    for mirror in (False, True):
        src = ImageOps.mirror(new_c) if mirror else new_c
        msk = ImageOps.mirror(mask0) if mirror else mask0
        for sc in scales:
            w, h = max(8, int(round(src.width * sc / D))), max(8, int(round(src.height * sc / D)))
            t = np.asarray(src.resize((w, h), Image.BILINEAR)).astype(np.float64) / 255.0
            m = np.asarray(msk.resize((w, h), Image.NEAREST)) > 127
            r = masked_ncc(G2, t, m)
            if r is None: continue
            i = np.argmax(r); y, x = divmod(i, r.shape[1])
            if r[y, x] > best[0]: best = (r[y, x], (sc, mirror, x * D, y * D))
    if best[1] is None: return None
    sc0, mirror, x0, y0 = best[1]
    src = ImageOps.mirror(new_c) if mirror else new_c
    msk = ImageOps.mirror(mask0) if mirror else mask0
    refine = (-2, None)
    for sc in np.arange(sc0 - 0.03, sc0 + 0.0301, 0.0025):
        w, h = int(round(src.width * sc)), int(round(src.height * sc))
        t = np.asarray(src.resize((w, h), Image.BILINEAR)).astype(np.float64) / 255.0
        m = np.asarray(msk.resize((w, h), Image.NEAREST)) > 127
        ys, xs = max(0, y0 - 12), max(0, x0 - 12)
        sub = G[ys:ys + h + 24, xs:xs + w + 24]
        r = masked_ncc(sub, t, m)
        if r is None: continue
        i = np.argmax(r); y, x = divmod(i, r.shape[1])
        if r[y, x] > refine[0]: refine = (r[y, x], (sc, mirror, xs + x, ys + y, w, h))
    return refine if refine[1] else None


def game_vs_new(game, new_c):
    r = register(game, new_c)
    if r is None: return None
    score, (sc, mirror, x, y, w, h) = r
    g = game.crop((x, y, x + w, y + h)).resize(new_c.size, Image.BILINEAR)
    if mirror: g = ImageOps.mirror(g)
    mask = content_mask(new_c)
    a = np.asarray(g.convert('RGB')).astype(int); b = np.asarray(new_c.convert('RGB')).astype(int)
    d = np.abs(a - b) * mask[:, :, None]
    n = max(1, mask.sum())
    mean = d.sum() / (n * 3)
    changed = ((d.sum(2) > 24) & mask).sum() * 100.0 / n
    return g, Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)), mean, changed, score, sc, mirror


def label(im, text):
    out = Image.new('RGB', (im.width, im.height + 22), (20, 20, 20))
    out.paste(im, (0, 22))
    ImageDraw.Draw(out).text((6, 4), text, fill=(240, 240, 240))
    return out


def main():
    root = sys.argv[1]
    ed = os.path.join(root, 'editor'); gd = os.path.join(root, 'game'); od = os.path.join(root, 'sheets')
    os.makedirs(od, exist_ok=True)
    tsv = []
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
            game = Image.open(pg).convert('RGB')
            who = 'MBTL' if name.startswith('mbtl_') else 'UNI2'
            gv = game_vs_new(game, new_c)
            if gv is None:
                tiles.insert(0, label(game, 'game (%s), registration failed' % who))
                print('%-28s game: registration failed' % name)
            else:
                g_al, gd_im, gmean, gchanged, score, sc, mirror = gv
                tiles.insert(0, label(g_al, 'game (%s) x%.3f%s ncc %.2f' % (who, sc, ' mirrored' if mirror else '', score)))
                tiles.append(label(gd_im, '|game - new| x4 (sprite px)'))
                print('%-28s game->new mean |d| %.2f, %.2f%% sprite pixels differ (scale %.3f%s, ncc %.2f)'
                      % (name, gmean, gchanged, sc, ' mirrored' if mirror else '', score))
                tsv.append('%s\t%.3f\t%.2f\t%.4f\t%d\t%.3f' % (name, gmean, gchanged, sc, mirror, score))
        W = sum(t.width for t in tiles) + 8 * (len(tiles) - 1); H = max(t.height for t in tiles)
        sheet = Image.new('RGB', (W, H + 24), (32, 32, 32))
        x = 0
        for t in tiles:
            sheet.paste(t, (x, 24)); x += t.width + 8
        ImageDraw.Draw(sheet).text((6, 6), '%s: %s' % (name, note), fill=(255, 220, 120))
        sheet.save(os.path.join(od, name + '.png'))
        print('%-28s old->new mean |d| %.2f, %.2f%% pixels changed' % (name, mean, changed))
    if tsv:
        with open(os.path.join(od, 'game_vs_new.tsv'), 'w') as f:
            f.write('case\tmean_abs_diff\tpct_sprite_px_diff\tscale\tmirrored\tncc\n' + '\n'.join(tsv) + '\n')


if __name__ == '__main__':
    main()
