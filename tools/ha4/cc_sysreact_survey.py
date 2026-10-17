#!/usr/bin/env python3
"""Survey MBAACC hit-reaction patterns 900-908 / 351 against 23/24/25/26/29 (converter M7 evidence).

For each character: per reaction pattern, which source pattern + frame indices it was cut from
(matched by CG sprite id), and the durations. usage: cc_sysreact_survey.py [mbaacc_data_dir] [chars...]
"""
import sys, os
sys.path.insert(0, "/mnt/c/dev/hantei-chan/docs/tag_research/tools")
import ha6lib

D = sys.argv[1] if len(sys.argv) > 1 else "/mnt/c/games/mbaacc/data"
CH = sys.argv[2:] or ["sion", "arc", "akiha", "ciel", "hisui", "kohaku", "shiki", "neco", "nechaos", "warc", "len", "satsuki"]
SRC = [23, 24, 25, 26, 27, 28, 29, 30, 350]
for ch in CH:
    try: m, _ = ha6lib.merged_character(D, ch, 0)
    except Exception as e: print(ch, "ERR", e); continue
    print("==", ch)
    for p in list(range(900, 909)) + [351]:
        pa = m.get(p)
        if not pa: print("  p%d missing" % p); continue
        desc = []
        for fr in pa.frames:
            s = fr.AF["sprite"]; hit = None
            for q in SRC:
                if q in m:
                    for i, f2 in enumerate(m[q].frames):
                        if f2.AF["sprite"] == s and f2.AF["usePat"] == fr.AF["usePat"]: hit = "%d.%d" % (q, i); break
                if hit: break
            desc.append("%s/d%d%s%s" % (hit or "spr%s" % s, fr.AF["dur"], "/a%d,%d,j%d" % (fr.AF["aniType"], fr.AF["aniFlag"], fr.AF["jump"]) , "/IF%s" % [t for t, _ in fr.IF] if fr.IF else ""))
        print("  p%d %-12s %s" % (p, pa.name[:12], " ".join(desc)))
    for q in (23, 24, 25):
        if q in m: print("  src p%d: %s" % (q, " ".join("d%d/a%d,%d,j%d%s" % (f.AF["dur"], f.AF["aniType"], f.AF["aniFlag"], f.AF["jump"], "/IF%s" % [t for t, _ in f.IF] if f.IF else "") for f in m[q].frames)))
