#!/usr/bin/env python3
"""Rosa Chinensis Four hand (rosa_fh.exe 2002 / omake 2005) data verifier: PAC (v1 plain, v0 name-keyed), IMG v3/v4 (encrypted) + v0..v2 (plain),
FOB (dMp-family VM, Rosa opcode table). Proves decode/encode round trips byte-exact. See docs/formats/rosa.md.
usage: rosa_verify.py [2002_Data_dir 2005_Data_dir]"""
import os, struct, sys

KEY = 0xFA261EFB
# ---------------------------------------------------------------- PAC
def read_pac(path):
    d = open(path, 'rb').read()
    flag, cnt = struct.unpack_from('<II', d, 0); cnt ^= KEY
    ents = []
    for i in range(cnt):
        e = bytearray(d[8 + 64 * i: 72 + 64 * i])
        for j in range(56): e[j] ^= (3 * (j * i - 28)) & 0xFF
        name = bytes(e[:56]).split(b'\0')[0].decode('latin1')
        raw_name = bytes(e[:56])
        size = struct.unpack_from('<I', e, 56)[0] ^ KEY; off = struct.unpack_from('<I', e, 60)[0]
        ents.append([name, off, size, raw_name])
    return flag, d, ents

def _namekey(name, size, buf):
    nm = name.upper().encode('latin1'); L = len(nm)
    for k in range(min(size, 9696)): buf[k] ^= (k + nm[k % L]) & 0xFF   # PackArchive_ReadEntry (same code in DMP.EXE 0x412350 / rosa_fh.exe 0x40B3A0)

def pac_entry(flag, d, ents, i):
    name, off, size, _ = ents[i]; b = bytearray(d[off:off + size])
    if not flag: _namekey(name, size, b)
    return bytes(b)

def write_pac(flag, ents, payloads):
    """re-serialize: header, 64-byte entries (offsets recomputed contiguously), payloads. ents = [(name, raw_name56)]"""
    n = len(ents); off = 8 + 64 * n; hdr = struct.pack('<II', flag, n ^ KEY); body = b''; table = b''
    for i, ((name, raw), p) in enumerate(zip(ents, payloads)):
        e = bytearray(raw)
        for j in range(56): e[j] ^= (3 * (j * i - 28)) & 0xFF
        table += bytes(e) + struct.pack('<II', len(p) ^ KEY, off + len(body))
        q = bytearray(p)
        if not flag: _namekey(name, len(p), q)
        body += bytes(q)
    return hdr + table + body

# ---------------------------------------------------------------- IMG v3/v4 (rosa_fh.exe 0x402B60, DMP.EXE 0x4079F0)
IMG_KEY = bytes.fromhex('8c4b924a2089c297f7')    # rosa_fh.exe 0x432458 "unk_432458", NUL terminated, length 9

def _stem(name):
    return os.path.splitext(os.path.basename(name))[0].upper().encode('latin1')[:16]

def img_stream(buf, stem, decrypt, key=IMG_KEY):
    """sub_41F4D0: each call restarts its index at 0 (one call per header field, one for the palette, one for the pixels)."""
    out = bytearray(len(buf)); L = len(stem); M = len(key)
    for i, c in enumerate(buf):
        m = (((i >> 3) & 0xFF) + ((~(8 * i)) & 0xFF)) & 0xFF
        if decrypt: out[i] = ((stem[i % L] ^ m ^ c) - key[i % M] - 109) & 0xFF
        else:       out[i] = ((c + key[i % M] + 109) & 0xFF) ^ stem[i % L] ^ m
    return bytes(out)

def rowbytes(bpp, w):
    v = {4: ((w + 1) >> 1) + 3, 8: w + 3, 16: 2 * w + 3, 24: 3 * w + 3}.get(bpp)
    return (v & ~3) if v is not None else w         # sub_4029C0 (and al,0FCh)

def img4_seed(stem, key=IMG_KEY):
    plain = (stem * 16)[:16]                        # the 16 header bytes decode to the stem repeated (verified on all 100 files)
    return bytes(plain[j] ^ key[j % len(key)] for j in range(16))

