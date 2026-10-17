import ctypes,struct,sys
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32'); k.OpenProcess.restype=W.HANDLE
k.ReadProcessMemory.argtypes=[W.HANDLE,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
h=k.OpenProcess(0x10,False,int(sys.argv[1]))
def rd(a,n):
    b=ctypes.create_string_buffer(n); r=ctypes.c_size_t(); k.ReadProcessMemory(h,a,b,n,ctypes.byref(r)); return b.raw
base=struct.unpack('<I',rd(0x7395D4,4))[0]; print(hex(base), struct.unpack('<I',rd(0x7395FC-0,4)))
for c in range(3):
    d=rd(base+336*c,336)
    print("ctrl",c)
    for i in range(20):
        t=struct.unpack_from('<i',d,4+4*i)[0]; code=struct.unpack_from('<I',d,164+4*i)[0]; bit=struct.unpack_from('<I',d,244+4*i)[0]
        if t: print(" slot",i,"type",t,"code %X"%code,"bit %X"%bit)
