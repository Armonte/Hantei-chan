#!/usr/bin/env python3
"""PB2K1 (Queen of Heart 2001 ~Party's Breaker~) character .DAT verifier.

usage: pb2k1_verify.py FILE [FILE ...]     (stage-1 plain bytes, e.g. from `fbarctool extract 01.dat NAME.DAT outdir`)
FILE is classified by name: *.DAT (character), CHARSEL.CT, *_C.CT (command table), *.WMT (win quotes), *COM.TXT / *.CPF (CPU script).
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
    # boxes (4 unsigned bytes) and AT record sanity, 17 patterns with the double-resolution bit
    for (i, o), end in zip(present, ends):
        bo = struct.unpack_from('<i', b, o + 4)[0]
        ao = struct.unpack_from('<i', b, o + 8)[0]
        io = struct.unpack_from('<i', b, o + 12)[0]
        eo = struct.unpack_from('<i', b, o + 16)[0]
        if b[o + 1] & 0x80:
            stats['dblres'] += 1
        if bo != -1:
            nb = ((ao if ao != -1 else io if io != -1 else eo if eo != -1 else end - o) - bo) // 4
            for k in range(nb):
                x1, y1, x2, y2 = b[o + bo + 4 * k:o + bo + 4 * k + 4]
                stats['boxes_total'] += 1
                if x1 <= x2 and y1 <= y2:
                    stats['boxes_ordered'] += 1
        if eo != -1:
            nef2 = (end - o - eo) // 12
            for k in range(nef2):
                if b[o + eo + 12 * k] == 6:
                    stats['ef6'][b[o + eo + 12 * k + 1]] += 1
    # sprite bank
    s = h5
    need(struct.unpack_from('<I', b, s + 12212)[0] == sum(struct.unpack_from('<IH', b, s + g + 44)[1] for g in struct.unpack_from('<1000I', b, s + 20) if g != 0xFFFFFFFF), 'bank spriteCount == sum of group counts')
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
            ox, oy, w, h, x, y, tex = struct.unpack_from('<7h', b, s + 12220 + 14 * j)
            need(w > 0 and h > 0 and x >= 0 and y >= 0 and x + w <= 256 and y + h <= 256, f'sprite {j} rect')
            need(0 <= ox <= 1024 and 0 <= oy <= 1024, f'sprite {j} offset')
            pix += w * h
            need(j not in first_seen, f'sprite {j} claimed twice')
            first_seen[j] = k
        need(g + 52 + pix <= h6, f'group {k} pixel data inside bank')
        stats['pixbytes'] += pix
    # names tail
    tail = b[h5 + h6:]
    names = [bytes(tail[64 * n:64 * n + 64]).split(b'\0')[0] for n in range(256)]
    nnames = sum(1 for n in names if n)
    first_group = min(g for g in go if g != 0xFFFFFFFF)
    need(struct.unpack_from('<I', b, s + 12216)[0] == h6 - first_group, 'groupRegionSize == bank size - first group offset')
    need(12220 + 14 * (max(first_seen) + 1) == first_group, 'sprite table (14-byte records at +12220) ends exactly at the first group')
    need(all(0 <= struct.unpack_from('<h', b, s + 12220 + 14 * j + 12)[0] < 50 for j in first_seen), 'texture page < 50')
    # every frame sprite id is a present group (frame.anim.spriteId indexes groupOffset[])
    for (i, o), end in zip(present, ends):
        for fr in range(b[o]):
            sid = struct.unpack_from('<H', b, o + 20 + 96 * fr)[0]
            need(sid < 1000 and go[sid] != 0xFFFFFFFF, 'pat %d frame %d spriteId %d is not a present group' % (i, fr, sid))
            stats['sprite_refs'] += 1
    need(sorted(first_seen) == list(range(len(first_seen))), 'group sprite ranges tile the sprite table')
    stats['files'] += 1
    return dict(patterns=len(present), frames=nfr, boxes=nbox, at=nat, if_=nif, ef=nef, groups=len(groups), sprites=nspr, names=nnames, h5=h5, h6=h6)

def verify_ct(path, stats):
    b = open(path, 'rb').read()
    need(len(b) == 4232, 'CT size 4232')
    n = struct.unpack_from('<I', b, 0)[0]
    defined = [i for i in range(100) if b[4 + 42 * i] != 0xFF]
    if len(defined) != n:
        stats['ct_count_mismatch'] += 1       # AZUSA: 21 vs 20 defined; the count only bounds the round-start clear loop
    for i in defined:
        r = b[4 + 42 * i:4 + 42 * (i + 1)]
        need(r[0] == i, 'commandId == index')
        need(r[1] == 0, 'record +1 zero')
        seq = r[2:34]
        e = seq.find(b'\xff')
        need(e >= 0, 'sequence terminated by 0xFF within the 32 bytes')
        if e > 16 or not all(c <= 9 or 0x41 <= c <= 0x46 or c in (0x2B, 0x54) for c in seq[:e]):
            stats['ct_odd_seq'] += 1              # CHISA record 0 holds editor text, REIKO94 record 92 a CP932 space token
        need(r[35] <= 2, 'moveClass')
        need(r[37] == 0, 'requestParam zero')
        stats['cmd'] += 1
    for i in range(100):
        if i not in defined:
            need(all(x == 0 for x in b[4 + 42 * i + 1:4 + 42 * (i + 1)]), 'undefined record is 0xFF + zeros')
    h = b[4204:]
    need(h[3] == 0 and h[4] == 0 and h[6:8] == b'\0\0', 'CT header zero bytes')
    need(h[1] == 9 and h[2] == 1 and h[5] == 0x39, 'CT header constants')
    k, d = struct.unpack_from('<f', h, 8)[0], struct.unpack_from('<4f', h, 12)
    need(0.5 < k < 2.0 and all(abs(x - 0.9) < 1e-5 for x in d), 'CT floats')
    return dict(commands=n)

def verify_wmt(path, stats):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<I', b, 0)[0]
    need(len(b) == 4 + 154 * n and n <= 100, 'WMT size == 4 + 154 * count')
    for i in range(n):
        r = b[4 + 154 * i:4 + 154 * (i + 1)]
        need(r[1] == 0, 'WMT +1 zero')
        t = r[4:]
        e = t.find(b'\0')
        need(e >= 0 and all(c == 0 for c in t[e:]), 'WMT text NUL padded')
        t[:e].decode('cp932')
    return dict(quotes=n)

def verify_cpu(path, stats):
    b = open(path, 'rb').read()
    need(len(b) in (56048, 59048), 'CPU script size')
    if len(b) == 59048:
        names = [b[56048 + 60 * i:56048 + 60 * (i + 1)].split(b'\0')[0].decode('cp932') for i in range(50)]
        stats['cpu_names'] += sum(1 for n in names if n)
    for r in range(24):
        sc = struct.unpack_from('<20i', b, 208 + 160 * r)
        w = struct.unpack_from('<20i', b, 208 + 160 * r + 80)
        need(all(0 <= x < 50 for x in sc) and all(0 <= x <= 100 for x in w), 'row %d ids/weights' % r)
    for sidx in range(50):
        for k in range(20):
            st = b[4048 + 1040 * sidx + 52 * k:4048 + 1040 * sidx + 52 * (k + 1)]
            need(st[0] <= 15 and st[1] == 0 and st[3] == 0, 'step code/pad')
            need(all(x == 0 for x in st[10:42]) and all(x == 0 for x in st[44:]), 'step unread bytes zero (+9 is a 0/1 editor flag)')
            need(st[9] in (0, 1), 'step +9 flag')
            need(st[8] in (0, 1) and st[42] & ~7 == 0, 'step end flag / flags')
    return dict(size=len(b))

def verify_charsel(path, stats):
    b = bytearray(open(path, 'rb').read())
    n = struct.unpack_from('<I', b, 0)[0]
    need(len(b) == 4 + 168 * n, 'CHARSEL.CT size')
    K = bytes.fromhex('837483408343838b82aa8ca982c282a982e882dc82b982f1')
    xd(b, 4, 168 * n, K)
    ids = []
    for i in range(n):
        e = b[4 + 168 * i:4 + 168 * (i + 1)]
        nm = [e[32 * k:32 * k + 32].split(b'\0')[0].decode('cp932') for k in range(4)]
        need(nm[1].endswith('.dat') and nm[2].endswith('_c.txt') and nm[3].endswith('com.txt'), 'entry names')
        ids.append(struct.unpack_from('<I', e, 132)[0])
    need(len(set(ids)) == n, 'unique charFileId')
    return dict(entries=n, ids=ids)

def main():
    stats = collections.Counter()
    stats['iftype'] = collections.Counter(); stats['eftype'] = collections.Counter(); stats['hdr8'] = collections.Counter(); stats['hdrC'] = collections.Counter(); stats['ef6'] = collections.Counter()
    bad = 0
    ok = 0
    for p in sys.argv[1:]:
        base = p.replace('\\', '/').split('/')[-1].upper()
        try:
            if base == 'CHARSEL.CT': r = verify_charsel(p, stats)
            elif base.endswith('_C.CT'): r = verify_ct(p, stats)
            elif base.endswith('.WMT'): r = verify_wmt(p, stats)
            elif base.endswith('COM.TXT') or base.endswith('.CPF'): r = verify_cpu(p, stats)
            else: r = verify(p, stats)
            ok += 1
            print('OK  ', p.split('/')[-2:], r)
        except Fail as e:
            bad += 1
            print('FAIL', p, e)
    print('IF types:', sorted(stats['iftype'].items()))
    print('EF types:', sorted(stats['eftype'].items()))
    print('EF type 6 sub-types:', sorted(stats['ef6'].items()))
    print('boxes:', stats['boxes_total'], 'ordered x1<=x2,y1<=y2:', stats['boxes_ordered'], 'double-res patterns:', stats['dblres'], 'commands:', stats['cmd'], 'frame sprite refs:', stats['sprite_refs'], 'cpu script names:', stats['cpu_names'], 'CT count mismatches:', stats['ct_count_mismatch'], 'odd sequences:', stats['ct_odd_seq'])
    print('hdr+8 values:', {hex(k): v for k, v in stats['hdr8'].items()}, 'hdr+0xC values:', {hex(k): v for k, v in stats['hdrC'].items()})
    print('files ok:', ok, '(character .DAT:', stats['files'], ') failed:', bad)
    sys.exit(1 if bad else 0)

main()
