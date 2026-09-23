#!/usr/bin/env python3
"""Golden test for the MBAC -> HA6 converter (ha4tool convert).

1. Tag-in / tag-out patterns: MBAACC kept these unchanged from MBAC
   (docs/tag_research/STATE_COMPARISON_MBAC_vs_MBAACC.md §2), so the converted
   pattern must equal the shipped MBAACC pattern field by field (CG sprite ids
   excluded: MBAACC re-cut the CG, so ids are not stable).
2. Every pattern with the same number and frame count in both games: per-field
   agreement rates (MBAACC edited many moves, so this is a statistic, not a pass/fail).

usage: ha4_golden.py <converted_dir> [mbaacc_data_dir]
Needs docs/tag_research/tools/ha6lib.py (path below).
"""
import sys, os, collections
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, "/mnt/c/dev/hantei-chan/docs/tag_research/tools")
import ha6lib

import struct

# ha6lib's AF parser skips the AFY<n> offset shorthand; this one decodes it
# (AFY7..9 -> y 7..9, AFYX -> 10, AFY1..3 -> 11..13, x = 0), so AFOF and AFY compare equal.
def _af(d, p, fr):
    A = fr.AF
    I = lambda o: struct.unpack_from("<i", d, o)[0]
    while True:
        t = d[p:p+4]; p += 4
        if t == b"AFGP": A["usePat"] = I(p); A["sprite"] = I(p+4); p += 8
        elif t == b"AFGX": A["usePat"] = I(p+4); A["sprite"] = I(p+8); p += 12
        elif t == b"AFOF": A["off"] = (I(p), I(p+4)); p += 8
        elif t[:3] == b"AFD":
            c = chr(t[3])
            if c.isdigit(): A["dur"] = int(c)
            elif c == "L": A["dur"] = I(p); p += 4
        elif t[:3] == b"AFY":
            c = chr(t[3]); v = 10 if c == "X" else int(c)
            A["off"] = (0, v + 10 if v < 4 else v)
        elif t[:3] == b"AFF":
            c = chr(t[3])
            if c in "12": A["aniType"] = int(c)
            elif c == "L": A["aniType"] = I(p); p += 4
            elif c == "E": A["aniFlag"] = I(p); p += 4
        elif t == b"AFAL": A["blend"] = (I(p), I(p+4)); p += 8
        elif t == b"AFRG": A["rgb"] = (I(p), I(p+4), I(p+8)); p += 12
        elif t in (b"AFAZ", b"AFAY", b"AFAX"): A[t.decode()] = round(struct.unpack_from("<f", d, p)[0], 4); p += 4
        elif t == b"AFZM": A["zoom"] = tuple(round(x, 4) for x in struct.unpack_from("<2f", d, p)); p += 8
        elif t == b"AFJP": A["jump"] = I(p); p += 4
        elif t == b"AFHK": A["interp"] = I(p); p += 4
        elif t == b"AFPR": A["priority"] = I(p); p += 4
        elif t == b"AFCT": A["loopCount"] = I(p); p += 4
        elif t == b"AFLP": A["loopEnd"] = I(p); p += 4
        elif t == b"AFJC": A["landJump"] = I(p); p += 4
        elif t == b"AFTN": A["AFTN"] = (I(p), I(p+4)); p += 8
        elif t in (b"AFPL", b"AFRT", b"AFID", b"AFPA", b"AFJH"): p += 4
        elif t == b"AFED": return p
        else: raise ValueError("AF tag %r @%x" % (t, p-4))
ha6lib._af = _af

# MBAACC name: (MBAC tag-in pattern, tag-out pattern), from STATE_COMPARISON §2
TAG = {"sion": (241, 242), "arc": (235, 236), "ciel": (243, 244), "akiha": (177, 178), "hisui": (241, 242),
       "kohaku": (188, 189), "shiki": (241, 242), "miyako": (241, 242), "warakia": (241, 242), "nero": (244, 245),
       "v_sion": (241, 242), "warc": (197, 198), "akaakiha": (241, 242), "m_hisui": (241, 242),
       "nanaya": (241, 242), "satsuki": (241, 242), "len": (241, 242), "neco": (241, 242), "aoko": (240, 241),
       "wlen": (241, 242), "nechaos": (241, 242), "kishima": (241, 242)}

