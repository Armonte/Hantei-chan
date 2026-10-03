#!/usr/bin/env python3
"""PB2K1 (Queen of Heart 2001 ~Party's Breaker~) character .DAT verifier.

usage: pb2k1_verify.py FILE.DAT [FILE.DAT ...]     (stage-1 plain bytes, e.g. from `fbarctool extract 01.dat NAME.DAT outdir`)
Applies the stage-2 ciphers (docs/formats/pb2k1.md section 2), then checks every invariant claimed in the document.
Exit status 0 = all files pass.
"""
import struct, sys, collections

K1 = bytes.fromhex('4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e')            # 'Memoryえらーってことにシテオク'
K2 = bytes.fromhex('4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb')  # 'Meンどう--な事はYAりたくNaいんだけどね'
K3 = bytes.fromhex('686982dcc56e6f834a82c982e581482082b28bea984a836982b1546f82be82c982e5')    # 'hiまﾅnoカにょ？ ご苦労ナこToだにょ'

def xd(b, off, n, key):
    L = len(key)
    for p in range(n):
        b[off + p] ^= (p + key[p % L]) & 0xFF

def decrypt(raw):
    b = bytearray(raw)
    xd(b, 0, 0x41C, K1)
    h5, h6 = struct.unpack_from('<II', b, 0x14)
    xd(b, 0x41C, h5 - 0x41C, K2)
    xd(b, h5, h6, K3)
    xd(b, h5 + h6, 0x4000, K3)
    return b

BOX, AT, IF, EF = 4, 26, 20, 12
FRAME = 96

class Fail(Exception):
    pass

def need(c, msg):
    if not c:
        raise Fail(msg)

