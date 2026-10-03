#!/usr/bin/env python3
"""Disassemble the transition (kind 1) scripts attached to GOF2 pattern frames.
usage: fob_trans_dump.py CHARA pattern [frame]   (uses rbo fobdis decoder; GOF2 FOB decodes with 0 errors)"""
import sys, struct, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'rbo'))
sys.path.insert(0, os.path.dirname(__file__))
import fobdis as F
from dt2_pattern_dump import *
def fob(n):
    for d in ('data05','data02'):
        p=f'{D}{d}/files/{n}.FOB'
        if os.path.exists(p): return F.load_fob(open(p,'rb').read())
def dump(f,seen,pc,maxn=80):
    code=f['code']; n=0
    while pc in seen and n<maxn:
        c,s,l,o=seen[pc]; print('   %6d %s'%(pc,F.fmt(code,pc,c,s,l,o))); n+=1
        if F.is_end(c,s): break
        pc+=l
if __name__=='__main__':
    nm=sys.argv[1]; p=int(sys.argv[2]); fr=[int(x) for x in sys.argv[3:]]
    a,S,pats=load(nm); f=fob(nm); seen,errs=F.descend(f)
    for fi in (fr or range(pats[p][0])):
        r=frame(S,pats,p,fi)
        A,B=struct.unpack_from('<II',r,0xBC)
        print(f'== {nm} pat {p} frame {fi}: listA={A} listB={B}')
        for lab,sec,k,idx in (('A',S[6],0,A),('B',S[7],1,B)):
            if not idx: continue
            ids=struct.unpack_from('<5I',sec,20*idx)
            print(f'  list{lab} ids={ids}')
            for i in ids:
                if i and i<len(f['types'][k]) and f['types'][k][i]>=0:
                    print(f'  -- kind{k} id {i}'); dump(f,seen,f['types'][k][i])
