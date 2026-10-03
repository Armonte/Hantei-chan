# gof2_sample.py <pid> <seconds> <out.txt> [interval_ms]
# Polls both fighter Gof2Obj (g_PlayerCharaObj @0x65B928) of a running GOF2 copy via ReadProcessMemory.
import ctypes, struct, sys, time
from ctypes import wintypes
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.restype=wintypes.HANDLE
k.ReadProcessMemory.argtypes=[wintypes.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
pid=int(sys.argv[1]); secs=float(sys.argv[2]); out=open(sys.argv[3],'w'); iv=(int(sys.argv[4]) if len(sys.argv)>4 else 50)/1000
h=k.OpenProcess(0x10,False,pid)
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)) or r.value!=n: return None
    return b.raw
def u32(a):
    b=rd(a,4); return None if b is None else struct.unpack('<I',b)[0]
t0=time.time(); last=[None,None]
out.write("# t_ms side obj chara pat frame ftick facing key(cur) keyprev clashState 11D8 11DC hitDur16 hitDur19 hitStop actionAge guardGauge hurtH classFlags\n")
while time.time()-t0<secs:
    for s in (0,1):
        try:
            p=u32(0x65b928+4*s)
            if not p: continue
            d=rd(p,0x1264)
            if d is None: continue
            g=lambda o: struct.unpack_from('<i',d,o)[0]
            kc=struct.unpack('<I',d[0x10d8:0x10dc])[0]
            kr=rd(kc,64) if kc else None
            key=struct.unpack_from('<I',kr,8)[0] if kr else 0; kp=struct.unpack_from('<I',kr,12)[0] if kr else 0
            cc=struct.unpack_from('<I',d,0x10e0)[0]; chv=u32(cc+16155*4) if cc else -1
            chv=-1 if chv is None else chv
            ar=struct.unpack_from('<I',d,0x620)[0]; gm=(u32(ar+0x7c) if ar else None); gm=-1 if gm is None else gm
            hc=g(0x6f0+8); hb=[]
            for i in range(min(max(hc,0),6)):
                bp=struct.unpack_from('<I',d,0x6f0+0x10+4*i)[0]; r=rd(bp,8) if bp else None
                if r: hb.append(struct.unpack('<4h',r))
            hy1=min([b[1] for b in hb]) if hb else 0; hy2=max([b[3] for b in hb]) if hb else 0
            row=(g(0x26c),g(0x270),g(0x274),g(0x2b0),key,kp,g(0x6c0),g(0x11d8),g(0x11dc),g(0x1098),g(0x1078),g(0x109c),g(0xfd8),g(0x5dc),gm,g(0x67c),g(0x6c4),g(0x294),g(0x298),hy1,hy2)
            if row!=last[s]:
                last[s]=row
                out.write("%d %d %X %d pat=%d fr=%d ft=%d face=%d key=%04X kprev=%04X clash=%d c16=%d c19=%d tick=%d hitstop=%d age=%d guardG=%d cls=%X gmask=%d atkact=%d sinceHit=%d stance=%X vuln=%X hurtY=%d..%d\n"%((time.time()-t0)*1000,s,p,chv,row[0],row[1],row[2],row[3],row[4],row[5],row[6],row[7],row[8],row[9],row[10],row[11],row[12],row[13]&0xffffffff,row[14],row[15],row[16],row[17]&0xffffffff,row[18]&0xffffffff,row[19],row[20]))
        except Exception as e: pass
    time.sleep(iv)
out.flush()
