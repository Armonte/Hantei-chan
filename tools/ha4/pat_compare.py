#!/usr/bin/env python3
"""Compare two PAniDataFile .pat files part by part (converted MBAC parts vs MBAACC's shipped .pat).

usage: pat_compare.py converted.pat mbaacc.pat
Compares every part-set / part (PRXY PRZM PRAN PRPR PRID PRRV PRAL PRFL PRCL PRSP) and every
cutout (PPUV PPSS PPCC PPTP PPTX) present in both files; textures by size and pixel bytes.
"""
import struct, sys, collections

def parse(path):
    b = open(path, "rb").read()
    assert b[:12] == b"PAniDataFile", path
    p = 0x24; n = len(b)
    sets, cuts, gfx = {}, {}, {}
    cs = cp = cc = cg = None
    while p < n:
        t = b[p:p+4]; p += 4
        if t == b"P_ST": cs = struct.unpack_from("<i", b, p)[0]; p += 4; sets[cs] = {}
        elif t == b"PANM": p += 32
        elif t == b"PRST": cp = struct.unpack_from("<i", b, p)[0]; p += 4; sets[cs][cp] = {}
        elif t == b"PRXY": sets[cs][cp]["PRXY"] = struct.unpack_from("<ii", b, p); p += 8
        elif t == b"PRZM": sets[cs][cp]["PRZM"] = tuple(round(x, 4) for x in struct.unpack_from("<ff", b, p)); p += 8
        elif t == b"PRAN": sets[cs][cp]["PRAN"] = round(struct.unpack_from("<f", b, p)[0], 4); p += 4
        elif t == b"PRA3": v = struct.unpack_from("<4f", b, p); sets[cs][cp]["PRAN"] = round(v[3], 4); p += 16
        elif t in (b"PRAL", b"PRRV", b"PRFL"): sets[cs][cp][t.decode()] = b[p] if b[p] != 2 or t != b"PRAL" else 1; p += 1
        elif t in (b"PRSP", b"PRCL"): sets[cs][cp][t.decode()] = b[p:p+4].hex(); p += 4
        elif t in (b"PRPR", b"PRID"): sets[cs][cp][t.decode()] = struct.unpack_from("<i", b, p)[0]; p += 4
        elif t in (b"PRED", b"P_ED", b"PPED", b"PGED"): pass
        elif t == b"PPST": cc = struct.unpack_from("<i", b, p)[0]; p += 4; cuts[cc] = {}
        elif t == b"PPNM": p += 32
        elif t in (b"PPCC", b"PPSS", b"PPJP"): cuts[cc][t.decode()] = struct.unpack_from("<2i", b, p); p += 8
        elif t == b"PPUV": cuts[cc]["PPUV"] = struct.unpack_from("<4i", b, p); p += 16
        elif t in (b"PPTP", b"PPTX", b"PPPA", b"PPPP"): cuts[cc][t.decode()] = struct.unpack_from("<i", b, p)[0]; p += 4
        elif t == b"PPTE": p += 4
        elif t == b"PGST": cg = struct.unpack_from("<i", b, p)[0]; p += 4; gfx[cg] = {}
        elif t == b"PGNM": p += 32
        elif t == b"PGTP": p += 4
        elif t == b"PGTX":
            w, h, bpp = struct.unpack_from("<3i", b, p); gfx[cg] = (w, h, b[p+12:p+12+w*h*bpp//8]); p += 12 + w*h*bpp//8
        elif t == b"VEST": p += 8
        elif t in (b"VNST", b"VEED"): pass
        elif t == b"_END": break
        else: raise ValueError("tag %r at %x" % (t, p - 4))
    return sets, cuts, gfx

def main():
    a, b = parse(sys.argv[1]), parse(sys.argv[2])
    C = collections.Counter(); ex = []
    for sid, parts in a[0].items():
        if sid not in b[0]: C["set only in converted"] += 1; continue
        for pid, pa in parts.items():
            pb = b[0][sid].get(pid)
            if pb is None: C["part only in converted"] += 1; continue
            for k in set(pa) | set(pb):
                eq = pa.get(k) == pb.get(k)
                C[("part." + k, eq)] += 1
                if not eq and len(ex) < 8: ex.append((sid, pid, k, pa.get(k), pb.get(k)))
    for cid, ca in a[1].items():
        cb = b[1].get(cid)
        if cb is None: C["cutout only in converted"] += 1; continue
        for k in set(ca) | set(cb):
            eq = ca.get(k) == cb.get(k); C[("cut." + k, eq)] += 1
            if not eq and len(ex) < 16: ex.append(("cut", cid, k, ca.get(k), cb.get(k)))
    for gid, ga in a[2].items():
        gb = b[2].get(gid)
        C[("texture equal", gb is not None and ga == gb)] += 1
    print("sets %d/%d, cutouts %d/%d, textures %d/%d (converted/mbaacc)" % (len(a[0]), len(b[0]), len(a[1]), len(b[1]), len(a[2]), len(b[2])))
    for k, v in sorted(C.items(), key=str): print("  %6d %s" % (v, k))
    for e in ex: print("  diff", e)

if __name__ == "__main__":
    main()
