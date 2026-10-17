#!/usr/bin/env python3
"""Generate the per-function tables of docs/formats/gof2_ai_move_queries.md from ai_truth/*.json (emu_ai_queries.py output) + the DT2 data."""
import json, glob, sys
from dt2_pattern_dump import *
IDB = {'Obj_IsAtMoveKeyFrameByChara':'Obj_IsAtHighAttackTellKeyFrameByChara 0x4668C0',
 'Obj_IsInMoveFrameRangeByChara':'Obj_IsInHighAttackPreHitWindowByChara 0x466F50',
 'Obj_IsInMoveFrameWindow2ByChara':'Obj_IsInLowAttackEarlyWindowByChara 0x467A10',
 'Obj_IsInMoveFrameRange3ByChara':'Obj_IsInLowAttackPreHitWindowByChara 0x467C50',
 'Obj_IsInMoveFrameWindow4ByChara':'Obj_IsInMidAttackEarlyWindowByChara 0x468100',
 'Obj_IsInMoveFrameRange5ByChara':'Obj_IsInMidAttackPreHitWindowByChara 0x468840',
 'Obj_IsInMoveFrameRange6ByChara':'Obj_IsInWeakAttackPreHitWindowByChara 0x469270',
 'Obj_IsInMoveFrameRange7ByChara':'Obj_IsInPostHitRecoveryWindowByChara 0x4698F0',
 'Obj_IsInPattern16Or19StartupWindowA':'Obj_IsInPat16Or19AtkStartup_StandGuardOnlySet 0x466660',
 'Obj_IsInPattern16Or19StartupWindowB':'Obj_IsInPat16Or19AtkStartup_CrouchGuardOnlySet 0x466770',
 'Obj_IsInPattern16Or19StartupWindowC':'Obj_IsInPat16Or19AtkStartup_AnyGuardSet 0x466850'}
ORDER = list(IDB)
def tt(S,pats,p,f):
    s=summary(S,pats,p); atk=[x[0] for x in s if x[5]]
    if not atk: return None
    nx=[a for a in atk if a>=f]
    return sum(x[2] for x in s[f:nx[0]]) if nx else -sum(x[2] for x in s[atk[-1]:f])
for n in ORDER:
    d=json.load(open(f'ai_truth/{n}.json'))[n]
    print(f'\n### {IDB[n]} (was `{n}`)\n')
    print('| chara | pattern | frames (minTicks) | nFrames | attack frames | first-hit guardMask | dmg | kind | ticks to hit at range start..end |')
    print('|---|---|---|---|---|---|---|---|---|')
    for c in sorted(d,key=int):
        a,S,pats=load(CH[int(c)])
        for p,rng in sorted(d[c].items(),key=lambda x:int(x[0])):
            p=int(p); ai=atinfo(S,pats,p)
            fr=','.join('%d-%d%s'%(r[0],r[1],'' if not r[2] else '(t>=%s)'%r[2]) for r in rng)
            if ai: g=ai[0]; gm='%d'%(g['guard']&7); dmg=g['dmg']; kind=g['kind']
            else: gm=dmg=kind='-'
            lo,hi=rng[0][0],rng[-1][1]
            if hi-lo>=60: tk='all'
            else:
                x=tt(S,pats,p,lo); y=tt(S,pats,p,hi); tk='-' if x is None else f'{x}..{y}'
            print(f'| {c} {CH[int(c)]} | {p} | {fr} | {pats[p][0]} | {",".join(str(x["f"]) for x in ai) or "-"} | {gm} | {dmg} | {kind} | {tk} |')
