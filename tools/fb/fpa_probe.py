import struct,sys,os
def xs(b,key,inc):
    if inc==0: inc=1
    k=[key&255,(key>>8)&255,(key>>16)&255,(key>>24)&255]; o=bytearray(b)
    for i in range(len(o)):
        o[i]^=k[i&3]; k[i&3]=(k[i&3]+inc)&255
    return bytes(o)
for p in sys.argv[1:]:
    f=open(p,'rb'); h=f.read(52); fs=os.path.getsize(p)
    ver,key,doff,dsz,nd,nf,flag,inc,chunk=struct.unpack('<9I',h[16:52])
    print(p,'pad14',h[14:16].hex(),'ver',ver,'doff',doff,'dsz',dsz,'nd',nd,'nf',nf,'flag',flag,'inc',inc,'chunk',hex(chunk),'fs',fs,'doff+dsz',doff+dsz, 'expect doff',52+268*nd+44*nf)
    dirs=[]
    for i in range(nd):
        r=f.read(268); pos,fi,sz=struct.unpack('<III',r[:12]); nm=xs(r[12:],key,sz&255); dirs.append((pos,fi,sz,nm))
    files=[]
    for i in range(nf):
        r=f.read(44); pos,ow,sz=struct.unpack('<III',r[:12]); nm=xs(r[12:],key,sz&255); files.append((pos,ow,sz,nm))
    # contiguity
    cur=0;gaps=0
    for pos,ow,sz,nm in files:
        if pos!=cur: gaps+=1
        cur=pos+sz
    print(' contiguous gaps',gaps,'end',cur,'dsz',dsz)
    # owner monotonic?
    mono=all(files[i][1]<=files[i+1][1] for i in range(nf-1))
    print(' owners monotonic',mono, 'dirinc zero',sum(1 for d in dirs if d[2]&255==0),'dir sizes',set(d[2]>>8 for d in dirs))
    # folder first idx/pos consistent
    bad=0
    for di,(pos,fi,sz,nm) in enumerate(dirs):
        idx=[i for i in range(nf) if files[i][1]==di]
        if idx:
            if idx[0]!=fi or files[idx[0]][0]!=pos: bad+=1
        else:
            pass
    print(' folder first/pos mismatches',bad,'empty folders',sum(1 for di in range(nd) if not any(1 for f in files if f[1]==di)))
    print(' junk file name slots',sum(1 for f in files if any(f[3][f[3].find(0)+1:]) ) if True else 0, 'dir',[ (d[3][:d[3].find(0)]) for d in dirs[:3]])
