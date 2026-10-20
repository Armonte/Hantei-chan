#!/usr/bin/env python3
"""Compare two render-parity run folders case by case (PLAN.md 4.4/4.5).

    diff.py <golden-dir> <run-dir> [--allow allowlist.tsv] [--report out.md]
            [--heat <dir>] [--only <glob>] [--quiet]

A run folder is what run_corpus.sh writes: a manifest.tsv with one row per case
(case, png, sha256, w, h, exit, round) and the PNGs next to it. Each case names
one captured PNG, so a single --batch file that holds several shots is treated
as that many cases (the manifest row is per shot, not per file).

Classification:
  PASS     byte-identical PNG in both runs (PLAN.md 4.5: byte-identical, nothing
           else), and both processes that wrote it exited 0.
  ALLOWED  differs, but matches an allowlist.tsv row whose bounds hold.
  FAIL     everything else: a case missing on either side, a case whose
           process exited non-zero or timed out (manifest `exit`, 124 =
           timeout) on either side, a PNG that re-encodes the same pixels with
           different bytes (reported as such, never a PASS), a size change.

Only a differing PNG is decoded, to measure the difference (max |d|, pixels,
bounding box) for the report, the heatmap and the allowlist bounds.

Non-PNG outputs (area E: the export's <name>.json manifest and
export_result.txt) are manifest rows too, with w = h = 0. They are compared by
sha256 of the file bytes after masking the wall-clock durations the exporter
writes, and nothing else: the "timing_ms" object of the JSON and the
"in N ms." of export_result.txt (WALLCLOCK below). Every other byte must match.

Exit code 1 on any FAIL.
"""
import argparse
import fnmatch
import hashlib
import os
import re
import sys
from functools import lru_cache

import numpy as np
from PIL import Image


# ---------------------------------------------------------------- manifest

class Row:
    __slots__ = ("case", "png", "sha256", "w", "h", "exit", "rnd")

    def __init__(self, case, png, sha256, w, h, exit_code, rnd):
        self.case = case
        self.png = png
        self.sha256 = sha256
        self.w = w
        self.h = h
        self.exit = exit_code
        self.rnd = rnd


def read_manifest(run_dir):
    path = os.path.join(run_dir, "manifest.tsv")
    if not os.path.exists(path):
        raise SystemExit("no manifest.tsv in %s" % run_dir)
    rows = {}
    with open(path, "r", encoding="utf-8") as f:
        header = f.readline().rstrip("\n").split("\t")
        need = ["case", "png", "sha256", "w", "h", "exit"]
        for name in need:
            if name not in header:
                raise SystemExit("manifest %s: no column %r" % (path, name))
        idx = {n: header.index(n) for n in need}
        idx["round"] = header.index("round") if "round" in header else -1
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            c = line.split("\t")
            rnd = c[idx["round"]] if idx["round"] >= 0 else "1"
            r = Row(c[idx["case"]], c[idx["png"]], c[idx["sha256"]],
                    int(c[idx["w"]]), int(c[idx["h"]]), int(c[idx["exit"]]), rnd)
            # A later round overrides an earlier one for the same case name.
            rows[r.case] = r
    return rows


# ---------------------------------------------------------------- images

@lru_cache(maxsize=256)
def canonical_bytes(path):
    im = Image.open(path)
    im.load()
    rgba = im.convert("RGBA")
    data = rgba.tobytes()
    return hashlib.sha256(data).hexdigest(), rgba.size[0], rgba.size[1]


def decode_rgba(path):
    im = Image.open(path)
    im.load()
    return np.asarray(im.convert("RGBA"), dtype=np.int16)


# The only bytes of a non-PNG output that may differ between two runs: measured
# wall-clock durations, which no renderer controls. Each pattern is replaced by
# a fixed token before hashing.
WALLCLOCK = (
    (re.compile(rb'"timing_ms": \{[^{}]*\}'), b'"timing_ms": {}'),   # <name>.json
    (re.compile(rb' in [0-9]+ ms\.'), b' in N ms.'),                   # export_result.txt
)


def masked_sha(path):
    data = open(path, "rb").read()
    for pat, rep in WALLCLOCK:
        data = pat.sub(rep, data)
    return hashlib.sha256(data).hexdigest()


def is_png(path):
    return path.lower().endswith(".png")


# ---------------------------------------------------------------- allowlist

class Allow:
    __slots__ = ("glob", "backend", "cls", "max_abs", "max_pixels", "region",
                 "evidence", "note", "pixels", "used")

    def __init__(self, fields, line_no):
        if len(fields) < 7:
            raise SystemExit("allowlist line %d: need 7 columns" % line_no)
        self.glob = fields[0]
        validate_glob(self.glob, line_no)
        self.backend = fields[1]
        self.cls = fields[2]
        self.max_abs = int(fields[3])
        self.max_pixels = int(fields[4])
        self.region = fields[5]
        self.evidence = fields[6]
        self.note = fields[7] if len(fields) > 7 else ""
        self.used = []

    def matches(self, case, backend):
        if self.backend not in ("*", "any", backend):
            return False
        return fnmatch.fnmatchcase(case, self.glob)


