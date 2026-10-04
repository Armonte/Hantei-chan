#!/usr/bin/env python3
"""Count Gof2Obj bytes per evidence class (traced / inferred / unused) for parts A, B, C and the whole 0x1264-byte struct.

Sources: docs/formats/ida/gof2obj_part{A,B,C}_fields.py (top-level rows) + the type headers (nested struct / union members).
Every row/member is expanded to leaves; a leaf's class is the FIRST evidence tag of its comment:
  [traced] / T, [inferred] / I, [unused] / U  (`[traced writer only]` / `[traced reads only]` count as traced; or a name starting unused_ / spare when untagged).
An untagged nested member inherits the tag of its parent row; a weaker parent tag (inferred/unused) caps its members. Union members overlap: a byte takes the BEST class
(traced > inferred > unused) over the union members covering it. Part ranges: A 0..1567, B 1568..3135, C 3136..4707.
(part C's 3136 row is the tail of part B's hitHistory row and is dropped, as in merge_gof2obj.py.)

usage: count_gof2obj_evidence.py [--list]      --list prints every inferred / unused leaf
"""
import os, re, sys
D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'formats', 'ida')
HDRS = ['gof2_container_types.h', 'gof2_frame_types.h', 'gof2_at_sections_types.h', 'gof2obj_partA_types.h',
        'gof2obj_partB_types.h', 'gof2obj_partC_types.h', 'gof2_yarare_types.h']
PRIM = {'char': 1, 'unsigned char': 1, 'signed char': 1, '__int8': 1, 'unsigned __int8': 1, '__int16': 2,
        'unsigned __int16': 2, 'short': 2, 'unsigned short': 2, 'int': 4, 'unsigned int': 4, '__int32': 4,
        'unsigned __int32': 4, 'float': 4, '__int64': 8, 'unsigned __int64': 8, 'int8_t': 1, 'uint8_t': 1,
        'int16_t': 2, 'uint16_t': 2, 'int32_t': 4, 'uint32_t': 4}
RANK = {'traced': 3, 'inferred': 2, 'unused': 1}
TAGRE = re.compile(r'(?:\+0x[0-9A-Fa-f]+\s+)?(?:\[(traced|inferred|unused)(?: [a-z ]+)?\]|\b([TIU])\b)')
TAGMAP = {'T': 'traced', 'I': 'inferred', 'U': 'unused'}

def tag_of(cm, name=''):
    m = TAGRE.match(cm.strip())
    if m: return m.group(1) or TAGMAP[m.group(2)]
    m = re.search(r'\[(traced|inferred|unused)(?: [a-z ]+)?\]', cm)
    if m: return m.group(1)
    return None

text = ''
for h in HDRS:
    text += open(os.path.join(D, h), encoding='utf-8').read() + '\n'
text = re.sub(r'^\s*struct\s+\w+\s*;\s*$', '', text, flags=re.M)
enums = {m.group(1): m.group(2).strip() for m in re.finditer(r'enum\s+(\w+)\s*:\s*([\w ]+?)\s*\{', text)}
fnptr = set(re.findall(r'typedef\s+[^;]*?\(\s*(?:__\w+\s+)?\*\s*(\w+)\s*\)\s*\([^;]*\)\s*;', text))
text = re.sub(r'typedef\s+[^;]*?\([^;]*\)\s*;', '', text)
TYPES = {}   # name -> (isunion, size, members[(ty, name, count, off, comment)])
def tsize(ty):
    if ty in PRIM: return PRIM[ty]
    if ty in enums: return PRIM[enums[ty]]
    return TYPES[ty][1]
