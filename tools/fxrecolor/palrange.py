#!/usr/bin/env python3
"""Which palette indices do type-0 EFFECT sprites use, and does the colour at those indices change between the
character's .pal slots?  usage: palrange.py <data_dir> [stem...]"""
import sys, os, struct, collections, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import cgparse, usage

def read_pal(path):
    b = open(path, 'rb').read(); n = struct.unpack_from('<I', b, 0)[0]
    return np.frombuffer(b, np.uint8, n * 1024, 4).reshape(n, 256, 4)  # BGRA

def effect_patterns(P):
    tg = set()
    for pid, p in P.items():
        for sp in p.spawns:
            for k, n in usage.spawn_targets(pid, sp):
                if k == 'pat': tg.add(n)
    return tg

def analyse(data, stem):
    bank = cgparse.Bank(f'{data}/{stem}.cg'); P = usage.load(usage.ha6_files(data, stem))
    pal = read_pal(f'{data}/{stem}.pal') if os.path.exists(f'{data}/{stem}.pal') else None
    tg = effect_patterns(P)
    nonzero = {i for i, m in bank.images.items() if m.type != 0}
    effpat = set(tg) | {pid for pid, p in P.items() if any(s in nonzero for s in p.sprites)}
    users = collections.defaultdict(set)
    for pid, p in P.items():
        for s in p.sprites: users[s].add(pid)
    hist_e = np.zeros(256, np.int64); hist_b = np.zeros(256, np.int64); ne = nb = 0
    for i, m in bank.images.items():
        if m.type != 0 or i not in users: continue
        h = np.bincount(np.frombuffer(bank.b, np.uint8, m.blob_len, m.blob_off), minlength=256)
        if users[i] <= effpat: hist_e += h; ne += 1
        else: hist_b += h; nb += 1
    res = dict(stem=stem, eff_type0_sprites=ne, body_sprites=nb)
    if pal is not None:
        nslot = pal.shape[0]
        used = (hist_e > 0); used[0] = False
        usedb = (hist_b > 0); usedb[0] = False
        def varying(mask):
            if not mask.any(): return None
            base = pal[0][mask][:, :3].astype(int)
            ds = [np.abs(pal[k][mask][:, :3].astype(int) - base).mean() for k in range(1, min(nslot, 36))]
            return round(float(np.mean(ds)), 1)
        res['eff_idx_used'] = int(used.sum()); res['body_idx_used'] = int(usedb.sum())
        res['eff_only_idx'] = int((used & ~usedb).sum())
        res['slot_var_eff'] = varying(used); res['slot_var_body'] = varying(usedb)
        res['slot_var_eff_only'] = varying(used & ~usedb)
        rng = []
        idx = np.flatnonzero(used & ~usedb)
        if len(idx):
            s = idx[0]; prev = s
            for v in list(idx[1:]) + [None]:
                if v is None or v != prev + 1: rng.append((int(s), int(prev))); s = v
                prev = v if v is not None else prev
        res['eff_only_ranges'] = rng[:8]
    return res

if __name__ == '__main__':
    data = sys.argv[1]; stems = sys.argv[2:] or sorted(os.path.basename(f)[:-3] for f in
        __import__('glob').glob(data + '/*.cg') if not os.path.basename(f).lower().startswith(('csel_', 'effect')))
    for s in stems:
        try: print(analyse(data, s))
        except Exception as e: print(s, 'ERR', e)