def verify(path, stats):
    raw = open(path, 'rb').read()
    b = decrypt(raw)
    h5, h6 = struct.unpack_from('<II', b, 0x14)
    need(b[:8] == bytes.fromhex('8e9e82cd978882bd'), 'magic')
    stats['hdr8'][struct.unpack_from('<I', b, 8)[0]] += 1
    stats['hdrC'][struct.unpack_from('<I', b, 0xC)[0]] += 1
    need(struct.unpack_from('<I', b, 0x10)[0] == 15, 'check value 15')
    need(len(b) == h5 + h6 + 0x4000, 'file size == h5+h6+0x4000')
    offs = struct.unpack_from('<256I', b, 0x1C)
    present = [(i, o) for i, o in enumerate(offs) if o != 0xFFFFFFFF]
    need(present[0][1] == 0x41C, 'first pattern at 0x41C')
    need(all(present[k][1] < present[k + 1][1] for k in range(len(present) - 1)), 'offsets ascending')
    ends = [o for _, o in present[1:]] + [h5]
    nfr = nbox = nat = nif = nef = 0
    for (i, o), end in zip(present, ends):
        fc = b[o]
        need(1 <= fc <= 100, f'pat {i} frameCount')
        bo, ao, io, eo = struct.unpack_from('<4i', b, o + 4)
        tabs = [(bo, BOX, 'box'), (ao, AT, 'at'), (io, IF, 'if'), (eo, EF, 'ef')]
        cur = 20 + FRAME * fc
        counts = {}
        # tables in order box, AT, IF, EF; absent = -1
        for k, (t, rs, nm) in enumerate(tabs):
            if t == -1:
                counts[nm] = 0
                continue
            need(t == cur, f'pat {i} {nm} table at {t} expected {cur}')
            nxt = None
            for t2, _, _ in tabs[k + 1:]:
                if t2 != -1:
                    nxt = t2
                    break
            e = nxt if nxt is not None else end - o
            need((e - t) % rs == 0 and e > t, f'pat {i} {nm} size')
            counts[nm] = (e - t) // rs
            cur = e
        need(o + cur == end, f'pat {i} tables tile the pattern ({o + cur} != {end})')
        nbox += counts['box']; nat += counts['at']; nif += counts['if']; nef += counts['ef']
        used = {'at': set(), 'if': set(), 'ef': set(), 'box': set()}
        for fr in range(fc):
            f = o + 20 + FRAME * fr
            F = b[f:f + FRAME]
            need(all(x == 0 for x in F[18:24]), f'pat {i} fr {fr} anim 18..23')
            need(all(x == 0 for x in F[41:44]) and all(x == 0 for x in F[45:47]) and all(x == 0 for x in F[49:51]), f'pat {i} fr {fr} state zero bytes')
            need(all(x == 0 for x in F[54:60]), f'pat {i} fr {fr} state 30..35')
            need(F[7] in (0, 1, 8), f'pat {i} drawMode')
            need(F[10] <= 5, f'pat {i} aniFlag')
            at = F[60]
            if at != 0xFF:
                need(at < counts['at'], f'pat {i} fr {fr} atIndex {at} >= {counts["at"]}')
                used['at'].add(at)
            for k in range(3):
                v = struct.unpack_from('<h', F, 62 + 2 * k)[0]
                if v != -1:
                    need(0 <= v < counts['if'], f'pat {i} ifIndex {v}')
                    used['if'].add(v)
            for k in range(4):
                v = struct.unpack_from('<h', F, 68 + 2 * k)[0]
                if v != -1:
                    need(0 <= v < counts['ef'], f'pat {i} efIndex {v}')
                    used['ef'].add(v)
            for k in range(10):
                v = struct.unpack_from('<h', F, 76 + 2 * k)[0]
                if v != -1:
                    need(0 <= v < counts['box'], f'pat {i} fr {fr} boxIndex[{k}] {v}')
                    used['box'].add(v)
            nfr += 1
            if F[10] in (0, 2, 4, 5):
                pass
        for nm in used:
            if used[nm]:
                need(max(used[nm]) < counts[nm], f'pat {i} {nm} range')
        # IF/EF records
        for r in range(counts['if']):
            t = o + io + IF * r
            need(b[t + 1:t + 4] == b'\0\0\0', f'pat {i} IF pad')
            need(b[t] in list(range(1, 29)) + [50], f'pat {i} IF type {b[t]}')
            stats['iftype'][b[t]] += 1
        for r in range(counts['ef']):
            t = o + eo + EF * r
            stats['eftype'][b[t]] += 1
    # sprite bank
    s = h5
    need(struct.unpack_from('<I', b, s + 16)[0] != 255, 'sprite bank is the 8-bit palette path')
    go = struct.unpack_from('<1000I', b, s + 20)
    groups = [(k, g) for k, g in enumerate(go) if g != 0xFFFFFFFF]
    need(all(g + 52 <= h6 for _, g in groups), 'group offsets inside bank')
    nspr = 0
    first_seen = {}
    for k, g in groups:
        first, cnt = struct.unpack_from('<IH', b, s + g + 44)
        nspr += cnt
        pix = 0
        for j in range(first, first + cnt):
            w, h, x, y, tex = struct.unpack_from('<5h', b, s + 12224 + 14 * j)
            need(w > 0 and h > 0 and x >= 0 and y >= 0 and x + w <= 256 and y + h <= 256, f'sprite {j} rect')
            pix += w * h
            need(j not in first_seen, f'sprite {j} claimed twice')
            first_seen[j] = k
        need(g + 52 + pix <= h6, f'group {k} pixel data inside bank')
        stats['pixbytes'] += pix
    # names tail
    tail = b[h5 + h6:]
    names = [bytes(tail[64 * n:64 * n + 64]).split(b'\0')[0] for n in range(256)]
    nnames = sum(1 for n in names if n)
    stats['files'] += 1
    return dict(patterns=len(present), frames=nfr, boxes=nbox, at=nat, if_=nif, ef=nef, groups=len(groups), sprites=nspr, names=nnames, h5=h5, h6=h6)

def main():
    stats = collections.Counter()
    stats['iftype'] = collections.Counter(); stats['eftype'] = collections.Counter(); stats['hdr8'] = collections.Counter(); stats['hdrC'] = collections.Counter()
    bad = 0
    for p in sys.argv[1:]:
        try:
            r = verify(p, stats)
            print('OK  ', p.split('/')[-2:], r)
        except Fail as e:
            bad += 1
            print('FAIL', p, e)
    print('IF types:', sorted(stats['iftype'].items()))
    print('EF types:', sorted(stats['eftype'].items()))
    print('hdr+8 values:', {hex(k): v for k, v in stats['hdr8'].items()}, 'hdr+0xC values:', {hex(k): v for k, v in stats['hdrC'].items()})
    print('files ok:', stats['files'], 'failed:', bad)
    sys.exit(1 if bad else 0)

main()
