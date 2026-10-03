#!/usr/bin/env python3
"""DATA01.PAC::DUSTNESS.DAT decode attempts (docs/formats/frenchbread_rbo_gof.md section 7).

Reads the file in place from the PAC (nothing is copied) and tries every cipher family known in the French-Bread formats against it, scoring each result
by: HAN2RBO magic, byte entropy, and agreement with the structure of DUSTINESS.DAT (the sibling file that IS valid HAN2RBO).
Usage: python3 tools/han2/dustness_attempts.py [DATA01.PAC]
"""
import struct, sys, math, collections, zlib, bz2, lzma
PAC = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/games/rbo/DATA/DATA01.PAC'
PK = 0xE3DF59AC

def entries(path):
    with open(path, 'rb') as f:
        _, n = struct.unpack('<II', f.read(8)); n ^= PK
        raw = f.read(68 * n)
    out = {}
    for i in range(n):
        e = raw[68 * i:68 * i + 68]
        nm = bytes(c ^ ((i * j * 3 + 61) & 0xff) for j, c in enumerate(e[:59])).split(b'\0')[0].decode('cp932', 'replace')
        out[nm.upper()] = (struct.unpack_from('<I', e, 60)[0], struct.unpack_from('<I', e, 64)[0] ^ PK)
    return out

def entropy(b):
    if not b: return 0
    c = collections.Counter(b); n = len(b)
    return -sum(v / n * math.log2(v / n) for v in c.values())

E = entries(PAC)
with open(PAC, 'rb') as f:
    def get(n):
        o, s = E[n]; f.seek(o); return f.read(s)
    dn, di = get('DUSTNESS.DAT'), get('DUSTINESS.DAT')
print('DUSTNESS %d bytes entropy %.3f; DUSTINESS %d bytes magic %r' % (len(dn), entropy(dn), len(di), di[:8]))

KEYS = {   # string keys: (i + key[i % len]) & 255 XOR, as Xor_StringKeyedInPlace / Crypto_XorWithKeyString / gof1 stage one
    'rbo pattern key (Me..-- , sub_405AF0 0x48A1A4)': bytes.fromhex('4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb'),
    'gof1 header key (Memory..)': bytes([0x4d,0x65,0x6d,0x6f,0x72,0x79,0x82,0xa6,0x82,0xe7,0x81,0x5b,0x82,0xc1,0x82,0xc4,0x82,0xb1,0x82,0xc6,0x82,0xc9,0x83,0x56,0x83,0x65,0x83,0x49,0x83,0x4e]),
    'gof1 pattern key': bytes([0x4d,0x65,0x83,0x93,0x82,0xc7,0x82,0xa4,0x2d,0x2d,0x82,0xc8,0x8e,0x96,0x82,0xcd,0x59,0x41,0x82,0xe8,0x82,0xbd,0x82,0xad,0x4e,0x61,0x82,0xa2,0x82,0xf1,0x82,0xbe,0x82,0xaf,0x82,0xc7,0x82,0xcb]),
    'gof1 blob key': bytes([0x68,0x69,0x82,0xdc,0xc5,0x6e,0x6f,0x83,0x4a,0x82,0xc9,0x82,0xe5,0x81,0x48,0x20,0x82,0xb2,0x8b,0xea,0x98,0x4a,0x83,0x69,0x82,0xb1,0x54,0x6f,0x82,0xbe,0x82,0xc9,0x82,0xe5]),
    'gof1 charsel key (file not found msg)': bytes.fromhex('837483408343838b82aa8ca982c282a982e882dc82b982f1'),
}
for nm in ('DUSTNESS.DAT', 'DUSTINESS.DAT', 'DUSTNESS', 'DUSTINESS', 'dustness.dat', 'dustiness.dat', 'DUSTNESS.FOB', 'DUSTINESS.FOB'):
    KEYS['name key ' + nm] = nm.encode()
