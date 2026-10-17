#!/usr/bin/env python3
"""dMp (Drill Milky Punch, DMP.EXE 2003) FOB probe: parse + linear-sweep disassemble + re-serialize, byte-exact check.
File layout (Script_LoadBank 0x412D40):  u32 nFuncs; {char name[32]; u32 pc}[nFuncs]; u32 codeSize; u8 code[codeSize]
(NO index tables, unlike RBO).  Instruction = u16 opcode + operands (docs/formats/dmp.md).
usage: dmp_fob_probe.py <GAMEDATA.PAC> [-v]   or   dmp_fob_probe.py file.fob ..."""
import sys, struct, re, subprocess, os

# opcode -> operand form.  forms: '' (2 bytes), 'imm' (u32), 'fl' (u16 flags), 'flk' (u16 flags,u16 kind),
# 'jcc' (u16 flags,u16 kind,u32 target), 'sw' (u16 flags,u32 tablePc), 'n' (u32 n), 'data' (u32 n + n dwords)
FORMS = {}
def _f(f, ops):
    for o in ops: FORMS[o] = f
_f('data', [0x01, 0x02])
_f('imm',  [0x03, 0x04, 0x1A, 0x1B])
_f('flk',  [0x09, 0x18])
_f('fl',   list(range(0x0A, 0x18)) + [0x27])
_f('jcc',  [0x19])
_f('sw',   [0x25])
_f('imm',  [0x26, 0x2F, 0x51])
# everything else known to the dispatcher is a bare 2-byte op; filled in by KNOWN below
ALL_OPS = [int(x,16) for x in '''01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1E 1F 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A 3B 3C 3D 3E 3F 40 41 42 43 44 45 46 47 48 49 4A 4B 4C 4D 4E 4F 50 51 54 55 57 58 59 5B 5C 61 62 63 67 68 69 6A 6B 6C 6E 6F 70 73 74 75 76 77 7E 7F 80 81 82 83 84 85 86 87 88 89 8A 8B 8C 8D 8E 8F 92 93 94 95 96 97 98 99 9D 9F A0'''.split()]
# opcodes present in shipped scripts but with NO case in Script_RunThread of this build (-> 'ScriptUnknown Code!!', thread slice ends):
# 0x90 0x91 (STAGE01/02, 2-byte natives after PUSH_IMM) and 0xA1 (end-of-code sentinel after the last YIELD of ZAKOTEXTURELOAD/INITCONFIG)
UNIMPLEMENTED = [0x90, 0x91, 0xA1]
for _o in ALL_OPS + UNIMPLEMENTED: FORMS.setdefault(_o, '')

def u16(b,p): return struct.unpack_from('<H',b,p)[0]
def u32(b,p): return struct.unpack_from('<I',b,p)[0]

def insn_len(code, pc):
    op = u16(code, pc); f = FORMS.get(op)
    if f is None: return None
    n = {'':2,'imm':6,'fl':4,'flk':6,'jcc':10,'sw':8}.get(f)
    if f == 'data':
        n = 6 + 4*u32(code, pc+2)
    return n

def parse(b):
    nf = u32(b,0); p = 4; funcs=[]
    for i in range(nf):
        funcs.append((b[p:p+32], u32(b,p+32))); p += 36
    csz = u32(b,p); p += 4
    code = b[p:p+csz]; tail = b[p+csz:]
    return funcs, code, tail

TERMINATORS = {0xA1, 0x1A, 0x1E, 0x1F, 0x21, 0x4B, 0x54, 0x2D, 0x55}

def descend(code, funcs):
    """Recursive descent from the named entry points (and JMP/JCC/CALL/SWITCH targets).
    Returns (insns {pc:len}, problems[])."""
    seen = {}; probs = []; work = [0] + [pc for _, pc in funcs]   # pc 0 = unnamed preamble chunk (LINE markers; INITCONFIG has it before GameInit@12)
    while work:
        pc = work.pop()
        while True:
            if pc in seen: break
            if pc < 0 or pc + 2 > len(code): probs.append(('pc out of range', pc)); break
            n = insn_len(code, pc)
            if n is None or pc + n > len(code):
                probs.append(('bad insn op %#x' % u16(code, pc), pc)); break
            seen[pc] = n
            op = u16(code, pc)
            if op == 0x19: work.append(u32(code, pc+6))
            elif op in (0x1A, 0x1B): work.append(u32(code, pc+2))
            elif op == 0x25:
                t = u32(code, pc+4)
                if t + 8 > len(code): probs.append(('switch table oob', pc)); break
                work.append(u32(code, t)); cnt = u32(code, t+4)
                for k in range(cnt):
                    if t + 8 + 8*k + 8 > len(code): probs.append(('switch entry oob', pc)); break
                    work.append(u32(code, t + 8 + 8*k + 4))
            if op in TERMINATORS: break
            pc += n
    return seen, probs

