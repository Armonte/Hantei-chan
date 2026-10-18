#!/usr/bin/env python3
"""Convert every PS3 PANIBin pat that has an arcade twin and compare per record
(P_ST / PPST / VEST) against the arcade PAniDataFile. PGST is skipped (arcade
embeds compressed textures; PS3 references external .dds.gz).
Usage: verify_panibin.py ARCADE_GRPDAT PS3_GRPDAT"""
import os, sys, collections
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'uni'))
import patwalk, panibin2pat as P
A, S = sys.argv[1], sys.argv[2]
tot = collections.Counter(); exact_files = []; partial = []
for root, _, fs in os.walk(S):
    for f in sorted(fs):
        if not f.endswith('.pat'): continue
        rel = os.path.relpath(os.path.join(root, f), S); a = os.path.join(A, rel)
        if not os.path.exists(a): continue
        mine = dict(P.records(P.parse(open(os.path.join(S, rel), 'rb').read())))
        ref = dict((k, v) for k, v in patwalk.records(open(a, 'rb').read()) if k.split()[0] in ('P_ST', 'PPST', 'VEST'))
        mine = {k: v for k, v in mine.items() if not k.startswith('PGST')}
        if 'VEST' in ref and 'VEST' in mine:   # VNST names are absent from PANIBin: compare params only
            n = ref['VEST'][4:8]; import struct; cnt = struct.unpack('<I', n)[0]
            ref['VEST'] = ref['VEST'][:12 + cnt * 64]; mine['VEST'] = mine['VEST'][:12 + cnt * 64]
        keys = set(mine) | set(ref); same = sum(mine.get(k) == ref.get(k) for k in keys)
        for k in keys:
            t = k.split()[0]; tot[t, mine.get(k) == ref.get(k)] += 1
        (exact_files if same == len(keys) else partial).append((rel, same, len(keys)))
print('files with all P_ST/PPST/VEST records byte-identical:', len(exact_files), '/', len(exact_files) + len(partial))
for t in ('P_ST', 'PPST', 'VEST'): print(' %s identical %d differ %d' % (t, tot[t, True], tot[t, False]))
for r in partial[:int(os.environ.get('N', 15))]: print('  differs', *r)
