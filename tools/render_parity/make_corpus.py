#!/usr/bin/env python3
"""Generate the render-parity corpus from the game data (PLAN.md 4.2).

    make_corpus.py [--out tools/render_parity/corpus] [--check]

Systematic, not hand-picked: for each game it dumps the character's frame data
with the baseline's ha6_dump.exe, finds frames that carry each feature the plan
lists (all four blend modes, flip H/V, rotation, scale, alpha, tint, AFPL
buckets, palette banks, PAT layers, spawns), and writes one --batch file per
game with a deterministic, sorted pick of three frames per feature.

Variants, as required: zoom 1/2/0.5, boxes 0/1, bg 0,0,0 / 255,255,255, and one
shot at 1920x1080 and one at 333x257. Shot names are
<game>_<chr>_p<pat>_f<frame>_t<tick>_<feature>_z<zoom>_<bg>_<boxes>.png

The batch open path is written as a Windows path (the editor takes those); the
plan's fixed install layout is in roots.py. Missing data is skipped, never
faked: a machine without a game simply gets no cases for it.

Output paths use the token OUT/, which run_corpus.sh rewrites to the run folder.
"""
import argparse
import collections
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from roots import BGLISTS, CHARS, ROOTS, STAGES_2D  # noqa: E402


BASELINE_DIR = "/mnt/c/dev/hantei-chan/render_parity/baseline_bin/45ea8ae59c1efd7bb6c1a9bc884c4205cef3174a"


def wslpath(path):
    return subprocess.run(["wslpath", "-w", path], capture_output=True, text=True).stdout.strip()


def read_txt(path):
    """The .txt project stack: the primary HA6 and the .pat (both optional)."""
    try:
        raw = open(path, "rb").read().decode("utf-8", "replace")
    except OSError:
        return None
    files, pat = [], None
    sec = None
    for line in raw.splitlines():
        line = line.strip()
        if line.startswith("["):
            sec = line.strip("[]").lower()
            continue
        if "=" not in line:
            continue
        k, v = line.split("=", 1)
        k, v = k.strip(), v.strip()
        if sec == "datafile" and k.lower().startswith("file") and k.lower() != "filenum":
            files.append(v)
        elif sec == "panifile" and k.lower().startswith("file") and k.lower() != "filenum" and v:
            pat = v
    d = os.path.dirname(path)
    main = None
    for f in files:
        if f.lower().startswith("_temp"):
            continue
        cand = os.path.normpath(os.path.join(d, f))
        if os.path.exists(cand):
            main = cand
            break
    if main is None and files:
        cand = os.path.normpath(os.path.join(d, files[-1]))
        if os.path.exists(cand):
            main = cand
    return {"main": main, "pat": os.path.normpath(os.path.join(d, pat)) if pat else None}


