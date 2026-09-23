#!/usr/bin/env python3
"""Census of UNI2/MBTL HA6 values: layer ids, box indices vs FSNH/FSNA, AFRT
placement, ATSn/ATSP, PUPS users, PFLG templates, AFAL modes."""
import sys,glob,collections
sys.path.insert(0,'/mnt/c/dev/hantei-chan/wt/uni/tools/uni')
from ha6walk import walk
C=collections.Counter(); ex={}
ROOT=sys.argv[1] if len(sys.argv)>1 else "/mnt/c/dev/hantei-chan/gamedata"
for g in ("uni2","mbtl"):
  for f in glob.glob(ROOT+'/'+g+'/data/**/*.[hH][aA]6',recursive=True):
    prev=None; layer=None; nlayers=0; fsnh=fsna=0
    for pid,tag,w,o in walk(open(f,'rb').read()):
        if tag=='FSTR': nlayers=0; layer=None; fsnh=fsna=0
        if tag=='AFGX': layer=w[0]; nlayers+=1; C[(g,'afgx_layer',w[0])]+=1
        if tag=='AFGP': C[(g,'AFGP')]+=1; ex[(g,'AFGP')]=(f,pid)
        if tag=='AFRT': C[(g,'AFRT_after_layer',layer,nlayers)]+=1; ex[(g,'AFRT',layer)]=(f,pid)
        if tag=='FSNH': fsnh=w[0]; C[(g,'FSNH',w[0])]+=1
        if tag=='FSNA': fsna=w[0]; C[(g,'FSNA',w[0])]+=1
        if tag in('HRNM','HRNS'):
            C[(g,'hurt_idx',w[0])]+=1
            if w[0]>=fsnh: C[(g,'hurt_idx>=FSNH')]+=1; ex[(g,'hurtover')]=(f,pid)
        if tag in('HRAT','HRAS'):
            C[(g,'atk_idx',w[0])]+=1
            if w[0]>=fsna: C[(g,'atk_idx>=FSNA')]+=1
        if tag=='IFPR' and w[0]>9: C[(g,'IFPR>9',w[0])]+=1; ex[(g,'IFPR>9')]=(f,pid)
        if tag=='EFPR' and w[0]>12: C[(g,'EFPR>12',w[0])]+=1
        if tag=='PFLG': C[(g,'PFLG',f.split('/')[-1].lower() if 'base' in f.lower() or 'temp' in f.lower() else 'chr',w[0])]+=1
        if tag in('ATS1','ATS2','ATS3','ATS4','ATS5','ATS6','ATSP'): C[(g,tag,'' if tag!='ATSP' else w[0])]+=1
        if tag=='PUPS': C[(g,'PUPS',w[0],f.split('/')[-1])]+=1
        if tag=='AFTN': C[(g,'AFTN',w)]+=1
        if tag=='AFAL': C[(g,'AFAL',w[0])]+=1
        if tag=='PDS2': C[(g,'PDS2[5]',w[6])]+=1
        if tag=='PTT2' and w[0]!=32: C[(g,'PTT2len',w[0])]+=1
        if tag=='PTCN': C[(g,'PTCN')]+=1
for k in sorted(C,key=str): print(k,C[k])
for k,v in ex.items(): print(k,v)
