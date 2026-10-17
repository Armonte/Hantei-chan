#!/usr/bin/env python3
"""Layout survey of a BMP Cutter3/2 bank (MBAACC .cg).  usage: cginfo.py bank.cg [-v]"""
import struct, sys, collections
b = open(sys.argv[1], 'rb').read()
u = lambda o: struct.unpack_from('<I', b, o)[0]
si = lambda o: struct.unpack_from('<i', b, o)[0]
d = 0x14 + 0x2000          # header dwords start after 8 palettes
pages, hd1, nalign, nimg, cu = u(d) + 1, u(d + 4), u(d + 8), u(d + 12), u(d + 16)
idx = [u(d + 48 + 4 * i) for i in range(3000)]
ao = u(d + 48 + 12000)
print('hdr', b[:16], 'size', len(b), 'pages', pages, 'hd1', hd1, 'nalign', nalign, 'nimg', nimg, 'cu', cu, 'align@', hex(ao), 'tail', [hex(u(d + 48 + 12000 + 4 * k)) for k in range(1, 3)], 'hdrdw', [u(d + 4 * k) for k in range(12)])
types = collections.Counter(); copy = 0; used = set(); overlaps = 0
cells = {}
tot = 0
for n in range(nimg):
    o = idx[n]
    if o == 0xFFFFFFFF or o + 72 > len(b): types['absent'] += 1; continue
    name = b[o:o + 32].split(b'\0')[0]
    t, w, h, bpp, x1, y1, x2, y2, as_, al = struct.unpack_from('<iIIIiiiiII', b, o + 32)
    types[(t, bpp)] += 1
    if len(sys.argv) > 2: print(n, name, t, w, h, bpp, (x1, y1, x2, y2), as_, al)
    for j in range(al):
        a = struct.unpack_from('<iiiihhhh', b, ao + 24 * (as_ + j))
        if a[7]: copy += 1
        if a[7] == 0:
            for yy in range(a[5] // cu, (a[5] + a[3]) // cu):
                for xx in range(a[4] // cu, (a[4] + a[2]) // cu):
                    k = (a[6], xx, yy)
                    if k in cells: overlaps += 1
                    cells[k] = n
print(types); print('copy_flag cells', copy, 'total owned cell slots', len(cells), 'overlapping', overlaps)
# data order check: image offsets monotonic and contiguous
offs = [(idx[n], n) for n in range(nimg) if idx[n] != 0xFFFFFFFF]
print('monotonic', [o for o, _ in offs] == sorted(o for o, _ in offs), 'first', hex(offs[0][0]), 'last', hex(offs[-1][0]))
# copy-block analysis
cnt = collections.Counter(); ex = []
for n in range(nimg):
    o = idx[n]
    if o == 0xFFFFFFFF: continue
    t, w, h, bpp, x1, y1, x2, y2, as_, al = struct.unpack_from('<iIIIiiiiII', b, o + 32)
    for j in range(al):
        a = struct.unpack_from('<iiiihhhh', b, ao + 24 * (as_ + j))
        if a[7]:
            tot += 1
            owners = set()
            for yy in range(a[5] // cu, (a[5] + a[3]) // cu):
                for xx in range(a[4] // cu, (a[4] + a[2]) // cu):
                    owners.add(cells.get((a[6], xx, yy), -1))
            cnt[(a[7], tuple(sorted(owners)) == (-1,), len(owners) == 1)] += 1
            if len(ex) < 6: ex.append((n, a, sorted(owners)))
print('copy flag stats (flag, allUnowned, singleOwner):', cnt); print(ex)