def dump_frames(ha6, npat=800):
    """-> {pattern: dict} parsed from ha6_dump.exe."""
    out = subprocess.run([os.path.join(BASELINE_DIR, "ha6_dump.exe"), wslpath(ha6)]
                         + [str(i) for i in range(npat)],
                         capture_output=True, text=True, errors="replace").stdout
    pats = {}
    cur_seq = None
    cur_frame = None
    for line in out.splitlines():
        m = re.match(r"^=== pattern (\d+) .* frames=(\d+) .* pups=(\d+) ATV2=(\d+) AFGX=(\d+)", line)
        if m:
            cur_seq = {"id": int(m.group(1)), "nframes": int(m.group(2)),
                       "pups": int(m.group(3)), "afgx": int(m.group(5)),
                       "frames": [], "layers": [], "patsets": set()}
            pats[cur_seq["id"]] = cur_seq
            continue
        if cur_seq is None:
            continue
        m = re.match(r"^frame (\d+) \[tick (\d+)\.\.(\d+)\] dur=(\d+) .* interp=(\d+)", line)
        if m:
            cur_frame = (int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
            cur_seq["frames"].append(cur_frame)
            continue
        m = re.match(r"^   layer (\d+): sprite=(-?\d+)( \(PAT\))? ofs=\((-?\d+),(-?\d+)\) blend=(\d+) "
                     r"rgba=\(([^)]*)\) rot=\(([^)]*)\) scale=\(([^)]*)\) prio=(\d+)", line)
        if m and cur_frame is not None:
            li = int(m.group(1))
            if li == 0:
                cur_seq["layers"].append({"pat": m.group(3) is not None, "blend": int(m.group(6)),
                                          "rgba": [float(x) for x in m.group(7).split(",")],
                                          "rot": [float(x) for x in m.group(8).split(",")],
                                          "scale": [float(x) for x in m.group(9).split(",")],
                                          "prio": int(m.group(10))})
    return pats


def features(seq):
    """feature -> [(pattern, frame, tick)] over one pattern's frames."""
    out = collections.defaultdict(list)
    if not seq["frames"]:
        return out
    f0 = seq["frames"][0]
    out["idle"].append((seq["id"], 0, f0[1]))
    for fl, fr in zip(seq["layers"], seq["frames"]):
        key = (seq["id"], fr[0], fr[1])
        if fl["blend"] in (1, 2, 3):
            out["blend%d" % fl["blend"]].append(key)
        if fl["rot"][0] != 0:
            out["flipV"].append(key)
        if fl["rot"][1] != 0:
            out["flipH"].append(key)
        if fl["rot"][2] != 0:
            out["rot"].append(key)
        if (fl["scale"][0], fl["scale"][1]) != (1.0, 1.0):
            out["scale"].append(key)
        if fl["rgba"][3] < 0.999:
            out["alpha"].append(key)
        if (fl["rgba"][0], fl["rgba"][1], fl["rgba"][2]) != (1.0, 1.0, 1.0):
            out["tint"].append(key)
        if fl["prio"] != 0:
            out["afpl%d" % fl["prio"]].append(key)
        if fl["pat"]:
            out["pat"].append(key)
    for fr in seq["frames"]:
        if fr[4] != 0:
            out["afhk"].append((seq["id"], fr[0], fr[1]))
    if seq["afgx"] > 1:
        out["afgx"].append((seq["id"], 0, f0[1]))
    if seq["pups"]:
        out["pups"].append((seq["id"], 0, f0[1]))
    return out


def pick(cands, n=3):
    """Deterministic pick, spread over patterns."""
    seen, out = set(), []
    for c in sorted(set(cands)):
        if c[0] in seen:
            continue
        out.append(c)
        seen.add(c[0])
        if len(out) >= n:
            break
    if len(out) < n:
        for c in sorted(set(cands)):
            if c not in out:
                out.append(c)
                if len(out) >= n:
                    break
    return out[:n]


VARIANTS = [  # zoom, bg, boxes, w, h
    (1.0, "0,0,0", 0, 480, 360),
    (1.0, "255,255,255", 0, 480, 360),
    (1.0, "0,0,0", 1, 480, 360),
    (2.0, "0,0,0", 0, 480, 360),
    (0.5, "0,0,0", 0, 480, 360),
    (1.0, "0,0,0", 0, 1920, 1080),
    (1.0, "0,0,0", 0, 333, 257),
]
VAR_SHORT = {"idle": VARIANTS, "blend1": VARIANTS, "blend2": VARIANTS, "blend3": VARIANTS,
             "flipH": VARIANTS, "flipV": VARIANTS, "afhk": VARIANTS, "pat": VARIANTS[:3],
             "pups": VARIANTS[:3], "afgx": VARIANTS[:2]}
DEFAULT_VARIANTS = VARIANTS[:2]


def case_name(game, tag, pat, frame, feat, zoom, bg, boxes, w, h):
    bgname = "b0" if bg.startswith("0") else "b255"
    z = "%g" % zoom
    z = z.replace("0.5", "05")
    # The default 480x360 body keeps the plan's name; the two size variants the
    # plan asks for (1920x1080, 333x257) get the size appended so they cannot
    # overwrite the default shot of the same case.
    size = "" if (w, h) == (480, 360) else "_%dx%d" % (w, h)
    return "%s_%s_p%d_f%d_t%d_%s_z%s_%s_%s%s" % (game, tag, pat, frame, frame, feat, z, bgname,
                                               "box1" if boxes else "box0", size)


def build_game(game, out_dir, log):
    root = ROOTS[game]
    lines = []
    for rel in CHARS.get(game, []):
        path = os.path.join(root, rel)
        if not os.path.exists(path):
            log.append("skip %s/%s (missing)" % (game, rel))
            continue
        info = read_txt(path)
        if not info or not info["main"] or not os.path.exists(info["main"]):
            log.append("skip %s/%s (no HA6)" % (game, rel))
            continue
        tag = os.path.splitext(os.path.basename(rel))[0]
        win = wslpath(path)
        pats = dump_frames(info["main"])
        feats = collections.defaultdict(list)
        for s in pats.values():
            for k, v in features(s).items():
                feats[k].extend(v)
        for feat in sorted(feats):
            if not feats[feat]:
                continue
            variants = VAR_SHORT.get(feat, DEFAULT_VARIANTS)
            for pat, frame, tick in pick(feats[feat], 3):
                for zoom, bg, boxes, w, h in variants:
                    name = case_name(game, tag, pat, frame, feat, zoom, bg, boxes, w, h)
                    parts = ["open %s" % win, "pattern %d" % pat, "frame %d" % frame,
                             "zoom %g" % zoom, "size %dx%d" % (w, h), "bg %s" % bg,
                             "boxes %d" % boxes, "spawns 0",
                             "export OUT/%s.png" % name]
                    lines.append("; ".join(parts))
    if lines:
        with open(os.path.join(out_dir, "chars_%s.batch" % game), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    return len(lines)


def bglist_entries(game):
    """DataFile names of every stage in bg/BgList.txt, in file order."""
    path = os.path.join(ROOTS[game], BGLISTS[game])
    if not os.path.exists(path):
        return []
    text = open(path, "rb").read().decode("cp932", "replace")
    return re.findall(r'DataFile\s*=\s*"([^"]+)"', text)


# 3D stage view (area C): centre, far left, far right.
S3D_CAMS = [("c", 0.0, 0.0, 1.0), ("l", -3.0, 0.0, 1.0), ("r", 3.0, 0.0, 1.0)]

# Area B (onion skin, pattern compare) and area E (PNG export) both use
# startup-only options, so every case needs its own process: one corpus file per
# case, marked "#mode: plain" so run_corpus.sh passes the file's #args: line to
# the exe without --batch / --ui-script.
B_CASES = [
    ("uni2", "data/chr000/chr000_0.txt", "chr000", 71),
    ("uni2", "data/chr019/chr019_0.txt", "chr019", 62),
    ("uni2", "data/chr020/chr020_0.txt", "chr020", 435),
    ("mbtl", "data/chr001/chr001.txt", "chr001", 157),
    ("mbaacc", "data/akiha.txt", "akiha", 0),
]
E_CASES = [
    ("uni2", "data/chr000/chr000_0.txt", "chr000", 71),
    ("uni2", "data/chr019/chr019_0.txt", "chr019", 153),
    ("uni2", "data/chr024/chr024_0.txt", "chr024", 76),
    ("mbtl", "data/chr001/chr001.txt", "chr001", 157),
    ("mbaacc", "data/akiha.txt", "akiha", 10),
]
E_VARIANTS = [
    ("tr", "--export-bg transparent --export-scale 2 --export-crop each"),
    ("ed", "--export-bg editor --export-scale 2 --export-crop each"),
    ("bx", "--export-bg transparent --export-scale 2 --export-boxes"),
    ("ns", "--export-bg transparent --export-scale 2 --export-nospawns"),
]


def plain_case(out_dir, name, args, note):
    """One process, one case: the file carries the exe's arguments."""
    with open(os.path.join(out_dir, "ui", name + ".txt"), "w", encoding="utf-8") as f:
        f.write("# %s\n#mode: plain\n#args: %s\n" % (note, args))


def build_overlay_export(out_dir, log):
    n = 0
    for game, rel, tag, pat in B_CASES:
        p = os.path.join(ROOTS[game], rel)
        if not os.path.exists(p):
            log.append("skip overlay %s/%s (missing)" % (game, rel))
            continue
        w = wslpath(p)
        for kind, opt in (("onion224", "--onion 2,2,4"), ("cmp", "--compare 0")):
            name = "overlay_%s_%s_p%d_%s" % (game, tag, pat, kind)
            plain_case(out_dir, name,
                       "%s --open %s --pattern %d --frame 0 --zoom 1 --size 640x480 "
                       "--bg 0,0,0 --capture-view OUT/%s.png --quit"
                       % (opt, w, pat, name),
                       "area B: %s on %s p%d" % (opt, tag, pat))
            n += 1
    for game, rel, tag, pat in E_CASES:
        p = os.path.join(ROOTS[game], rel)
        if not os.path.exists(p):
            log.append("skip export %s/%s (missing)" % (game, rel))
            continue
        w = wslpath(p)
        for vtag, opts in E_VARIANTS:
            name = "export_%s_%s_%s_p%d" % (vtag, game, tag, pat)
            plain_case(out_dir, name,
                       "--export-png OUT/%s --export-range current %s --open %s "
                       "--pattern %d --frame 0 --zoom 1 --size 640x480 --bg 0,0,0"
                       % (name, opts, w, pat),
                       "area E: %s on %s p%d" % (vtag, tag, pat))
            n += 1
    return n


# The smoke subset (PLAN.md 4.4: about 60 cases for quick local checks). One
# committed batch file, so the ctest render_parity run is one process per game
# instead of the whole corpus. Pick is deterministic: the idle frame of every
# listed character, zoom 1 and 2, plus the two odd-size bodies.
SMOKE_VARIANTS = [(1.0, 0, 480, 360), (2.0, 0, 480, 360), (1.0, 0, 333, 257)]


def build_smoke(out_dir, log):
    lines = []
    for game, entries in (("uni2", CHARS["uni2"]), ("mbtl", CHARS["mbtl"]),
                          ("unib", CHARS["unib"]), ("dfci", CHARS["dfci"]),
                          ("mbaacc", CHARS["mbaacc"])):
        for rel in entries:
            p = os.path.join(ROOTS[game], rel)
            if not os.path.exists(p):
                continue
            tag = os.path.splitext(os.path.basename(rel))[0]
            for zoom, boxes, w, h in SMOKE_VARIANTS:
                name = "smoke_%s_%s_p0_f0_z%g_%dx%d" % (game, tag, zoom, w, h)
                lines.append("open %s; pattern 0; frame 0; zoom %g; size %dx%d; bg 0,0,0; "
                             "boxes %d; spawns 0; export OUT/%s.png"
                             % (wslpath(p), zoom, w, h, boxes, name))
    if lines:
        with open(os.path.join(out_dir, "smoke.batch"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
    return len(lines)


def build_stages(out_dir, log):
    """Area C: the MBAACC/MBAC 2D stage tabs as --batch, every UNI2/MBTL stage
    and 5 DFCI/UNIB stages through the 3D stage view as ui-scripts."""
    n = 0
    # -- MBAACC 2D stages: bgmake .dat, the batch `open` path --------------
    sub, names, ext = STAGES_2D["mbaacc"]
    lines = []
    for name in names:
        p = os.path.join(ROOTS["mbaacc"], sub, name + ext)
        if not os.path.exists(p):
            log.append("skip mbaacc stage %s (missing)" % name)
            continue
        for cam, cx, cy in (("c", 0, 0), ("l", -120, 0), ("r", 120, 0)):
            lines.append("open %s; zoom 1; size 1280x720; bg 0,0,0; boxes 0; center %d,%d; "
                         "export OUT/stage_mbaacc_%s_%s.png" % (wslpath(p), cx, cy, name, cam))
    if lines:
        with open(os.path.join(out_dir, "stages_2d.batch"), "w", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")
        n += len(lines)

    # -- MBAC 2D stages: MBAC .DAT only opens through the header-based open --
    sub, names, ext = STAGES_2D["mbac"]
    script = ["# MBAC stages (.DAT via header-based open)"]
    for name in names:
        p = os.path.join(ROOTS["mbac"], sub, name + ext)
        if not os.path.exists(p):
            log.append("skip mbac stage %s (missing)" % name)
            continue
        script.append("openany %s" % wslpath(p))
        script.append("wait 120")
        script.append("capture OUT/stage_mbac_%s.png" % name.lower())
        n += 1
    if len(script) > 1:
        script.append("quit")
        with open(os.path.join(out_dir, "ui", "stages_mbac.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(script) + "\n")

    # -- MBAC characters: .DAT through the header-based open, a few patterns --
    chars = []
    for rel in CHARS["mbac"]:
        p = os.path.join(ROOTS["mbac"], rel)
        if not os.path.exists(p):
            log.append("skip mbac char %s (missing)" % rel)
            continue
        tag = os.path.splitext(os.path.basename(rel))[0].lower()
        chars.append("# %s" % tag)
        chars += ["openany %s" % wslpath(p), "wait 120"]
        for pat in (0, 10, 100):
            for zoom, ztag in ((1.0, "z1"), (0.5, "z05")):
                chars += ["pattern %d" % pat, "viewframe 0", "zoomview %d" % int(zoom * 100),
                          "wait 40", "capture OUT/mbac_%s_p%d_f0_idle_%s.png" % (tag, pat, ztag)]
                n += 1
    if len(chars) > 1:
        chars.append("quit")
        with open(os.path.join(out_dir, "ui", "mbac_chars.txt"), "w", encoding="utf-8") as f:
            f.write("\n".join(chars) + "\n")

    # -- 3D stage view: UNI2 + MBTL every listed stage, DFCI + UNIB five -----
    s3d = {"uni2": ("bg", ROOTS["uni2"]), "mbtl": ("bg", ROOTS["mbtl"])}
    for game, (sub, root) in s3d.items():
        entries = bglist_entries(game)
        if not entries:
            log.append("skip s3d %s (no BgList)" % game)
            continue
        script = ["# 3D stage view: every %s stage in bg/BgList.txt, three camera positions" % game]
        for e in entries:
            d = os.path.join(root, sub, e)
            if not os.path.isdir(d):
                log.append("skip s3d %s/%s (missing)" % (game, e))
                continue
            script.append("stage3d open %s %s" % (wslpath(os.path.join(root, sub)), e))
            for tag, x, y, z in S3D_CAMS:
                script += ["stage3d frame 0",
                           "stage3d cam %g %g %g" % (x, y, z),
                           "capture OUT/stage3d_%s_%s_%s.png" % (game, e, tag)]
                n += 1
        script.append("quit")
        with open(os.path.join(out_dir, "ui", "stage3d_%s.txt" % game), "w", encoding="utf-8") as f:
            f.write("\n".join(script) + "\n")

    for game, root, sub in (("dfci", ROOTS["dfci"], "bg"), ("unib", ROOTS["unib"], "bg")):
        entries = bglist_entries(game)
        if not entries:
            log.append("skip s3d %s (no BgList)" % game)
            continue
        script = ["# 3D stage view: five %s stages, three camera positions" % game]
        for e in entries[:5]:
            d = os.path.join(root, sub, e)
            if not os.path.isdir(d):
                log.append("skip s3d %s/%s (missing)" % (game, e))
                continue
            script.append("stage3d open %s %s" % (wslpath(os.path.join(root, sub)), e))
            for tag, x, y, z in S3D_CAMS:
                script += ["stage3d frame 0",
                           "stage3d cam %g %g %g" % (x, y, z),
                           "capture OUT/stage3d_%s_%s_%s.png" % (game, e, tag)]
                n += 1
        script.append("quit")
        with open(os.path.join(out_dir, "ui", "stage3d_%s.txt" % game), "w", encoding="utf-8") as f:
            f.write("\n".join(script) + "\n")
    return n


# Browsed folders that change under other tools are swapped for a fixed copy
# that prepare_fixtures.py builds: the archive browser draws the browsed
# folder's file count, so a live folder would make the shot drift.
# C:\games\gof1\run is the GOF1 run copy, where PovertyCaster writes its logs.
FIXED_FOLDERS = {
    "browse C:\\games\\gof1\\run": "browse C:\\dev\\hantei-chan\\work\\render_parity\\fixtures\\gof1_run",
}


def build_ui(out_dir, log):
    """Area D: copy every committed shots script into corpus/ui, with every
    capture re-pointed at OUT/<sub>_<script>_<name>.png so a case name is unique
    across scripts (run_corpus.sh rewrites OUT/ to the run folder), and every
    FIXED_FOLDERS browse re-pointed at its fixture."""
    src_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "ui")
    n = 0
    for sub in ("shots", "shots_cgm", "shots_layout", "s8"):
        d = os.path.normpath(os.path.join(src_root, sub))
        if not os.path.isdir(d):
            continue
        for name in sorted(os.listdir(d)):
            if not name.endswith(".txt"):
                continue
            raw = open(os.path.join(d, name), encoding="utf-8", errors="replace").read()
            stem = name[:-4]
            out_lines = []
            for line in raw.splitlines():
                m = re.match(r'^(\s*capture(?:view)?\s+)(.+?)\s*$', line)
                if m:
                    base = re.split(r'[/\\]', m.group(2))[-1]
                    base = re.sub(r'\.png$', '', base)
                    base = re.sub(r'[^0-9A-Za-z_.-]', '_', base)
                    line = "%sOUT/%s_%s_%s.png" % (m.group(1), sub, stem, base)
                if line.strip() in FIXED_FOLDERS:
                    line = line.replace(line.strip(), FIXED_FOLDERS[line.strip()])
                out_lines.append(line)
            raw = "\n".join(out_lines) + "\n"
            out = os.path.join(out_dir, "ui", "%s__%s" % (sub, name))
            with open(out, "w", encoding="utf-8") as f:
                f.write(raw)
            n += 1
    return n


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "corpus"))
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)
    os.makedirs(os.path.join(args.out, "ui"), exist_ok=True)
    log = []
    total = 0
    for game in ("uni2", "mbtl", "unib", "dfci", "mbaacc", "mbac"):
        n = build_game(game, args.out, log)
        total += n
        print("%-8s %5d shots" % (game, n))
    stages = build_stages(args.out, log)
    total += stages
    print("%-8s %5d shots" % ("stages", stages))
    ui = build_ui(args.out, log)
    print("%-8s %5d scripts" % ("ui", ui))
    overlay = build_overlay_export(args.out, log)
    total += overlay
    print("%-8s %5d cases" % ("overlay+export", overlay))
    smoke = build_smoke(args.out, log)
    print("%-8s %5d cases" % ("smoke", smoke))
    for l in log:
        print("  " + l)
    print("total %d" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
