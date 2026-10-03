#!/usr/bin/env python3
"""QoH'99 .chr cipher (LoadAndDecryptCharacterFile 0x42C3A0 in qoh99_dec.exe).

File = 16-byte name block | 4-byte tag | 24-byte counts | body sections.
Name block: name_stem ^ PRIM (cycling) ; the plaintext stem (upper-cased, e.g. "CORIN") is the stream key.
Every other region (tag, counts, and each of the 7 body sections) is decoded independently, index i
restarting at 0 at the start of each region:
    p[i] = ((c[i] ^ K[i % len(K)] ^ ((~(8*i) + (i>>3)) & 0xFF)) - PRIM[i % N] - 109) & 0xFF
Inverse (exact, it is a bijection per byte):
    c[i] = ((p[i] + PRIM[i % N] + 109) & 0xFF) ^ K[i % len(K)] ^ ((~(8*i) + (i>>3)) & 0xFF)
"""
import struct, sys, os

PRIM_STATIC = bytes.fromhex("bf62a497a0da0dbd91bc97af92afe5aff0")  # g_primary_xor_key image bytes at 0x4A3834 (17 used)
# InitializeEncryptionKeys (0x48CFA0) at startup: reads 256 bytes at file offset 32725477 of the encrypted config
# (g_encrypted_config_path), b0 = first byte; for j<17: key[j] ^= b0; then key[j] ^= 'S' (first byte of "SZUKI MASAMI");
# key[17] = 0.  Solved from the shipped files: b0 = 0x7E (constant for every file).
KEY_B0 = 0x7E
PRIM = bytes(b ^ KEY_B0 ^ 0x53 for b in PRIM_STATIC)
N = len(PRIM)

def _mask(n):
    return bytes(((~(8 * i)) + (i >> 3)) & 0xFF for i in range(n))

def key_from_name_block(block16):
    """Heuristic (the game takes the stem from the file path): smallest period of the decoded 16 bytes."""
    raw = bytes(b ^ PRIM[i % N] for i, b in enumerate(block16[:16]))
    for p in range(1, 17):
        if all(raw[i] == raw[i % p] for i in range(16)) and p < 16:
            return raw[:p]
    return raw

def key_from_path(path):
    stem = os.path.basename(path)
    stem = stem[:stem.rfind(".")] if "." in stem else stem
    return stem.encode("ascii").upper()

def dec_region(buf, key):
    n = len(buf); m = _mask(n)
    return bytes((((buf[i] ^ key[i % len(key)] ^ m[i]) - PRIM[i % N] - 109) & 0xFF) for i in range(n))

def enc_region(buf, key):
    n = len(buf); m = _mask(n)
    return bytes((((buf[i] + PRIM[i % N] + 109) & 0xFF) ^ key[i % len(key)] ^ m[i]) for i in range(n))

def enc_name_block(stem_exact16):
    return bytes(b ^ PRIM[i % N] for i, b in enumerate(stem_exact16))

# body section order on disk (count index in c[0..5], element size)
# counts c0..c5 -> runtime a1[3],a1[1],a1[5],a1[7],a1[9],a1[11]
SECTIONS = [  # name, count idx, elem size, decrypt order
    ("anim",    4, 8),    # 8*c4
    ("table24", 2, 24),   # 24*c2
    ("image",   0, 12),   # 12*c0
    ("palette", None, 1024),
    ("tiles",   1, 260),  # 260*c1
    ("sprite",  3, 64),   # 64*c3
    ("hurt",    5, 24),   # 24*c5
]

def decrypt_chr(data, key=None):
    """Return (plain_bytes_same_layout, key). Plain layout identical to file layout (name block kept raw)."""
    if key is None:
        key = key_from_name_block(data[:16])
    tag = dec_region(data[16:20], key)
    counts = dec_region(data[20:44], key)
    c = struct.unpack("<6I", counts)
    out = bytearray(data[:16]) + tag + counts
    pos = 44
    for name, ci, es in SECTIONS:
        n = es if ci is None else es * c[ci]
        out += dec_region(data[pos:pos + n], key)
        pos += n
    out += data[pos:]  # trailing (should be empty)
    return bytes(out), key

def encrypt_chr(plain, key):
    c = struct.unpack("<6I", plain[20:44])
    out = bytearray(plain[:16]) + enc_region(plain[16:20], key) + enc_region(plain[20:44], key)
    pos = 44
    for name, ci, es in SECTIONS:
        n = es if ci is None else es * c[ci]
        out += enc_region(plain[pos:pos + n], key)
        pos += n
    out += plain[pos:]
    return bytes(out)

if __name__ == "__main__":
    import glob
    root = sys.argv[1] if len(sys.argv) > 1 else "/mnt/c/games/qoh"
    ok = True
    for f in sorted(glob.glob(root + "/*/*.chr")):
        d = open(f, "rb").read()
        kp = key_from_path(f)
        p, k = decrypt_chr(d, kp)
        rt = encrypt_chr(p, k)
        print(os.path.basename(f), len(d), "key", k, "header-heuristic-match", key_from_name_block(d) == kp, "roundtrip", rt == d)
        ok &= rt == d
    print("ALL ROUNDTRIP", ok)
