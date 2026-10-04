#!/usr/bin/env python3
"""Build the in-game proof files for CG type 5 (docs/cg/type5_report.md).
  t5proof.py <proof_dir> <char> <ids> [--additive] [--bpp N] [--palswap]
Rewrites <proof_dir>/data/<char>.cg with the given images (ids like 0-11) converted from type 0 to type 5
(index plane + alpha plane, alpha = soft left-to-right ramp), optionally patches <char>.HA6 pattern 0 layers to additive,
and optionally writes a palette variant. Files in the hard-linked proof dir are removed (one explicit path) and written as new files."""
import sys, os, struct, argparse
sys.path.insert(0, os.path.dirname(__file__))
from cgparse import Bank
from cgpatch import patch_image, patch_images
from ha6walk import walk

import tempfile
TMP = os.path.join(tempfile.gettempdir(), '_t5_%d.cg' % os.getpid())
def rd(p): return open(p, 'rb').read()
def wr_new(p, data):
    if os.path.lexists(p): os.remove(p)
    open(p, 'wb').write(data)

def build_t5_blob(bank, m, ramp=True, const=None):
    blob = bank.b[m.blob_off:m.blob_off + m.blob_len]
    out = bytearray(); pos = 0
    y1, y2 = m.bounds[1], m.bounds[3]
    for dx, dy, w, h, sx, sy, page, copy in bank.blocks(m):
        if copy: continue
        idx = blob[pos:pos + w * h]; pos += w * h
        alpha = bytearray(w * h)
        for r in range(h):
            for c in range(w):
                i = idx[r * w + c]
                if i == 0: continue
                if ramp:
                    t = (dy + r - y1) / max(1, (y2 - y1))
                    a = int(255 - 215 * t)             # 255 at the top of the sprite, 40 at the bottom
                else: a = const if const is not None else 255
                alpha[r * w + c] = a
        out += idx + alpha
    assert pos == len(blob), (pos, len(blob))
    return bytes(out)

def parse_ids(s):
    r = []
    if s == 'all0': return None
    if s == 'all0': return None
    for part in s.split(','):
        if '-' in part: a, b = part.split('-'); r += range(int(a), int(b) + 1)
        else: r.append(int(part))
    return r

def patch_ha6_additive_all(buf, mode=2, alpha=255):
    """every layer of every pattern: existing AFAL -> (mode,alpha), else insert one before AFED."""
    b = bytearray(buf); edits = []; have = False; n = 0
    for pid, tag, w, o in walk(buf):
        if tag == 'AFST': have = False
        if tag == 'AFAL': have = True; edits.append(('set', o))
        if tag == 'AFED' and not have: edits.append(('ins', o))
    for kind, o in reversed(edits):
        if kind == 'set': struct.pack_into('<ii', b, o + 4, mode, alpha)
        else: b[o:o] = b'AFAL' + struct.pack('<ii', mode, alpha)
        n += 1
    return bytes(b), n

def patch_ha6_additive_all(buf, mode=2, alpha=255):
    b = bytearray(buf); edits = []; have = False
    for pid, tag, w, o in walk(buf):
        if tag == 'AFST': have = False
        if tag == 'AFAL': have = True; edits.append(('set', o))
        if tag == 'AFED' and not have: edits.append(('ins', o))
    for kind, o in reversed(edits):
        if kind == 'set': struct.pack_into('<ii', b, o + 4, mode, alpha)
        else: b[o:o] = b'AFAL' + struct.pack('<ii', mode, alpha)
    return bytes(b), len(edits)

def patch_ha6_additive(buf, ids, mode=2, alpha=255):
    """insert AFAL (mode,alpha) before AFED of every pattern-0 layer whose AFGP image is in ids (layers have no AFAL in pattern 0)."""
    ins = []; cur = None
    for pid, tag, w, o in walk(buf):
        if pid != 0 and pid > 0: break
        if tag == 'AFGP': cur = w[1] in ids
        if tag == 'AFED' and cur: ins.append(o)
    b = bytearray(buf)
    for o in reversed(ins): b[o:o] = b'AFAL' + struct.pack('<ii', mode, alpha)
    return bytes(b), len(ins)

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('proof'); ap.add_argument('char'); ap.add_argument('ids')
    ap.add_argument('--additive', action='store_true'); ap.add_argument('--bpp', type=int, default=32)
    ap.add_argument('--flat', action='store_true', help='alpha 255 where index != 0 (no ramp)')
    ap.add_argument('--alpha', type=int, default=None, help='constant alpha (overrides the ramp)')
    ap.add_argument('--palswap', action='store_true', help='rotate R->G->B in every palette of <char>.pal')
    a = ap.parse_args()
    d = os.path.join(a.proof, 'data'); ids = parse_ids(a.ids)
    src = rd('/mnt/c/games/mbaacc/data/%s.cg' % a.char)
    if ids is None:
        open(TMP, 'wb').write(src); bk0 = Bank(TMP); ids = [i for i, m in bk0.images.items() if m.type == 0]
    for n in ids:
        bank = Bank_ = None
    open(TMP, 'wb').write(src); bank = Bank(TMP); edits = {}
    if ids is None: ids = [i for i, m in bank.images.items() if m.type == 0]
    for n in ids:
        m = bank.images[n]; assert m.type == 0, (n, m.type)
        edits[n] = (5, a.bpp, build_t5_blob(bank, m, not a.flat and a.alpha is None, a.alpha))
    cur = patch_images(src, edits)
    wr_new(os.path.join(d, a.char + '.cg'), cur)
    print('cg written', len(src), '->', len(cur))
    if a.additive:
        for f in sorted(os.listdir('/mnt/c/games/mbaacc/data')):
            if f.lower().startswith(a.char.lower()) and f.lower().endswith('.ha6') and not f.lower().endswith('_r.ha6'):
                hb, k = patch_ha6_additive_all(rd('/mnt/c/games/mbaacc/data/' + f))
                wr_new(os.path.join(d, f), hb); print('HA6', f, 'layers patched', k)
    if a.palswap:
        p = bytearray(rd('/mnt/c/games/mbaacc/data/%s.pal' % a.char))
        for o in range(4, len(p) - 3, 4):
            bch, g, r, al = p[o:o + 4]
            p[o:o + 4] = bytes((r, bch, g, al))   # B<-R, G<-B, R<-G : hue rotation
        wr_new(os.path.join(d, a.char + '.pal'), bytes(p)); print('pal swapped')
