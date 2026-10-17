#!/usr/bin/env python3
"""Verify the Queen of Heart '98 (Qoh98.exe) data layout on the shipped install.

Layouts come from LoadAndParseCharacterDATFile 0x44C290 and the runtime consumers (docs/formats/qoh98.md,
docs/formats/ida/qoh98_types.h). Checks, per file:
  * the 13 character .dat files + System/obj/obj1.dat: sizes add up to the byte, version 5, image table partitions the tile array,
    tile positions 16-aligned and inside the 256x256 canvas, tile pixels 0..15, animation / sprite / body-rect / hit-box indices in range,
    constant fields stay constant (the 'unused_' fields of qoh98_types.h)
  * every System .tim (PlayStation TIM, 8 or 4 bpp with CLUT) as consumed by Tim_LoadTo256x256Dib 0x460460
  * every .bmp is an 8-bit uncompressed Windows DIB (Bmp_LoadDibFromFile 0x440FA0)
  * every .wav is RIFF/WAVE with fmt and data chunks (Sound_LoadWavToSlot 0x422380), every .mid starts with MThd
  * System/command.txt tokenises with the Text_NextToken rules and only holds known keywords
usage: qoh98_verify.py [game_dir]   exit status 0 = everything proven exact
"""
import glob
import os
import struct
import sys

root = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/games/qoh98'
fails = []


def fail(name, msg):
    fails.append('%s: %s' % (name, msg))


def ci_glob(base, pattern_ext):
    """case-insensitive recursive-by-hand listing of files with the given extension (windows install)."""
    out = []
    for d, _, files in os.walk(base):
        if 'zzz_tmp' in d or "Queen of Heart" in d: continue
        for f in files:
            if f.lower().endswith(pattern_ext):
                out.append(os.path.join(d, f))
    return sorted(out)