def serialize(funcs, code):
    o = struct.pack('<I', len(funcs))
    for nm, pc in funcs: o += nm + struct.pack('<I', pc)
    return o + struct.pack('<I', len(code)) + code

def opnames():
    h = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'formats', 'ida', 'dmp_types.h')
    d = {}
    try:
        for m in re.finditer(r'DMPOP_(\w+) = 0x([0-9A-Fa-f]+)', open(h).read()): d[int(m.group(2), 16)] = m.group(1)
    except OSError: pass
    return d

def disasm(code, funcs, only=None):
    names = opnames(); seen, _ = descend(code, funcs)
    fpc = {pc: nm.split(b'\0')[0].decode('latin1') for nm, pc in funcs}
    out = []
    for pc in sorted(seen):
        if only is not None and pc not in only: continue
        op = u16(code, pc); n = seen[pc]; f = FORMS[op]
        if pc in fpc: out.append('%s:' % fpc[pc])
        a = ''
        if f == 'data': a = 'n=%d %s' % (u32(code, pc+2), code[pc+6:pc+n].hex())
        elif f == 'imm': a = '%#x' % u32(code, pc+2)
        elif f == 'fl': a = 'flags=%#x' % u16(code, pc+2)
        elif f == 'flk': a = 'flags=%#x kind=%#x' % (u16(code, pc+2), u16(code, pc+4))
        elif f == 'jcc': a = 'flags=%#x kind=%#x ->%#x' % (u16(code, pc+2), u16(code, pc+4), u32(code, pc+6))
        elif f == 'sw': a = 'flags=%#x table=%#x' % (u16(code, pc+2), u32(code, pc+4))
        out.append('  %05x  %-26s %s' % (pc, names.get(op, '%#x' % op), a))
    return '\n'.join(out)

def main(argv):
    if argv and argv[0] == '--dis':
        b = open(argv[1], 'rb').read(); funcs, code, tail = parse(b)
        print(disasm(code, funcs)); return 0

    files = {}
    if argv and argv[0].lower().endswith('.pac'):
        out = subprocess.run(['/home/teo/dev/hantei-chan-wt/fb-formats/build/fbarctool.exe','ls',
                              'C:/'+argv[0].split('/mnt/c/',1)[1] if argv[0].startswith('/mnt/c/') else argv[0]],
                             capture_output=True, text=True).stdout
        with open(argv[0],'rb') as fh:
            for l in out.splitlines():
                m = re.match(r'\s*(\d+)\s+(\d+)\s+(\d+)\s+(\S+\.FOB)$', l)
                if m:
                    fh.seek(int(m.group(2))); files[m.group(4)] = fh.read(int(m.group(3)))
    else:
        for a in argv:
            files[os.path.basename(a)] = open(a,'rb').read()
    ok = bad = 0; ninsn = 0; hist = {}; covered = total = 0
    for name, b in sorted(files.items()):
        funcs, code, tail = parse(b)
        seen, probs = descend(code, funcs)
        rt = serialize(funcs, code) + tail
        good = (not probs) and rt == b and len(tail) == 0
        for pc in seen:
            op = u16(code,pc); hist[op] = hist.get(op,0)+1
        ninsn += len(seen); covered += sum(seen.values()); total += len(code)
        if good: ok += 1
        else:
            bad += 1; print('FAIL', name, probs[:3])
    print('code bytes decoded %d of %d (rest = data regions kept verbatim)' % (covered, total))
    print('files %d ok %d bad %d instructions %d distinct ops %d' % (len(files), ok, bad, ninsn, len(hist)))
    if '-v' in argv:
        print(' '.join('%02X:%d' % kv for kv in sorted(hist.items())))
    return 0 if bad == 0 else 1

if __name__ == '__main__':
    sys.exit(main([a for a in sys.argv[1:] if a != '-v'] + (['-v'] if '-v' in sys.argv else [])))
