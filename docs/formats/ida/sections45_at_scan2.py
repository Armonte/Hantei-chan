# AT-record (RboAtRecord, 120 B) reader scanner v2 (IDAPython, x86-32; written for docs/formats/ida/sections45_ex.md).
# Flow-sensitive forward taint (block-level join, 6 visits/block cap) over every function + queue-based interprocedural passes.
# Taint elements:
#   ('AT',off)  pointer to the current attack record (+off)   source: mov r,[x+AT_OFF]   (actor+0x750 in rbo.exe)
#   ('ST',off)  pointer to the actor attack state (+off)      source: lea r,[x+ST_OFF]   (actor+0x714 in rbo.exe; state+0x3C == AT_OFF)
#   ('V',off)   value loaded from AT field +off
# Outputs: q3['reads'] = (func, ea, field_off, text, 'R'|'W'); q3['bits'] = (func, ea, field_off, mnemonic, imm|None); q3['esc'] = tainted value stored to memory.
# usage: exec(open(path).read()); q3_init(at_off, st_off, st_at_off=0x3C  [0x40 in ex2/ex3]); then call q3_step() until it returns ('done', ...).
import idc, idautils, ida_ua, ida_funcs, ida_idp, ida_gdl, time
q3_dc = {}
q3 = dict(reads=[], bits=[], esc=[], calls=[], callargs=[], summ={}, done=set(), queue=[], phase=0, pos=0, funcs=[], err=[], cfg={}, round=0, stf=[])

def q3_r(o):
    # IDA numbers byte regs al..bl = 16..19, ah..bh = 20..23; fold onto the dword register index
    return o.reg if o.reg < 16 else ((o.reg - 16) & 3 if o.reg < 24 else o.reg)

def q3_mem(op):
    if op.type == ida_ua.o_mem:
        return (None, None, 0, op.addr & 0xffffffff)
    if op.type in (ida_ua.o_displ, ida_ua.o_phrase):
        disp = (op.addr & 0xffffffff) if op.type == ida_ua.o_displ else 0
        if disp >= 0x80000000: disp -= 1 << 32
        if op.specflag1 & 1:
            sib = op.specflag2 & 0xff; b = sib & 7; i = (sib >> 3) & 7
            return (b, None if i == 4 else i, disp, None)
        return (op.phrase, None, disp, None)
    return None

def q3_init(at_off, st_off, st_at=0x3C):
    q3['cfg'].update(at=at_off, st=st_off, stat=st_at)
    q3['funcs'] = list(idautils.Functions()); q3['pos'] = 0; q3['phase'] = 0; q3['round'] = 0
    for k in ('reads', 'bits', 'esc', 'callargs', 'err', 'stf', 'calls'): del q3[k][:]
    q3['summ'].clear(); q3['done'].clear(); del q3['queue'][:]

def q3_join(a, b):
    if a is None: return {k: set(v) for k, v in b.items()}, True
    ch = False
    for k, v in b.items():
        s = a.setdefault(k, set())
        if not v <= s: s |= v; ch = True
    return a, ch

