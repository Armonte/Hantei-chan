#!/usr/bin/env python3
"""Round 8 (render-match8): list the props of one part set of a .pat, or write a copy in which every prop of that
set but one is moved off-screen (PRXY 30000,30000) (length-preserving byte patch, so the game and the editor read the same file).
Usage: pat_solo_prop.py IN.pat SET              list slots (PRXY, PRZM, PRA3, PRRV, PRCL, PRPR, PRID)
       pat_solo_prop.py IN.pat SET KEEP OUT.pat  move every slot except KEEP off-screen"""
import struct, sys
sys.path.insert(0, __import__('os').path.dirname(__file__) + '/..')
from patwalk import SUB, u32

def scan(b, want):
    p = 0x24; slots = []
    while p < len(b):
        t = b[p:p+4]; p += 4
        if t == b'_END': break
        if t != b'P_ST': raise ValueError('top tag %r at 0x%x' % (t, p - 4))
        rid = u32(b, p); p += 4
        while True:
            st = b[p:p+4]; p += 4
            if st == b'P_ED': break
            if st == b'PANM': p += 32
            elif st == b'PANA': p += 1 + b[p]
            elif st == b'PRST':
                slot = u32(b, p); p += 4; rec = {'slot': slot}
                while True:
                    pt = b[p:p+4].decode('latin1'); p += 4
                    if pt == 'PRED': break
                    n = SUB[pt]; rec[pt] = (p, b[p:p+n]); p += n
                if rid == want: slots.append(rec)
            else: raise ValueError('P_ST tag %r at 0x%x' % (st, p - 4))
        if rid == want: return slots
    return slots

def main():
    """KEEP (a slot or a comma list) mode moves every other slot of the set off-screen (PRXY = 30000,30000: in place, PRCL is absent in set 45)."""
    b = bytearray(open(sys.argv[1], 'rb').read()); want = int(sys.argv[2])
    slots = scan(b, want)
    for r in slots:
        d = {k: v[1] for k, v in r.items() if k != 'slot'}
        f = lambda k, fmt: struct.unpack('<' + fmt, d[k]) if k in d else None
        print(r['slot'], 'XY', f('PRXY', 'ii'), 'ZM', f('PRZM', 'ff'), 'A3', f('PRA3', 'ffff'), 'AN', f('PRAN', 'f'),
              'AS', f('PRAS', 'ii'), 'RV', f('PRRV', 'B'), 'FL', f('PRFL', 'B'), 'AL', f('PRAL', 'B'),
              'CL', d['PRCL'].hex() if 'PRCL' in d else None, 'PR', f('PRPR', 'i'), 'ID', f('PRID', 'i'))
    if len(sys.argv) > 4:
        keep = [int(x) for x in sys.argv[3].split(',')]   # one slot or a comma list
        for r in slots:
            if r['slot'] in keep: continue
            if 'PRXY' not in r: raise SystemExit('slot %d has no PRXY (cannot move in place)' % r['slot'])
            struct.pack_into('<ii', b, r['PRXY'][0], 30000, 30000)
        open(sys.argv[4], 'wb').write(b); print('wrote', sys.argv[4], 'keep slot', keep)

main()
