#!/usr/bin/env python3
"""Offline check of docs/formats/ida/gof1_types.h: recomputes every struct member offset (natural alignment 1 for chars, 2 for shorts, 4 otherwise; the file
uses explicit padding so no padding is ever inserted) and compares it with the `// +0xOFFSET` comment. Prints the struct sizes. Exit 1 on any mismatch."""
import re, sys

path = sys.argv[1] if len(sys.argv) > 1 else "/mnt/c/dev/hantei-chan/docs/formats/ida/gof1_types.h"
text = open(path, encoding="utf-8").read()
enums = {m.group(1): {"unsigned char": 1, "unsigned short": 2, "unsigned int": 4}[m.group(2)]
         for m in re.finditer(r"^enum (\w+) : (unsigned \w+) \{", text, re.M)}
prim = {"char": 1, "unsigned char": 1, "__int16": 2, "unsigned __int16": 2, "int": 4, "unsigned int": 4, "void *": 4}
sizes, bad = {}, 0
cur = None
for line in text.splitlines():
    m = re.match(r"struct (\w+) \{", line)
    if m:
        cur, off = m.group(1), 0
        continue
    if line.startswith("};") and cur:
        sizes[cur] = off
        cur = None
        continue
    if not cur:
        continue
    m = re.match(r"\s*((?:enum |struct )?[\w ]+?\s*\*?)\s*(\w+)((?:\[(?:0x[0-9A-Fa-f]+|\d+)\])*);\s*//\s*\+0x([0-9A-Fa-f]+)", line)
    if not m:
        print("UNPARSED", cur, line[:70])
        bad += 1
        continue
    ty, name, dims, want = m.group(1).strip(), m.group(2), m.group(3), int(m.group(4), 16)
    ty = ty.replace("enum ", "").replace("struct ", "").strip()
    if "*" in ty:
        sz = 4
    elif ty in enums:
        sz = enums[ty]
    elif ty in prim:
        sz = prim[ty]
    elif ty in sizes:
        sz = sizes[ty]
    else:
        print("UNKNOWN TYPE", ty)
        bad += 1
        continue
    n = 1
    for d in re.findall(r"\[(0x[0-9A-Fa-f]+|\d+)\]", dims):
        n *= int(d, 0)
    if want != off:
        print("MISMATCH %s.%s: comment +0x%X, computed +0x%X" % (cur, name, want, off))
        bad += 1
    off += sz * n
for k, v in sizes.items():
    print("%-24s %#x (%d)" % (k, v, v))
sys.exit(1 if bad else 0)
