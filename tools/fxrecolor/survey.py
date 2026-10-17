#!/usr/bin/env python3
"""MBAACC/MBAC/MB CG survey: per bank image-type census + effect-pattern palette-behaviour table.
usage: survey.py <data_dir> <out_dir>
Class of an image = its CG type: 0 = indexed into the bank palette (follows the character palette slot),
2/4 = indexed with its OWN embedded palette (fixed colours), 1/3 = RGB (fixed)."""
import sys, os, glob, csv, collections, re
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import cgparse, usage, palrange

def pclass(types):
    s = set(types)
    if not s: return 'nosprite'
    if s == {0}: return 'follows-palette'
    if s <= {2, 4}: return 'fixed-indexed'
    if s <= {1, 3}: return 'rgb'
    if 0 in s: return 'mixed-follows+fixed'
    return 'mixed-fixed'

def slot_variance(bank, pal, sprites, cache):
    """mean |RGB diff| between slot 0 and slots 1..35 over the palette indices the sprites use (0 = colour never changes)."""
    used = np.zeros(256, bool)
    for sid in sprites:
        m = bank.images.get(sid)
        if m is None or m.type != 0: continue
        if sid not in cache:
            cache[sid] = np.bincount(np.frombuffer(bank.b, np.uint8, m.blob_len, m.blob_off), minlength=256) > 0
        used |= cache[sid]
    used[0] = False
    if pal is None or not used.any(): return None
    base = pal[0][used][:, :3].astype(int)
    return float(np.mean([np.abs(pal[k][used][:, :3].astype(int) - base).mean() for k in range(1, min(pal.shape[0], 36))]))

def run(data, out):
    os.makedirs(out, exist_ok=True)
    census = []; pat_rows = []
    for cg in sorted(glob.glob(data + '/*.cg'), key=str.lower):
        stem = os.path.basename(cg)[:-3]
        try: bank = cgparse.Bank(cg)
        except Exception as e: print('SKIP', stem, e); continue
        tc = collections.Counter(m.type for m in bank.images.values())
        row = {'bank': stem, 'images': len(bank.images)}
        for t in (0, 1, 2, 3, 4): row['t%d' % t] = tc.get(t, 0)
        files = usage.ha6_files(data, stem)
        row['ha6'] = len(files)
        if files and not stem.lower().startswith('csel_'):
            P = usage.load(files)
            tg = collections.defaultdict(set)
            for pid, p in P.items():
                for sp in p.spawns:
                    for k, n in usage.spawn_targets(pid, sp):
                        if k == 'pat': tg[n].add(pid)
            users = collections.defaultdict(set)
            for pid, p in P.items():
                for s in p.sprites: users[s].add(pid)
            pal = palrange.read_pal(f'{data}/{stem}.pal') if os.path.exists(f'{data}/{stem}.pal') else None
            vcache = {}
            effimgs = set(i for i, m in bank.images.items() if m.type != 0)
            row['spriteUsedByAnyPattern'] = sum(1 for i in bank.images if i in users)
            row['nonbody_unused'] = sum(1 for i in effimgs if i not in users)
            cls = collections.Counter()
            for pid in sorted(P):
                p = P[pid]
                types = [bank.images[s].type for s in p.sprites if s in bank.images]
                spawned = pid in tg
                hasnon = any(t != 0 for t in types)
                if not (spawned or hasnon): continue    # body pattern
                c = pclass(types)
                var = slot_variance(bank, pal, p.sprites, vcache) if 0 in types else None
                if c == 'follows-palette' and var is not None and var < 2.0: c = 'indexed-palette-constant'
                cls[c] += 1
                pat_rows.append({'bank': stem, 'pid': pid, 'name': p.name, 'spawnedBy': ' '.join(map(str, sorted(tg[pid])[:6])),
                                 'class': c, 'nsprites': len(p.sprites), 'types': ''.join(map(str, sorted(set(types)))),
                                 'slotVar': '' if var is None else round(var, 1), 'blend': ' '.join(map(str, sorted(p.blend))), 'hurt': p.hurt, 'atk': p.attack,
                                 'spriteIds': ' '.join(str(x) for x in sorted(p.sprites)[:12])})
            for k, v in cls.items(): row['pat_' + k] = v
        census.append(row)
    keys = []
    for r in census:
        for k in r:
            if k not in keys: keys.append(k)
    with open(out + '/census.csv', 'w', newline='') as f:
        w = csv.DictWriter(f, keys); w.writeheader(); w.writerows(census)
    if pat_rows:
        with open(out + '/effect_patterns.csv', 'w', newline='') as f:
            w = csv.DictWriter(f, list(pat_rows[0])); w.writeheader(); w.writerows(pat_rows)
    return census

if __name__ == '__main__':
    r = run(sys.argv[1], sys.argv[2])
    for x in r: print(x)