# Never a glob that covers a whole area (PLAN.md 4.6, last line). Each entry
# names one feature group at most, so a bare "<game>_*" is rejected when the
# allowlist is loaded, before any case is compared.
AREA_GLOBS = ("*", "any_*", "uni2_*", "mbtl_*", "mbac_*", "mbaacc_*", "unib_*",
              "dfci_*", "ui_*", "stage_*", "stage3d_*", "export_*", "shots_*",
              "s8_*", "shots_cgm_*", "shots_layout_*")


def validate_glob(glob, line_no):
    if glob in AREA_GLOBS:
        raise SystemExit("allowlist line %d: glob %r covers a whole area" % (line_no, glob))


def load_allowlist(path):
    entries = []
    if not path:
        return entries
    with open(path, "r", encoding="utf-8") as f:
        for i, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.split("\t")[0] == "case_glob":   # the committed header row
                continue
            entries.append(Allow(line.split("\t"), i))
    return entries


# ---------------------------------------------------------------- comparing

def edges_mask(golden):
    """True where a pixel touches a change of alpha or of colour (geometry or
    alpha edge), dilated by one pixel."""
    a = golden[..., 3]
    lum = golden[..., :3].sum(axis=2)
    edge = np.zeros(golden.shape[:2], dtype=bool)
    for m in (a, lum):
        d = np.zeros_like(edge)
        d[:-1, :] |= m[:-1, :] != m[1:, :]
        d[:, :-1] |= m[:, :-1] != m[:, 1:]
        edge |= d
    out = edge.copy()
    out[1:, :] |= edge[:-1, :]
    out[:-1, :] |= edge[1:, :]
    out[:, 1:] |= edge[:, :-1]
    out[:, :-1] |= edge[:, 1:]
    return out


def compare(golden_path, run_path, allow, case, backend):
    """-> (class, details dict)."""
    gsha, gw, gh = canonical_bytes(golden_path)
    gfile = hashlib.sha256(open(golden_path, "rb").read()).hexdigest()
    rfile = hashlib.sha256(open(run_path, "rb").read()).hexdigest()
    if gfile == rfile:
        return "PASS", {"max_abs": 0, "pixels": 0, "pct": 0.0}
    rsha, rw, rh = canonical_bytes(run_path)
    if gsha == rsha:
        # PLAN.md 4.5: PASS is a byte-identical PNG. The same pixels in other
        # bytes means the encoder or its settings changed: report it, fail it.
        return "FAIL", {"max_abs": 0, "pixels": 0, "pct": 0.0,
                        "note": "re-encoded, same pixels, different PNG bytes"}
    if (gw, gh) != (rw, rh):
        return "FAIL", {"max_abs": 255, "pixels": gw * gh, "pct": 100.0,
                        "note": "size %dx%d vs %dx%d" % (gw, gh, rw, rh)}

    g = decode_rgba(golden_path)
    r = decode_rgba(run_path)
    if g.shape != r.shape:
        return "FAIL", {"max_abs": 255, "pixels": g.size // 4, "pct": 100.0,
                        "note": "channel layout differs"}
    d = np.abs(g - r)
    per_px = d.max(axis=2)
    n = int(np.count_nonzero(per_px))
    max_abs = int(per_px.max()) if n else 0
    total = g.shape[0] * g.shape[1]
    pct = 100.0 * n / max(1, total)
    ys, xs = np.nonzero(per_px)
    bbox = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())) if n else None
    details = {"max_abs": max_abs, "pixels": n, "pct": pct, "bbox": bbox,
               "mean": float(d.mean()) if n else 0.0}

    for a in allow or []:
        if not a.matches(case, backend):
            continue
        if max_abs > a.max_abs or n > a.max_pixels:
            details["allowlist_rejected"] = (
                "%s: max_abs %d>%d or pixels %d>%d" % (a.glob, max_abs, a.max_abs, n, a.max_pixels))
            continue
        if a.region == "edges":
            em = edges_mask(g)
            if not em.shape == per_px.shape or np.any(per_px[~em]):
                details["allowlist_rejected"] = "%s: differences off edges" % a.glob
                continue
        a.used.append(case)
        details["allowlist"] = a
        return "ALLOWED", details
    return "FAIL", details


# ---------------------------------------------------------------- heatmaps

