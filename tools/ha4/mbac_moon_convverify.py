#!/usr/bin/env python3
"""Output-side check of the HA4 -> HA6 converter: reads a converted roster (<dir>/<char>/<char>.ha6) and counts,
per CONVERTER_FIDELITY.md mismatch id, how many records still carry the wrong value.

mbac_moon_convfidelity.py counts *exposure* (rows a mismatch can hit, from the raw .DAT); this script counts
*residual* errors in a given conversion, so running it on roster/ and roster_v2/ gives the before/after table.

Usage: mbac_moon_convverify.py ROSTER_DIR [ROSTER_DIR ...]   (one table column per dir)

Expected values (the S8 rules, IDA evidence in CONVERTER_FIDELITY.md):
  M1  parts-frame box (sprite < 10000) = (x-320, y-448); CG frame (x-128, y-224)
  M2  EF 1/2/3/4/8/11 p0/p1 = framedata_ha4.cpp EfPosRules (spawner frame kind + p2/p3 flag modes)
  M3  ATVV[2] = 0 (mbacPC never reads AT +0x4E; non-zero = stun value leaked into CC guard damage)
  M7  character data (17 and 23-26 present) has 900-908, 351, 354
  M8  IF 51: p0 != -1, and a non-zero p2 names an existing pattern of the same file
  M12 EF 1/8/11 p2 has no bits >= 0x100 (mbacPC reads the low byte; MBAA gives them other meanings)
S8b (ActCadenza 1.03A reference):
  G1  <c>_cc_c.txt Guard = the 1.03A <CHR>_C.CT floats f1..f4 (CT+0x2270; 06.p CT, else the 02.p one)
  G2  giants: GAKIHA (GiantFlag 2) has 23-26/44/45/900-908/351/354; G_CHAOS (GiantFlag 1) has 44/45
  L1  no EF1 target that is empty in the converted HA6 but present in the vanilla MBAACC stack (spawn leak)
  S1  an IF51 EX window (frame with p0 -1) other than 6/6/8 has a matching <c>_actcadenza_moon.ini
"""
import collections, glob, os, sys
T = "/mnt/c/dev/hantei-chan/docs"
sys.path.insert(0, T + "/bg_research/tools"); sys.path.insert(0, T + "/tag_research/tools")
import ha4lib, ha6lib

MBAC = "/mnt/c/dev/hantei-chan/gamedata/mbac/02"
PATCH103A = "/mnt/c/dev/hantei-chan/gamedata/mbac_v11_english/06"
MBAACC = "/mnt/c/dev/hantei-chan/gamedata/mbaacc_ref/vanilla_0002"
import re, struct


def ct_guts(n):
    for d in (PATCH103A, MBAC):
        p = os.path.join(d, n + "_C.CT")
        if os.path.exists(p):
            b = open(p, "rb").read()
            return [round(x, 4) for x in struct.unpack_from("<4f", b, 0x2270)]
    return None


