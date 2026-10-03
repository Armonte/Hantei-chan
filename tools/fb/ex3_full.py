import sys, struct
sys.path.insert(0,'.')
from ex3_gage import *
def encode(d, hs, bs=2000, hz=4096, mc=256, th=7):
    bl = parse(d, hs); dec = b''.join(expand(t, s) for g, t, s in bl)
    g = Gage(bs, hz, mc, th); pos = 0; out = bytearray(d[:hs])
    while pos < len(dec):
        t, s, pos = g.block(dec, pos)
        out += write_table(t) + struct.pack('>H', len(s)) + s
    return bytes(out)
for f in sys.argv[1:]:
    d = open(f, 'rb').read()
    for hs in (64, 68, 72):
        try:
            bl = parse(d, hs); dec = b''.join(expand(t, s) for g, t, s in bl)
            if struct.unpack('<I', d[hs-4:hs])[0] == len(dec): break
        except Exception: pass
    e = encode(d, hs)
    if e == d: print('IDENTICAL', f)
    else:
        i = next((k for k in range(min(len(e), len(d))) if e[k] != d[k]), min(len(e), len(d)))
        print('DIFF', f, 'at', i, len(e), len(d))
