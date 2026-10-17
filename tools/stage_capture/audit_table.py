#!/usr/bin/env python3
"""Per-stage feature + accuracy table for docs/bg_research/STAGE_AUDIT.md.

  audit_table.py <census.jsonl> <cmp/results.jsonl> <triples/triples.json> <game capture dir> <bg dir> <bgm.txt>

Features come from bg_test --census; accuracy from compare_all.sh (game-state render vs game capture, every
view) and triples.py (the 9d751c6 renderer vs the game at c0). Prints Markdown.
"""
import sys, json, os, re, collections

cen_p, cmp_p, tri_p, cap, bgdir, bgm_p = sys.argv[1:7]
cen = {}
for l in open(cen_p):
    d = json.loads(l.replace('\\', '/'))
    cen[os.path.basename(d['file']).lower()] = d
cmp = collections.defaultdict(list)
if os.path.exists(cmp_p):
    for l in open(cmp_p):
        try:
            r = json.loads(l)
        except Exception:
            continue
        cmp[r['id']] = [x for x in cmp[r['id']] if x['view'] != r['view']] + [r]   # the last run of a view wins
tri = {r['id']: r for r in json.load(open(tri_p))} if os.path.exists(tri_p) else {}
ini = open(f'{bgdir}/BgList.ini', 'rb').read().decode('cp932', 'replace').replace('\r', '')
secs = {}
for m in re.finditer(r'^\[Bg_(\d{3})\]([^\[]*)', ini, re.M):
    kv = {}
    for line in m.group(2).split('\n'):
        line = line.split('//')[0]
        if '=' in line:
            k, v = line.split('=', 1)
            kv[k.strip()] = v.strip()
    secs[int(m.group(1))] = kv
bgm = open(bgm_p, 'rb').read().decode('cp932', 'replace').replace('\r', '')
bgmf = {}
for m in re.finditer(r'^\[BGM_(\d{3})\]\s*\nFile\s*=\s*(\S+)', bgm, re.M):
    bgmf[int(m.group(1))] = m.group(2)
EN = {}
src = open(os.path.join(os.path.dirname(__file__), '../../src/background/bg_project.cpp'), encoding='utf-8').read()
tbl = src[src.index('kMbaaccEnNames[100]'):]
tbl = tbl[tbl.index('{') + 1: tbl.index('};')]
for i, v in enumerate(x.strip().rstrip(',') for x in tbl.strip().split('\n')):
    if v.startswith('"'):
        EN[i] = v.strip('"')
EXT = {55, 57, 58, 99}
EXO = {3, 4, 18, 20, 21, 23, 31, 32, 43, 44, 47, 49, 50, 51, 52, 54, 55, 57, 58, 99}

print('| Id | File | Name | Objects (fg) | CG / PAT frames | Blend 0/1/2/3 | Motion, events | Weather / lights | StageColorVal, giant | DXT5 | BGM | Views | Pool (1/30/120/600) | After: exact / close / mae (worst view) | Before (c0) exact / close |')
print('|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|')
for i in sorted(secs):
    kv = secs[i]
    df = kv.get('DataFile', f'bg{i:02d}')
    c = cen.get(df.lower() + '.dat')
    if not c:
        continue
    ev = []
    if c['cmdSpawn']: ev.append(f"spawn {c['cmdSpawn']}")
    if c['cmdRandSpawn']: ev.append(f"rand-spawn {c['cmdRandSpawn']}")
    if c['cmdRandVel']: ev.append(f"rand-vel {c['cmdRandVel']}")
    if c['triggers']: ev.append(f"triggers {c['triggers']}")
    if c['interp']: ev.append(f"+20 lerp {c['interp']}")
    if c['moving']: ev.append(f"velocity frames {c['moving']}")
    if c['noAutoSpawn']: ev.append(f"no-autospawn {c['noAutoSpawn']}")
    wl = []
    if c['dropObj']: wl.append({0: 'petals + bloom', 1: 'rain', -1: 'grid room'}.get(c['dropType'], '?'))
    if c['lights']: wl.append(f"{c['lights']} light(s)")
    cv = kv.get('StageColorVal', '')
    flags = (cv or '-') + (', giant' if kv.get('IsGiantStage', '0') not in ('0', '') else '')
    excl = ('!Tr ' if i in EXT else '') + ('!VS' if i in EXO else '')
    rs = cmp.get(i, [])
    views = len(rs)
    pools = [r['pool'] for r in rs if r.get('pool')]
    pool = '-' if not pools else ('match' if all(p == 'match' for p in pools) and len(pools) == 4 else ('match (%d/4)' % pools.count('match') if 'diff' not in pools else 'DIFF'))
    if rs:
        w = min(rs, key=lambda r: r['m']['exact'])
        acc = '%.2f / %.2f / %.3f (%s)' % (w['m']['exact'], min(r['m']['close'] for r in rs), max(r['m']['mae'] for r in rs), w['view'])
    else:
        acc = 'not captured'
    t = tri.get(i)
    bef = '%.2f / %.2f' % (t['before']['exact'], t['before']['close']) if t else '-'
    print(f"| {i:02d} | {df} | {EN.get(i, '')} {excl} | {c['objects']} ({c['fgObjects']}) | {c['cgFrames']} / {c['patFrames']} | "
          f"{'/'.join(map(str, c['blend']))} | {', '.join(ev) or '-'} | {', '.join(wl) or '-'} | {flags} | "
          f"{'yes' if c['dxt5'] else '-'} | {bgmf.get(i, '-')} | {views} | {pool} | {acc} | {bef} |")