def s8b(roster, n, m, h, C):
    c = n.lower()
    g = ct_guts(n)
    cmd = os.path.join(roster, c, c + "_cc_c.txt")
    if g and os.path.exists(os.path.join(MBAC, n + "_C.TXT")):
        C["G1_n"] += 1
        got = None
        if os.path.exists(cmd):
            mm = re.search(rb"^Guard = (.*)$", open(cmd, "rb").read(), re.M)
            if mm: got = [round(float(x), 4) for x in mm.group(1).decode().strip().split(",")]
        want = [x if x != 0 else 1.0 for x in g]
        if got != want: C["G1_bad"] += 1
    if c in ("gakiha", "g_chaos"):
        need = [44, 45] if c == "g_chaos" else [23, 24, 25, 26, 44, 45] + list(range(900, 909)) + [351, 354]
        C["G2_n"] += len(need)
        C["G2_bad"] += sum(1 for k in need if not (h.get(k) and h[k].frames))
    stack = {}
    for suf in ("", "_r", "_0", "_0_r"):
        f = [x for x in os.listdir(MBAACC) if x.lower() == (c + suf + ".ha6")] if os.path.isdir(MBAACC) else []
        if f:
            for k, v in ha6lib.parse_ha6(os.path.join(MBAACC, f[0])).items():
                if v.frames: stack[k] = v
    if stack:
        for no, hp in h.items():
            for fr in hp.frames:
                for (t, no2, prm) in fr.EF:
                    if t != 1: continue
                    C["L1_n"] += 1
                    if not (h.get(no2) and h[no2].frames) and no2 in stack: C["L1_bad"] += 1
    win = {}
    for mp in m.values():
        for mf in mp.frames:
            for (_, t, prm) in mf.IF:
                if t == 51 and prm[0] < 0:
                    st = mf.AS["stance"] if mf.AS else 0
                    win[st] = max(win.get(st, -1), mf.AF["wait"])
    if win:
        C["S1_n"] += 1
        ex = (win.get(0, 6), win.get(2, 6), win.get(1, 8))
        side = os.path.join(roster, c, c + "_actcadenza_moon.ini")
        if ex != (6, 6, 8):
            ok = False
            if os.path.exists(side):
                t = open(side, "rb").read().decode("cp932")
                vals = dict(re.findall(r"(ExShieldTime_\w+) = (\d+)", t))
                ok = all(int(vals.get(k, d)) == v for k, d, v in
                         (("ExShieldTime_Stand", 6, ex[0]), ("ExShieldTime_Crouch", 6, ex[1]), ("ExShieldTime_Air", 8, ex[2])))
            if not ok: C["S1_bad"] += 1


def ef_rules(t, parts, p2, p3):
    """-> ((xbias, xhalf), (ybias, yhalf)), mirror of framedata_ha4.cpp EfPosRules."""
    bx, by = (320, 448) if parts else (128, 224)
    rx, ry = [0, False], [0, False]
    if t in (2, 3, 4): return (bx, False), (by, False)
    if t not in (1, 8, 11): return tuple(rx), tuple(ry)
    if p3 & 0x200: return tuple(rx), tuple(ry)
    if p2 & 0x10: rx[0] = ((-64 if parts else 0) if (p3 & 0x100) else (256 if parts else 128))
    elif not (p3 & 0x100): rx[0] = bx
    if p2 & 0x80: ry[1] = parts
    elif not (p2 & 0x10): ry[0] = by
    return tuple(rx), tuple(ry)


