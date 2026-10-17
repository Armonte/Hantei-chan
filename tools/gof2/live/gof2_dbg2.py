# gof2_dbg.py <pid> <seconds> <out.txt> [force]
# int3 debugger harness for a 32-bit GOF2 copy (python x64 ctypes, Wow64 contexts, never kills the target).
#  bp A 0x456FC0 Sound_RestartBufferAtVolume entry : eax=slot edi=volume [esp]=return
#  bp B 0x456DFA DirectSound Lock call inside sub_456D90 (buffer loader): ebx=slot, [esp+8]=wav data bytes
# 'force' = after the battle is up, call HitJudge_StartHitEffect(obj=P1 fighter, kind, dur) via a remote stub for kinds 1..5
import ctypes, struct, sys, time
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.OpenProcess.restype=W.HANDLE; k.OpenThread.restype=W.HANDLE; k.VirtualAllocEx.restype=ctypes.c_void_p
k.VirtualAllocEx.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_size_t,W.DWORD,W.DWORD]
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.WriteProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
k.CreateRemoteThread.restype=W.HANDLE
k.CreateRemoteThread.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_void_p,W.DWORD,ctypes.c_void_p]
k.FlushInstructionCache.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_size_t]
LAUNCH=sys.argv[1]=='launch'
secs=float(sys.argv[2]); out=open(sys.argv[3],'w'); force=len(sys.argv)>4 and sys.argv[4]=='force'
class SI(ctypes.Structure): _fields_=[('cb',W.DWORD),('r',W.LPWSTR),('d',W.LPWSTR),('t',W.LPWSTR),('x',W.DWORD),('y',W.DWORD),('xs',W.DWORD),('ys',W.DWORD),('xc',W.DWORD),('yc',W.DWORD),('fa',W.DWORD),('fl',W.DWORD),('sw',W.WORD),('cb2',W.WORD),('r2',ctypes.c_void_p),('i',W.HANDLE),('o',W.HANDLE),('e',W.HANDLE)]
class PI(ctypes.Structure): _fields_=[('hp',W.HANDLE),('ht',W.HANDLE),('pid',W.DWORD),('tid',W.DWORD)]
if LAUNCH:
    si=SI(); si.cb=ctypes.sizeof(si); pi=PI()
    ok=k.CreateProcessW(r'C:\dev\hantei-chan\work\gof_game\GOF2.exe',None,None,None,False,0x1,None,r'C:\dev\hantei-chan\work\gof_game',ctypes.byref(si),ctypes.byref(pi))
    if not ok: raise SystemExit('CreateProcess failed %d'%ctypes.get_last_error())
    pid=pi.pid
    open('C:\\dev\\hantei-chan\\work\\last_gof_pid.txt','w').write(str(pid))
else:
    pid=int(sys.argv[1])
PROC=0x1F0FFF
h=k.OpenProcess(PROC,False,pid)
if LAUNCH: h=pi.hp
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t()
    if not k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)) or r.value!=n: return None
    return b.raw
def wr(a,data):
    r=ctypes.c_size_t(); k.WriteProcessMemory(h,a,data,len(data),ctypes.byref(r)); k.FlushInstructionCache(h,a,len(data))
def u32(a):
    b=rd(a,4); return None if b is None else struct.unpack('<I',b)[0]
class CTX(ctypes.Structure):
    _fields_=[('ContextFlags',W.DWORD),('Dr0',W.DWORD),('Dr1',W.DWORD),('Dr2',W.DWORD),('Dr3',W.DWORD),('Dr6',W.DWORD),('Dr7',W.DWORD),
      ('Fl',ctypes.c_ubyte*112),('SegGs',W.DWORD),('SegFs',W.DWORD),('SegEs',W.DWORD),('SegDs',W.DWORD),
      ('Edi',W.DWORD),('Esi',W.DWORD),('Ebx',W.DWORD),('Edx',W.DWORD),('Ecx',W.DWORD),('Eax',W.DWORD),
      ('Ebp',W.DWORD),('Eip',W.DWORD),('SegCs',W.DWORD),('EFlags',W.DWORD),('Esp',W.DWORD),('SegSs',W.DWORD),('Ext',ctypes.c_ubyte*512)]
assert ctypes.sizeof(CTX)==716
def getctx(th):
    c=CTX(); c.ContextFlags=0x10007
    if not k.Wow64GetThreadContext(th,ctypes.byref(c)): raise OSError(ctypes.get_last_error())
    return c
def setctx(th,c):
    if not k.Wow64SetThreadContext(th,ctypes.byref(c)): raise OSError(ctypes.get_last_error())
BPS={0x456FC0:'SND',0x456DFA:'LOAD',0x409249:'INPUT'}
orig={}
def log(s): out.write("%d %s\n"%((time.time()-T0)*1000,s)); out.flush()
if not LAUNCH and not k.DebugActiveProcess(pid): raise SystemExit("DebugActiveProcess failed %d"%ctypes.get_last_error())
k.DebugSetProcessKillOnExit(False)
T0=time.time()
for a in BPS:
    orig[a]=rd(a,1); wr(a,b'\xCC')
log("attached; bps %s"%{hex(a):n for a,n in BPS.items()})
class DE(ctypes.Structure): _fields_=[('code',W.DWORD),('pid',W.DWORD),('tid',W.DWORD),('pad',W.DWORD),('u',ctypes.c_ubyte*160)]
threads={}
pending={}   # tid -> bp address awaiting single-step reinsert
def th(tid):
    if tid not in threads: threads[tid]=k.OpenThread(0x1FFFFF,False,tid)
    return threads[tid]
