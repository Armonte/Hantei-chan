import re,sys,collections
files=sys.argv[1:]
out=collections.Counter(); ex=[]
for f in files:
    st={}
    for l in open(f):
        m=re.match(r"(\d+) (\d) (\w+) (-?\d+) pat=(\d+) fr=(\d+) ft=(\d+) face=(\d) key=(\w+) kprev=(\w+) .*gmask=(-?\d+) atkact=(\d+)",l)
        if not m: continue
        t,s,ch,pat,fr,key,kprev,gm,aa=int(m.group(1)),int(m.group(2)),int(m.group(4)),int(m.group(5)),int(m.group(6)),int(m.group(9),16),int(m.group(10),16),int(m.group(11)),int(m.group(12))
        o=st.get(1-s)
        if (key&0x40F) in (0x404,0x408,0x405,0x406,0x409,0x40A) and (kprev&0x400)==0 and o and ch>=0 and not (f.startswith('vs1') and s==0):
            out[(key&0x40F, 'self_pat', pat)]+=1
            ex.append((f,t,s,ch,hex(key&0x40F),pat,'opp',o[0],o[1],o[2]))
        st[s]=(pat,fr,ch)
print(len(ex))
c=collections.Counter((e[4],e[5]) for e in ex)
for k,v in sorted(c.items()): print(k,v)
print("examples with opponent in/near 16/19 or attack:")
c2=collections.Counter((e[4],e[8]) for e in ex)
for k,v in sorted(c2.items(), key=lambda x:-x[1])[:25]: print("AI key",k[0],"opp pat",k[1],"x",v)
