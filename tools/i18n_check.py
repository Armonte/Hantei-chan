#!/usr/bin/env python3
"""i18n coverage check.
  python3 tools/i18n_check.py [--list] [files...]
Parses TXT("...")/LBL("...") keys (adjacent literals are concatenated; "Text##id" labels use the part before ##)
in the given files (default: the menu/main/right/box set) and verifies each has a row in src/i18n_ja_*.inc.
Warns about duplicate keys across all tables, then lists remaining ImGui::*/im::* calls with a bare English literal."""
import re, sys, glob, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def _all_sources():
    out = []
    for d, _, fs in os.walk(os.path.join(ROOT, "src")):
        for f in fs:
            if f.endswith((".cpp", ".h")) and not f.startswith("i18n_"):
                out.append(os.path.relpath(os.path.join(d, f), ROOT).replace("\\", "/"))
    return sorted(out)
DEFAULT = _all_sources()   # every UI source; pass explicit files to narrow it
LIT = r'"(?:[^"\\]|\\.)*"'
SEQ = r'(?:' + LIT + r'(?:\s*' + LIT + r')*)'
def unescape(s):
    out = []; i = 0
    s = s  # \x escapes are UTF-8 bytes: collect then decode below
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]; i += 2
            if n == 'n': out.append('\n')
            elif n == 't': out.append('\t')
            elif n == '"': out.append('"')
            elif n == '\\': out.append('\\')
            elif n == "'": out.append("'")
            elif n == 'x':
                j = i
                while j < len(s) and s[j] in '0123456789abcdefABCDEF': j += 1
                out.append('\x00' + chr(int(s[i:j], 16))); i = j
            else: out.append('\\' + n)
        else: out.append(c); i += 1
    r = ''.join(out)
    if '\x00' in r:
        b = bytearray(); k = 0
        while k < len(r):
            if r[k] == '\x00': b.append(ord(r[k+1])); k += 2
            else: b += r[k].encode('utf-8'); k += 1
        r = b.decode('utf-8', 'replace')
    return r
def joinlits(seq):
    return ''.join(unescape(m[1:-1]) for m in re.findall(LIT, seq))
def load_table():
    rows = {}; dups = []
    for f in sorted(glob.glob(os.path.join(ROOT, "src/i18n_ja_*.inc"))):
        txt = open(f, encoding='utf-8').read()
        for m in re.finditer(r'\{\s*(' + SEQ + r')\s*,\s*(' + SEQ + r')\s*\}', txt):
            k = joinlits(m.group(1))
            if k in rows: dups.append((k, rows[k][1], os.path.basename(f)))
            else: rows[k] = (joinlits(m.group(2)), os.path.basename(f))
    return rows, dups
def keys_of(path):
    s = open(os.path.join(ROOT, path), encoding='utf-8').read()
    ks = {}
    for m in re.finditer(r'\b(TXT|LBL|BitField|ShowFrameField|ShowPatternField|PickRow|Tooltip)\(\s*(' + SEQ + r')\s*[,)]', s):
        k = joinlits(m.group(2))
        if m.group(1) == 'LBL' and '##' in k: k = k.split('##')[0]
        if k.startswith('##'): continue   # id-only label (nothing visible to translate)
        ks.setdefault(k, s.count('\n', 0, m.start()) + 1)
    return ks
HELPERS = ('BitField', 'ShowFrameField', 'Tooltip', 'PickRow', 'HelpMarker', 'ShowFrameFieldInt', 'LabeledInt')
def bare_literals(path):
    out = []
    for n, line in enumerate(open(os.path.join(ROOT, path), encoding='utf-8'), 1):
        if re.match(r'\s*//', line): continue
        for m in re.finditer(r'(?:\b(?:ImGui|im)::|\b)(\w+)\(\s*(' + LIT + r')', line):
            lit = unescape(m.group(2)[1:-1]).split('##')[0]
            if m.group(1) in HELPERS: continue   # helpers translate internally; their literals are keys (keys_of)
            if not m.group(0).lstrip().startswith(('ImGui::','im::')): continue
            if m.group(1) in ('DockBuilderDockWindow', 'GetID', 'PushID', 'DragDropPayload', 'SetDragDropPayload', 'AcceptDragDropPayload', 'BeginChild', 'BeginTable', 'BeginTabBar', 'BeginPopup', 'OpenPopup', 'BeginPopupContextItem', 'BeginPopupContextWindow', 'InvisibleButton', 'InputScalarN', 'BeginPopupContext'): continue
            if re.search(r'[A-Za-z]{2}', lit) and not lit.startswith('%'): out.append((n, line.strip()[:110]))
    return out
def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    files = args or DEFAULT
    rows, dups = load_table()
    missing = 0; total = 0
    for f in files:
        ks = keys_of(f); total += len(ks)
        for k, ln in ks.items():
            if k not in rows:
                missing += 1; print("MISSING %s:%d  %r" % (f, ln, k))
        for n, l in bare_literals(f): print("BARE    %s:%d  %s" % (f, n, l))
    fmt = re.compile(r'%(?:[-+ #0]*)(?:\d+|\*)?(?:\.\d+)?(?:hh|h|ll|l|z|j|t|L)?[diouxXeEfFgGaAcspn]')
    for k, (v, f) in rows.items():   # a translated format string must keep the same specifiers in the same order (it is passed to printf)
        a, b = fmt.findall(k.replace('%%', '')), fmt.findall(v.replace('%%', ''))
        if a != b and not k.startswith('ATVD'): print("FORMAT  %s: %r -> %r" % (f, a, b)); missing += 1
    for k, a, b in dups: print("DUPLICATE key %r (in %s and %s)" % (k, a, b))
    print("files=%d unique keys=%d table rows=%d missing=%d duplicates=%d" % (len(files), total, len(rows), missing, len(dups)))
    if '--list' in sys.argv:
        for f in files:
            for k in keys_of(f): print(repr(k))
    return 1 if missing else 0
if __name__ == '__main__': sys.exit(main())