def write_heat(golden_path, run_path, out_path, d):
    g = decode_rgba(golden_path).astype(np.int16)
    r = decode_rgba(run_path).astype(np.int16)
    per_px = np.abs(g - r).max(axis=2)
    base = (g[..., :3] // 3).astype(np.uint8)
    heat = np.zeros_like(base)
    m = per_px > 0
    scaled = np.clip(per_px * 4, 0, 255).astype(np.uint8)
    heat[..., 0] = np.where(m, scaled, 0)
    heat[..., 1] = np.where(per_px > 8, scaled // 2, 0)
    out = np.clip(base + heat, 0, 255).astype(np.uint8)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    Image.fromarray(out, "RGB").save(out_path)


# ---------------------------------------------------------------- reporting

def exit_text(code):
    if code == 124:
        return "124 (timeout)"
    if code < 0:
        return "%d (no producing process recorded)" % code
    return str(code)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("golden")
    ap.add_argument("run")
    ap.add_argument("--allow", default=None)
    ap.add_argument("--report", default=None)
    ap.add_argument("--heat", default=None)
    ap.add_argument("--only", default=None, help="glob on the case name")
    ap.add_argument("--backend", default="gl")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    allow = load_allowlist(args.allow)
    gold = read_manifest(args.golden)
    run = read_manifest(args.run)

    cases = sorted(set(gold) | set(run))
    if args.only:
        cases = [c for c in cases if fnmatch.fnmatchcase(c, args.only)]

    counts = {"PASS": 0, "ALLOWED": 0, "FAIL": 0, "MISSING": 0, "EXIT": 0}
    fails = []
    allowed = []
    for case in cases:
        g = gold.get(case)
        r = run.get(case)
        if g is None or r is None:
            counts["FAIL"] += 1
            counts["MISSING"] += 1
            fails.append((case, {"max_abs": 0, "pixels": 0, "pct": 0.0,
                                 "note": "missing in " + ("run" if r is None else "golden")}))
            continue
        gp = os.path.join(args.golden, g.png)
        rp = os.path.join(args.run, r.png)
        if not os.path.exists(gp) or not os.path.exists(rp):
            counts["FAIL"] += 1
            counts["MISSING"] += 1
            fails.append((case, {"max_abs": 0, "pixels": 0, "pct": 0.0, "note": "no output file"}))
            continue
        if g.exit != 0 or r.exit != 0:
            # The process that wrote this case failed or timed out: what it left
            # behind is not a valid render, whatever its bytes are.
            counts["FAIL"] += 1
            counts["EXIT"] += 1
            fails.append((case, {"max_abs": 0, "pixels": 0, "pct": 0.0,
                                 "note": "exit golden %s / run %s" % (exit_text(g.exit), exit_text(r.exit))}))
            continue
        if not is_png(gp):
            if masked_sha(gp) == masked_sha(rp):
                counts["PASS"] += 1
            else:
                counts["FAIL"] += 1
                fails.append((case, {"max_abs": 0, "pixels": 0, "pct": 0.0,
                                     "note": "non-PNG output differs (sha256, wall-clock fields masked)"}))
            continue
        cls, d = compare(gp, rp, allow, case, args.backend)
        counts[cls] += 1
        if cls == "FAIL":
            fails.append((case, d))
            if args.heat:
                write_heat(gp, rp, os.path.join(args.heat, case + ".png"), d)
        elif cls == "ALLOWED":
            allowed.append((case, d))

    total = counts["PASS"] + counts["ALLOWED"] + counts["FAIL"]   # MISSING / EXIT are kinds of FAIL
    if args.report:
        with open(args.report, "w", encoding="utf-8") as f:
            f.write("# render parity: %s\n\n" % args.run)
            f.write("golden: %s\n\naliases: backend %s\n\n" % (args.golden, args.backend))
            f.write("| class | cases |\n|---|---|\n")
            for k in ("PASS", "ALLOWED", "FAIL"):
                f.write("| %s | %d |\n" % (k, counts[k]))
            f.write("| total | %d |\n\n" % total)
            f.write("missing on one side: %d\n\n" % counts["MISSING"])
            f.write("process exit non-zero or timed out: %d\n\n" % counts["EXIT"])
            if allowed:
                f.write("## Allowed cases\n\n| case | class | max abs | pixels | % |\n|---|---|---|---|---|\n")
                for case, d in allowed:
                    f.write("| %s | %s | %d | %d | %.3f |\n" % (
                        case, d["allowlist"].cls, d["max_abs"], d["pixels"], d["pct"]))
                f.write("\n")
            if fails:
                fails.sort(key=lambda kv: -kv[1]["pixels"])
                f.write("## Worst FAILs\n\n| case | max abs | pixels | % | note | heat |\n|---|---|---|---|---|---|\n")
                for case, d in fails[:20]:
                    link = "[heat](%s)" % (case + ".png") if args.heat else ""
                    f.write("| %s | %d | %d | %.3f | %s | %s |\n" % (
                        case, d["max_abs"], d["pixels"], d["pct"], d.get("note", ""), link))
                f.write("\n")

    if not args.quiet:
        print("PASS %d / ALLOWED %d / FAIL %d (total %d, missing %d, bad exit %d)" % (
            counts["PASS"], counts["ALLOWED"], counts["FAIL"], total,
            counts["MISSING"], counts["EXIT"]))
        for case, d in fails[:20]:
            print("  FAIL %-64s max %3d px %8d %6.3f%% %s" % (
                case[:64], d["max_abs"], d["pixels"], d["pct"], d.get("note", "")))
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
