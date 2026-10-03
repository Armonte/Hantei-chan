import re,sys,collections
f=sys.argv[1]; targets=set(int(x) for x in sys.argv[2].split(','))
rows=[]
for l in open(f):
    m=re.match(r"(\d+) (\d) (\w+) (-?\d+) pat=(\d+) fr=(\d+) ft=(\d+) face=(\d) key=(\w+) kprev=(\w+) clash=(-?\d+) c16=(\d+) c19=(\d+) tick=(\d+) hitstop=(\d+) age=(\d+) guardG=(-?\d+) cls=(\w+) gmask=(-?\d+) atkact=(\d+) sinceHit=(-?\d+) stance=(\w+) vuln=(\w+) hurtY=(\S+)",l)
    if m: rows.append(m.groups())
bys=collections.defaultdict(list)
for r in rows: bys[r[1]].append(r)
for s,rs in bys.items():
    for i,r in enumerate(rs):
        p=int(r[4])
        if p in targets and (i==0 or int(rs[i-1][4])!=p):
            nxt=next((x for x in rs[i+1:] if int(x[4])!=p),None)
            dur=(int(nxt[0])-int(r[0])) if nxt else -1
            print("t=%s side=%s chara=%s prev=%s ->%s key=%s guardG=%s hurtY=%s gmask=%s next=%s after %dms"%(r[0],s,r[3],rs[i-1][4] if i else '-',p,r[8],r[16],r[23],r[18],nxt[4] if nxt else '-',dur))
