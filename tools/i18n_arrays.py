#!/usr/bin/env python3
"""List English string-table entries (const char* arrays) that have no row in src/i18n_ja_*.inc.
  python3 tools/i18n_arrays.py [--json out.json]   (string-aware: arrays whose items contain ';' or '}' are handled)"""
import re, glob, ast, sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import i18n_check as C
SKIP = {'kMbaaccEnNames', 'kLayer', 'kAF', 'kAS', 'kAT'}   # game data / HA6 record tags, not UI text
def arrays(s):
    for m in re.finditer(r'const\s+char\s*\*\s*(?:const\s*)?(\w+)\s*\[\s*\w*\s*\]\s*=\s*\{', s):
        i = m.end(); depth = 1; instr = False; j = i
        while j < len(s) and depth:
            c = s[j]
            if instr:
                if c == '\\': j += 1
                elif c == '"': instr = False
            elif c == '"': instr = True
            elif c == '{': depth += 1
            elif c == '}': depth -= 1
            j += 1
        yield m.group(1), s[i:j - 1]
def main():
    rows, _ = C.load_table(); out = []; where = {}
    for f in sorted(glob.glob(os.path.join(C.ROOT, 'src/**/*'), recursive=True)):
        if not f.endswith(('.cpp', '.h')) or os.path.basename(f).startswith('i18n_'): continue
        s = open(f, encoding='utf-8', errors='replace').read()
        for name, body in arrays(s):
            if name in SKIP: continue
            for l in re.findall(C.LIT, body):
                x = C.unescape(l[1:-1])
                if x not in rows and re.search(r'[A-Za-z]{2}', x) and x not in where:
                    where[x] = (os.path.relpath(f, C.ROOT), name); out.append(x)
    for x in out: print('%-14s %s' % (where[x][1][:14], x.replace('\n', '\\n')))
    if '--json' in sys.argv: json.dump(out, open(sys.argv[sys.argv.index('--json') + 1], 'w'), ensure_ascii=False)
    print(len(out), 'untranslated array entries')
main()
