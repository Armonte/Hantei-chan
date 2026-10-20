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

The browsed game folders have to be fixed too (the archive browser shows
their file counts): `C:/games/rbo_run` is a directory junction to the RBO
install, and the GOF1 shots browse `work/render_parity/fixtures/gof1_run`
(hard links to the game files) instead of the live `C:/games/gof1/run`.

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


GOF1_FILES = ("gof.exe", "gof_00.p", "gof_01.p", "gof_02.p", "gof_03.p")


def winpath(p):
    """/mnt/c/a/b -> C:\\a\\b, also for a path that does not exist yet."""
    assert p.startswith("/mnt/c/"), p
    return "C:\\" + p[len("/mnt/c/"):].replace("/", "\\")


def win_cmd(*args):
    # cmd.exe refuses a \\wsl.localhost working folder, so run it from C:.
    return subprocess.run(["cmd.exe", "/c"] + list(args), cwd="/mnt/c", capture_output=True, text=True)


def win_sees(path):
    """True when the Windows side can open <path> (a WSL symlink on DrvFs
    is visible to Linux but not to the exe)."""
    r = win_cmd("if", "exist", winpath(path), "(echo", "yes)", "else", "(echo", "no)")
    return r.stdout.strip().endswith("yes")


def ensure_junction(link, target, probe):
    """Make <link> a Windows directory junction to <target> and prove the exe's
    side resolves it by opening <link>\\<probe> through cmd.exe."""
    wl, wt = winpath(link), winpath(target)
    probe_path = wl + "\\" + probe
    if win_cmd("if", "exist", probe_path, "(echo", "yes)").stdout.strip() == "yes":
        say("%s is a Windows-visible link to %s" % (wl, wt))
        return True
    if os.path.lexists(link):
        if not os.path.islink(link):
            print("fixtures: %s exists and is not a link; not touching it" % link, file=sys.stderr)
            return False
        os.unlink(link)   # the WSL symlink only, never the target
    r = win_cmd("mklink", "/J", wl, wt)
    if r.returncode != 0:
        print("fixtures: mklink /J %s %s failed: %s%s" % (wl, wt, r.stdout, r.stderr), file=sys.stderr)
        return False
    if win_cmd("if", "exist", probe_path, "(echo", "yes)").stdout.strip() != "yes":
        print("fixtures: %s still not visible from Windows" % probe_path, file=sys.stderr)
        return False
    say("%s -> %s (junction)" % (wl, wt))
    return True


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

    # ---- C:/games/rbo_run: the RBO shots browse it. It has to be something
    # the Windows exe can traverse: a WSL symlink (what this script made until
    # 2026-10-19) is an LX reparse point that Windows cannot open, so every RBO
    # shot showed the C:/games root and its live "Files in games (N)" count
    # instead of RBO. A directory junction is resolved by Windows itself. The
    # RBO browser lists DATA's top-level files only (non-recursive), which do
    # not change; the live folders next to it (pcdev, logs) are never listed.
    if not ensure_junction("/mnt/c/games/rbo_run", "/mnt/c/games/rbo", "DATA\\Update01.PAC"):
        return 1

    # ---- the GOF1 shots (04_ex3_image, 08_gof1_character_view) browse a
    # fixed folder instead of C:/games/gof1/run: that folder is the live run
    # copy (PovertyCaster logs and screenshots land there), and the browser
    # shows its file count ("Files in run (N)"). The fixture holds NTFS hard
    # links to the five game files only, so the listing is the same every run
    # and no game data is copied.
    gof1 = os.path.join(work, "render_parity/fixtures/gof1_run")
    fresh_dir(gof1)
    for name in GOF1_FILES:
        src = os.path.join("/mnt/c/games/gof1/run", name)
        if not os.path.exists(src):
            print("fixtures: missing %s" % src, file=sys.stderr)
            return 1
        os.link(src, os.path.join(gof1, name))
    if not win_sees(os.path.join(gof1, "gof_03.p")):
        print("fixtures: Windows cannot see %s" % gof1, file=sys.stderr)
        return 1
    say("work/render_parity/fixtures/gof1_run <- hard links to %s" % ", ".join(GOF1_FILES))

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
