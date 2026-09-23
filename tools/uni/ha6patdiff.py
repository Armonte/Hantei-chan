#!/usr/bin/env python3
"""List the patterns whose bytes differ between two HA6 files.
Usage: ha6patdiff.py A.ha6 B.ha6  -> prints count and ids (exit 0)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ha6walk import walk

def blocks(path):
    buf = open(path, 'rb').read()
    out = {}; cur = None; start = 0
    for pid, tag, w, o in walk(buf):
        if tag == 'PSTR': cur = pid; start = o
        elif tag == 'PEND' and cur is not None: out[cur] = buf[start:o + 4]; cur = None
    return out

a, b = blocks(sys.argv[1]), blocks(sys.argv[2])
diff = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
print(len(diff), 'pattern(s) differ:', ' '.join(map(str, diff[:40])) + (' ...' if len(diff) > 40 else ''))
