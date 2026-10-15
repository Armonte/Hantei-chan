#!/usr/bin/env python3
"""Cross-check script usage of native tables (Table.Member) against the generated API.
usage: sqrat_api_xcheck.py <game>_api.json <gamedata/game dir>  -> prints coverage and misses."""
import json, re, glob, sys, collections
api = json.load(open(sys.argv[1], encoding='utf-8'))
names = collections.defaultdict(set)
for e in api:
    names[e['table']].add(e['name'])
tables = {t for t in names if not t.startswith('(')}
use = collections.Counter()
script_defined = set()
for f in glob.glob(sys.argv[2] + '/**/*.txt', recursive=True):
    t = open(f, 'rb').read().decode('cp932', 'replace')
    for m in re.finditer(r'\b([A-Za-z_]\w*)\.([A-Za-z_]\w*)\s*<-', t):
        script_defined.add((m.group(1), m.group(2)))
    for m in re.finditer(r'\b([A-Za-z_]\w*)\.([A-Za-z_]\w*)', t):
        if m.group(1) in tables:
            use[(m.group(1), m.group(2))] += 1
miss = {k: c for k, c in use.items() if k[1] not in names[k[0]] and k not in script_defined}
print('native tables referenced:', len({t for t, _ in use}), 'distinct members:', len(use),
      'resolved:', len(use) - len(miss), 'unresolved:', len(miss))
for (t, n), c in sorted(miss.items(), key=lambda x: -x[1])[:40]:
    print('  %s.%s x%d' % (t, n, c))
