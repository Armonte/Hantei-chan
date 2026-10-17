#!/usr/bin/env python3
"""Check that Gof2ObjYarareState (IDB-only Yarare hit-memory view) overlays Gof2Obj bytes 1776..4707 exactly.

Every Gof2Obj field with 1776 <= offset <= 4707 must appear in Gof2ObjYarareState at offset-1776 with the same
total size and the same name (NAME_MAP lists documented renames, currently none). Also checks the reverse direction,
the struct size (0xB74) and that no Gof2Obj field straddles the range end.
Reads the reflection tables of the generated header (run gen_cpp_types.py first).

usage: check_yarare_view.py [src/han2/gof2_obj_gen.h]     exit 1 on any difference
"""
import re, sys
BASE, END = 1776, 4707
SIZE = 0xB74
NAME_MAP = {}   # Gof2Obj field name -> Yarare field name, only for documented differences

path = sys.argv[1] if len(sys.argv) > 1 else 'src/han2/gof2_obj_gen.h'
text = open(path, encoding='utf-8').read()

def table(struct):
    m = re.search(r'k%sFields\[\] = \{\n(.*?)\n\};' % struct, text, re.S)
    if not m: raise SystemExit('no reflection table for ' + struct)
    rows = {}
    for l in m.group(1).split('\n'):
        mm = re.match(r'\s*\{"(\w+)", 0x([0-9A-F]+), (\d+), (\d+),', l)
        rows[int(mm.group(2), 16)] = (mm.group(1), int(mm.group(3)) * int(mm.group(4)))
    return rows

obj, yar = table('Gof2Obj'), table('Gof2ObjYarareState')
bad = 0
def rep(msg):
    global bad; bad += 1; print('MISMATCH', msg)

m = re.search(r'sizeof\(Gof2ObjYarareState\) == 0x([0-9A-F]+)', text)
if not m or int(m.group(1), 16) != SIZE: rep('Gof2ObjYarareState size != 0x%X' % SIZE)
n = 0
for off, (name, sz) in sorted(obj.items()):
    if off < BASE or off > END: 
        if off < BASE and off + sz > BASE: rep('Gof2Obj.%s straddles the range start' % name)
        continue
    if off + sz - 1 > END: rep('Gof2Obj.%s straddles the range end' % name)
    n += 1
    y = yar.get(off - BASE)
    if not y: rep('Gof2Obj.%s @%d: no Yarare field at +0x%X' % (name, off, off - BASE)); continue
    if y[1] != sz: rep('%s @%d: size obj %d vs yarare %d' % (name, off, sz, y[1]))
    if y[0] != NAME_MAP.get(name, name): rep('@%d: name obj %s vs yarare %s' % (off, name, y[0]))
for off, (name, sz) in yar.items():
    if (off + BASE) not in obj: rep('Yarare.%s has no Gof2Obj field at %d' % (name, off + BASE))
print('checked %d Gof2Obj fields in %d..%d against %d Yarare fields: %s' % (n, BASE, END, len(yar), 'OK' if not bad else '%d mismatches' % bad))
sys.exit(1 if bad else 0)
