# gof2_clash_force.py <pid> <seconds> <out.txt> [maxdx]
# Forces an attack-vs-stance clash in a running GOF2 copy (no debugger attach needed; works next to gof2_dbg2.py).
# Loop: when both fighters are close (|dx|<=maxdx) and idle (pattern 0), request pattern A on the attacker (16 or 19)
# and, after `lag` game ticks of the attacker, pattern B on the defender (15 for 16, 18 for 19).  Requests go through
# Obj_RequestPattern 0x430450 (eax=obj, stack arg=pattern, retn 4) executed by a remote stub thread.
# lag cycles 0,1,2,3 ; attacker side alternates.  Every trial logs the latches/clash state of both sides until they settle.
import ctypes, struct, sys, time
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.restype=W.HANDLE; k.VirtualAllocEx.restype=ctypes.c_void_p
k.VirtualAllocEx.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_size_t,W.DWORD,W.DWORD]
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.WriteProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.CreateRemoteThread.restype=W.HANDLE
k.CreateRemoteThread.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_void_p,W.DWORD,ctypes.c_void_p]
pid=int(sys.argv[1]); secs=float(sys.argv[2]); out=open(sys.argv[3],'w'); maxdx=int(sys.argv[4]) if len(sys.argv)>4 else 130
setdx=int(sys.argv[5]) if len(sys.argv)>5 else 0   # >0: poke the defender worldX (+0x11FC) to attacker.x -/+ setdx on the attacker's facing side before each trial
h=k.OpenProcess(0x1F0FFF,False,pid)
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)) or r.value!=n: return None
    return b.raw
def u32(a):
    b=rd(a,4); return None if b is None else struct.unpack('<I',b)[0]
def i32(a):
    b=rd(a,4); return None if b is None else struct.unpack('<i',b)[0]
stub=k.VirtualAllocEx(h,None,4096,0x3000,0x40); slot=[0]
def request(obj,pat):
    code=b'\xB8'+struct.pack('<I',obj)+b'\x68'+struct.pack('<I',pat)+b'\xBA'+struct.pack('<I',0x430450)+b'\xFF\xD2'+b'\x31\xC0\xC2\x04\x00'
    off=slot[0]*64; slot[0]=(slot[0]+1)%60
    r=ctypes.c_size_t(); k.WriteProcessMemory(h,stub+off,code,len(code),ctypes.byref(r))
    k.CreateRemoteThread(h,None,0,stub+off,None,0,None)
T0=time.time()
def log(s): out.write("%d %s\n"%((time.time()-T0)*1000,s)); out.flush()
def st(o):
    d=rd(o,0x1264)
    if d is None: return None
    g=lambda off: struct.unpack_from('<i',d,off)[0]
    return dict(pat=g(0x26c),fr=g(0x270),ft=g(0x274),x=g(0x11fc),clash=g(0x6c0),c16=g(0x11d8),c19=g(0x11dc),tick=g(0x1098),hs=g(0x1078),atk=g(0x67c),cls=g(0x5dc),hit=g(0x6c4))
def fmt(s,d): return "s%d pat=%d fr=%d ft=%d x=%d clash=%d c16=%d c19=%d tick=%d hitstop=%d atkact=%d"%(s,d['pat'],d['fr'],d['ft'],d['x'],d['clash'],d['c16'],d['c19'],d['tick'],d['hs'],d['atk'])
# (attacker pattern, defender pattern or 0 = none): stance clash, no defender, attack-vs-attack
SCEN=[(19,18),(16,0),(19,0),(16,16),(19,19),(16,19),(19,16),(16,18),(19,15),(16,15)]
trial=0; lastend=0; lag=0
log("start maxdx=%d"%maxdx)
while time.time()-T0<secs:
    o=[u32(0x65b928),u32(0x65b92c)]
    if not o[0] or not o[1]: time.sleep(0.05); continue
    s=[st(o[0]),st(o[1])]
    if s[0] is None or s[1] is None: continue
    if time.time()-lastend<1.2: time.sleep(0.01); continue
    if s[0]['pat'] not in (0,7,8) or s[1]['pat'] not in (0,7,8) or abs(s[0]['x']-s[1]['x'])>maxdx: time.sleep(0.004); continue
    a=trial%2; d=1-a; pa,pb=SCEN[(trial//2)%len(SCEN)]; lag=(trial//(2*len(SCEN)))%2
    trial+=1
    log("TRIAL %d attacker=side%d pat=%d defender=side%d pat=%d lag=%d dx=%d"%(trial,a,pa,d,pb,lag,abs(s[0]['x']-s[1]['x'])))
    if setdx:
        fc=i32(o[a]+0x2b0); nx=s[a]['x']+(-setdx if fc==1 else setdx)
        k.WriteProcessMemory(h,o[d]+0x11fc,struct.pack('<i',nx),4,ctypes.byref(ctypes.c_size_t()))
        log('POKE defender x=%d (attacker x=%d face=%d)'%(nx,s[a]['x'],fc))
    request(o[a],pa)
    t_req=time.time(); a0=None
    # wait for lag attacker ticks then request defender stance
    base=s[a]['tick']; tt=time.time()
    while time.time()-tt<0.5:
        sa=st(o[a])
        if sa and sa['pat']==pa:
            if a0 is None: a0=sa['tick']
            if sa['tick']-a0>=lag: break
        time.sleep(0.001)
    if pb: request(o[d],pb)
    last=None; end=time.time()+1.6; seen=False; mhs=[0,0]; pats=[set(),set()]
    while time.time()<end:
        s=[st(o[0]),st(o[1])]
        if s[0] is None or s[1] is None: break
        row=(s[0]['pat'],s[0]['fr'],s[0]['clash'],s[0]['c16'],s[0]['c19'],s[0]['hs'],s[1]['pat'],s[1]['fr'],s[1]['clash'],s[1]['c16'],s[1]['c19'],s[1]['hs'])
        if row!=last:
            log("  "+fmt(0,s[0])+" | "+fmt(1,s[1])); last=row
        if any(x['c16'] or x['c19'] for x in s): seen=True
        for q in (0,1): mhs[q]=max(mhs[q],s[q]['hs']); pats[q].add(s[q]['pat'])
        time.sleep(0.003)
    log("END trial %d latch_seen=%s maxhitstop=%s pats=%s"%(trial,seen,mhs,[sorted(x) for x in pats]))
    lastend=time.time()
log("done")
