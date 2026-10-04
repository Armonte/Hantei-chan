import re,sys
f,side=sys.argv[1],int(sys.argv[2]); t0=float(sys.argv[3]) if len(sys.argv)>3 else 0
last=None
for l in open(f):
    m=re.match(r"(\d+) (\d) (\w+) (-?\d+) pat=(\d+) fr=(\d+) ft=(\d+) face=(\d) key=(\w+) kprev=(\w+) clash=(-?\d+) c16=(\d+) c19=(\d+) tick=(\d+) hitstop=(\d+) age=(\d+) guardG=(-?\d+) cls=(\w+) gmask=(-?\d+) atkact=(\d+) sinceHit=(-?\d+) stance=(\w+) vuln=(\w+) hurtY=(\S+)",l)
    if not m: continue
    g=m.groups()
    if int(g[1])!=side or int(g[0])<t0: continue
    k=(g[4],g[8],g[10],g[11],g[12],g[19])
    if k!=last:
        print("t=%s chara=%s pat=%s fr=%s key=%s clash=%s c16=%s c19=%s hitstop=%s gmask=%s atk=%s stance=%s vuln=%s hurtY=%s"%(g[0],g[3],g[4],g[5],g[8],g[10],g[11],g[12],g[14],g[18],g[19],g[21],g[22],g[23]))
        last=k
