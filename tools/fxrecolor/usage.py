#!/usr/bin/env python3
"""Per-character pattern/sprite usage + spawn graph from all HA6 files of a character (MBAACC layout:
X.HA6 plus X_0/_1/_2 moon variants, X_r, X_9).  Read-only analysis."""
import re, os, glob, collections, ha6walk

SPAWN_REL = (101, 111, 108)
def ha6_files(data, cg_stem):
    pat = re.compile(r'^' + re.escape(cg_stem) + r'(_[0-9r])*\.HA6$', re.I)
    return sorted(f for f in glob.glob(data + '/*.HA6') if pat.match(os.path.basename(f)))

class Pat:
    def __init__(s):
        s.sprites = collections.Counter(); s.usepat = set(); s.spawns = []  # (type, number, params)
        s.hurt = 0; s.attack = 0; s.state = 0; s.blend = collections.Counter(); s.name = ''; s.nframes = 0
        s.files = set(); s.hitfx = []; s.psts = None

def load(files):
    pats = collections.defaultdict(Pat)
    for f in files:
        tag = os.path.basename(f)
        cur_ef = None
        for pid, t, w, o in ha6walk.walk(open(f, 'rb').read()):
            if pid < 0: continue
            P = pats[pid]; P.files.add(tag)
            if t == 'AFGP':
                if w[0]: P.usepat.add(w[1])
                else: P.sprites[w[1]] += 1
            elif t == 'AFGX':
                if w[1]: P.usepat.add(w[2])
                else: P.sprites[w[2]] += 1
            elif t == 'FSTR': P.nframes += 1
            elif t in ('HRNM',): P.hurt += 1
            elif t == 'HRAT': P.attack += 1
            elif t == 'ASST': P.state += 1
            elif t == 'AFAL': P.blend[w[0]] += 1
            elif t == 'PSTS': P.psts = w[0]
            elif t == 'PTT2' and not P.name: P.name = w[1].split(b'\0')[0].decode('cp932', 'replace')
            elif t == 'EFST': cur_ef = {'type': None, 'no': None, 'p': ()}
            elif t == 'EFTP' and cur_ef is not None: cur_ef['type'] = w[0]
            elif t == 'EFNO' and cur_ef is not None: cur_ef['no'] = w[0]
            elif t == 'EFPR' and cur_ef is not None: cur_ef['p'] = tuple(w[1:])
            elif t == 'EFED' and cur_ef is not None:
                P.spawns.append((cur_ef['type'], cur_ef['no'], cur_ef['p'])); cur_ef = None
            elif t == 'ATHE': P.hitfx.append(w)
    return pats

def spawn_targets(pid, sp):
    """-> ('pat', id) same-bank pattern, ('eff', id) effect.HA6 actor, ('preset', n)."""
    ty, no, p = sp
    if ty in (1, 1000): return [('pat', no)]
    if ty == 101: return [('pat', pid + no)]
    if ty in (11, 111):
        base = no + (pid if ty == 111 else 0); rng = p[4] if len(p) > 4 else 0
        return [('pat', base + k) for k in range(max(1, rng or 1))]
    if ty in (8, 108): return [('eff', no + (pid if ty == 108 else 0))]
    if ty == 3: return [('preset', no)]
    return []
