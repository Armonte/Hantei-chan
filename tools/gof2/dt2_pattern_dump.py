#!/usr/bin/env python3
"""Dump GOF2 .DT2 pattern/frame summaries. usage: dt2_pattern_dump.py <CHARA|path> [pattern ...]
Chara ids (engine order, charaContext+16155): 0 KANAE 1 DATE 2 IMAGAWA 3 AMIY 4 MAEDA 5 SANADA 6 TOKUGAWA 7 CYOSOKABE 8 TAKEDA 9 UESUGI 10 NEGAI 11 KATSUKO
data05 overrides data02."""
import struct, sys, os
CH=['KANAE','DATE','IMAGAWA','AMIY','MAEDA','SANADA','TOKUGAWA','CYOSOKABE','TAKEDA','UESUGI','NEGAI','KATSUKO']
D='/mnt/c/games/gof/Data/'
def path(n):
    for d in ('data05','data02'):
        p=f'{D}{d}/files/{n}.DT2'
        if os.path.exists(p): return p
def load(name):
    p=path(name) if not os.path.exists(name) else name
    b=open(p,'rb').read()
    off,sz=struct.unpack_from('<II',b,0x20)
    a=b[off:off+sz]
    secs=struct.unpack_from('<9I',a,0x30)
    pos=0x60; S=[]
    for s in secs: S.append(a[pos:pos+s]); pos+=s
    pats=[struct.unpack_from('<3I',S[0],12*i) for i in range(256)]
    return a,S,pats
def frame(S,pats,p,f):
    n,_,first=pats[p]
    return S[1][404*(first+f):404*(first+f+1)]
def summary(S,pats,p):
    n,_,first=pats[p]
    out=[]
    for f in range(n):
        r=frame(S,pats,p,f)
        spr,ox,oy,dur=struct.unpack_from('<hhhH',r,0)
        ani=r[0x0b]; jt=r[0x0c]
        atk=struct.unpack_from('<I',r,0x16C)[0]
        hurt=struct.unpack_from('<I',r,0x114)[0]
        la,lb=struct.unpack_from('<II',r,0xBC)
        out.append((f,spr,dur,ani,jt,atk,hurt,la,lb))
    return out
if __name__=='__main__':
    nm=sys.argv[1]; ps=[int(x) for x in sys.argv[2:]]
    a,S,pats=load(nm)
    for p in ps or range(256):
        n=pats[p][0]
        if not n: continue
        print(f'pat {p}: {n} frames')
        for t in summary(S,pats,p): print('   f%d spr=%d dur=%d ani=%d jt=%d atkBoxes=%d hurt=%d scrA=%d scrB=%d'%t)

def atrec(S,r):
    idx=struct.unpack_from('<i',r,0xC8)[0]
    if idx<0: return None
    return S[3][236*idx:236*(idx+1)]
def atinfo(S,pats,p):
    out=[]
    for f in range(pats[p][0]):
        r=frame(S,pats,p,f); a=atrec(S,r)
        if a is None: continue
        g=lambda o: struct.unpack_from('<i',a,o)[0]
        out.append(dict(f=f,hitClass=g(0),hitStop=g(4),dmg=g(0x10),kind=g(0x80),guard=g(0x7c),knock=g(0x34),stance=g(0x90),hitFlags=g(0x4c),react=g(0x50),snd=g(0x2c)))
    return out
