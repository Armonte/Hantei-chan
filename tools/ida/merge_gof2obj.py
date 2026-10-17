#!/usr/bin/env python3
"""Merge the three GOF2 Obj part files (docs/formats/ida/gof2obj_part{A,B,C}_*) into one struct Gof2Obj decl (gof2_obj_types.h).
Reconciliation: part C's first row (3136, 888 B) is the tail of part B's hitHistoryByAttackerPattern (1976..4023) and is dropped.
Checks: contiguous, no overlap, total 0x1264 bytes."""
import os, re, sys
D = os.path.join(os.path.dirname(__file__), '..', '..', 'docs', 'formats', 'ida')
def load(p):
    ns = {}; exec(open(os.path.join(D, f'gof2obj_part{p}_fields.py')).read(), ns); return ns['FIELDS']
rows = []
for p in 'ABC':
    for r in load(p):
        if p == 'C' and r[0] == 3136: continue
        rows.append(r)
rows.sort(key=lambda r: r[0])
pos = 0
for off, size, ty, name, cm in rows:
    if off != pos: sys.exit(f'GAP/OVERLAP at {off} expected {pos} ({name})')
    pos = off + size
if pos != 0x1264: sys.exit(f'total {pos:#x} != 0x1264')
out = ['// GOF2 Obj (pool slot, 0x1264 bytes) merged from parts A/B/C by tools/ida/merge_gof2obj.py. Parse after the part type headers.']
for off, size, ty, name, cm in rows:
    m = re.match(r'(.*?)(\[.*\])$', name)
    decl = f'{ty} {m.group(1)}{m.group(2)}' if m else f'{ty} {name}'
    cm = re.sub(r'\s+', ' ', cm).strip()
    out.append(f' {decl}; // +{off:#x} {cm}')
body = '\n'.join(out[1:])
hdr = out[0]
open(os.path.join(D, 'gof2_obj_types.h'), 'w').write(f'{hdr}\nstruct Gof2Obj {{\n{body}\n}};\n')
print(len(rows), 'rows, 0x1264 bytes OK')
