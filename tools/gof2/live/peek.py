import ctypes,struct,sys
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32'); k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t(); k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)); return b.raw
u=lambda a: struct.unpack('<I',rd(a,4))[0]
ds=u(0x7396d0); print("DefStatus %x +32=%d +48=%d +52=%d"%(ds,u(ds+32),u(ds+48),u(ds+52)))
p=u(0x65b928); na=u(p+0x1254); print("nodeA4 %x"%na)
obj=na+84; vt=u(obj); print("obj %x vtbl %x vf16 %x"%(obj,vt,u(vt+16)))
print("gauge fields", [struct.unpack('<i',rd(obj+4*i,4))[0] for i in range(0,16)])
