#!/usr/bin/env python3
"""Compare roster_v2 vs roster_v3 converted .pat files: strip the empty VEST block
(VEST, u32 0, u32 0x10, VNST, VEED = 20 bytes) from v2 and test byte equality with v3.
Usage: v2_v3_patdiff.py [gamedata/mbac_to_cc dir]"""
import glob, os, sys
root = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/dev/hantei-chan/gamedata/mbac_to_cc'
VEST = b'VEST\x00\x00\x00\x00\x10\x00\x00\x00VNSTVEED'
for a in sorted(glob.glob(f'{root}/roster_v2/**/*.pat', recursive=True)):
    b = a.replace('roster_v2', 'roster_v3')
    A, B = open(a, 'rb').read(), open(b, 'rb').read()
    S = A.replace(VEST, b'')
    rest = '' if S == B else f' first_rest_diff=0x{next(i for i in range(min(len(S),len(B))) if S[i]!=B[i]):x}'
    print(f'{os.path.basename(a):14} v2={len(A)} v3={len(B)} vest@0x{A.find(VEST):x} v2_minus_vest==v3:{S == B}{rest}')
