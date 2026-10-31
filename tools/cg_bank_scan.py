# tools/cg_bank_scan.py <bank.cg> [id,id,...] : list BMP Cutter3 images whose header looks odd (inverted / huge bounds, blocks
# outside the bounds, alignment ranges past the table) plus any ids given. Read-only; used for issue #87 (BOF blank slots).
import struct, sys
d = open(sys.argv[1], 'rb').read()
H = struct.unpack_from('<12I', d, 0x2014)
print('magic', d[:12], 'H', H, 'size', len(d))
kIdx = 0x2044; kMax = 3000; kTail = kIdx + 4*kMax
ao, mid, sz = struct.unpack_from('<3I', d, kTail)
print('alignOff', hex(ao), 'mid', hex(mid), 'sizefield', sz)
nimg, nal = H[3], H[2]
offs = struct.unpack_from('<%dI' % kMax, d, kIdx)
bad = 0
for i, o in enumerate(offs):
    if o == 0 or o == 0xFFFFFFFF or o >= len(d): continue
    name = d[o:o+32].split(b'\0')[0]
    ty, w, h, bpp, x1, y1, x2, y2, as_, al = struct.unpack_from('<8iII', d, o+32)
    bw, bh = x2 - x1 + 1, y2 - y1 + 1
    flag = []
    if ty != -1 and (bw <= 0 or bh <= 0): flag.append('nonpos-bounds')
    if bw > 4096 or bh > 4096: flag.append('huge')
    if x1 < 0 or y1 < 0: flag.append('neg-origin')
    if x2 >= w or y2 >= h: flag.append('bounds>canvas')
    blks = []
    for j in range(al & 0xFFFF):
        if as_ + j >= nal: flag.append('al-oob'); break
        bx, by, bw_, bh_, sx, sy, pg, cp = struct.unpack_from('<4i4h', d, ao + 24*(as_+j))
        blks.append((bx, by, bw_, bh_, sx, sy, pg, cp))
        if ty != -1 and (bx < x1 or by < y1 or bx + bw_ - 1 > x2 or by + bh_ - 1 > y2): flag.append('blk-outside-bounds')
    if flag or (len(sys.argv) > 2 and str(i) in sys.argv[2].split(',')):
        bad += 1
        print(i, name, 'ty', ty, 'wh', w, h, 'bpp', bpp, 'bounds', (x1, y1, x2, y2), 'al', as_, al, sorted(set(flag)), blks[:4])
print('flagged', bad)