for m in re.finditer(r'(struct|union)\s+(\w+)\s*\{(.*?)\n\};', text, re.S):
    isu = m.group(1) == 'union'; mem = []; off = 0; usz = 0
    for line in m.group(3).split('\n'):
        line = line.strip()
        if not line or line.startswith('//'): continue
        mm = re.match(r'(.+?)\s+(\*?\w+)((?:\[\w+\])*);\s*(?://\s*(.*))?$', line)
        if not mm: raise SystemExit('cannot parse: ' + line)
        ty, fn, dims, cm = mm.group(1).strip(), mm.group(2), mm.group(3), mm.group(4) or ''
        n = 1
        for d in re.findall(r'\[(\w+)\]', dims): n *= int(d, 0)
        ty = re.sub(r'^(struct|union|enum)\s+', '', ty)
        ptr = fn.startswith('*') or ty.endswith('*') or ty in fnptr
        sz = 4 if ptr else tsize(ty)
        mem.append((None if ptr else ty, fn.lstrip('*'), n, 0 if isu else off, cm, sz))
        if isu: usz = max(usz, sz * n)
        else: off += sz * n
    TYPES[m.group(2)] = (isu, usz if isu else off, mem)

def fields(p):
    ns = {}; exec(open(os.path.join(D, f'gof2obj_part{p}_fields.py')).read(), ns); return ns['FIELDS']

def expand(base, path, ty, n, sz, inherit, out):
    """append leaves (path, off, size, cls) for an object/array at base"""
    if ty in TYPES:
        isu, tsz, mem = TYPES[ty]
        for i in range(n):
            b = base + i * tsz
            p = path + (f'[{i}]' if n > 1 else '')
            for (mty, mn, mc, moff, cm, msz) in mem:
                cls = tag_of(cm) or inherit
                if inherit and cls and RANK[inherit] < RANK[cls]: cls = inherit   # a container tagged inferred/unused weakens its members
                if cls is None and (mn.startswith('unused_') or mn in ('spare',)): cls = 'unused'
                expand(b + moff, p + '.' + mn, mty, mc, msz, cls, out)
    else:
        out.append((path, base, sz * n, inherit))

rows = []
for p in 'ABC':
    for r in fields(p):
        if p == 'C' and r[0] == 3136: continue
        rows.append(r)
rows.sort(key=lambda r: r[0])
leaves = []
for off, size, ty, name, cm in rows:
    m = re.match(r'(.*?)\[(\d+)\]$', name)
    nm, cnt = (m.group(1), int(m.group(2))) if m else (name, 1)
    ty = re.sub(r'^(struct|union|enum)\s+', '', ty).strip()
    cls = tag_of(cm)
    if cls is None and nm.startswith('unused_'): cls = 'unused'
    if ty.endswith('*') or ty in fnptr: leaves.append((nm, off, size, cls)); continue
    if ty in TYPES:
        expand(off, nm, ty, cnt, tsize(ty), cls, leaves)
    else:
        leaves.append((nm, off, size, cls))
assert all(l[3] for l in leaves), [l for l in leaves if not l[3]]
cls_at = [None] * 0x1264
for path, off, sz, cls in leaves:
    for b in range(off, off + sz):
        if cls_at[b] is None or RANK[cls] > RANK[cls_at[b]]: cls_at[b] = cls
assert all(cls_at), 'uncovered bytes'
def count(lo, hi):
    c = {'traced': 0, 'inferred': 0, 'unused': 0}
    for b in range(lo, hi): c[cls_at[b]] += 1
    return c
print('%-6s %8s %9s %7s %6s' % ('part', 'traced', 'inferred', 'unused', 'total'))
for nm, lo, hi in (('A', 0, 1568), ('B', 1568, 3136), ('C', 3136, 0x1264), ('ALL', 0, 0x1264)):
    c = count(lo, hi)
    print('%-6s %8d %9d %7d %6d' % (nm, c['traced'], c['inferred'], c['unused'], hi - lo))
ni = sum(1 for l in leaves if l[3] == 'inferred'); nu = sum(1 for l in leaves if l[3] == 'unused')
print('leaves: %d total, %d inferred, %d unused' % (len(leaves), ni, nu))
if '--list' in sys.argv:
    for cl in ('inferred', 'unused'):
        print('--', cl)
        for path, off, sz, c in leaves:
            if c == cl: print('  +%#x (%d) %d B  %s%s' % (off, off, sz, path, '' if cls_at[off] == cl else '   [bytes covered by better-evidenced union member]'))
