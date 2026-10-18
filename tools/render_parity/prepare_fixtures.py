#!/usr/bin/env python3
"""Recreate the mutable scratch fixtures the corpus UI scripts read, so a run
always starts from the same bytes (PLAN.md 4.3: the golden has to be a function
of the corpus, never of the previous run).

    prepare_fixtures.py [--work /mnt/c/dev/hantei-chan/work]

Three of the committed shot scripts work on fixtures they also write back:

* `shots__s2_pat_editor.txt` ends with `savechar`, which rewrites
  `work/ab/s2/chr000.pat` (and leaves a `.bak`). The script header says where
  the copy comes from: `gamedata/uni2/data/chr000/chr000.pat`.
* the `shots_cgm__cgm_*.txt` scripts are the CG-manager smoke fixtures from
  `tools/ui/cgm_smoke.sh` (`fixture()`): MBTL chr000 plus BaseData.HA6 under
  `work/ab_s45/fx/mbtl/data/`, the MBAACC akiha files under `.../mbaacc/data/`,
  and `new_sprite.png`. The smoke saves banks, palettes and a rebuilt bank into
  that tree.
* `cgm_smoke.sh` also pre-exports `work/ab_s45/fx/plain` (a cgtool export with
  one picture recoloured), which `cgm_4_folders` imports.

The read-only PNG inputs (`work/pacnyx/*.png`, `work/texundo/*.png`) are inputs
only; they are checked for existence and never rewritten.
"""
import argparse
import os
import shutil
import subprocess
import sys

GAMEDATA = "/mnt/c/dev/hantei-chan/gamedata"
MBAACC = "/mnt/c/games/mbaacc"


def say(what):
    print("fixtures: " + what)


def fresh_dir(path):
    shutil.rmtree(path, ignore_errors=True)
    os.makedirs(path, exist_ok=True)


def copy_tree(src, dst):
    shutil.copytree(src, dst, dirs_exist_ok=True)


def write_png(path, size, boxes):
    from PIL import Image, ImageDraw
    im = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for box, fill in boxes:
        d.rectangle(box, fill=fill)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    im.save(path)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default="/mnt/c/dev/hantei-chan/work")
    args = ap.parse_args(argv)
    work = args.work

    # ---- work/ab/s2: a scratch UNI2 .pat (the s2 script header) ------------
    src_pat = os.path.join(GAMEDATA, "uni2/data/chr000/chr000.pat")
    if not os.path.exists(src_pat):
        print("fixtures: missing %s" % src_pat, file=sys.stderr)
        return 1
    s2 = os.path.join(work, "ab/s2")
    fresh_dir(s2)
    shutil.copyfile(src_pat, os.path.join(s2, "chr000.pat"))
    say("work/ab/s2/chr000.pat <- uni2 chr000.pat")

    # ---- work/ab_s45/fx: the CG-manager smoke fixture (cgm_smoke.sh) -------
    fx = os.path.join(work, "ab_s45/fx")
    fresh_dir(fx)
    os.makedirs(os.path.join(fx, "mbtl/data"), exist_ok=True)
    os.makedirs(os.path.join(fx, "mbaacc/data"), exist_ok=True)
    copy_tree(os.path.join(GAMEDATA, "mbtl/data/chr000"), os.path.join(fx, "mbtl/data/chr000"))
    shutil.copyfile(os.path.join(GAMEDATA, "mbtl/data/BaseData.HA6"),
                    os.path.join(fx, "mbtl/data/BaseData.HA6"))
    for name in sorted(os.listdir(os.path.join(MBAACC, "data"))):
        if name.startswith("akiha") or name.startswith("b_akiha"):
            shutil.copyfile(os.path.join(MBAACC, "data", name), os.path.join(fx, "mbaacc/data", name))
    write_png(os.path.join(fx, "new_sprite.png"), (64, 96),
              [((4, 4, 59, 91), (255, 0, 255, 255)), ((16, 16, 47, 79), (0, 255, 0, 255))])
    say("work/ab_s45/fx <- mbtl chr000 + mbaacc akiha + new_sprite.png")

    # ---- work/ab_s45/fx/plain: the cgtool export cgm_4 imports ------------
    plain = os.path.join(fx, "plain")
    tool = "/home/teo/dev/hantei-chan-wt/render-d3d9/build/cgmtool.exe"
    cg = os.path.join(fx, "mbtl/data/chr000/chr000.cg")
    if os.path.exists(tool) and os.path.exists(cg):
        win = lambda p: subprocess.run(["wslpath", "-w", p], capture_output=True, text=True).stdout.strip()
        subprocess.run([tool, "export", win(cg), win(plain), "--cgtool"],
                       capture_output=True, timeout=300)
        # image 10's top-left quarter recoloured magenta, exactly as cgm_smoke.sh does
        import glob
        from PIL import Image
        hits = sorted(glob.glob(os.path.join(plain, "*_ID_10.png")))
        if hits:
            p = hits[0]
            im = Image.open(p).convert("RGBA")
            w, h = im.size
            px = im.load()
            bb = im.getbbox() or (0, 0, w, h)
            for y in range(bb[1], (bb[1] + bb[3]) // 2):
                for x in range(bb[0], (bb[0] + bb[2]) // 2):
                    if px[x, y][3]:
                        px[x, y] = (255, 0, 255, 255)
            im.save(p)
            say("work/ab_s45/fx/plain <- cgtool export with image 10 edited")
        else:
            print("fixtures: cgtool export produced no *_ID_10.png", file=sys.stderr)
    else:
        print("fixtures: cgmtool.exe or chr000.cg missing, 'plain' not rebuilt", file=sys.stderr)

    # ---- /mnt/c/games/rbo_run: the RBO shots browse it. It was a copy of the
    # install at capture time and later went missing, so the browser listed a
    # different number of entries. Point it at the install again (a symlink),
    # which is what the golden's file counts came from.
    rbo_link = '/mnt/c/games/rbo_run'
    rbo_src = '/mnt/c/games/rbo'
    if os.path.isdir(rbo_src) and not os.path.exists(rbo_link):
        try:
            os.symlink(rbo_src, rbo_link)
            say('C:/games/rbo_run -> C:/games/rbo')
        except OSError as e:
            print('fixtures: could not link rbo_run: %s' % e, file=sys.stderr)

    # ---- read-only PNG inputs the scripts import --------------------------
    needed = [
        os.path.join(work, "pacnyx/test_sheet_256.png"),
        os.path.join(work, "pacnyx/not_square_256x200.png"),
        os.path.join(work, "pacnyx/effect_96x64.png"),
        os.path.join(work, "texundo/pat0_edit.png"),
        os.path.join(work, "texundo/rbo_sheet0_edit.png"),
    ]
    missing = [p for p in needed if not os.path.exists(p)]
    for p in missing:
        print("fixtures: MISSING read-only input %s" % p, file=sys.stderr)
    if missing:
        return 1
    say("read-only PNG inputs present (%d)" % len(needed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
