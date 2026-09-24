#!/usr/bin/env python3
"""PAniDataFile (.pat) walker following the UNI2/MBTL loader (PatFile_Load and
its P_ST/PPST/PGST/VEST parsers in uni2.exe 0x4A6350). Splits the file into
top-level records so two files can be compared record by record.
Usage: patwalk.py A.pat [B.pat]"""
import struct, sys

def u32(b, o): return struct.unpack_from('<I', b, o)[0]

SUB = {  # fixed payload sizes in bytes
    'PRXY': 8, 'PRAL': 1, 'PRRV': 1, 'PRFL': 1, 'PRZM': 8, 'PRSP': 4, 'PRAN': 4, 'PRA3': 16, 'PRAS': 8,
    'PRPR': 4, 'PRID': 4, 'PRCL': 4, 'PRPA': 4,
    'PPNM': 32, 'PPXY': 8, 'PPCC': 8, 'PPUV': 16, 'PPWH': 8, 'PPSS': 8, 'PPTE': 4, 'PPCL': 4, 'PPPA': 4,
    'PPGR': 4, 'PPTP': 4, 'PPVT': 4, 'PPPP': 4, 'PPTX': 4, 'PPJP': 8,
    'PGNM': 32, 'PGTP': 4, 'PGTE': 4,
}

def records(b):
    assert b[:12] == b'PAniDataFile', 'not a pat'
    header = b[:0x20]
    p = 0x20
    assert b[p:p+4] == b'_STR'; p += 4
    ver = None
    out = [('HDR', b[:p])]
    while p < len(b):
        t = b[p:p+4].decode('latin1'); s = p; p += 4
        if t == '_END': out.append(('_END', b[s:p])); break
        if t == 'P_ST':
            rid = u32(b, p); p += 4
            while True:
                st = b[p:p+4].decode('latin1'); p += 4
                if st == 'P_ED': break
                if st == 'PANM': p += 32
                elif st == 'PANA': n = b[p]; p += 1 + n
                elif st == 'PRST':
                    p += 4
                    while True:
                        pt = b[p:p+4].decode('latin1'); p += 4
                        if pt == 'PRED': break
                        if pt not in SUB: raise ValueError('PR tag %r at 0x%x' % (pt, p-4))
                        p += SUB[pt]
                else: raise ValueError('P_ tag %r at 0x%x' % (st, p-4))
            out.append(('P_ST %d' % rid, b[s:p]))
        elif t == 'PPST':
            rid = u32(b, p); p += 4
            while True:
                pt = b[p:p+4].decode('latin1'); p += 4
                if pt == 'PPED': break
                if pt == 'PPNA': n = b[p]; p += 1 + n; continue
                if pt not in SUB: raise ValueError('PP tag %r at 0x%x' % (pt, p-4))
                p += SUB[pt]
            out.append(('PPST %d' % rid, b[s:p]))
        elif t == 'PGST':
            rid = u32(b, p); p += 4
            while True:
                pt = b[p:p+4].decode('latin1'); p += 4
                if pt == 'PGED': break
                if pt == 'PGT2':
                    size, w, h = struct.unpack_from('<3I', b, p); q = p + 12
                    typ = b[q:q+4]; q += 4
                    unc = {b'DXT5': w*h + 128, b'DXT1': w*h//2 + 128}.get(typ, w*h*4 + 128)
                    if size == unc: p = q + 8 + size  # 2 dwords + raw
                    else:
                        csize = u32(b, q + 16); p = q + 24 + csize
                elif pt == 'PGTX':
                    w, h, bpp = struct.unpack_from('<3I', b, p); p += 12 + w*h*4
                elif pt in SUB: p += SUB[pt]
                else: raise ValueError('PG tag %r at 0x%x' % (pt, p-4))
            out.append(('PGST %d' % rid, b[s:p]))
        elif t == 'VEST':
            n, ln = struct.unpack_from('<II', b, p); p += 8 + n*ln*4
            while True:
                vt = b[p:p+4].decode('latin1'); p += 4
                if vt == 'VEED': break
                if vt == 'VNST': p += 32*n
                else: raise ValueError('VE tag %r' % vt)
            out.append(('VEST', b[s:p]))
        else:
            raise ValueError('top tag %r at 0x%x' % (t, s))
    return out

if __name__ == '__main__':
    a = records(open(sys.argv[1], 'rb').read())
    if len(sys.argv) < 3:
        import collections
        print(collections.Counter(k.split()[0] for k, _ in a)); sys.exit(0)
    b = records(open(sys.argv[2], 'rb').read())
    da = dict(a); db = dict(b)
    keys = [k for k, _ in a] + [k for k, _ in b if k not in da]
    diff = [k for k in keys if da.get(k) != db.get(k)]
    import collections
    print(len(a), len(b), 'records;', len(diff), 'differ;', collections.Counter(k.split()[0] for k in diff))
    for k in diff[:int(sys.argv[3]) if len(sys.argv) > 3 else 3]:
        x, y = da.get(k, b''), db.get(k, b'')
        i = next((i for i in range(min(len(x), len(y))) if x[i] != y[i]), min(len(x), len(y)))
        print(' ', k, 'len', len(x), len(y), 'first diff +0x%x' % i, x[max(0,i-8):i+24].hex(), '|', y[max(0,i-8):i+24].hex())
