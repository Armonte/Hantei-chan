"""GOF2 PAT v4 part-record statistics (is the 92-byte part identical to RboPatPart?).
Usage: python3 pat_part_scan.py   (scans every .PAT in Data\\data0*.dat, later archive wins)"""
import sys, struct, glob, collections
sys.path.insert(0, '/home/teo/dev/hantei-chan-wt/rbo-support/tools/han2')
from han2lib import pac_entries
pats = {}
for a in sorted(glob.glob('/mnt/c/games/gof/Data/data0*.dat')):
    for n, off, size in pac_entries(a):
        if n.upper().endswith('.PAT'): pats[n.upper()] = (a, off, size)
nz = collections.Counter(); flips = collections.Counter(); tot = 0; hdr = collections.Counter(); imgsz = collections.Counter()
for n, (a, off, size) in sorted(pats.items()):
    with open(a, 'rb') as f: f.seek(off); b = f.read(size)
    hdr[struct.unpack_from('<6I', b, 0)] += 1
    img = struct.unpack_from('<I', b, 72024)[0]
    offs = [o for o in struct.unpack_from('<2000I', b, 24) if o]
    assert all((o - 72028) % 92 == 0 for o in offs), n
    assert all(offs[i + 1] - offs[i] == 3680 for i in range(len(offs) - 1)), n
    assert img - 72028 == len(offs) * 3680 or True
    for p in range(72028, img, 92):
        tot += 1
        v = struct.unpack_from('<23I', b, p)
        flips[v[4]] += 1
        for i in (0x2E // 4,):  # hi words live inside dwords 0x2C,0x30,0x34,0x38 -> check dword range
            pass
        for o in (0x2C, 0x30, 0x34, 0x38):
            if v[o // 4] >> 16: nz['srcdword_hi_%X' % o] += 1
        if v[0x3C // 4] >> 8: nz['layer_hi'] += 1
        for o in range(0x48, 0x5C, 4):
            if v[o // 4]: nz['res_%X' % o] += 1
        if v[0x24 // 4] >> 24: nz['add_byte27'] += 1
print('pat files', len(pats), 'parts', tot)
print('headers', dict(hdr))
print('flip_flags values', dict(flips))
print('nonzero counters', dict(nz))
