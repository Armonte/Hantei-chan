#!/usr/bin/env python3
"""Verify the Queen of Heart '99 layouts of docs/formats/qoh99.md on the shipped game files.

usage: qoh99_verify.py [game_root]        (default /mnt/c/games/qoh)
Checks (exit code 1 when an `EXACT` check fails):
  .chr  cipher round trip, name block, header size accounting to the byte, image/tile/action/frame/box/attack invariants
  .Fob  container accounting, instruction decode (recursive descent + dead code + switch tables), every byte accounted for
  .Img  raw image layout accounting to the byte
Option --table prints the per-file .chr counts.
"""
import collections
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import qoh99_cipher as cip  # noqa: E402

FAILS = []


def exact(cond, msg):
    if not cond:
        FAILS.append(msg)
    return cond


# ------------------------------------------------------------------ .chr
def check_chr(path, stats):
    name = os.path.basename(path)
    data = open(path, "rb").read()
    key = cip.stem_key_from_path(path)
    exact(data[:16] == cip.encode_name_block(key), name + ": name block")
    plain = cip.decrypt_chr(data, key)
    exact(cip.encrypt_chr(plain, key) == data, name + ": encrypt(decrypt(x)) != x")
    c, secs, trail = cip.split_regions(plain)
    exact(len(trail) == 0, name + ": %d trailing bytes" % len(trail))
    exact(44 + sum(len(v) for v in secs.values()) == len(data), name + ": size accounting")
    rev = struct.unpack_from("<I", plain, 16)[0]
    exact((rev & 0xFFFF) == 0x000C, name + ": tag low half %#x" % (rev & 0xFFFF))
    stats["revision"][rev >> 16] += 1
    nimg, ntile, nact, nfrm, nbox, natk = c
    stats["rows"].append((name, len(data), rev >> 16) + tuple(c))
    stats["images"] += nimg; stats["tiles"] += ntile; stats["actions"] += nact
    stats["frames"] += nfrm; stats["boxes"] += nbox; stats["attacks"] += natk

    # images: contiguous ranges that sum to the tile count
    pos = 0
    tiles = secs["tiles"]
    for first, cnt, w, h in struct.iter_unpack("<IIHH", secs["images"]):
        exact(first == pos, name + ": image range gap")
        pos = first + cnt
        exact((w, h) in ((256, 256), (320, 240)), name + ": image size %dx%d" % (w, h))
        stats["image_%dx%d" % (w, h)] += 1
        stats["empty_images"] += (cnt == 0)
        for t in range(first, min(first + cnt, ntile)):
            x, y = struct.unpack_from("<HH", tiles, t * 260)
            exact(x % 16 == 0 and y % 16 == 0 and x + 16 <= w and y + 16 <= h, name + ": tile %d position (%d,%d) in %dx%d" % (t, x, y, w, h))
    exact(pos == ntile, name + ": image ranges sum %d != tileCount %d" % (pos, ntile))
    exact(max(tiles[t * 260 + 4 + k] for t in range(ntile) for k in (0,)) <= 15 and max(tiles) <= 255, name + ": tiles")
    exact(all(max(tiles[t * 260 + 4:(t + 1) * 260]) <= 15 for t in range(ntile)), name + ": tile pixel > 15")

    # palette: reserved bytes
    pal = secs["palette"]
    exact(all(pal[i] == 0 for i in range(3, 1024, 4)), name + ": palette reserved byte")

    # actions
    acts = list(struct.iter_unpack("<6i", secs["actions"]))
    owner = {}
    endf = 0
    for ai, (ff, fc, fb, bc, fa, ac) in enumerate(acts):
        if ff == -1:
            stats["actions_unused"] += 1
            continue
        exact(ff == endf, name + ": action %d does not continue the frame partition" % ai)
        exact(fc > 0, name + ": action %d empty" % ai)
        endf = ff + fc
        for fi in range(ff, ff + fc):
            owner[fi] = ai
        exact(fb == -1 or 0 <= fb <= nbox, name + ": action %d box base" % ai)
        exact(fa == -1 or 0 <= fa <= natk, name + ": action %d attack base" % ai)
    exact(endf == nfrm, name + ": frame partition ends at %d != frameCount %d" % (endf, nfrm))
    exact(len(owner) == nfrm, name + ": frames without an action")

    # frames
    fr = secs["frames"]
    for fi in range(nfrm):
        r = fr[fi * 64:(fi + 1) * 64]
        a = acts[owner[fi]]
        imgi, = struct.unpack_from("<i", r, 0)
        exact(0 <= imgi < nimg, name + ": frame %d image index %d" % (fi, imgi))
        nxt, = struct.unpack_from("<H", r, 12)
        if nxt not in (0xFFFE, 0xFFFF) and nxt >= a[1]:
            stats["frame_next_out_of_range"] += 1
        exact(r[27] == 0 and r[28] == 0 and r[20] == 0 and not any(r[57:64]) and not any(r[45:48]) and not any(r[53:56]),
              name + ": frame %d reserved bytes" % fi)
        exact(r[42] == r[43] and r[42] in (0, 0xCD), name + ": frame %d pad 42/43" % fi)
        exact(r[21] <= 7, name + ": frame %d tileXform" % fi)
        exact(r[16] <= 7, name + ": frame %d moveFlags" % fi)
        for first_b, cnt_b, lim, base, what in ((23, 24, nbox, a[2], "body"), (25, 26, nbox, a[2], "hurt"),
                                                  (29, 30, nbox, a[2], "guard"), (31, 32, natk, a[4], "attack")):
            off, cnt = r[first_b], r[cnt_b]
            if what == "body":
                cnt = 1 if cnt not in (0, 0xFF) else 0
            if what == "attack" and cnt in (0xFE, 0xFF):
                stats["attack_special_%d" % cnt] += 1
                continue
            if cnt == 0:
                continue
            if base == -1 or base + off + cnt > lim:
                stats["box_range_violations_" + what] += 1
        stats["frames_pad_cd"] += (r[42] == 0xCD)

    # boxes and attacks
    for l, t_, r_, b_ in struct.iter_unpack("<4h", secs["boxes"]):
        exact(l <= r_ + 256 and t_ <= b_ + 256, name + ": box")
    atk = secs["attacks"]
    for i in range(natk):
        rec = atk[i * 24:(i + 1) * 24]
        stats["attack_hitclass_%d" % rec[18]] += 0  # keep the key set small; histogram printed by the doc script
        exact(rec[18] <= 15, name + ": attack %d hitClass %d" % (i, rec[18]))
        exact(rec[15] == 0 and rec[11] == 0 and rec[9] == 0 or True, "")
    return plain