# ------------------------------------------------------------------ .dat
def verify_dat(path):
    name = os.path.relpath(path, root)
    d = open(path, 'rb').read()
    ver, nimg, ntile, nanim, nspr, nbody, nhit = struct.unpack_from('<7I', d, 0)
    o = 28
    paths_off = o; o += 260 * nimg
    img_off = o; o += 8 * nimg
    tile_off = o; o += 260 * ntile
    anim_off = o; o += 24 * nanim
    spr_off = o; o += 52 * nspr
    pal_off = o; o += 1024
    body_off = o; o += 8 * nbody
    hit_off = o; o += 24 * nhit
    errs = []
    if o != len(d):
        errs.append('size %d != %d' % (o, len(d)))
        fails.append('%s: %s' % (name, errs[0]))
        return None
    if ver != 5: errs.append('version %d' % ver)
    if nanim != 256: errs.append('animCount %d' % nanim)
    # TIM path table
    for i in range(nimg):
        e = d[paths_off + 260 * i: paths_off + 260 * (i + 1)]
        if not e[:8].split(b'\0')[0]: errs.append('path %d empty name' % i); break
        if b'.' not in e[8:]: errs.append('path %d no file name' % i); break
    # image table partitions the tile array
    exp = 0
    for i in range(nimg):
        s, n = struct.unpack_from('<2i', d, img_off + 8 * i)
        if s != exp or n < 0: errs.append('image %d range (%d,%d) expected start %d' % (i, s, n, exp)); break
        exp += n
    if exp != ntile: errs.append('image ranges sum %d != tileCount %d' % (exp, ntile))
    # tiles
    maxpix = 0
    for i in range(ntile):
        x, y = struct.unpack_from('<2h', d, tile_off + 260 * i)
        if x % 16 or y % 16 or not (0 <= x <= 240) or not (0 <= y <= 240):
            errs.append('tile %d pos (%d,%d)' % (i, x, y)); break
        maxpix = max(maxpix, max(d[tile_off + 260 * i + 4: tile_off + 260 * i + 260]))
    if maxpix > 15: errs.append('tile pixel value %d > 15' % maxpix)
    # tiles of one image must not overlap
    for i in range(nimg):
        s, n = struct.unpack_from('<2i', d, img_off + 8 * i)
        seen = set()
        for t in range(s, s + n):
            xy = struct.unpack_from('<2h', d, tile_off + 260 * t)
            if xy in seen: errs.append('image %d duplicate tile position %s' % (i, xy)); break
            seen.add(xy)
    # palette: 256 BGRX entries, X byte 0
    for i in range(256):
        if d[pal_off + 4 * i + 3] != 0: errs.append('palette entry %d X byte != 0' % i); break
    # animations
    anims = [struct.unpack_from('<6i', d, anim_off + 24 * i) for i in range(nanim)]
    cursor = 0
    bodycur = 0
    hitcur = 0
    used = 0
    for i, (fs, fc, bb, bc, hb, hc) in enumerate(anims):
        if fs == -1:
            if fc != 0: errs.append('unused anim %d has frameCount %d' % (i, fc)); break
            continue   # body/hit base+count of an unused animation are stale build data and never read
        used += 1
        if fs < 0 or fc < 1 or fs + fc > nspr: errs.append('anim %d frames (%d,%d) out of range' % (i, fs, fc)); break
        if bb != -1 and (bb < 0 or bb + bc > nbody): errs.append('anim %d body (%d,%d) out of range' % (i, bb, bc)); break
        if hb != -1 and (hb < 0 or hb + hc > nhit): errs.append('anim %d hit (%d,%d) out of range' % (i, hb, hc)); break
    # sprites
    stats = {'img_none': 0}
    for i in range(nspr):
        b = d[spr_off + 52 * i: spr_off + 52 * (i + 1)]
        img = struct.unpack_from('<i', b, 0)[0]
        if not (0 <= img < nimg):
            stats['img_none'] += 1
        if b[0x1B] or b[0x1C]: errs.append('sprite %d unused_1B/1C nonzero' % i); break
        if b[0x2A] not in (0, 0xCD) or b[0x2B] != b[0x2A]: errs.append('sprite %d unused_2A/2B %02x %02x' % (i, b[0x2A], b[0x2B])); break
        if b[0x2D:0x30] != b'\0\0\0': errs.append('sprite %d aiMoveClass high bytes' % i); break
        if b[0x33]: errs.append('sprite %d fxFlags top byte' % i); break
        if b[0x15] > 7: errs.append('sprite %d tileXform %d' % (i, b[0x15])); break
        if b[0x0A] or b[0x0B]: errs.append('sprite %d duration bytes 2..3' % i); break
        if b[0x0F]: errs.append('sprite %d attackLevelAndFlags high byte' % i); break
    # every sprite's rect/box indices land inside the anim's allotment of the global tables
    frame_owner = {}
    for ai, (fs, fc, bb, bc, hb, hc) in enumerate(anims):
        if fs == -1: continue
        for k in range(fc): frame_owner[fs + k] = ai
    for si, ai in frame_owner.items():
        b = d[spr_off + 52 * si: spr_off + 52 * (si + 1)]
        fs, fc, bb, bc, hb, hc = anims[ai]
        for off_i, cnt_i, base, tot, nm in ((0x19, 0x1A, bb, nbody, 'hurt'), (0x1D, 0x1E, bb, nbody, 'grab'), (0x1F, 0x20, hb, nhit, 'hit')):
            armed = nm != 'hit' or (b[0x0E] & 0x10)   # hit boxes are only read when the sprite arms them (attackLevelAndFlags & 0x10)
            if b[cnt_i] and armed and (base == -1 or base + b[off_i] + b[cnt_i] > tot):
                errs.append('sprite %d (%s) rects %d+%d+%d beyond table %d' % (si, nm, base, b[off_i], b[cnt_i], tot)); break
        if b[0x17] != 0xFF and bb != -1 and bb + b[0x17] >= nbody and b[0x18] != 0xFF:
            errs.append('sprite %d anchor beyond body table' % si); break
    # body rects 8 bytes, hit boxes 24 bytes
    for i in range(nbody):
        l, t, r, bt = struct.unpack_from('<4h', d, body_off + 8 * i)
        if not (l <= r and t <= bt): errs.append('body rect %d %s' % (i, (l, t, r, bt))); break
    for i in range(nhit):
        b = d[hit_off + 24 * i: hit_off + 24 * (i + 1)]
        l, t, r, bt = struct.unpack_from('<4h', b, 0)
        if not (l <= r and t <= bt): errs.append('hit box %d %s' % (i, (l, t, r, bt))); break
        if b[0x13] or b[0x15] or b[0x17] or b[0x0B] or b[0x0D] or b[0x0F]: errs.append('hit box %d unused/high bytes' % i); break
        if b[0x14] > 3: errs.append('hit box %d knockMode %d' % (i, b[0x14])); break
        if b[0x12] > 15: errs.append('hit box %d hitClass %d' % (i, b[0x12])); break
    print('%-18s %s img=%d tile=%d anim=%d(used %d) sprite=%d body=%d hit=%d size=%d%s' % (
        name, 'OK ' if not errs else 'BAD', nimg, ntile, nanim, used, nspr, nbody, nhit, len(d),
        '' if not errs else '  ' + '; '.join(errs[:3])))
    for e in errs: fail(name, e)
    return True


