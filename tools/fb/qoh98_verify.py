#!/usr/bin/env python3
"""Verify the Queen of Heart '98 character .dat container layout on all 13 files.
Layout from LoadAndParseCharacterDATFile (0x44C290). Usage: qoh98_verify.py [game_dir]"""
import struct, sys, glob, os
root = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/games/qoh98'
bad = 0
for f in sorted(glob.glob(os.path.join(root, '*', '*.dat'))):
    d = open(f, 'rb').read()
    ver, nimg, ntile, nrect, nspr, nanim, nhurt = struct.unpack_from('<7I', d, 0)
    o = 28
    paths = [d[o + i*260:o + (i+1)*260] for i in range(nimg)]; o += 260*nimg
    imgs = [struct.unpack_from('<2I', d, o + 8*i) for i in range(nimg)]; o += 8*nimg
    tiles_off = o; o += 260*ntile
    rects_off = o; o += 24*nrect
    spr_off = o; o += 52*nspr
    pal_off = o; o += 1024
    anim_off = o; o += 8*nanim
    hurt_off = o; o += 24*nhurt
    errs = []
    if o != len(d): errs.append('size %d != %d' % (o, len(d)))
    if ver != 5: errs.append('version %d' % ver)
    # image table: (first_tile, tile_count) partitions the tile array in order
    exp = 0
    for i, (s, n) in enumerate(imgs):
        if s != exp: errs.append('img %d start %d != %d' % (i, s, exp)); break
        exp += n
    if exp != ntile: errs.append('img tiles sum %d != %d' % (exp, ntile))
    # tile header = u16 x, u16 y (16x16 8bpp block follows)
    for i in range(ntile):
        x, y = struct.unpack_from('<2H', d, tiles_off + 260*i)
        if x % 16 or y % 16 or x > 256 or y > 256:
            errs.append('tile %d pos %d,%d' % (i, x, y)); break
    for i, p in enumerate(paths):
        if not p[:8].split(b'\0')[0]: errs.append('empty path name %d' % i); break
    print('%-8s %s counts img=%d tile=%d rect=%d spr=%d anim=%d hurt=%d size=%d %s' % (
        os.path.basename(f), 'OK ' if not errs else 'BAD', nimg, ntile, nrect, nspr, nanim, nhurt, len(d), errs[:3]))
    bad += bool(errs)
sys.exit(1 if bad else 0)