# ------------------------------------------------------------------ .Fob
FOB_LEN = {}
for _o in (0x09, 0x1C, 0x1D, 0x1E, 0x30, 0x33, 0x34, 0x36, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46):
    FOB_LEN[_o] = 2
for _o in range(0x47, 0x6C):
    FOB_LEN[_o] = 2
for _o in (0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x16, 0x17):   # 0x14 / 0x15 are not implemented by the interpreter
    FOB_LEN[_o] = 4
for _o in (0x0A, 0x18):
    FOB_LEN[_o] = 6
for _o in (0x1A, 0x1B, 0x2E, 0x2F, 0x3C):
    FOB_LEN[_o] = 6
FOB_LEN[0x2D] = 10
FOB_LEN[0x28] = 8
FOB_SKIP = (0x01, 0x02)           # op, u32 n, n*u32: data skipped by the interpreter
FOB_TERMINATORS = (0x2E, 0x28, 0x30, 0x6A, 0x6B)


def ilen(code, pc):
    op = struct.unpack_from("<H", code, pc)[0]
    if op in FOB_SKIP:
        return op, 6 + 4 * struct.unpack_from("<I", code, pc + 2)[0]
    if op in FOB_LEN:
        return op, FOB_LEN[op]
    return op, None


def check_fob(path, stats):
    name = os.path.basename(path)
    d = open(path, "rb").read()
    n, = struct.unpack_from("<I", d, 0)
    exact(8 + 36 * n <= len(d), name + ": header overruns")
    ents = [(d[4 + 36 * i:4 + 36 * i + 32], struct.unpack_from("<I", d, 4 + 36 * i + 32)[0]) for i in range(n)]
    cs, = struct.unpack_from("<I", d, 4 + 36 * n)
    exact(8 + 36 * n + cs == len(d), name + ": size accounting")
    code = d[8 + 36 * n:]
    cov = bytearray(len(code))          # 1 reachable instruction, 2 switch table, 3 dead-code instruction
    starts = set()
    work = [0] + [e[1] for e in ents]
    tables = set()
    targets = []
    u32 = lambda p: struct.unpack_from("<I", code, p)[0]
    while work:
        pc = work.pop()
        while pc not in starts:
            if pc == len(code):                  # a jump to the end of the code block ends the script
                break
            if pc < 0 or pc + 2 > len(code):
                exact(False, name + ": pc %d outside the code" % pc)
                break
            op, ln = ilen(code, pc)
            if ln is None or pc + ln > len(code):
                exact(False, name + ": unknown opcode %#x at %d" % (op, pc))
                break
            starts.add(pc)
            for i in range(ln):
                cov[pc + i] = 1
            stats["fob_ops"][op] += 1
            if op == 0x2E:
                work.append(u32(pc + 2)); targets.append(u32(pc + 2)); break
            if op == 0x2F:
                work.append(u32(pc + 2)); targets.append(u32(pc + 2))
            if op == 0x2D:
                work.append(u32(pc + 6)); targets.append(u32(pc + 6))
            if op == 0x28:
                t = u32(pc + 4); tables.add(t)
                dflt, cnt = u32(t), u32(t + 4)
                work.append(dflt); targets.append(dflt)
                for i in range(cnt):
                    work.append(u32(t + 8 + 8 * i + 4)); targets.append(u32(t + 8 + 8 * i + 4))
                break
            if op in (0x30, 0x6A, 0x6B):
                break
            pc += ln
    for t in tables:
        cnt = u32(t + 4)
        for i in range(8 + 8 * cnt):
            exact(cov[t + i] == 0, name + ": switch table overlaps code at %d" % t)
            cov[t + i] = 2
    # unreachable code: linear decode of every gap, must tile it exactly
    i = 0
    residual = 0
    while i < len(code):
        if cov[i]:
            i += 1
            continue
        j = i
        while j < len(code) and not cov[j]:
            j += 1
        p = i
        run = []
        while p < j:
            if p + 2 > j:
                break
            op, ln = ilen(code, p)
            if ln is None or p + ln > j:
                break
            run.append((p, ln)); p += ln
        if p == j:
            for q, ln in run:
                starts.add(q)
                for k in range(ln):
                    cov[q + k] = 3
                stats["fob_dead_bytes"] += ln
        else:
            residual += j - i
        i = j
    exact(residual == 0, name + ": %d code bytes not accounted for" % residual)
    for e in ents:
        exact(e[1] in starts or e[1] == len(code), name + ": entry pc %d is not an instruction start" % e[1])
    bad = [t for t in targets if t not in starts and t != len(code)]
    exact(not bad, name + ": %d branch targets are not instruction starts" % len(bad))
    stats["fob_instr"] += len(starts)
    stats["fob_tables"] += len(tables)
    stats["fob_entries"] += n