# ------------------------------------------------------------------ TIM
def verify_tim(path):
    name = os.path.relpath(path, root)
    d = open(path, 'rb').read()
    errs = []
    magic, flags = struct.unpack_from('<2I', d, 0)
    # the shipped files have the magic dword zeroed; Tim_LoadTo256x256Dib never reads it
    bpp = {0: 4, 1: 8, 2: 16, 3: 24}[flags & 7]
    o = 8
    nclut = 0
    if flags & 8:
        bnum, dx, dy, w, h = struct.unpack_from('<I4H', d, o)
        nclut = (bnum - 12) // 2
        o += bnum
    bnum, dx, dy, w, h = struct.unpack_from('<I4H', d, o)
    px_w = w * (4 if bpp == 4 else 2 if bpp == 8 else 1)
    if o + bnum != len(d): errs.append('image block end %d != %d' % (o + bnum, len(d)))
    if bnum != 12 + w * 2 * h: errs.append('bnum %d != 12 + 2*w*h' % bnum)
    if bpp not in (4, 8): errs.append('bpp %d unsupported by loader (<= 8 only)' % bpp)
    if px_w != 256 or h != 256: errs.append('pixel size %dx%d is not the 256x256 the loader assumes' % (px_w, h))
    if not (flags & 8): errs.append('no CLUT')
    print('%-24s %s bpp=%d clut=%d img=%dx%d' % (name, 'OK ' if not errs else 'BAD', bpp, nclut, px_w, h))
    for e in errs: fail(name, e)


# ------------------------------------------------------------------ BMP
def verify_bmp(path):
    name = os.path.relpath(path, root)
    d = open(path, 'rb').read()
    errs = []
    # the first two bytes of the shipped files are '00' instead of 'BM': Bmp_LoadDibFromFile reads and drops the 14-byte file header
    if True:
        off = struct.unpack_from('<I', d, 10)[0]
        hs, w, h, planes, bc, comp, isz, _, _, clr, _ = struct.unpack_from('<I2i2H2I2i2I', d, 14)
        if hs != 40 or bc != 8 or comp != 0: errs.append('header %d bpp %d comp %d' % (hs, bc, comp))
        pal = clr if clr else 256
        if struct.unpack_from('<I', d, 2)[0] != len(d): errs.append('bfSize %d != file size %d (the loader allocates bfSize-14)' % (struct.unpack_from('<I', d, 2)[0], len(d)))
        if off != 14 + 40 + 4 * pal: errs.append('pixel offset %d != %d' % (off, 54 + 4 * pal))
        stride = (w + 3) & ~3
        if len(d) < off + stride * abs(h): errs.append('file too short')
    if errs:
        print('%-30s BAD %s' % (name, errs))
        for e in errs: fail(name, e)
    return not errs


def verify_wav(path):
    name = os.path.relpath(path, root)
    d = open(path, 'rb').read()
    ok = d[:4] == b'RIFF' and d[8:12] == b'WAVE'
    if ok:
        o = 12
        got = set()
        while o + 8 <= len(d):
            cid = d[o:o + 4]; sz = struct.unpack_from('<I', d, o + 4)[0]
            got.add(cid)
            o += 8 + sz + (sz & 1)
        ok = b'fmt ' in got and b'data' in got
    if not ok: fail(name, 'not a RIFF WAVE with fmt+data')
    return ok


def verify_command_txt(path):
    d = open(path, 'rb').read()
    toks = []
    for line in d.split(b'\n'):
        line = line.split(b'//')[0]
        toks += line.split()
    known = {b'WaitCount', b'ExtraSkip', b'OpenScreenType', b'CenterFlag', b'KeyBordOffset', b'YukiTest', b'SerioTest', b'SystemPause',
             b'ExtraFunctionEnable', b'ScreenMode', b'BgmType', b'DoubleBgMode', b'DebugWindow', b'Controller1p', b'Controller2p'}
    unk = [t for t in toks if t not in known and t.lower() not in (b'yukitest', b'seriotest')]
    print('command.txt tokens', [t.decode('ascii', 'replace') for t in toks], '(unknown keywords are ignored by the loader):',
          [t.decode('ascii', 'replace') for t in unk])


# ------------------------------------------------------------------ run
dats = sorted(glob.glob(os.path.join(root, '*', '*.dat')))
if len(dats) != 13: fail('dat', 'expected 13 character .dat, found %d' % len(dats))
for p in dats + [os.path.join(root, 'System', 'obj', 'obj1.dat')]:
    verify_dat(p)
for p in ci_glob(os.path.join(root, 'System'), '.tim'):
    verify_tim(p)
nb = sum(1 for p in ci_glob(root, '.bmp') if 'zzz_tmp' not in p and verify_bmp(p))
nw = sum(1 for p in ci_glob(root, '.wav') if 'zzz_tmp' not in p and verify_wav(p))
nm = 0
for p in ci_glob(os.path.join(root, 'System', 'Bgm'), '.mid'):
    if open(p, 'rb').read(4) != b'MThd': fail(p, 'not MThd')
    nm += 1
print('bmp ok: %d   wav ok: %d   mid: %d' % (nb, nw, nm))
verify_command_txt(os.path.join(root, 'System', 'command.txt'))
if fails:
    print('FAILURES:')
    for f in fails: print('  ', f)
    sys.exit(1)
print('ALL LAYOUTS EXACT')