ninp=0
stub=None; forced=[]; nextforce=None; fkinds=[1,2,3,5,4]; fi=0
def start_force(kind,dur=900):
    global stub
    p1=u32(0x65b928)
    if not p1: return False
    wr(p1+0x10ec,struct.pack('<i',0))
    if stub is None: stub=k.VirtualAllocEx(h,None,4096,0x3000,0x40)
    code=b'\xBF'+struct.pack('<I',p1)+b'\x68'+struct.pack('<I',dur)+b'\x68'+struct.pack('<I',kind)+b'\xB8'+struct.pack('<I',0x4317D0)+b'\xFF\xD0'+b'\x31\xC0\xC2\x04\x00'
    wr(stub,code)
    tid=W.DWORD(); ht=k.CreateRemoteThread(h,None,0,stub,None,0,None)
    log("FORCE kind=%d dur=%d obj=0x%X thread=%s"%(kind,dur,p1,bool(ht)))
    return True
def battle_up():
    p=u32(0x65b928)
    if not p: return False
    d=rd(p+0x109c,4); d2=rd(p+0x10e0,4)
    if not d or not d2: return False
    cc=struct.unpack('<I',d2)[0]
    return cc!=0 and u32(cc+16155*4) is not None and u32(cc+16155*4)<12
import os
INJ='/mnt/c/dev/hantei-chan/work/live/inject.txt'.replace('/mnt/c','C:')
INJ='C:\\dev\\hantei-chan\\work\\live\\inject.txt'
inj_m=0; seq=[]; seq_i=0; seq_t=0; cur=(0,0); armed=False
def poll_inject():
    global inj_m,seq,seq_i,seq_t,cur
    try: m=os.path.getmtime(INJ)
    except OSError: return
    if m!=inj_m:
        inj_m=m; seq=[]
        for l in open(INJ):
            l=l.split('#')[0].split()
            if len(l)>=3: seq.append((int(l[0],16),int(l[1],16),int(l[2])))
        seq_i=0; seq_t=time.time(); cur=(seq[0][0],seq[0][1]) if seq else (0,0)
        log("INJECT seq %s"%seq)
    if seq and seq_i<len(seq):
        if seq[seq_i][2]>0 and (time.time()-seq_t)*1000>=seq[seq_i][2]:
            seq_i+=1; seq_t=time.time()
            if seq_i<len(seq): cur=(seq[seq_i][0],seq[seq_i][1])
            else: cur=(0,0)
FRC='C:\\dev\\hantei-chan\\work\\live\\force.txt'
def poll_force():
    if os.path.exists(FRC):
        try:
            L=open(FRC).read().split('\n'); os.remove(FRC)
        except OSError: return
        for l in L:
            l=l.split()
            if len(l)>=2: start_force(int(l[0]),int(l[1]))
last_tick=None; up_since=None
while time.time()-T0<secs:
    poll_inject(); poll_force()
    if os.path.exists('C:\\dev\\hantei-chan\\work\\live\\stop.txt'): break
    ev=DE()
    if not k.WaitForDebugEvent(ctypes.byref(ev),30):
        if force:
            if battle_up():
                up_since=up_since or time.time()
                if time.time()-up_since>3 and (nextforce is None or time.time()>nextforce):
                    if start_force(fkinds[fi%len(fkinds)]): fi+=1; nextforce=time.time()+6
            else: up_since=None
        continue
    cont=0x00010002
    if ev.code==1:
        ecode=struct.unpack_from('<I',bytes(ev.u),0)[0]; eaddr=struct.unpack_from('<Q',bytes(ev.u),16)[0]
        t=th(ev.tid)
        if ecode in (0x80000003,0x4000001F) and eaddr in BPS:
            c=getctx(t); a=eaddr
            if BPS[a]=='INPUT':
                ci=c.Esi//336; ninp+=1
                if ninp<6 or ninp%600==0: log('INPUT hit n=%d esi=%d base=%X seq=%d cur=%s'%(ninp,c.Esi,u32(0x7395D4) or 0,len(seq),cur))
                if ci<2 and seq: wr((u32(0x7395D4) or 0)+c.Esi+0x144,struct.pack('<I',cur[ci]))
            elif BPS[a]=='SND':
                ret=u32(c.Esp); r2=u32(c.Esp+4)
                log("SND slot=%d vol=%d flags=%s ret=0x%X arg=%s tid=%d"%(c.Eax,ctypes.c_int(c.Edi).value,hex(r2 if r2 is not None else 0),ret or 0,hex(u32(c.Esp+8) or 0),ev.tid))
            else:
                log("LOAD slot=%d wavbytes=%s"%(c.Ebx,u32(c.Esp+8)))
            wr(a,orig[a]); c.Eip=a; c.EFlags|=0x100; setctx(t,c); pending[ev.tid]=a
        elif ecode in (0x80000004,0x4000001E) and ev.tid in pending:
            a=pending.pop(ev.tid); wr(a,b'\xCC')
        elif ecode in (0x80000003,0x4000001F,0x4000001E): pass
        else:
            cont=0x80010001; c=getctx(t); log('EXC code=%X addr=%X eip=%X eax=%X edi=%X esp=%X'%(ecode,eaddr,c.Eip,c.Eax,c.Edi,c.Esp))
    elif ev.code==5: log("EXIT_PROCESS"); break
    k.ContinueDebugEvent(ev.pid,ev.tid,cont)
for a in BPS: wr(a,orig[a])
k.DebugActiveProcessStop(pid)
log("detached")
