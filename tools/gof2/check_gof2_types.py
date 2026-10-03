#!/usr/bin/env python3
"""Machine-check docs/formats/ida/gof2_frame_types.h and gof2_container_types.h:
every struct field line must carry `// +0xOFFSET` equal to the running offset; prints per-struct size and the
byte count per evidence class (T traced, I inferred, E editor-only, U unused, V validated)."""
import re, sys
from collections import Counter

PRIM = {'__int16': 2, 'unsigned __int16': 2, 'unsigned char': 1, 'unsigned int': 4, 'int': 4, 'char': 1}
STRUCT_SIZES = {}


def parse(path):
    lines = open(path, encoding='utf8').read().splitlines()
    enums = {}
    for l in lines:
        m = re.match(r'enum (\w+) : (unsigned \w+) \{', l)
        if m:
            enums[m.group(1)] = PRIM[m.group(2)]
    structs, cur = {}, None
    for l in lines:
        m = re.match(r'struct (\w+) \{', l)
        if m:
            cur = m.group(1)
            structs[cur] = []
            continue
        if l.startswith('};'):
            cur = None
            continue
        if cur and l.strip():
            m = re.match(r'\s+(.+?[\s\*])(\w+)(?:\[(\d+)\])?;\s*// \+0x([0-9A-Fa-f]+)(?:\.\.0x[0-9A-Fa-f]+)?(?: (?:([TIEUV]): )?|$)', l)
            if not m:
                print('UNPARSED', l)
                continue
            structs[cur].append(m.groups())
    return enums, structs


def main(paths):
    bad = 0
    for p in paths:
        enums, structs = parse(p)
        for name, fields in structs.items():
            off, cls = 0, Counter()
            for ty, fn, cnt, o, c in fields:
                ty = ty.strip()
                if '*' in ty or '(' in ty:
                    sz = 4
                elif ty in PRIM:
                    sz = PRIM[ty]
                elif ty in enums:
                    sz = enums[ty]
                elif ty in STRUCT_SIZES:
                    sz = STRUCT_SIZES[ty]
                elif ty.startswith('struct '):
                    sz = STRUCT_SIZES[ty.split()[1]]
                else:
                    print('unknown type', ty, fn)
                    sz = 4
                sz *= int(cnt) if cnt else 1
                if int(o, 16) != off:
                    print('OFFSET MISMATCH', name, fn, hex(off), '+0x' + o)
                    bad += 1
                cls[c or '-'] += sz
                off += sz
            STRUCT_SIZES[name] = off
            print('%-24s size 0x%X  %s' % (name, off, dict(cls)))
    print('mismatches:', bad)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(sys.argv[1:]) else 0)