def img4_decode(b, name):
    stem = _stem(name)
    assert struct.unpack_from('<II', b, 0) == (0, 4) or struct.unpack_from('<II', b, 0) == (0, 3)
    ver = struct.unpack_from('<I', b, 4)[0]; seed = bytes(b[8:24])
    dec = bytes(seed[j] ^ IMG_KEY[j % 9] for j in range(16))
    assert dec[:len(stem)] == stem, 'file name does not match the embedded stem (sub_41F540 fails)'
    fields = [struct.unpack('<I', img_stream(b[24 + 4 * k:28 + 4 * k], stem, True))[0] for k in range(5)]
    flag, pc, bpp, w, h = fields
    p = 44; pal = img_stream(b[p:p + 4 * pc], stem, True) if pc else b''; p += 4 * pc
    pix = img_stream(b[p:p + h * rowbytes(bpp, w)], stem, True)
    assert p + len(pix) == len(b)
    return dict(version=ver, flag=flag, bpp=bpp, width=w, height=h, palette=pal, pixels=pix, seed=seed)

def img4_encode(im, name):
    stem = _stem(name)
    seed = im.get('seed') or img4_seed(stem)
    hdr = struct.pack('<II', 0, im['version']) + seed
    for v in (im['flag'], len(im['palette']) // 4, im['bpp'], im['width'], im['height']):
        hdr += img_stream(struct.pack('<I', v), stem, False)
    return hdr + img_stream(im['palette'], stem, False) + img_stream(im['pixels'], stem, False)

# ---------------------------------------------------------------- IMG v0..v2 (plain; the 2005 repack uses version 1)
def img1_decode(b):
    z, ver, flag, pc, bpp, w, h = struct.unpack_from('<7I', b, 0)
    p = 28; pal = b[p:p + 4 * pc]; p += 4 * pc
    return dict(version=ver, flag=flag, bpp=bpp, width=w, height=h, palette=pal, pixels=b[p:], zero=z)

def img1_encode(im):
    return struct.pack('<7I', im['zero'], im['version'], im['flag'], len(im['palette']) // 4, im['bpp'], im['width'], im['height']) + im['palette'] + im['pixels']

def img_to_rgb(im):
    """top-down rows (bottomUp flag is 0 in the game's loader), BGR byte order, 4-byte aligned rows; 8 bpp = BGRX palette"""
    w, h, bpp = im['width'], im['height'], im['bpp']; rb = rowbytes(bpp, w); px = im['pixels']; pal = im['palette']; rows = []
    for r in range(h):
        row = px[r * rb:(r + 1) * rb]
        if bpp == 24: rows.append(b''.join(bytes((row[3*x+2], row[3*x+1], row[3*x])) for x in range(w)))
        else: rows.append(b''.join(bytes((pal[4*row[x]+2], pal[4*row[x]+1], pal[4*row[x]])) for x in range(w)))
    return rows

# ---------------------------------------------------------------- FOB (Rosa dialect of the dMp VM; docs/formats/rosa.md section 3)
FORMS = {}
def _f(f, ops):
    for o in ops: FORMS[o] = f
_f('data', [0x01, 0x02]); _f('imm', [0x03, 0x04, 0x1A, 0x1B, 0x25, 0x2E, 0x50]); _f('flk', [0x09, 0x18])
_f('fl', list(range(0x0A, 0x18)) + [0x26]); _f('jcc', [0x19]); _f('sw', [0x24])
for _o in list(range(0x05, 0x09)) + [0x1C, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23] + list(range(0x27, 0x2E)) + list(range(0x2F, 0x55)) + list(range(0x56, 0x6A)):
    FORMS.setdefault(_o, '')
def u16(b, p): return struct.unpack_from('<H', b, p)[0]
def u32(b, p): return struct.unpack_from('<I', b, p)[0]
def ilen(c, pc):
    f = FORMS.get(u16(c, pc))
    if f is None: return None
    return 6 + 4 * u32(c, pc + 2) if f == 'data' else {'': 2, 'imm': 6, 'fl': 4, 'flk': 6, 'jcc': 10, 'sw': 8}[f]
TERM = {0x1A, 0x1E, 0x1F, 0x20, 0x2C, 0x4A, 0x53, 0x54}
def fob_parse(b):
    nf = u32(b, 0); p = 4; funcs = []
    for i in range(nf): funcs.append((bytes(b[p:p + 32]), u32(b, p + 32))); p += 36
    cs = u32(b, p); p += 4
    return funcs, bytes(b[p:p + cs]), bytes(b[p + cs:])
def fob_serialize(funcs, code, tail=b''):
    return struct.pack('<I', len(funcs)) + b''.join(n + struct.pack('<I', pc) for n, pc in funcs) + struct.pack('<I', len(code)) + code + tail
def fob_descend(c, funcs):
    seen = {}; probs = []; work = [0] + [pc for _, pc in funcs]
    while work:
        pc = work.pop()
        while pc not in seen:
            if pc < 0 or pc + 2 > len(c): probs.append(('oob', pc)); break
            n = ilen(c, pc)
            if n is None or pc + n > len(c): probs.append(('bad opcode %#x' % u16(c, pc), pc)); break
            seen[pc] = n; op = u16(c, pc)
            if op == 0x19: work.append(u32(c, pc + 6))
            elif op in (0x1A, 0x1B): work.append(u32(c, pc + 2))
            elif op == 0x24:
                t = u32(c, pc + 4)
                if t + 8 > len(c): probs.append(('switch table oob', pc)); break
                work.append(u32(c, t))
                for k in range(u32(c, t + 4)):
                    if t + 16 + 8 * k > len(c): probs.append(('switch entry oob', pc)); break
                    work.append(u32(c, t + 8 + 8 * k + 4))
            if op in TERM: break
            pc += n
    return seen, probs

# ---------------------------------------------------------------- driver
def main(argv):
    base02 = argv[0] if argv else '/mnt/c/dev/frenchbread/benibara_rendan/files/Rosa Chinensis Four hand/Data/'
    base05 = argv[1] if len(argv) > 1 else '/mnt/c/dev/frenchbread/yamayuri_rendan/files/omake/Rosa Chinensis Four hand/Data/'
    ok = True
    def rep(label, good, n):
        nonlocal ok; ok &= (good == n); print('%-52s %d / %d %s' % (label, good, n, '' if good == n else 'FAIL'))
    sets = {}
    for tag, base in (('2002', base02), ('2005', base05)):
        fl, d, e = read_pac(base + 'PAC.PAC'); sets[tag] = (fl, d, e)
        pl = [pac_entry(fl, d, e, i) for i in range(len(e))]
        rebuilt = write_pac(fl, [(x[0], x[3]) for x in e], pl)
        rep('%s PAC.PAC re-serialize byte-exact (flag=%d)' % (tag, fl), int(rebuilt == d), 1)
    # sound PAC (japanese file name 'oto')
    for tag, base in (('2002', base02), ('2005', base05)):
        for fn in os.listdir(base):
            if not fn.startswith('PAC'):
                fl, d, e = read_pac(base + fn); pl = [pac_entry(fl, d, e, i) for i in range(len(e))]
                rep('%s sound PAC (%d x MP3) re-serialize' % (tag, len(e)), int(write_pac(fl, [(x[0], x[3]) for x in e], pl) == d), 1)
    fl2, d2, e2 = sets['2002']; fl5, d5, e5 = sets['2005']; n5 = {x[0]: i for i, x in enumerate(e5)}
    n = g = x4 = x5 = same = 0; diff = []
    for i, ent in enumerate(e2):
        if not ent[0].endswith('.IMG'): continue
        n += 1; raw = pac_entry(fl2, d2, e2, i); im = img4_decode(raw, ent[0])
        g += int(img4_encode(im, ent[0]) == raw)                                  # encode(decode(x)) == x, 2002
        raw5 = pac_entry(fl5, d5, e5, n5[ent[0]]); im5 = img1_decode(raw5)
        x5 += int(img1_encode(im5) == raw5)                                       # 2005 plain v1 round trip
        im5s = dict(im5, version=4, seed=None)
        if img4_encode(im5s, ent[0]) == raw: same += 1                            # 2005 -> 2002 conversion identical
        else: diff.append(ent[0])
    rep('2002 IMG v4 encode(decode(x)) == x', g, n)
    rep('2005 IMG v1 serialize(parse(x)) == x', x5, n)
    print('2005 IMG converted to v4 == 2002 file: %d / %d (differs: %s)' % (same, n, ','.join(diff)))
    nf = ok_f = 0
    for tag, (fl, d, e) in sets.items():
        for i, ent in enumerate(e):
            if not ent[0].endswith('.FOB'): continue
            raw = pac_entry(fl, d, e, i); funcs, code, tail = fob_parse(raw); seen, probs = fob_descend(code, funcs)
            nf += 1; ok_f += int(not probs and not tail and fob_serialize(funcs, code, tail) == raw)
    rep('FOB (2002+2005) parse/decode/serialize byte-exact', ok_f, nf)
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
