#!/usr/bin/env python3
"""GOF1 character .DAT parser + invariant verifier (input = decrypted container from gof1_pack.py chars/decdat).
Layout and evidence: docs/formats/gof1.md.   usage: gof1_dat.py <file.DAT> [...]   (exit 1 on any violated invariant)"""
import collections, struct, sys

HDR = 0x444
FRAME = 116
AT_SZ, IF_SZ, EF_SZ, BOX_SZ, PART_SZ = 26, 28, 20, 8, 92
PARTS_PER_PATTERN = 40
UNUSED_FRAME_RANGES = [(0x0F, 0x10), (0x14, 0x18), (0x19, 0x28), (0x2C, 0x30), (0x3D, 0x40), (0x4A, 0x50)]
# frame bytes that are constant zero in every shipped frame and have no reader: AF +0x0F,+0x14..17,+0x19..27 ; AS +0x2C..2F,+0x3D..3F,+0x4A..4F
# (AS +0x3A specialCancel and frame +0x51 are data-bearing but unread, so they are named, not listed here)


class Pattern:
    pass


def parse(d):
    h = dict(zip(("signatureA", "signatureB", "unused08", "unused0C", "version", "patternAreaEnd", "partsSize", "cgOffset", "cgSize"),
                 struct.unpack_from("<9I", d, 0)))
    tab = struct.unpack_from("<256i", d, 0x44)
    present = sorted((o, i) for i, o in enumerate(tab) if o != -1)
    pats = []
    for j, (o, i) in enumerate(present):
        end = present[j + 1][0] if j + 1 < len(present) else h["patternAreaEnd"]
        p = Pattern()
        p.index, p.off, p.end = i, o, end
        p.nframes, p.moveInfo, p.moveLevel, p.byte3 = d[o], d[o + 1], d[o + 2], d[o + 3]
        p.boxOff, p.atOff, p.ifOff, p.efOff = struct.unpack_from("<4I", d, o + 4)
        p.frames = [d[o + 20 + FRAME * k:o + 20 + FRAME * (k + 1)] for k in range(p.nframes)]
        pts = [("frames", 20 + FRAME * p.nframes)]
        for nm, v in (("box", p.boxOff), ("at", p.atOff), ("if", p.ifOff), ("ef", p.efOff)):
            if v != 0xFFFFFFFF:
                pts.append((nm, v))
        order = ("frames", "box", "at", "if", "ef")
        p.order_ok = [n for n, _ in pts] == [n for n in order if n in dict(pts)] and \
            all(pts[k][1] <= pts[k + 1][1] for k in range(len(pts) - 1))
        p.sz = {}
        for k, (nm, v) in enumerate(pts):
            e = pts[k + 1][1] if k + 1 < len(pts) else end - o
            p.sz[nm] = (v, e - v)

        def recs(nm, rs):
            if nm not in p.sz:
                return []
            v, n = p.sz[nm]
            return [d[o + v + rs * k:o + v + rs * (k + 1)] for k in range(n // rs)]
        p.boxes, p.ats, p.ifs, p.efs = recs("box", BOX_SZ), recs("at", AT_SZ), recs("if", IF_SZ), recs("ef", EF_SZ)
        pats.append(p)
    return h, tab, pats


def verify(d, name=""):
    """Returns (list of violations, Counter of measured facts)."""
    bad, st = [], collections.Counter()
    h, tab, pats = parse(d)
    chk = lambda c, m: bad.append(m) if not c else None
    chk(h["version"] == 18, "version != 18")
    chk(d[8:0x10] == bytes(8) and d[0x24:0x44] == bytes(0x20), "header unused bytes not zero")
    chk(h["cgOffset"] == h["patternAreaEnd"] + h["partsSize"], "cgOffset != patternAreaEnd + partsSize")
    chk(len(d) == h["cgOffset"] + h["cgSize"] + 0x4000, "file size != cgOffset + cgSize + 0x4000")
    st["cgSize==0"] += h["cgSize"] == 0
    offs = [o for o in tab if o != -1]
    chk(bool(offs) and offs[0] == HDR and offs == sorted(offs), "pattern table not ascending from 0x444")
    chk(all(o == -1 or HDR <= o < h["patternAreaEnd"] for o in tab), "pattern offset out of range")
    for p in pats:
        st["patterns"] += 1
        st["frames"] += p.nframes
        ctx = "%s pat %d" % (name, p.index)
        chk(1 <= p.nframes <= 100, ctx + " frameCount")
        chk(p.byte3 in (0, 116), ctx + " header byte 3 not 0/116")
        st["hdr_byte3_%d" % p.byte3] += 1
        chk(p.order_ok, ctx + " table order")
        for nm, rs in (("box", BOX_SZ), ("at", AT_SZ), ("if", IF_SZ), ("ef", EF_SZ)):
            if nm in p.sz:
                chk(p.sz[nm][1] % rs == 0, ctx + " %s table size not a multiple of %d" % (nm, rs))
        used = {"box": [], "at": [], "if": [], "ef": []}
        for k, fr in enumerate(p.frames):
            for a, b in UNUSED_FRAME_RANGES:
                chk(fr[a:b] == bytes(b - a), ctx + " frame %d unused range %#x..%#x not zero" % (k, a, b))
            if fr[0x50] != 0xFF:
                used["at"].append(fr[0x50])
            used["if"] += [v for v in struct.unpack_from("<3h", fr, 0x52) if v != -1]
            used["ef"] += [v for v in struct.unpack_from("<4h", fr, 0x58) if v != -1]
            bx = struct.unpack_from("<10h", fr, 0x60)
            used["box"] += [v for v in bx if v != -1]
            chk(bx[9] == -1, ctx + " box slot 9 used")
            chk(fr[0x08] <= 9 and fr[0x0B] <= 5 and fr[0x39] <= 2 and fr[0x38] <= 2, ctx + " enum range")
            chk(fr[0x28] <= 1 and fr[0x29] <= 1 and fr[0x2A] <= 1 and fr[0x2B] <= 1, ctx + " AS move flags not 0/1")
        for nm, tbl in (("box", p.boxes), ("at", p.ats), ("if", p.ifs), ("ef", p.efs)):
            u = used[nm]
            chk(all(0 <= x < len(tbl) for x in u), ctx + " %s index out of range" % nm)
            chk(u == list(range(len(u))) and len(u) == len(tbl), ctx + " %s indices not 0,1,2.. in frame order" % nm)
            st["n_" + nm] += len(tbl)
        for r in p.ats:
            chk(r[0x11] == 0 and r[0x13:] == bytes(7) and r[0x0C:0x0F] == bytes(3), ctx + " AT unused bytes")
        for r in p.ifs:
            chk(r[1:4] == bytes(3) and r[0x14:] == bytes(8), ctx + " IF unused bytes")
            st["if_type_%d" % r[0]] += 1
        for r in p.efs:
            chk(r[0x0C:] == bytes(8), ctx + " EF unused bytes")
            st["ef_type_%d" % r[0]] += 1
    # parts blob (old PAT v2, magic 0x01234567)
    pb = d[h["patternAreaEnd"]:h["patternAreaEnd"] + h["partsSize"]]
    chk(struct.unpack_from("<II", pb, 0) == (2, 0x01234567) and pb[8:0x18] == bytes(16), "parts header")
    po = struct.unpack_from("<1000I", pb, 0x18)
    present = [o for o in po if o]
    chk(present == sorted(present) and present[0] == 0x8CBC, "parts pattern offsets")
    chk(all(b - a == PARTS_PER_PATTERN * PART_SZ for a, b in zip(present, present[1:])), "parts pattern spacing != 3680")
    st["parts_patterns"] += len(present)
    texoff = struct.unpack_from("<I", pb, 0x8CB8)[0]
    chk(texoff == present[-1] + PARTS_PER_PATTERN * PART_SZ, "texture region does not follow the last parts pattern")
    tr = pb[texoff:]
    n = struct.unpack_from("<I", tr, 0x18)[0]
    toff = struct.unpack_from("<50I", tr, 0x1C)
    tsz = struct.unpack_from("<50I", tr, 0xD64)
    chk(sum(1 for x in toff if x) == n and all((toff[i] != 0) == (tsz[i] != 0) for i in range(50)), "texture count")
    chk(all(tsz[i] * tsz[i] * 4 == (toff[i + 1] - toff[i] if i + 1 < n else len(tr) - toff[i]) for i in range(n)), "texture raster size")
    chk(tr[0x14:0x18] == (1).to_bytes(4, "little") and tr[:0x14] == bytes(0x14) and tr[0xE2C:toff[0]] == bytes(toff[0] - 0xE2C),
        "texture region header/padding")
    st["textures"] += n
    for i in range(len(present)):
        for k in range(PARTS_PER_PATTERN):
            part = pb[present[i] + k * PART_SZ: present[i] + (k + 1) * PART_SZ]
            chk(part[0x48:] == bytes(20) and part[0x27] == 0 and part[0x3D:0x40] == bytes(3), "part unused bytes")
    st["parts"] += len(present) * PARTS_PER_PATTERN
    return bad, st


if __name__ == "__main__":
    rc = 0
    tot = collections.Counter()
    for f in sys.argv[1:]:
        try:
            bad, st = verify(open(f, "rb").read(), f)
        except Exception as e:      # a damaged/older file must not stop the sweep
            bad, st = ["exception %r" % (e,)], collections.Counter()
        tot.update(st)
        print("%-60s patterns %3d frames %4d  %s" % (f, st["patterns"], st["frames"], "OK" if not bad else "FAIL"))
        for b in bad[:10]:
            print("   ", b)
        rc |= bool(bad)
    print(dict(tot))
    sys.exit(rc)
