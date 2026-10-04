#!/usr/bin/env python3
"""Queen of Heart '99 .chr cipher (LoadAndDecryptCharacterFile 0x42C3A0 / ValidateAndDecryptFileHeader 0x42C1F0 in qoh99_dec.exe).

File layout (all regions are enciphered independently):
    16   name block   stem of the file name, upper-case, XOR PRIM (cycling); NOT zero padded: the bytes after the stem repeat the stem
    4    tag          revision stamp
    24   counts       six u32 c0..c5
    body regions in this order: boxes 8*c4, actions 24*c2, images 12*c0, palette 1024, tiles 260*c1, frames 64*c3, attacks 24*c5

Key material
    PRIM       17 bytes. Image bytes of g_primary_xor_key (0x4A3834) are bf62a497a0da0dbd91bc97af92afe5aff0.
               InitializeEncryptionKeys (0x48CFA0) rewrites them at start-up:  key[j] ^= b0; key[j] ^= 'S' for j in 0..16, key[17] = 0, where b0 is
               the first of 256 bytes read at file offset 32725477 of the (encrypted) config file.  b0 = 0x7E for the shipped game
               (solved from the shipped files: it is the only value that makes every name block decode to its own file name).
    stem key   upper-case stem of the file name ("CORIN" for .\\Corin\\Corin.Chr): CharUpperA of the path tail without directory and extension.

Per-region decode (i restarts at 0 for every region, the stem index restarts too):
    p[i] = ((c[i] ^ stem[i % len(stem)] ^ ((~(8*i) + (i >> 3)) & 0xFF)) - PRIM[i % 17] - 109) & 0xFF
    (the loader wraps the PRIM index when g_secondary_xor_key[j] == 0; g_secondary_xor_key = g_primary_xor_key + 1, so the period is 17)
Per-region encode (exact inverse, the map is a bijection per byte):
    c[i] = (((p[i] + PRIM[i % 17] + 109) & 0xFF) ^ stem[i % len(stem)] ^ ((~(8*i) + (i >> 3)) & 0xFF))
The name block is stored as stem XOR PRIM repeated to 16 bytes ("CORINCORINCORINC" before the XOR) and is NOT part of the stream.
"""
import glob
import os
import struct
import sys

PRIM_STATIC = bytes.fromhex("bf62a497a0da0dbd91bc97af92afe5aff0")  # image of g_primary_xor_key (17 bytes used)
KEY_B0 = 0x7E                                                      # first byte read by InitializeEncryptionKeys, see module docstring
PRIM = bytes(b ^ KEY_B0 ^ 0x53 for b in PRIM_STATIC)               # runtime key (924f89ba8df72090bc91ba82bf82c882dd)
N = len(PRIM)
HEADER_SIZE = 44

# body regions in file order: (name, count index into c0..c5 or None for the fixed palette, element size)
SECTIONS = [
    ("boxes", 4, 8),
    ("actions", 2, 24),
    ("images", 0, 12),
    ("palette", None, 1024),
    ("tiles", 1, 260),
    ("frames", 3, 64),
    ("attacks", 5, 24),
]


def _mask(n):
    return bytes(((~(8 * i)) + (i >> 3)) & 0xFF for i in range(n))


def stem_key_from_path(path):
    base = os.path.basename(path.replace("\\", "/"))
    stem = base[:base.rfind(".")] if "." in base else base
    return stem.encode("ascii").upper()


def stem_key_from_name_block(block16):
    """Recover the stem from the name block alone (smallest period of the decoded 16 bytes). Heuristic, the game uses the path."""
    raw = bytes(b ^ PRIM[i % N] for i, b in enumerate(block16[:16]))
    for p in range(1, 16):
        if all(raw[i] == raw[i % p] for i in range(16)):
            return raw[:p]
    return raw


def encode_name_block(stem):
    """Name block bytes for a stem (stem repeated to 16 bytes, XOR PRIM)."""
    rep = (stem * 16)[:16]
    return bytes(b ^ PRIM[i % N] for i, b in enumerate(rep))


def dec_region(buf, key):
    m = _mask(len(buf))
    return bytes((((buf[i] ^ key[i % len(key)] ^ m[i]) - PRIM[i % N] - 109) & 0xFF) for i in range(len(buf)))


def enc_region(buf, key):
    m = _mask(len(buf))
    return bytes((((buf[i] + PRIM[i % N] + 109) & 0xFF) ^ key[i % len(key)] ^ m[i]) for i in range(len(buf)))


def split_regions(plain):
    """(counts, {name: bytes}, trailing) for a decoded file; raises ValueError when the sizes do not add up exactly."""
    c = struct.unpack("<6I", plain[20:44])
    pos = HEADER_SIZE
    secs = {}
    for name, ci, es in SECTIONS:
        n = es if ci is None else es * c[ci]
        if pos + n > len(plain):
            raise ValueError("section %s overruns the file" % name)
        secs[name] = plain[pos:pos + n]
        pos += n
    return c, secs, plain[pos:]


def decrypt_chr(data, key):
    """Decode a whole .chr. Output keeps the file layout (name block stays as stored); region order unchanged."""
    tag = dec_region(data[16:20], key)
    counts = dec_region(data[20:44], key)
    c = struct.unpack("<6I", counts)
    out = bytearray(data[:16]) + tag + counts
    pos = HEADER_SIZE
    for name, ci, es in SECTIONS:
        n = es if ci is None else es * c[ci]
        out += dec_region(data[pos:pos + n], key)
        pos += n
    out += data[pos:]
    return bytes(out)


def encrypt_chr(plain, key):
    c = struct.unpack("<6I", plain[20:44])
    out = bytearray(plain[:16]) + enc_region(plain[16:20], key) + enc_region(plain[20:44], key)
    pos = HEADER_SIZE
    for name, ci, es in SECTIONS:
        n = es if ci is None else es * c[ci]
        out += enc_region(plain[pos:pos + n], key)
        pos += n
    out += plain[pos:]
    return bytes(out)


def find_files(root, ext):
    """Files with extension ext (case-insensitive) one directory below root (the shipped layout: <root>/<character>/<name>.chr)."""
    out = []
    for d in sorted(os.listdir(root)):
        p = os.path.join(root, d)
        if os.path.isdir(p) and not d.startswith("_"):
            for f in sorted(os.listdir(p)):
                if f.lower().endswith(ext):
                    out.append(os.path.join(p, f))
    return out


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "/mnt/c/games/qoh"
    ok = True
    files = find_files(root, ".chr")
    for f in files:
        d = open(f, "rb").read()
        key = stem_key_from_path(f)
        plain = decrypt_chr(d, key)
        rt = encrypt_chr(plain, key)
        nb = d[:16] == encode_name_block(key)
        print("%-14s %8d key=%-9s nameblock=%s roundtrip=%s" % (os.path.basename(f), len(d), key.decode(), nb, rt == d))
        ok &= (rt == d) and nb
    print("files=%d ALL_OK=%s" % (len(files), ok))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
