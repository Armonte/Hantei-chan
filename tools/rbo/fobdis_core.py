import struct
def load_fob(b):
    p=0
    nf,=struct.unpack_from('<I',b,p);p+=4
    funcs=[]
    for i in range(nf):
        nm=b[p:p+32].split(b'\0')[0].decode('latin1');pc,=struct.unpack_from('<I',b,p+32);funcs.append((nm,pc));p+=36
    nt,=struct.unpack_from('<I',b,p);p+=4
    types=[]
    for i in range(nt):
        c,=struct.unpack_from('<I',b,p);p+=4
        types.append(list(struct.unpack_from('<%di'%c,b,p)));p+=4*c
    cs,=struct.unpack_from('<I',b,p);p+=4
    code=b[p:p+cs]
    return dict(funcs=funcs,types=types,code=code,trail=len(b)-(p+cs))

U16=struct.Struct('<H'); U32=struct.Struct('<I')
ARITH={0xB:'add',0xC:'sub',0xD:'mul',0xE:'div',0xF:'mod',0x10:'shl',0x11:'shr',0x12:'and',0x13:'or',0x15:'xor',0x17:'land',0x18:'lor'}
class Bad(Exception): pass
def decode(code,pc):
    """return (cls,sub,length,ops dict) ; raises Bad"""
    if pc<0 or pc+2>len(code): raise Bad('oob')
    cls,=U16.unpack_from(code,pc)
    if cls in (0x18,0x19): return cls,0,2,{}
    if cls>0x17: raise Bad('class %x'%cls)
    if pc+4>len(code): raise Bad('oob')
    sub,=U16.unpack_from(code,pc+2)
    def u16(o): return U16.unpack_from(code,pc+o)[0]
    def u32(o): return U32.unpack_from(code,pc+o)[0]
    if cls==0:
        if sub>1: raise Bad('c0 sub')
        n=u32(4); return cls,sub,8+4*n,{'n':n}
    if cls==1:
        if sub==0: return cls,sub,8,{'imm':u32(4)}
        if sub==1: return cls,sub,8,{'off':u32(4)}
        if 2<=sub<=9 or sub==0x1A: return cls,sub,4,{}
        if sub==0xA: return cls,sub,8,{'flags':u16(4),'kind':u16(6)}
        if sub==0x19: return cls,sub,8,{'flags':u16(4),'kind':u16(6)}
        if 0xB<=sub<=0x1B: return cls,sub,6,{'flags':u16(4)}
        raise Bad('c1 sub %x'%sub)
    if cls==2:
        if sub==0: return cls,sub,12,{'flags':u16(4),'kind':u16(6),'target':u32(8)}
        if sub==1: return cls,sub,10,{'flags':u16(4),'table':u32(6)}
        if sub in (2,3): return cls,sub,8,{'target':u32(4)}
        if 4<=sub<=7: return cls,sub,4,{}
        raise Bad('c2 sub')
    if cls==3:
        if sub in (0,2,5): return cls,sub,8,{'n':u32(4)}
        if sub in (1,3,4): return cls,sub,4,{}
        raise Bad('c3 sub')
    return cls,sub,4,{}
