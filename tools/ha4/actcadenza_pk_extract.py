#!/usr/bin/env python3
"""Extract an ActCadenza PK archive (00..10.p, the 1.03A 06.p patch).

Format, from mbacPC.exe (Ver.1.00 IDB) PKArchive_Open 0x431670 / PKArchive_ReadEntry 0x431A30:
  +0x00 "PKFileInfo\\0" (16 B)   +0x10 cipherType u32 (0 = name key, 1 = plain, 2 = xor 0xCA)
  +0x14 count u32 ^ 0xE3DF59AC   +0x18 count x 0x48-byte entries:
        name[0x40] (first 63 bytes ^= 3*(i*index+26)), +0x40 offset u32, +0x44 size u32 ^ 0xE3DF59AC
  Entry data: the first min(size,1977) bytes ^= j + KEY[j % len] + 3, KEY = upper-case file name.
Usage: actcadenza_pk_extract.py ARCHIVE.p OUTDIR [--only GLOB]
"""
import sys, os, struct, fnmatch

def read_dir(blob):
    assert blob[:10] == b"PKFileInfo", "not a PK archive"
    ctype, cnt = struct.unpack_from("<II", blob, 0x10)
    cnt ^= 0xE3DF59AC
    ents = []
    for k in range(cnt):
        e = bytearray(blob[0x18 + k * 0x48: 0x18 + (k + 1) * 0x48])
        for i in range(63):
            e[i] ^= (3 * (i * k + 26)) & 0xFF
        name = bytes(e[:0x40]).split(b"\0")[0].decode("ascii")
        off, size = struct.unpack_from("<II", e, 0x40)
        ents.append((name, off, size ^ 0xE3DF59AC))
    return ctype, ents

def decode(blob, ctype, name, off, size):
    d = bytearray(blob[off:off + size])
    n = min(size, 1977)
    if ctype == 1:
        return bytes(d)
    if ctype == 2:
        for j in range(n): d[j] ^= 0xCA
        return bytes(d)
    key = name.upper().encode("ascii")
    for j in range(n):
        d[j] ^= (j + key[j % len(key)] + 3) & 0xFF
    return bytes(d)

def main():
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__); return 2
    only = a[a.index("--only") + 1] if "--only" in a else None
    blob = open(a[0], "rb").read()
    ctype, ents = read_dir(blob)
    os.makedirs(a[1], exist_ok=True)
    n = 0
    for name, off, size in ents:
        if only and not fnmatch.fnmatch(name.upper(), only.upper()):
            continue
        open(os.path.join(a[1], name), "wb").write(decode(blob, ctype, name, off, size))
        n += 1
    print(f"{a[0]}: cipher {ctype}, {len(ents)} entries, {n} written to {a[1]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
