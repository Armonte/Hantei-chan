#!/usr/bin/env python3
"""Census of the .pat features whose rendering S1 fixed (PRAL 2/3, PRRV flips, PRSP alpha, PRAS, shape type 2,
PPPA palette slots) plus the HA6 frames that put them on screen (AFGX PAT layers, AFHK interpolation, PUPS).
Used to pick the game-vs-editor comparison cases in docs/audit_2026_10_14/evidence/compare/.

Usage: pat_render_census.py <gamedata/uni2 root> [--cases N]
Prints counts, then for each feature up to N (pattern, frame, layer, part set) references in the character
HA6 files that draw it."""
import collections, os, struct, sys
sys.path.insert(0, os.path.dirname(__file__))
from patwalk import SUB
from ha6walk import walk


def u32(b, o): return struct.unpack_from('<I', b, o)[0]
def i32(b, o): return struct.unpack_from('<i', b, o)[0]


def parse_pat(b):
    """-> (partsets {id: [prop dict]}, cutouts {id: dict}, shapes [type,...])"""
    sets, cuts, shapes = {}, {}, []
    p = 0x24
    while p < len(b):
        t = b[p:p+4].decode('latin1'); p += 4
        if t == '_END': break
        if t == 'P_ST':
            sid = u32(b, p); p += 4; props = []
            while True:
                st = b[p:p+4].decode('latin1'); p += 4
                if st == 'P_ED': break
                if st == 'PANM': p += 32
                elif st == 'PANA': p += 1 + b[p]
                elif st == 'PRST':
                    pr = {'slot': u32(b, p)}; p += 4
                    while True:
                        pt = b[p:p+4].decode('latin1'); p += 4
                        if pt == 'PRED': break
                        if pt in ('PRAL', 'PRRV', 'PRFL'): pr[pt] = b[p]
                        elif pt in ('PRSP', 'PRCL'): pr[pt] = b[p:p+4]
                        elif pt in ('PRAS', 'PRXY'): pr[pt] = (i32(b, p), i32(b, p+4))
                        elif pt in ('PRID', 'PRPR', 'PRPA'): pr[pt] = i32(b, p)
                        p += SUB[pt]
                    props.append(pr)
            sets[sid] = props
        elif t == 'PPST':
            cid = u32(b, p); p += 4; c = {}
            while True:
                pt = b[p:p+4].decode('latin1'); p += 4
                if pt == 'PPED': break
                if pt == 'PPNA': p += 1 + b[p]; continue
                if pt in ('PPPA', 'PPCL', 'PPPP', 'PPVT', 'PPTP', 'PPGR'): c[pt] = i32(b, p)
                p += SUB[pt]
            cuts[cid] = c
        elif t == 'PGST':
            p += 4
            while True:
                pt = b[p:p+4].decode('latin1'); p += 4
                if pt == 'PGED': break
                if pt == 'PGT2':
                    size, w, h = struct.unpack_from('<3I', b, p); q = p + 12
                    typ = b[q:q+4]; q += 4
                    unc = {b'DXT5': w*h + 128, b'DXT1': w*h//2 + 128}.get(typ, w*h*4 + 128)
                    p = q + 8 + size if size == unc else q + 24 + u32(b, q + 16)
                elif pt == 'PGTX':
                    w, h, bpp = struct.unpack_from('<3I', b, p); p += 12 + w*h*4
                else: p += SUB[pt]
        elif t == 'VEST':
            n, ln = struct.unpack_from('<II', b, p); p += 8
            for k in range(n): shapes.append(struct.unpack_from('<%di' % ln, b, p + 4*ln*k))
            p += 4*ln*n
            while True:
                vt = b[p:p+4].decode('latin1'); p += 4
                if vt == 'VEED': break
                if vt == 'VNST': p += 32*n
        else:
            raise ValueError('top tag %r' % t)
    return sets, cuts, shapes


def features(prop, cuts, shapes):
    f = set()
    if 'PRCL' in prop and prop['PRCL'][3] == 0 and not (prop.get('PRSP') and prop['PRSP'][3]):
        return f  # invisible (the game skips parts with diffuse alpha 0)
    if prop.get('PRAL') == 2: f.add('PRAL2')
    if prop.get('PRAL') == 3: f.add('PRAL3')
    rv = prop.get('PRRV', 0)
    if rv: f.add('PRRV%d' % rv)
    sp = prop.get('PRSP')
    if sp and sp[3]: f.add('PRSPalpha')
    if prop.get('PRAS', (0, 0)) != (0, 0): f.add('PRAS')
    c = cuts.get(prop.get('PRID', -1), {})
    if c.get('PPPA', c.get('PPCL', 0)) > 0: f.add('PPPA')
    si = c.get('PPPP', c.get('PPVT', -1))
    if 0 <= si < len(shapes) and shapes[si][0] == 2: f.add('SHAPE2')
    return f


def ha6_frames(buf):
    """-> list of (pattern, frame, pups, afhk, [(layer, usePat, sprite)])"""
    out = []; pid = -1; pups = 0; fr = -1; cur = None
    for p, tag, w, o in walk(buf):
        if tag == 'PSTR': pid = p; pups = 0; fr = -1
        elif tag == 'PUPS': pups = w[0]
        elif tag == 'FSTR': fr += 1; cur = [pid, fr, pups, 0, []]; out.append(cur)
        elif tag == 'AFGX' and cur: cur[4].append((w[0], w[1], w[2]))
        elif tag == 'AFGP' and cur: cur[4].append((0, w[0], w[1]))
        elif tag == 'AFHK' and cur: cur[3] = w[0]
    return out


def main():
    root = sys.argv[1]
    ncases = int(sys.argv[sys.argv.index('--cases') + 1]) if '--cases' in sys.argv else 6
    totals = collections.Counter(); files = 0
    cases = collections.defaultdict(list)
    for dp, _, fs in sorted(os.walk(root)):
        for fn in sorted(fs):
            if not fn.endswith('.pat'): continue
            path = os.path.join(dp, fn)
            try: sets, cuts, shapes = parse_pat(open(path, 'rb').read())
            except Exception as e: print('skip', path, e); continue
            files += 1
            setfeat = {}
            for sid, props in sets.items():
                fs_ = set()
                for pr in props:
                    f = features(pr, cuts, shapes); fs_ |= f
                    for k in f: totals[k] += 1
                setfeat[sid] = fs_
            ha6 = path[:-4] + '.ha6'
            if not os.path.exists(ha6): continue
            try: frames = ha6_frames(open(ha6, 'rb').read())
            except Exception as e: print('skip', ha6, e); continue
            rel = os.path.relpath(ha6, root)
            for pat, fr, pups, afhk, layers in frames:
                for lid, use, spr in layers:
                    if use != 1 or spr not in setfeat: continue
                    for k in setfeat[spr]:
                        cases[k].append((rel, pat, fr, lid, spr))
                    if afhk: cases['AFHK'].append((rel, pat, fr, lid, spr, 'AFHK=%d' % afhk))
                    if pups and 'PPPA' in setfeat[spr]: cases['PUPS+PPPA'].append((rel, pat, fr, lid, spr, 'PUPS=%d' % pups))
    print('%d pat files; parts per feature:' % files, dict(totals))
    for k in sorted(cases):
        v = cases[k]
        print('\n%s: %d layer-frames' % (k, len(v)))
        seen = set()
        for c in v:
            key = c[:2]
            if key in seen: continue
            seen.add(key); print('  ', c)
            if len(seen) >= ncases: break


if __name__ == '__main__':
    main()
