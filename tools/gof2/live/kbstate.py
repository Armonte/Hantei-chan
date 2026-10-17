import ctypes,struct,sys,time
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32'); k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t(); k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)); return b.raw
u=lambda a: struct.unpack('<I',rd(a,4))[0]
kb=u(0x7395FC+8); st=u(kb+32); print("kb obj %x state %x dev %x"%(kb,st,u(kb+24)))
t0=time.time(); last=None
while time.time()-t0<float(sys.argv[2]):
    s=rd(st,256); pr=[i for i,b in enumerate(s) if b&0x80]
    if pr!=last: print("%.2f"%(time.time()-t0),[hex(i) for i in pr]); last=pr
    time.sleep(0.01)