def q3_analyze(f, argt):
    cfg = q3['cfg']; AT = cfg['at']; ST = cfg['st']
    fn = ida_funcs.get_func(f)
    if fn is None: return set()
    fc = {b.start_ea: b for b in ida_gdl.FlowChart(fn)}
    init = {}
    for k, v in argt.items():
        if k == 'ecx': init[('r', 1)] = set(v)
        elif k == 'edx': init[('r', 2)] = set(v)
        else: init[('s', 4 + 4 * k)] = set(v)
    inn = {fn.start_ea: init}; work = [fn.start_ea]; ret = set(); iters = 0; visits = {}; ebp_pos = [None]
    while work and iters < 1500:
        iters += 1
        bs = work.pop(); visits[bs] = visits.get(bs, 0) + 1
        if visits[bs] > 6: continue
        b = fc.get(bs)
        if b is None: continue
        st = {k: set(v) for k, v in inn[bs].items()}
        a = b.start_ea
        while a < b.end_ea:
            dc = q3_dc.get(a)
            if dc is None:
                insn = ida_ua.insn_t(); l = ida_ua.decode_insn(insn, a)
                if l == 0: dc = (0, None, None, None, None, None)
                else: dc = (l, idc.print_insn_mnem(a), idc.get_spd(a) or 0, [o for o in insn.ops if o.type != ida_ua.o_void], insn, ida_idp.has_insn_feature(insn.itype, ida_idp.CF_CHG1))
                q3_dc[a] = dc
            l, mn, spd, ops, insn, chg1 = dc
            if l == 0: a += 1; continue
            if mn == 'mov' and len(ops) == 2 and ops[0].type == ida_ua.o_reg and q3_r(ops[0]) == 5 and ops[1].type == ida_ua.o_reg and q3_r(ops[1]) == 4 and ebp_pos[0] is None:
                ebp_pos[0] = spd
            def slot_of(base, disp):
                if base == 4: return spd + disp
                if base == 5 and ebp_pos[0] is not None: return ebp_pos[0] + disp
                return None
            def R(r): return st.get(('r', r), set())
            def setr(r, v):
                if v: st[('r', r)] = set(v)
                else: st.pop(('r', r), None)
            srcval = set()
            imm = None
            for o in ops:
                if o.type == ida_ua.o_imm: imm = o.value & 0xffffffff
            # tests on tainted register values: bits
            if mn in ('test', 'and', 'or', 'cmp', 'xor', 'bt', 'shr', 'shl', 'sub', 'add', 'sar') and ops and ops[0].type == ida_ua.o_reg and not (len(ops) == 2 and ops[1].type == ida_ua.o_reg and q3_r(ops[1]) == q3_r(ops[0]) and mn in ('xor', 'sub', 'test', 'cmp', 'and', 'or')):
                for t in R(q3_r(ops[0])):
                    if t[0] == 'V':
                        iv = imm if len(ops) > 1 and ops[1].type == ida_ua.o_imm else ('reg' if len(ops) > 1 and ops[1].type == ida_ua.o_reg else None)
                        if isinstance(iv, int) and 20 <= ops[0].reg < 24: iv <<= 8      # ah/ch/dh/bh tests bits 8..15
                        q3['bits'].append((f, a, t[1], mn, iv))
            if mn in ('test', 'cmp') and len(ops) == 2 and ops[0].type == ida_ua.o_reg and ops[1].type == ida_ua.o_reg:
                for t in R(q3_r(ops[1])):
                    if t[0] == 'V': q3['bits'].append((f, a, t[1], mn, 'reg' + str(q3_r(ops[0]))))
            for oi, op in enumerate(ops):
                m = q3_mem(op)
                if m is None: continue
                base, idx, disp, ab = m
                if base is None or (base in (4, 5) and slot_of(base, disp) is not None): continue
                for t in R(base):
                    if t[0] == 'AT':
                        e = t[1] + disp
                        wr = (oi == 0 and chg1 and mn not in ('cmp', 'test'))
                        if mn == 'lea': q3['reads'].append((f, a, e, idc.GetDisasm(a), 'LEA')); continue
                        q3['reads'].append((f, a, e, idc.GetDisasm(a), 'W' if wr else 'R'))
                        if idx is not None: q3['reads'].append((f, a, e, 'VARIDX ' + idc.GetDisasm(a), 'R'))
                        if not wr: srcval.add(('V', e))
                        if len(ops) == 2 and ops[1].type == ida_ua.o_imm and mn in ('test', 'and', 'or', 'cmp'):
                            q3['bits'].append((f, a, e, mn, imm))
                    elif t[0] == 'ST':
                        e = t[1] + disp
                        if e == cfg['stat'] and idx is None and mn != 'lea': srcval.add(('AT', 0))
                        else: q3['stf'].append((f, a, e))
            # transfer
            if mn == 'push' and ops:
                o = ops[0]; v = set()
                if o.type == ida_ua.o_reg: v = R(q3_r(o))
                elif o.type != ida_ua.o_imm:
                    m = q3_mem(o)
                    if m and m[0] in (4, 5) and slot_of(m[0], m[2]) is not None: v = st.get(('s', slot_of(m[0], m[2])), set())
                    else: v = srcval
                if v: st[('s', spd - 4)] = set(v)
                else: st.pop(('s', spd - 4), None)
            elif mn == 'pop' and ops and ops[0].type == ida_ua.o_reg:
                setr(q3_r(ops[0]), st.get(('s', spd), set()))
            elif mn == 'call':
                o = ops[0]; tgt = o.addr if o.type in (ida_ua.o_near, ida_ua.o_far) else None
                args = {}
                for i in range(0, 12):
                    v = st.get(('s', spd + 4 * i))
                    if v: args[i] = set(v)
                if R(1): args['ecx'] = set(R(1))
                if R(2): args['edx'] = set(R(2))
                rv = set()
                if tgt is not None:
                    rv = set(q3['summ'].get(tgt, set()))
                    if args and ida_funcs.get_func(tgt): q3['queue'].append((tgt, args)); q3['calls'].append((f, a, tgt, {k: sorted(v) for k, v in args.items()}))
                elif args: q3['callargs'].append((f, a, idc.GetDisasm(a), {k: sorted(v) for k, v in args.items()}))
                for r in (0, 1, 2): st.pop(('r', r), None)
                if rv: st[('r', 0)] = rv
            elif mn.startswith('ret'):
                ret |= {t for t in R(0) if t[0] != 'V'}
            elif mn in ('mov', 'movzx', 'movsx') and len(ops) == 2:
                d, s = ops
                if d.type == ida_ua.o_reg:
                    v = set()
                    if s.type == ida_ua.o_reg: v = R(q3_r(s))
                    elif s.type != ida_ua.o_imm:
                        m = q3_mem(s)
                        if m:
                            base, idx, disp, ab = m
                            if base in (4, 5) and slot_of(base, disp) is not None: v = st.get(('s', slot_of(base, disp)), set())
                            else:
                                v = set(srcval)
                                if base is not None and idx is None and disp == AT and mn == 'mov': v.add(('AT', 0))
                    setr(q3_r(d), v)
                else:
                    m = q3_mem(d); v = R(q3_r(s)) if s.type == ida_ua.o_reg else set()
                    if m:
                        base, idx, disp, ab = m
                        if base in (4, 5) and slot_of(base, disp) is not None:
                            k = ('s', slot_of(base, disp))
                            if v: st[k] = set(v)
                            else: st.pop(k, None)
                        elif v: q3['esc'].append((f, a, idc.GetDisasm(a), sorted(v)))
            elif mn == 'lea' and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                m = q3_mem(ops[1]); v = set()
                if m:
                    base, idx, disp, ab = m
                    if base is not None and base not in (4, 5):
                        if disp == ST and idx is None: v.add(('ST', 0))
                        for t in R(base):
                            if t[0] in ('AT', 'ST'): v.add((t[0], t[1] + disp))
                setr(q3_r(ops[0]), v)
            elif mn in ('add', 'sub') and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                t = R(q3_r(ops[0])); nv = set()
                if ops[1].type == ida_ua.o_imm:
                    iv = ops[1].value & 0xffffffff
                    if iv >= 0x80000000: iv -= 1 << 32
                    for x in t:
                        if x[0] in ('AT', 'ST'): nv.add((x[0], x[1] + (iv if mn == 'add' else -iv)))
                else: nv = {x for x in t if x[0] in ('AT', 'ST')}
                if mn == 'add' and ops[1].type == ida_ua.o_imm and (ops[1].value & 0xffffffff) == ST: nv.add(('ST', 0))   # add reg, ST_OFF (actor -> attack state)
                setr(q3_r(ops[0]), nv)
            elif mn in ('cmp', 'test', 'nop', 'jmp', 'xchg') or mn.startswith('j'):
                pass
            elif mn.startswith('movs') or mn.startswith('rep'):
                for r in (6, 7):
                    for t in R(r): q3['esc'].append((f, a, 'MOVS ' + idc.GetDisasm(a), [t]))
            else:
                if ops and ops[0].type == ida_ua.o_reg and chg1 and mn not in ('not', 'neg', 'shr', 'shl', 'sar', 'and', 'or'): setr(q3_r(ops[0]), set())
                if mn in ('and', 'or') and ops and ops[0].type == ida_ua.o_reg and len(ops) == 2 and ops[1].type == ida_ua.o_reg: setr(q3_r(ops[0]), set())
                if mn in ('imul', 'mul', 'idiv', 'div', 'cdq', 'cwde'): st.pop(('r', 0), None); st.pop(('r', 2), None)
            a += l
        for s in b.succs():
            ns, ch = q3_join(inn.get(s.start_ea), st)
            if s.start_ea not in inn or ch:
                inn[s.start_ea] = ns; work.append(s.start_ea)
    return ret

