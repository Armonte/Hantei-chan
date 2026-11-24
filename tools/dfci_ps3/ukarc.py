#!/usr/bin/env python3
"""List / extract / compare a DFCI PS3 UKArc pack (PackFile/data/*.pac.gz).

Layout (big-endian; 0x1234 is a byte-order marker like PANIBin):
  0x00 char[8]  "UKArc\\0\\0\\0"
  0x08 u16 version(=2)  u16 bom(=0x1234)  u32 count
  0x10 entry[count], 0x4C bytes each:
        char name[0x40]  u32 size  u32 unknown(hash/crc?)  u32 offset
  file data at 0x10 + offset (stored, uncompressed).
Usage: ukarc.py PACK[.gz] [--extract DIR] [--compare ARCADE_GAME_ROOT]
"""
import argparse, gzip, os, struct, sys


def load(path):
    d = open(path, "rb").read()
    if d[:2] == b"\x1f\x8b":
        d = gzip.decompress(d)
    if d[:5] != b"UKArc":
        raise SystemExit("not UKArc")
    ver, bom, cnt = struct.unpack(">HHI", d[8:16])
    ents = []
    for i in range(cnt):
        e = d[16 + i * 0x4C: 16 + (i + 1) * 0x4C]
        name = e[:0x40].split(b"\0")[0].decode("cp932", "replace")
        size, unk, off = struct.unpack(">III", e[0x40:])
        ents.append((name, size, unk, d[0x10 + off: 0x10 + off + size]))
    return ver, ents


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pack"); ap.add_argument("--extract"); ap.add_argument("--compare")
    a = ap.parse_args()
    ver, ents = load(a.pack)
    for name, size, unk, blob in ents:
        st = ""
        if a.compare:
            p = os.path.join(a.compare, name)
            if not os.path.exists(p):
                st = "only-ps3"
            else:
                arc = open(p, "rb").read()
                st = "identical" if arc == blob else f"DIFF(arcade {len(arc)})"
        print(f"{size:10d} {unk:08x} {blob[:16]!r:40s} {name} {st}")
        if a.extract:
            dst = os.path.join(a.extract, name)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, "wb").write(blob)


if __name__ == "__main__":
    main()
