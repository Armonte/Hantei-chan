import ctypes,struct,sys,time
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32'); k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t(); k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)); return b.raw
base=struct.unpack('<I',rd(0x7395D4,4))[0]
t0=time.time(); last=None
while time.time()-t0<float(sys.argv[2]):
    w=[struct.unpack('<I',rd(base+336*c+324,4))[0] for c in range(2)]
    ks=rd(struct.unpack('<I',rd(0x58f359c-0,4))[0],4) if False else b''
    if w!=last: print("%.2f"%(time.time()-t0),[hex(x) for x in w]); last=w
    time.sleep(0.01)
