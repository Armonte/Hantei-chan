import struct,sys
K=0xE3DF59AC
def parse(p):
    f=open(p,'rb'); h=f.read(24)
    assert h[:10]==b'PKFileInfo'
    typ=struct.unpack('<I',h[16:20])[0]; n=struct.unpack('<I',h[20:24])[0]^K
    idx=f.read(72*n); ents=[]
    for i in range(n):
        e=bytearray(idx[i*72:(i+1)*72])
        for j in range(63): e[j]^=(3*(i*j+26))&0xFF
        nm=bytes(e[:63]); z=nm.find(0); name=nm[:z]
        off,sz=struct.unpack('<II',e[64:72]); sz^=K
        ents.append((name,off,sz,bytes(e[z:63]),e[63]))
    return h,typ,n,ents
for p in sys.argv[1:]:
    h,typ,n,ents=parse(p)
    import os
    fs=os.path.getsize(p)
    ds=24+72*n
    pos=ds; gaps=0; ooo=0; junk=0; hi=0
    order=sorted(range(n),key=lambda i:ents[i][1])
    exp=ds
    for i in order:
        name,off,sz,tail,b63=ents[i]
        if off!=exp: gaps+=1
        exp=off+sz
        if any(tail[1:]) or b63: junk+=1
    sortedn = all(ents[i][0]<=ents[i+1][0] for i in range(n-1))
    print(p,'hdr',h[10:16].hex(),'type',typ,'n',n,'fs',fs,'end',exp,'gaps',gaps,'orderByIdx',order==list(range(n)),'junkTail',junk,'sortedNames',sortedn, 'ex',ents[0][0],ents[-1][0])