def af_key(fr):
    a = fr.AF
    return {k: a.get(k) for k in ("dur", "aniType", "aniFlag", "jump", "landJump", "loopCount", "loopEnd",
                                  "priority", "usePat", "off", "blend", "rgb", "AFAX", "AFAY", "AFAZ",
                                  "zoom", "interp", "AFTN")}

def fields(fr, prevAS):
    AS = fr.AS if fr.AS is not None else prevAS
    d = {"AF." + k: v for k, v in af_key(fr).items()}
    for k, v in (AS or {}).items(): d["AS." + k] = v
    d["AT"] = tuple(sorted((fr.AT or {}).items()))
    d["EF"] = tuple((t, n, tuple(p)) for t, n, p in fr.EF)
    d["IF"] = tuple((t, tuple(p)) for t, p in fr.IF)
    # boxes compared by area: the game swaps inverted corners (mbacPC Actor_BoxToWorldRect,
    # MBAA the same), and Hantei-chan's HA6 writer normalizes them (FixBoxesForSave).
    d["boxes"] = tuple(sorted((k, (min(b[0], b[2]), min(b[1], b[3]), max(b[0], b[2]), max(b[1], b[3])))
                              for k, b in fr.boxes.items()))
    return d, AS

def compare(pa, pb):
    diffs = []
    asA = asB = None
    for i, (fa, fb) in enumerate(zip(pa.frames, pb.frames)):
        da, asA = fields(fa, asA); db, asB = fields(fb, asB)
        for k in da:
            if da[k] != db.get(k): diffs.append((i, k, da[k], db.get(k)))
    return diffs

def main():
    conv = sys.argv[1]
    aacc = sys.argv[2] if len(sys.argv) > 2 else "/mnt/c/games/mbaacc/data"
    ok = bad = 0
    print("## 1. Tag-in / tag-out golden patterns\n")
    for ch, pats in TAG.items():
        c = ha6lib.parse_ha6(os.path.join(conv, ch + ".ha6"))
        base = ha6lib._find(aacc, ch + ".HA6")
        m = ha6lib.parse_ha6(base) if base else {}
        for p in pats:
            pa, pb = c.get(p), m.get(p)
            if not pa or not pb or len(pa.frames) != len(pb.frames):
                print("MISSING %-9s p%d (conv %s, mbaacc %s)" % (ch, p, pa and len(pa.frames), pb and len(pb.frames)))
                bad += 1; continue
            d = compare(pa, pb)
            if d:
                bad += 1
                print("DIFF    %-9s p%-3d %2d frames: %d field diffs, first: %s" % (ch, p, len(pa.frames), len(d), d[:3]))
            else:
                ok += 1
                print("OK      %-9s p%-3d %2d frames identical (all fields but CG sprite id)" % (ch, p, len(pa.frames)))
    print("\ngolden: %d/%d tag patterns identical\n" % (ok, ok + bad))

    print("## 2. Field agreement over all patterns with equal frame counts\n")
    agree = collections.Counter(); total = collections.Counter(); npat = nfr = 0
    for ch in TAG:
        c = ha6lib.parse_ha6(os.path.join(conv, ch + ".ha6"))
        try: m, _ = ha6lib.merged_character(aacc, ch, 0)
        except Exception: continue
        for p, pa in c.items():
            pb = m.get(p)
            if not pb or len(pb.frames) != len(pa.frames) or not pa.frames: continue
            npat += 1; nfr += len(pa.frames)
            asA = asB = None
            for fa, fb in zip(pa.frames, pb.frames):
                da, asA = fields(fa, asA); db, asB = fields(fb, asB)
                for k in da:
                    total[k] += 1; agree[k] += da[k] == db.get(k)
    print("%d patterns / %d frames compared (MBAACC crescent-moon merged data)\n" % (npat, nfr))
    print("| field | agree | % |\n|---|---|---|")
    for k in sorted(total):
        print("| %s | %d/%d | %.1f |" % (k, agree[k], total[k], 100.0 * agree[k] / total[k]))
    return 0 if bad == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
