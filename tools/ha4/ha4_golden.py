#!/usr/bin/env python3
"""Golden test for the MBAC -> HA6 converter (ha4tool convert).

1. Tag-in / tag-out patterns: MBAACC kept these unchanged from MBAC
   (docs/tag_research/STATE_COMPARISON_MBAC_vs_MBAACC.md §2), so the converted
   pattern must equal the shipped MBAACC pattern field by field (CG sprite ids
   excluded: MBAACC re-cut the CG, so ids are not stable).
2. Every pattern with the same number and frame count in both games: per-field
   agreement rates (MBAACC edited many moves, so this is a statistic, not a pass/fail).
   ATVV is also split per value: [0] red / [1] damage / [3] meter keep MBAC's numbers,
   [2] (CC guard-gauge damage) is 0 by policy (CONVERTER_FIDELITY.md M3).
3. Behaviour checks (pass/fail; the fixes of docs/mbac_moon/CONVERTER_FIDELITY.md, which
   sections 1-2 could not see because they compare CG frames and accept verbatim copies):
   M1 parts-frame boxes at HA6 scale (G_CHAOS / GAKIHA reference boxes, (x-320, y-448)),
   M7 reaction patterns 900-908 / 351 / 354 in every character file,
   M8 no IF 51 with p0 < 0, and every non-zero p2 names a pattern with frames,
   M3 ATVV[2] = 0 on every attack frame.

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
    print("\n### ATVV per value (same frames as above, attack frames present in both)\n")
    vv_ok = [0] * 4; vv_n = 0
    for ch in TAG:
        c = ha6lib.parse_ha6(os.path.join(conv, ch + ".ha6"))
        try: m, _ = ha6lib.merged_character(aacc, ch, 0)
        except Exception: continue
        for p, pa in c.items():
            pb = m.get(p)
            if not pb or len(pb.frames) != len(pa.frames): continue
            for fa, fb in zip(pa.frames, pb.frames):
                va, vb = (fa.AT or {}).get("ATVV"), (fb.AT or {}).get("ATVV")
                if va is None or vb is None: continue
                vv_n += 1
                for k in range(4): vv_ok[k] += va[k] == vb[k]
    for k, nm in enumerate(("red damage", "damage", "guard damage (M3 policy: 0)", "meter")):
        print("ATVV[%d] %-28s %d/%d %.1f%%" % (k, nm, vv_ok[k], vv_n, 100.0 * vv_ok[k] / max(1, vv_n)))

    beh = behaviour(conv)
    print("\nbehaviour: %d failures" % beh)
    return 0 if bad == 0 and beh == 0 else 1


# Reference boxes from the raw MBAC .DAT (CONVERTER_FIDELITY.md M1): (char, pattern, frame, box, HA6 value).
REF_BOXES = [("g_chaos", 100, 10, 25, (-130, -425, 384, 6)),   # raw (190,23,704,454), Arc Drive
             ("g_chaos", 0, 0, 0, (-48, -259, 36, -1)),         # raw (272,189,356,447), push box
             ("g_chaos", 0, 0, 2, (-119, -518, 125, -271)),     # raw (201,-70,445,177), hurt box
             ("gakiha", 0, 0, 1, (4, -297, 116, -148))]         # raw (324,151,436,300)
SYSTEM = list(range(900, 909)) + [351, 354]

def behaviour(conv):
    fails = 0
    def bad(msg):
        nonlocal fails
        fails += 1
        if fails <= 40: print("BAD " + msg)
    print("\n## 3. Behaviour checks (CONVERTER_FIDELITY.md)\n")
    cache = {}
    def load(ch):
        if ch not in cache:
            f = os.path.join(conv, ch + ".ha6")
            cache[ch] = ha6lib.parse_ha6(f) if os.path.exists(f) else None
        return cache[ch]
    n = 0
    for ch, p, fr, box, want in REF_BOXES:
        m = load(ch)
        if m is None: continue
        got = m[p].frames[fr].boxes.get(box) if p in m and fr < len(m[p].frames) else None
        n += 1
        norm = lambda b: (min(b[0], b[2]), min(b[1], b[3]), max(b[0], b[2]), max(b[1], b[3]))
        if got is None or norm(got) != norm(want):
            bad("M1 %s p%d f%d box %d = %s, want %s" % (ch, p, fr, box, got, want))
    print("M1 reference parts-frame boxes checked: %d" % n)
    chars = if51 = atk = 0
    for f in sorted(os.listdir(conv)):
        if not f.endswith(".ha6"): continue
        ch = f[:-4]; m = load(ch)
        have = {k for k, v in m.items() if v.frames}
        if {17, 23, 24, 25, 26} <= have:
            chars += 1
            miss = [s for s in SYSTEM if s not in have]
            if miss: bad("M7 %s missing reaction patterns %s" % (ch, miss))
        for p, pa in m.items():
            for i, fr in enumerate(pa.frames):
                for t, prm in fr.IF:
                    if t != 51: continue
                    if51 += 1
                    prm = list(prm) + [0, 0, 0]
                    if prm[0] < 0: bad("M8 %s p%d f%d IF51 p0 %d" % (ch, p, i, prm[0]))
                    if prm[2] and prm[2] not in have: bad("M8 %s p%d f%d IF51 p2 %d is not a pattern" % (ch, p, i, prm[2]))
                if fr.AT and any(k >= 25 for k in fr.boxes):
                    atk += 1
                    vv = fr.AT.get("ATVV")
                    if vv and vv[2] != 0: bad("M3 %s p%d f%d ATVV[2] = %d" % (ch, p, i, vv[2]))
    print("M7 characters with the reaction set checked: %d" % chars)
    print("M8 IF 51 records checked: %d" % if51)
    print("M3 attack frames checked: %d" % atk)
    return fails

if __name__ == "__main__":
    sys.exit(main())
