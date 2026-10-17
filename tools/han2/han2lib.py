"""Shared helpers for RBO/GOF2 HAN2RBO research scripts (PAC walk, container split).
Format spec: docs/formats/frenchbread_rbo_gof.md."""
import struct, glob, os

PAC_KEY = 0xE3DF59AC
RBO_PACS = ['DATA01.PAC', 'DATA02.PAC', 'Update01.PAC', 'Ex1Disc.PAC', 'Ex2Disc.PAC', 'Ex3Disc.pac']

def pac_entries(path):
    with open(path, 'rb') as f:
        magic, n = struct.unpack('<II', f.read(8)); n ^= PAC_KEY
        raw = f.read(68 * n)
    out = []
    for i in range(n):
        e = raw[68 * i:68 * i + 68]
        name = bytes(c ^ ((i * j * 3 + 61) & 0xff) for j, c in enumerate(e[:59])).split(b'\0')[0].decode('cp932', 'replace')
        out.append((name, struct.unpack_from('<I', e, 60)[0], struct.unpack_from('<I', e, 64)[0] ^ PAC_KEY))
    return out

def han2_files(game='rbo', exts=('.DAT', '.DT2')):
    """Yield (archive, name, bytes) for every HAN2RBO entry."""
    if game == 'rbo':
        arcs = ['/mnt/c/games/rbo/DATA/' + p for p in RBO_PACS]
    else:
        arcs = sorted(glob.glob('/mnt/c/games/gof/Data/data0*.dat'))
    for a in arcs:
        with open(a, 'rb') as f:
            for name, off, size in pac_entries(a):
                if not name.upper().endswith(exts): continue
                f.seek(off); hdr = f.read(8)
                if hdr != b'HAN2RBO ': continue
                f.seek(off); yield os.path.basename(a), name, f.read(size)

def split(b):
    """Return dict with header fields, pattern-area sections, frame size."""
    sub = struct.unpack_from('<I', b, 0x18)[0]
    lead, nsec = (11, 8) if sub == 1 else (12, 9)
    pa_off = struct.unpack_from('<I', b, 0x20)[0]
    base = 0x40
    sizes = struct.unpack_from(f'<{nsec}I', b, base + lead * 4)
    hlen = (lead + nsec + 3) * 4
    secs = []; o = base + hlen
    for s in sizes: secs.append(b[o:o + s]); o += s
    fsz = 300 if sub == 1 else 404
    return dict(sub=sub, kind=struct.unpack_from('<I', b, 8)[0], sizes=sizes, secs=secs, fsz=fsz,
                frames=[secs[1][i:i + fsz] for i in range(0, len(secs[1]), fsz)])