def q3_step(budget=6.0):
    t0 = time.time()
    while time.time() - t0 < budget:
        if q3['phase'] == 0:
            if q3['pos'] >= len(q3['funcs']):
                q3['round'] += 1; q3['pos'] = 0
                if q3['round'] < 2:
                    for k in ('reads', 'bits', 'esc', 'callargs', 'stf', 'calls'): del q3[k][:]
                    del q3['queue'][:]; continue
                q3['phase'] = 2; continue
            f = q3['funcs'][q3['pos']]; q3['pos'] += 1
            try:
                r = q3_analyze(f, {})
                if r: q3['summ'][f] = r
            except Exception as e: q3['err'].append((hex(f), repr(e)))
        else:
            if not q3['queue']: return ('done', len(q3['reads']), len(q3['bits']), len(q3['esc']), len(q3['callargs']), len(q3['err']))
            tgt, args = q3['queue'].pop()
            key = (tgt, tuple(sorted((str(k), tuple(sorted(v))) for k, v in args.items())))
            if key in q3['done']: continue
            q3['done'].add(key)
            try:
                r = q3_analyze(tgt, args)
                if r: q3['summ'].setdefault(tgt, set()).update(r)
            except Exception as e: q3['err'].append((hex(tgt), repr(e)))
    return ('run', q3['phase'], q3['round'], q3['pos'], len(q3['funcs']), len(q3['queue']), len(q3['reads']))
