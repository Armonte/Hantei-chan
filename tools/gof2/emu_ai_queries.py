#!/usr/bin/env python3
"""Emulate the GOF2.exe AI move-query predicates (0x466000..0x46B400) with unicorn and print exact truth tables.
Each predicate reads only obj+0x10E0 (charaContext; chara id = dword at ctx+0xFC6C (decompiler shows int-index 16155)), anime.patternNo (obj+0x26C),
anime.frameNo (+0x270), anime.frameTicks (+0x274). Convention for the __usercall ones: eax=obj (4668c0..4698f0, 466660..46b3e0 except
the two fastcall sets 466000/4661f0 which take obj in edx). Output: {func: {chara: {pattern: [(frameLo,frameHi,minTicks)]}}}.
usage: emu_ai_queries.py [exe]  -> writes JSON to stdout"""
import sys, json, struct, pefile
from unicorn import *
from unicorn.x86_const import *
EXE = sys.argv[1] if len(sys.argv) > 1 else '/mnt/c/games/gof/GOF2.exe'
pe = pefile.PE(EXE); base = pe.OPTIONAL_HEADER.ImageBase
mu = Uc(UC_ARCH_X86, UC_MODE_32)
size = (pe.OPTIONAL_HEADER.SizeOfImage + 0xFFF) & ~0xFFF
mu.mem_map(base, size)
mu.mem_write(base, pe.get_memory_mapped_image())
OBJ, CTX, STK, RET = 0x10000000, 0x20000000, 0x30000000, 0x40000000
mu.mem_map(OBJ, 0x2000); mu.mem_map(CTX, 0x11000); mu.mem_map(STK, 0x10000); mu.mem_map(RET, 0x1000)
mu.mem_write(RET, b'\xf4')
mu.mem_write(OBJ + 0x10E0, struct.pack('<I', CTX))
FUNCS = {
 'Obj_IsAtMoveKeyFrameByChara': (0x4668c0, 'eax'), 'Obj_IsInMoveFrameRangeByChara': (0x466f50, 'eax'),
 'Obj_IsInMoveFrameWindow2ByChara': (0x467a10, 'eax'), 'Obj_IsInMoveFrameRange3ByChara': (0x467c50, 'eax'),
 'Obj_IsInMoveFrameWindow4ByChara': (0x468100, 'eax'), 'Obj_IsInMoveFrameRange5ByChara': (0x468840, 'eax'),
 'Obj_IsInMoveFrameRange6ByChara': (0x469270, 'eax'), 'Obj_IsInMoveFrameRange7ByChara': (0x4698f0, 'eax'),
 'Obj_IsInPattern16Or19StartupWindowA': (0x466660, 'eax'), 'Obj_IsInPattern16Or19StartupWindowB': (0x466770, 'eax'),
 'Obj_IsInPattern16Or19StartupWindowC': (0x466850, 'ecx'),
}
def call(ea, reg, chara, pat, frame, ticks):
    mu.mem_write(CTX + 0xFC6C, struct.pack('<I', chara))
    mu.mem_write(OBJ + 0x26C, struct.pack('<III', pat, frame, ticks))
    mu.reg_write(UC_X86_REG_ESP, STK + 0x8000); mu.mem_write(STK + 0x8000, struct.pack('<I', RET))
    for r in ('eax', 'ecx', 'edx'): mu.reg_write({'eax': UC_X86_REG_EAX, 'ecx': UC_X86_REG_ECX, 'edx': UC_X86_REG_EDX}[r], 0)
    mu.reg_write({'eax': UC_X86_REG_EAX, 'ecx': UC_X86_REG_ECX, 'edx': UC_X86_REG_EDX}[reg], OBJ)
    mu.emu_start(ea, RET, count=2000)
    return mu.reg_read(UC_X86_REG_EAX) & 0xFF
def table(ea, reg, charas=range(12), maxframe=40):
    out = {}
    for c in charas:
        for p in range(256):
            hit = []
            for f in range(maxframe):
                a = call(ea, reg, c, p, f, 0); b = call(ea, reg, c, p, f, 200)
                if a or b:
                    if a == b: hit.append((f, 0))
                    else:
                        t = next(t for t in range(201) if call(ea, reg, c, p, f, t))
                        hit.append((f, t if b else None))
            if hit:
                # merge consecutive frames with the same minTicks
                rng = []
                for f, t in hit:
                    if rng and rng[-1][1] == f - 1 and rng[-1][2] == t: rng[-1][1] = f
                    else: rng.append([f, f, t])
                out.setdefault(c, {})[p] = rng
    return out
if __name__ == '__main__':
    # one process per function (unicorn is single-threaded): emu_ai_queries.py <exe> <funcname> -> JSON for that function
    n = sys.argv[2]; ea, reg = FUNCS[n]
    json.dump({n: table(ea, reg, maxframe=100)}, sys.stdout)
