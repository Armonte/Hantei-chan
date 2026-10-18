#!/usr/bin/env python3
"""Pick the ~60-case smoke subset from a rendered run (PLAN.md 4.4, the ctest
`render_parity` entry).

    make_smoke.py <run-dir> <out-dir>

It copies the manifest rows and PNGs of the picked cases into <out-dir>, so the
ctest smoke run diffs against <out-dir> the same way it diffs the full golden
(golden_smoke/ is outside git, per PLAN.md 6 Phase H deliverables).

The pick is deterministic: one case per area (character / stage / UI / export),
spread across games, plus the odd-size and zoom variants that catch half-pixel
and stride bugs.
"""
import argparse
import os
import shutil
import sys


def want(case, have):
    groups = [
        ("char_uni2", lambda c: c.startswith("uni2_")),
        ("char_mbtl", lambda c: c.startswith("mbtl_")),
        ("char_mbaacc", lambda c: c.startswith("mbaacc_")),
        ("char_dfci", lambda c: c.startswith("dfci_")),
        ("char_unib", lambda c: c.startswith("unib_")),
        ("stage2d", lambda c: c.startswith("stage_mbaacc_") or c.startswith("stage_mbac_")),
        ("stage3d", lambda c: c.startswith("stage3d_")),
        ("ui", lambda c: c.startswith("shots_") or c.startswith("s8_")
                         or c.startswith("shots_cgm_") or c.startswith("shots_layout_")),
        ("odd", lambda c: c.endswith("_333x257")),
        ("z2", lambda c: "_z2_" in c),
        ("z05", lambda c: "_z05_" in c),
        ("box1", lambda c: c.endswith("_box1")),
        ("bg255", lambda c: "_b255_" in c),
        ("pat", lambda c: "_pat_" in c),
        ("afhk", lambda c: "_afhk_" in c),
        ("blend", lambda c: "_blend" in c),
        ("flip", lambda c: "_flip" in c),
    ]
    for name, pred in groups:
        if pred(case) and have[name] < 4:
            have[name] += 1
            return True
    return False


def read_manifest(run):
    rows = []
    with open(os.path.join(run, "manifest.tsv")) as f:
        header = f.readline().rstrip("\n").split("\t")
        i = {n: header.index(n) for n in header}
        for line in f:
            c = line.rstrip("\n").split("\t")
            if len(c) >= 6:
                rows.append(c)
    return i, rows


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("out")
    ap.add_argument("--limit", type=int, default=60)
    args = ap.parse_args(argv)
    idx, rows = read_manifest(args.run)
    have = {}
    for name in ("char_uni2", "char_mbtl", "char_mbaacc", "char_dfci", "char_unib",
                 "stage2d", "stage3d", "ui", "odd", "z2", "z05", "box1", "bg255",
                 "pat", "afhk", "blend", "flip"):
        have[name] = 0
    picked = []
    for r in sorted(rows, key=lambda r: r[idx["case"]]):
        if len(picked) >= args.limit:
            break
        if want(r[idx["case"]], have):
            picked.append(r)
    # top up deterministically if a group was thin
    for r in sorted(rows, key=lambda r: r[idx["case"]]):
        if len(picked) >= args.limit:
            break
        if r not in picked:
            picked.append(r)
    os.makedirs(os.path.join(args.out, "shots"), exist_ok=True)
    with open(os.path.join(args.out, "manifest.tsv"), "w") as f:
        f.write("\t".join(["case", "png", "sha256", "w", "h", "exit", "round"]) + "\n")
        for r in picked:
            f.write("\t".join([r[idx["case"]], r[idx["png"]], r[idx["sha256"]],
                                r[idx["w"]], r[idx["h"]], r[idx["exit"]],
                                r[idx["round"]] if "round" in idx else "1"]) + "\n")
            src = os.path.join(args.run, r[idx["png"]])
            dst = os.path.join(args.out, r[idx["png"]])
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
    print("smoke: %d cases -> %s" % (len(picked), args.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