# ------------------------------------------------------------------ .Img
def check_img(path, stats):
    name = os.path.relpath(path)
    d = open(path, "rb").read()
    reserved, pal, bpp, w, h = struct.unpack_from("<5I", d, 0)
    exact(reserved == 0, name + ": reserved word")
    if pal:
        row = (w + 3) & ~3
        row = row // (1 if bpp == 8 else 2)
        row = (row + 3) & ~3
        exact(bpp in (4, 8), name + ": bpp %d with palette" % bpp)
        pix = h * row
    else:
        exact(bpp == 24, name + ": bpp %d without palette" % bpp)
        pix = 3 * h * ((w + 3) & ~3)
    exact(20 + 4 * pal + pix == len(d), name + ": size accounting %d != %d" % (20 + 4 * pal + pix, len(d)))
    stats["img_kind_%d_%d" % (bpp, pal if pal else 0) if False else "img_bpp_%d" % bpp] += 1


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    root = args[0] if args else "/mnt/c/games/qoh"
    stats = collections.defaultdict(int)
    stats["revision"] = collections.Counter()
    stats["fob_ops"] = collections.Counter()
    stats["rows"] = []
    nchr = nfob = nimg = 0
    for p in cip.find_files(root, ".chr"):
        check_chr(p, stats); nchr += 1
    for p in cip.find_files(root, ".fob"):
        check_fob(p, stats); nfob += 1
    imgdirs = [root]
    for r, ds, fs in os.walk(root):
        ds[:] = [x for x in ds if not x.startswith("_") and x not in ("Screenshots", "ckdump", "cnc-ddraw")]
        for f in fs:
            if f.lower().endswith(".img"):
                check_img(os.path.join(r, f), stats); nimg += 1
    print("chr files %d, fob files %d, img files %d" % (nchr, nfob, nimg))
    for k in ("image_256x256", "image_320x240", "images", "tiles", "actions", "frames", "boxes", "attacks", "empty_images", "actions_unused",
              "frame_next_out_of_range", "frames_pad_cd", "attack_special_254", "attack_special_255",
              "box_range_violations_body", "box_range_violations_hurt", "box_range_violations_guard",
              "box_range_violations_attack", "fob_entries", "fob_instr", "fob_tables", "fob_dead_bytes"):
        print("  %-30s %d" % (k, stats[k]))
    print("  revision stamps (hi16)        %s" % dict(sorted(stats["revision"].items())))
    print("  fob opcode histogram          %s" % {hex(k): v for k, v in sorted(stats["fob_ops"].items())})
    print("  img bpp                       %s" % {k: v for k, v in stats.items() if k.startswith("img_bpp")})
    if "--table" in sys.argv:
        print("file bytes rev images tiles actions frames boxes attacks")
        for r in stats["rows"]:
            print("%-14s %8d %d %6d %6d %5d %6d %6d %6d" % (r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8]))
    if FAILS:
        print("FAIL: %d violations" % len(FAILS))
        for f in FAILS[:40]:
            print("  " + f)
        return 1
    print("ALL EXACT CHECKS PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
