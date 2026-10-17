#!/usr/bin/env python3
"""List the (pattern, frame) pairs of RBO / GOF2 characters whose PAT pose has rotated or scaled parts.
Used to pick frames for the part-transform before/after screenshots (docs/formats/fb_part_transform.md).
usage: find_rotated_poses.py [rbo|gof2] [name-substring]"""
import struct, sys, os
sys.path.insert(0, os.path.dirname(__file__))
from han2lib import han2_files, split

def poses(pat, n):
    offs = struct.unpack_from(f'<{n}I', pat, 24)
    img = struct.unpack_from('<I', pat, 24 + 4 * n + 32 * n)[0]
    base = 24 + 36 * n + 4
    for p, o in enumerate(offs):
        if not o: continue
        parts = []
        for k in range(40):
            r = pat[o + 92 * k: o + 92 * k + 92]
            if len(r) < 92: break
            if struct.unpack_from('<I', r, 0x34)[0] == 0: continue
            rot = struct.unpack_from('<i', r, 0x1C)[0]; sx, sy = struct.unpack_from('<ii', r, 0x14)
            ox, oy = struct.unpack_from('<ii', r, 0x40)
            parts.append((rot, sx, sy, ox, oy, r[0x3C]))
        yield p, parts

game = sys.argv[1] if len(sys.argv) > 1 else 'rbo'
sub = sys.argv[2].upper() if len(sys.argv) > 2 else ''
for arc, name, b in han2_files(game, ('.DAT',) if game == 'rbo' else ('.DT2',)):
    if sub not in name.upper(): continue
    d = split(b)
    if game == 'rbo':
        pat_off, pat_size = struct.unpack_from('<II', b, 0x28)
        pat = b[pat_off:pat_off + pat_size]; n = 1000
        base = 0x40
    else:
        continue   # GOF2 poses live in <C>00.PAT, not the .DT2
    if len(pat) < 36028: continue
    pose = {p: parts for p, parts in poses(pat, n)}
    # frame -> pattern via the pattern table (section 0)
    pt = d['secs'][0]
    for pi in range(len(pt) // 12):
        cnt, flags, first = struct.unpack_from('<III', pt, 12 * pi)
        for fi in range(cnt):
            fr = d['frames'][first + fi]
            spr = struct.unpack_from('<h', fr, 0)[0]
            if spr in pose:
                rp = [x for x in pose[spr] if x[0] % 10000 != 0]
                if len(rp) >= 2:
                    print(f"{arc}/{name} pattern {pi} frame {fi} pose {spr}: {len(pose[spr])} parts, {len(rp)} rotated, rot={[x[0] for x in rp][:6]}")
