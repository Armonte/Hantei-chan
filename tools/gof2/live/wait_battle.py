import ctypes,struct,sys,time
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32'); k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
def u32(a):
    b=ctypes.create_string_buffer(4); r=ctypes.c_size_t()
    return struct.unpack('<I',b.raw)[0] if k.ReadProcessMemory(h,a,b,4,ctypes.byref(r)) and r.value==4 else None
def tick():
    p=u32(0x65b928); return u32(p+0x1098) if p else None
t0=time.time()
while time.time()-t0<float(sys.argv[2]):
    a=tick(); time.sleep(0.3); b=tick()
    if a is not None and b is not None and a!=b: print("BATTLE tick",a,b); break
