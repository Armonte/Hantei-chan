#!/usr/bin/env python3
"""Scan directories for French-Bread archives / character files by header magic (no tree-wide find: pass explicit roots).
usage: fb_scan.py [--depth N] root...   -> TSV: kind, count, total bytes, example"""
import os,struct,sys,collections
KE3=0xE3DF59AC; KPB=0xFA261EFB
def classify(p):
    try:
        with open(p,'rb') as f: h=f.read(64)
    except OSError: return None
    if len(h)<8: return None
    if h[:10]==b'PKFileInfo': return 'PKFileInfo(AC .p)'
    if h[:14]==b'FilePacHeaderA': return 'FilePacHeaderA(AA .p)'
    if h[:8]==b'HAN2RBO ': return 'HAN2RBO'
    if h[:7]==b'Hantei4' : return 'HA4(Hantei4)'
    if h[:4]==b'HA6\0' or h[:3]==b'HA6': return 'HA6'
    a,b=struct.unpack('<II',h[:8])
    if a==1 and 0<(b^KE3)<100000: return 'PAC v1 key E3DF59AC (RBO/GOF2 .pac/.dat)'
    if a in (0,1) and 0<(b^KPB)<100000: return 'PB/GOF1 archive key FA261EFB v%d'%a
    if a in (0,1) and 0<(b^KE3)<100000: return 'E3DF-keyed v%d (MB .p old)'%a
    if h[:4]==b'LLIF' or h[:4]==b'LLIF'[::-1]: return 'EX3(LLIF)'
    return None
def main():
    a=sys.argv[1:]; depth=4
    if a and a[0]=='--depth': depth=int(a[1]); a=a[2:]
    agg=collections.defaultdict(lambda:[0,0,''])
    for root in a:
        base=root.rstrip('/').count('/')
        for d,ds,fs in os.walk(root):
            if d.count('/')-base>=depth: ds[:]=[]
            for fn in fs:
                e=os.path.splitext(fn)[1].lower()
                if e not in ('.p','.pac','.dat','.dt2','.pat','.chp','.fob','.ex3','.ha6','.ha4','.bin','.cpf',''): continue
                p=os.path.join(d,fn)
                k=classify(p)
                if not k: continue
                s=os.path.getsize(p); g=agg[(root,k,e)]
                g[0]+=1; g[1]+=s
                if not g[2]: g[2]=p
    for (r,k,e),(n,s,x) in sorted(agg.items()): print('\t'.join([r,k,e,str(n),str(s),x]))
main()
