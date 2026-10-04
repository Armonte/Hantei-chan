#!/usr/bin/env python3
"""Tag-level diff of two HA6 files: prints the first N differing tag events
with context (pattern, tag, payload). Usage: ha6diff.py A.ha6 B.ha6 [N]"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ha6walk import walk
a = list(walk(open(sys.argv[1], 'rb').read()))
b = list(walk(open(sys.argv[2], 'rb').read()))
n = int(sys.argv[3]) if len(sys.argv) > 3 else 3
import difflib
def key(e): return (e[0], e[1], tuple(e[2]) if not isinstance(e[2], tuple) else e[2])
sm = difflib.SequenceMatcher(None, [key(e) for e in a], [key(e) for e in b], autojunk=False)
shown = 0
for op, i1, i2, j1, j2 in sm.get_opcodes():
    if op == 'equal': continue
    print('---', op, 'pattern', a[i1][0] if i1 < len(a) else '?')
    ctx = max(0, i1 - 6)
    print('  ctx:', ' '.join(e[1] for e in a[ctx:i1]))
    print('  A:', [(e[1], e[2]) for e in a[i1:i2]][:12])
    print('  B:', [(e[1], e[2]) for e in b[j1:j2]][:12])
    shown += 1
    if shown >= n: break
print('total diff blocks:', sum(1 for o in sm.get_opcodes() if o[0] != 'equal'))