# the Hantei4/HAN2 header XOR (SZUKI): keys found in rbo.exe / rbo_ex*.exe near the section decrypt; searched as printable strings
for nm in ('SZUKI', 'szuki', 'Szuki'):
    KEYS['SZUKI ' + nm] = nm.encode()

def keyed(b, key, off=0):
    return bytes(x ^ ((i + off + key[(i + off) % len(key)]) & 255) for i, x in enumerate(b))
def dword_stream(b, k):
    out = bytearray(b)
    for i in range(0, len(b) - 3, 4): struct.pack_into('<I', out, i, struct.unpack_from('<I', b, i)[0] ^ k)
    return bytes(out)
def lcg_stream(b, seed):        # the replay / save-file stream: x = (1021 - 354542487 x) & 0x7FFFFFFF, two steps per output
    out = bytearray(b); s = seed
    for i in range(0, len(b) - 3, 4):
        a = (1021 - 354542487 * s) & 0x7FFFFFFF; s = (1021 - 354542487 * a) & 0x7FFFFFFF
        struct.pack_into('<I', out, i, struct.unpack_from('<I', b, i)[0] ^ ((s & 0x7FFF0000) | (a >> 15 & 0xFFFF)))
    return bytes(out)

results = []
def score(label, data):
    magic = data[:8] == b'HAN2RBO '
    # structure agreement with the sibling: its container header is u32 kind 0, sub 1/2 at +0x10..; compare the first 0x40 bytes position by position
    agree = sum(1 for a, c in zip(data[:0x40], di[:0x40]) if a == c)
    results.append((magic, agree, entropy(data[:65536]), label))

score('raw', dn)
for k, key in KEYS.items():
    for off in range(0, 4): score('%s (+%d)' % (k, off), keyed(dn, key, off))
for k in (PK, 0xFA261EFB, 0x47D338, 0x1234, 0x7FFFFFFF):
    score('dword xor 0x%X' % k, dword_stream(dn, k))
for seed in (0, 1, PK, 0xFA261EFB, struct.unpack_from('<I', dn, 4)[0], struct.unpack_from('<I', dn, 0)[0]):
    score('lcg stream seed 0x%X' % seed, lcg_stream(dn, seed))
# keystream from known plaintext: if the plaintext starts with the HAN2RBO header, key[i] = dn[i] ^ 'HAN2RBO '[i] - i; is it a substring of any known key?
ks = bytes(((dn[i] ^ b'HAN2RBO '[i]) - i) & 255 for i in range(8))
print('keystream if plaintext = HAN2RBO header (minus i):', ks.hex(), '; plain xor:', bytes(a ^ c for a, c in zip(dn[:8], b'HAN2RBO ')).hex())
for k, key in KEYS.items():
    if ks in key or bytes(a ^ c for a, c in zip(dn[:8], b'HAN2RBO ')) in key: print('  keystream is a substring of', k)
# compression
for name, fn in (('zlib', zlib.decompress), ('zlib raw', lambda x: zlib.decompress(x, -15)), ('bz2', bz2.decompress), ('lzma', lzma.decompress)):
    for off in range(0, 17):
        try: out = fn(dn[off:]); print('  %s decodes at +%d -> %d bytes' % (name, off, len(out))); score('%s +%d' % (name, off), out)
        except Exception: pass
results.sort(key=lambda r: (-r[0], r[2]))
print('best by (magic, low entropy):')
for r in results[:8]: print('  magic=%s agree=%d entropy=%.3f  %s' % r)
print('any HAN2RBO magic:', any(r[0] for r in results))
# periodicity: if DUSTNESS were DUSTINESS-like data under a short repeating key, dn ^ di would repeat; test the best period of dn ^ di
x = bytes(a ^ c for a, c in zip(dn[:4096], di[:4096]))
best = max(range(1, 512), key=lambda p: sum(1 for i in range(p, len(x)) if x[i] == x[i - p]))
print('best self-agreement period of DUSTNESS^DUSTINESS: %d (%.1f%% matching bytes)' % (best, 100.0 * sum(1 for i in range(best, len(x)) if x[i] == x[i - best]) / (len(x) - best)))
