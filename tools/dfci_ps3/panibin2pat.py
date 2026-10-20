#!/usr/bin/env python3
"""DFCI PS3 PANIBin (.pat) -> arcade PAniDataFile (.pat) converter.

PANIBin layout (big endian, see docs/formats/dfci_ps3.md):
  0x00 'PANIBin\\0'  0x08 u8 1, u8 0, u16 0x1234 (BOM)  0x0C u32 size-0x10
  0x10 8 x (u32 offset, u32 count), offsets relative to 0x50:
       [0] textures   32B  (name)
       [1] cutout id->index i32   [2] cutouts   84B
       [3] partset id->index i32  [4] parts     56B
       [5] partsets  232B         [6] u32[1000] (unknown, 0..shapes-1)
       [7] shapes     64B (16 dwords, = VEST params; no VNST names)
Usage: panibin2pat.py in.pat out.pat [--verify arcade.pat]"""
import struct, sys

def be(fmt, b, o): return struct.unpack_from('>' + fmt, b, o)
def cstr(b): return b.split(b'\0', 1)[0]

def parse(b):
    assert b[:8] == b'PANIBin\0', 'not PANIBin'
    ver, bom, size = be('HHI', b, 8)
    assert bom == 0x1234 and ver == 0x100
    h = be('16I', b, 0x10); T = [(0x50 + h[i], h[i + 1]) for i in range(0, 16, 2)]
    rec = lambda t, sz, i: b[T[t][0] + i * sz:T[t][0] + (i + 1) * sz]
    tex = [cstr(rec(0, 32, i)) for i in range(T[0][1])]
    ppidx = be('%di' % T[1][1], b, T[1][0]); psidx = be('%di' % T[3][1], b, T[3][0])
    cut = {}
    for cid, k in enumerate(ppidx):
        if k < 0: continue
        r = rec(2, 84, k); d = be('13i', r, 32)
        cut[cid] = dict(name=cstr(r[:32]), tp=d[0], te=d[1] & 0xffffffff, uv=d[2:6], ss=d[6:8], cc=d[8:10], tail=d[10:13])
    sets = {}
    for sid, k in enumerate(psidx):
        if k < 0: continue
        r = rec(5, 232, k); slots = be('40i', r, 32); parts = {}
        for pid, pk in enumerate(slots):
            if pk < 0: continue
            p = rec(4, 56, pk)
            parts[pid] = dict(id=be('i', p, 0)[0], pr=be('i', p, 4)[0], xy=be('2i', p, 8), u16=p[16:20], zm=p[20:28],
                              a3=p[28:44], cl=p[44:48], sp=p[48:52], flags=p[52:56])
        sets[sid] = dict(name=cstr(r[:32]), parts=parts, order=r[192:232])
    shapes = [rec(7, 64, i) for i in range(T[7][1])]
    return dict(tex=tex, cut=cut, sets=sets, shapes=shapes, unk6=b[T[6][0]:T[6][0] + 4 * T[6][1]])

L = lambda fmt, *v: struct.pack('<' + fmt, *v)
sw = lambda x: b''.join(x[i:i + 4][::-1] for i in range(0, len(x), 4))   # BE dwords -> LE

def enc_set(sid, s):
    o = [b'P_ST', L('I', sid), b'PANA', bytes([len(s['name'])]), s['name']]
    for pid in sorted(s['parts']):
        p = s['parts'][pid]; o += [b'PRST', L('I', pid)]
        if p['xy'] != (0, 0): o += [b'PRXY', L('2i', *p['xy'])]
        if p['flags'][1]: o += [b'PRAL', p['flags'][1:2]]
        if p['flags'][0]: o += [b'PRRV', p['flags'][0:1]]
        if p['flags'][2]: o += [b'PRFL', p['flags'][2:3]]
        if p['zm'] != b'\x3f\x80\0\0\x3f\x80\0\0': o += [b'PRZM', sw(p['zm'])]
        if p['cl'] != b'\xff' * 4: o += [b'PRCL', p['cl'][::-1]]
        if p['sp'] != b'\0' * 4: o += [b'PRSP', p['sp'][::-1]]
        if p['a3'] != b'\0' * 16: o += [b'PRA3', sw(p['a3'])]
        if p['pr']: o += [b'PRPR', L('i', p['pr'])]
        o += [b'PRID', L('i', p['id']), b'PRED']
    return b''.join(o + [b'P_ED'])

def enc_cut(cid, c):
    o = [b'PPST', L('I', cid)]
    if c['name']: o += [b'PPNA', bytes([len(c['name'])]), c['name']]
    if c['cc'] != (0, 0): o += [b'PPCC', L('2i', *c['cc'])]
    o += [b'PPUV', L('4i', *c['uv']), b'PPSS', L('2i', *c['ss'])]
    if c['tp']: o += [b'PPTP', L('i', c['tp'])]
    o += [b'PPTE', L('I', c['te'])]
    if c['tail'][1:3] != (0, 0): o += [b'PPJP', L('2i', *c['tail'][1:3])]
    if c['tail'][0]: o += [b'PPPP', L('i', c['tail'][0])]
    return b''.join(o + [b'PPED'])

def enc_tex(i, n):
    return b'PGST' + L('I', i) + b'PGNM' + n.ljust(32, b'\0') + b'PGED'

def enc_ve(sh):
    return b'VEST' + L('II', len(sh), 16) + b''.join(sw(s) for s in sh) + b'VNST' + b'\0' * 32 * len(sh) + b'VEED'

def records(m):
    r = [('P_ST %d' % k, enc_set(k, v)) for k, v in m['sets'].items()]
    r += [('PPST %d' % k, enc_cut(k, v)) for k, v in m['cut'].items()]
    if m['shapes']: r.append(('VEST', enc_ve(m['shapes'])))
    r += [('PGST %d' % i, enc_tex(i, n)) for i, n in enumerate(m['tex']) if n]
    return r

def convert(b):
    hdr = b'PAniDataFile'.ljust(0x20, b'\0') + b'_STR'
    return hdr + b''.join(x for _, x in records(parse(b))) + b'_END'

if __name__ == '__main__':
    src = open(sys.argv[1], 'rb').read()
    open(sys.argv[2], 'wb').write(convert(src))