def check(roster, C):
    for dat in sorted(glob.glob(os.path.join(MBAC, "*.DAT"))):
        n = os.path.basename(dat)[:-4]
        if n.startswith("CSEL"): continue
        ha6 = os.path.join(roster, n.lower(), n.lower() + ".ha6")
        if not os.path.exists(ha6): C["missing_char"] += 1; continue
        m, _ = ha4lib.parse_dat(dat)
        h = ha6lib.parse_ha6(ha6)
        h = h[0] if isinstance(h, tuple) else h
        C["chars"] += 1
        s8b(roster, n, m, h, C)
        if all(k in m for k in (17, 23, 24, 25, 26)):
            C["M7_chars_checked"] += 1
            for k in list(range(900, 909)) + [351, 354]:
                if not (h.get(k) and h[k].frames): C["M7_missing_patterns"] += 1
        for no, mp in m.items():
            hp = h.get(no)
            if not hp or len(hp.frames) != len(mp.frames): C["pattern_shape_mismatch"] += 1; continue
            for mf, hf in zip(mp.frames, hp.frames):
                parts = 0 <= mf.AF["sprite"] < 10000
                ox, oy = (320, 448) if parts else (128, 224)
                for k, v in mf.boxes.items():
                    want = (v[0] - ox, v[1] - oy, v[2] - ox, v[3] - oy)
                    got = hf.boxes.get(k)
                    tag = "M1_parts_box" if parts else "box_cg"
                    C[tag + "_n"] += 1
                    if got is None or tuple(got) != want: C[tag + "_bad"] += 1
                for (_, t, no2, p, q), (T2, N2, PP) in zip(mf.EF, hf.EF):
                    PP = list(PP) + [0] * (12 - len(PP))     # the HA6 writer drops trailing zero params
                    if t in (1, 8, 11):
                        C["M12_n"] += 1
                        if len(PP) > 2 and PP[2] & ~0xFF: C["M12_bad"] += 1
                    if t not in (1, 2, 3, 4, 8, 11): continue
                    (bx, hx), (by, hy) = ef_rules(t, parts, p[2] & 0xFF, p[3])
                    want = ((p[0] >> 1) if hx else p[0] - bx, (p[1] >> 1) if hy else p[1] - by)
                    tag = "M2_ef_parts" if parts else "ef_cg"
                    C[tag + "_n"] += 1
                    if tuple(PP[:2]) != want: C[tag + "_bad"] += 1
                if mf.AT and hf.AT is not None:
                    C["M3_at_n"] += 1
                    vv = hf.AT.get("ATVV", (0, 0, 0, 0))
                    if vv[2] != 0: C["M3_atvv2_nonzero"] += 1
                for (T2, PR) in hf.IF:
                    if T2 != 51: continue
                    PR = list(PR) + [0] * (12 - len(PR))
                    C["M8_if51_n"] += 1
                    if PR and PR[0] == -1: C["M8_p0_minus1"] += 1
                    if len(PR) > 2 and PR[2] and not (h.get(PR[2]) and h[PR[2]].frames): C["M8_p2_bad_pattern"] += 1
    return C


ROWS = [("M1", "parts-frame boxes wrong", "M1_parts_box_bad", "M1_parts_box_n"),
        ("--", "CG-frame boxes wrong (control)", "box_cg_bad", "box_cg_n"),
        ("M2", "parts-frame EF positions wrong", "M2_ef_parts_bad", "M2_ef_parts_n"),
        ("M2/M9", "CG-frame EF positions wrong", "ef_cg_bad", "ef_cg_n"),
        ("M3", "ATVV[2] non-zero (stun leaked)", "M3_atvv2_nonzero", "M3_at_n"),
        ("M7", "missing 900-908/351/354", "M7_missing_patterns", "M7_chars_checked"),
        ("M8", "IF51 p0 == -1", "M8_p0_minus1", "M8_if51_n"),
        ("M8", "IF51 p2 not a pattern", "M8_p2_bad_pattern", "M8_if51_n"),
        ("M12", "EF1/8/11 p2 bits >= 0x100", "M12_bad", "M12_n"),
        ("G1", "Guard != 1.03A CT f1..f4", "G1_bad", "G1_n"),
        ("G2", "giant reaction slots missing", "G2_bad", "G2_n"),
        ("L1", "EF1 leaks into MBAACC-only slots", "L1_bad", "L1_n"),
        ("S1", "EX window != 6/6/8 without sidecar", "S1_bad", "S1_n")]


def main():
    dirs = sys.argv[1:]
    if not dirs: print(__doc__); return 2
    res = [check(d, collections.Counter()) for d in dirs]
    print("| id | residual error | " + " | ".join(os.path.basename(d.rstrip("/")) for d in dirs) + " |")
    print("|---|---|" + "---|" * len(dirs))
    for mid, txt, bad, n in ROWS:
        print(f"| {mid} | {txt} | " + " | ".join(f"{C.get(bad, 0)} / {C.get(n, 0)}" for C in res) + " |")
    for C, d in zip(res, dirs):
        print(f"# {d}: chars={C['chars']} missing_char={C['missing_char']} pattern_shape_mismatch={C['pattern_shape_mismatch']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
