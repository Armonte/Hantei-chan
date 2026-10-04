import sys, struct
sys.path.insert(0,'.')
from ex3_gage import *
f=sys.argv[1]; bs=int(sys.argv[2]); mc=int(sys.argv[3]); th=int(sys.argv[4])
d=open(f,'rb').read()
for hs in (64,68,72):
    try:
        bl=parse(d,hs); dec=b''.join(expand(t,s) for g,t,s in bl)
        if struct.unpack('<I',d[hs-4:hs])[0]==len(dec): break
    except Exception: pass
hsz=int(sys.argv[5]) if len(sys.argv)>5 else 4096
g=Gage(bs,hsz,mc,th); pos=0; ok=0; bad=0
for bi,(groups,tab,sym) in enumerate(bl):
    t2,s2,pos2=g.block(dec,pos)
    same_sym = s2==bytes(sym); same_tab = t2==tab
    if same_sym and same_tab: ok+=1
    else:
        bad+=1
        if bad<=3: print('block',bi,'sym',same_sym,'tab',same_tab,len(s2),len(sym),len(t2),len(tab))
    pos=pos2
print(f,'blocks ok',ok,'bad',bad)
