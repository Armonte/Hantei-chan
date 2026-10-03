#!/usr/bin/env python3
"""Verify the GOF2 .DT2 invariants documented in docs/formats/ida/gof2_frame.md on every GOF2 .DT2.
Needs han2lib (rbo-support worktree: tools/han2/han2lib.py) on --han2lib. Usage: check_gof2_invariants.py [--han2lib DIR]"""
import argparse, struct, sys
from collections import Counter

ap = argparse.ArgumentParser()
ap.add_argument('--han2lib', default='/home/teo/dev/hantei-chan-wt/rbo-support/tools/han2')
args = ap.parse_args()
sys.path.insert(0, args.han2lib)
import han2lib  # noqa: E402

I = lambda f, o: struct.unpack_from('<i', f, o)[0]
# the 24 box slots the engine reads, ascending frame offset (Gof2FrameRecord)
BOX_SLOTS = [0x110] + list(range(0x118, 0x130, 4)) + [0x140, 0x144, 0x148, 0x150, 0x154, 0x158, 0x160, 0x164, 0x168] + list(range(0x170, 0x190, 4))
GROUPS = {'kasanari': (0x10C, [0x110]), 'hurt': (0x114, list(range(0x118, 0x130, 4))), 'etc': (0x13C, [0x140, 0x144, 0x148]),
          'sousai': (0x14C, [0x150, 0x154, 0x158]), 'tobi': (0x15C, [0x160, 0x164, 0x168]), 'attack': (0x16C, list(range(0x170, 0x190, 4)))}


def first_use(frames, off, base=0):
    seen = 0
    for f in frames:
        v = I(f, off)
        if v <= base or v <= seen:
            continue
        if v != seen + 1:
            return None
        seen += 1
    return seen


res = Counter()
files = list(han2lib.han2_files('gof'))
for arc, name, b in files:
    s = han2lib.split(b)
    fr = s['frames']
    res['files'] += 1
    res['frames'] += len(fr)
    lead = struct.unpack_from('<12I', b, 0x40)
    tail = struct.unpack_from('<3I', b, 0x40 + 21 * 4)
    res['lead_ok'] += lead == (0, 8, 0, 5, 0, 0, 0, 0, 1, 0, 0, 0)
    res['tail_zero'] += tail == (0, 0, 0)
    res['kind3_sub2'] += struct.unpack_from('<I', b, 8)[0] == 3 and struct.unpack_from('<I', b, 0x18)[0] == 2
    res['frames_mod404'] += len(s['secs'][1]) % 404 == 0
    ents = [struct.unpack_from('<III', s['secs'][0], 12 * i) for i in range(256)]
    nxt, ok = 0, True
    for c, fl, first in ents:
        res['pattern_flags_nonzero'] += fl != 0
        if c:
            ok &= first == nxt
            nxt += c
    res['pattern_packed'] += ok and nxt == len(fr)
    nbox = len(s['secs'][2]) // 8
    seen, ok = {}, True
    for f in fr:
        for off in BOX_SLOTS:
            v = I(f, off)
            if v == -1 or v in seen:
                continue
            if v == len(seen):
                seen[v] = 1
            else:
                ok = False
        # group counts
    res['box_first_use'] += ok and len(seen) == nbox
    for g, (co, sl) in GROUPS.items():
        res['count_eq_slots_' + g] += all(I(f, co) == sum(1 for o in sl if I(f, o) != -1) for f in fr)
    res['hasAttack_iff'] += all((I(f, 0xC4) == 1) == (I(f, 0x16C) > 0) == (I(f, 0xC8) >= 0) for f in fr)
    atk = [I(f, 0xC8) for f in fr if I(f, 0xC8) >= 0]
    res['attack_seq'] += atk == list(range(len(atk))) and len(s['secs'][3]) == 236 * len(atk)
    for nm, off, si, rs in (('listA', 0xBC, 6, 20), ('listB', 0xC0, 7, 20), ('effect', 0x190, 8, 96)):
        n = first_use(fr, off)
        res[nm + '_first_use_1based'] += n is not None and (n + 1) * rs == len(s['secs'][si]) and s['secs'][si][:rs] == bytes(rs)
    res['sec2_mod8'] += len(s['secs'][2]) % 8 == 0
    res['sec2_sec3_both_or_neither'] += (len(s['secs'][2]) == 0) == (len(s['secs'][3]) == 0)
for k, v in sorted(res.items()):
    print('%-32s %d' % (k, v))
print('box slots (ascending frame offsets):', ' '.join('0x%X' % o for o in BOX_SLOTS))
