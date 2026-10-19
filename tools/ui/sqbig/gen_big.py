#!/usr/bin/env python3
"""Make VectorTable-shaped Squirrel files of N lines (CP932, CRLF, tabs) by cycling the stock file's body.
gen_big.py <stock VectorTable.txt> <out dir> N [N ...]  ->  <out dir>/big_<N>.txt"""
import sys, os
src, out = sys.argv[1], sys.argv[2]
lines = open(src, 'rb').read().split(b'\r\n')
body = lines[2:]
os.makedirs(out, exist_ok=True)
for n in map(int, sys.argv[3:]):
    res = lines[:2]
    i = 0
    while len(res) < n:
        res.append(body[i % len(body)]); i += 1
    data = b'\r\n'.join(res[:n])
    open(os.path.join(out, f'big_{n}.txt'), 'wb').write(data)
    print(n, len(data))
