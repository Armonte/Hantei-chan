# Section 4/5 reader scanner v2 (IDAPython, x86-32; written for docs/formats/ida/sections45_ex.md).
# Flow-sensitive (block-level join) forward taint over every function, plus summary-based interprocedural passes.
# Taint elements (values held in registers / stack slots):
#   ('C',off)  pointer = CharData slot base + off   (slot size SLOT=0x734C, view = CharData+0x1C, sec4 ptr slot = +0x30, sec5 = +0x34)
#   ('A',off)  pointer into g_CharData array, slot unknown (off normalised modulo SLOT)
#   ('S',n)    pattern-area section n pointer (n = (CharData off - 0x20)/4 ; 4 and 5 are targets), ('B',) = view base pointer (+0x1C)
# usage: exec(open(path).read()); q2s_init(gcd, getter, cd_off, cont_off); then call q2s_step() repeatedly (<8 s each) until it returns 'done'.
import idc, idautils, ida_ua, ida_funcs, ida_idp, ida_gdl, time
SLOT = 0x734C
q2s_dc = {}
q2s = dict(hits=[], esc=[], callargs=[], summ={}, done=set(), queue=[], phase=0, pos=0, funcs=[], err=[], cfg={}, nested=0)

def q2s_mem(op):
    if op.type == ida_ua.o_mem:
        idx = None
        if op.specflag1 & 1:
            i = (op.specflag2 >> 3) & 7; idx = None if i == 4 else i
        return (None, idx, 0, op.addr & 0xffffffff)
    if op.type in (ida_ua.o_displ, ida_ua.o_phrase):
        disp = (op.addr & 0xffffffff) if op.type == ida_ua.o_displ else 0
        if disp >= 0x80000000: disp -= 1 << 32
        if op.specflag1 & 1:
            sib = op.specflag2 & 0xff; b = sib & 7; i = (sib >> 3) & 7
            if b == 5 and op.type == ida_ua.o_displ and False: pass
            return (b, None if i == 4 else i, disp, None)
        return (op.phrase, None, disp, None)
    return None

def q2s_init(gcd, getter, nslots, cd_off=0x7EC, cont_off=0x7F0):
    c = q2s['cfg']; c.update(n=nslots, gcd=gcd, getter=getter, cd=cd_off, cont=cont_off)
    q2s['funcs'] = list(idautils.Functions()); q2s['pos'] = 0; q2s['phase'] = 0
    for k in ('hits', 'esc', 'callargs', 'err'): del q2s[k][:]
    q2s['summ'].clear(); q2s['done'].clear(); del q2s['queue'][:]

def q2s_off(t, disp):
    if t[0] == 'A': return (t[1] + disp) % SLOT
    return t[1] + disp

def q2s_join(a, b):
    if a is None: return {k: set(v) for k, v in b.items()}, True
    ch = False
    for k, v in b.items():
        s = a.setdefault(k, set())
        if not v <= s: s |= v; ch = True
    return a, ch

def q2s_analyze(f, argt):
    """argt: dict argindex -> set(taint) ; also keys 'ecx','edx' for register args. Returns set of return taints."""
    cfg = q2s['cfg']; gcd = cfg['gcd']; getter = cfg['getter']
    fn = ida_funcs.get_func(f)
    if fn is None: return set()
    fc = {b.start_ea: b for b in ida_gdl.FlowChart(fn)}
    init = {}
    for k, v in argt.items():
        if k == 'ecx': init[('r', 1)] = set(v)
        elif k == 'edx': init[('r', 2)] = set(v)
        else: init[('s', 4 + 4 * k)] = set(v)
    inn = {fn.start_ea: init}
    work = [fn.start_ea]; ret = set(); iters = 0; visits = {}
    ebp_pos = [None]
    while work and iters < 1500:
        iters += 1
        bs = work.pop()
        visits[bs] = visits.get(bs, 0) + 1
        if visits[bs] > 6: continue
        b = fc.get(bs)
        if b is None: continue
        st = {k: set(v) for k, v in inn[bs].items()}
        a = b.start_ea
        while a < b.end_ea:
            dc = q2s_dc.get(a)
            if dc is None:
                insn = ida_ua.insn_t(); l = ida_ua.decode_insn(insn, a)
                if l == 0: dc = (0, None, None, None, None)
                else: dc = (l, idc.print_insn_mnem(a), idc.get_spd(a) or 0, [o for o in insn.ops if o.type != ida_ua.o_void], insn)
                q2s_dc[a] = dc
            l, mn, spd, ops, insn = dc
            if l == 0: a += 1; continue
            if mn == 'mov' and len(ops) == 2 and ops[0].type == ida_ua.o_reg and ops[0].reg == 5 and ops[1].type == ida_ua.o_reg and ops[1].reg == 4 and ebp_pos[0] is None:
                ebp_pos[0] = spd
            def slot_of(base, disp):
                if base == 4: return spd + disp
                if base == 5 and ebp_pos[0] is not None: return ebp_pos[0] + disp
                return None
            def R(r): return st.get(('r', r), set())
            def setr(r, v):
                if v: st[('r', r)] = set(v)
                else: st.pop(('r', r), None)
            srcval = set(); txt = None
            # ---- memory operand inspection
            for op in ops:
                m = q2s_mem(op)
                if m is None: continue
                base, idx, disp, ab = m
                if ab is not None:
                    if gcd <= ab < gcd + cfg['n'] * SLOT:
                        e = (ab - gcd) % SLOT
                        if e in range(0x1C, 0x40) :
                            q2s['hits'].append((f, a, 'ABS_CD+%#x' % e, idc.GetDisasm(a)))
                            if 0x20 <= e <= 0x3C and e % 4 == 0 and op.type == ida_ua.o_mem and mn != 'lea': srcval.add(('S', (e - 0x20) // 4))
                    continue
                if base is None: continue
                if base in (4, 5) and slot_of(base, disp) is not None: continue
                for t in R(base):
                    if t[0] == 'S' or t[0] == 'B':
                        q2s['hits'].append((f, a, 'DEREF_' + ('S%d' % t[1] if t[0] == 'S' else 'BASE'), idc.GetDisasm(a))); continue
                    e = q2s_off(t, disp)
                    if mn == 'lea': continue
                    if idx is None:
                        if 0x20 <= e <= 0x3C and e % 4 == 0:
                            q2s['hits'].append((f, a, 'SEC%d_PTR_ACCESS' % ((e - 0x20) // 4), idc.GetDisasm(a)))
                            srcval.add(('S', (e - 0x20) // 4))
                        elif e == 0x1C: q2s['hits'].append((f, a, 'BASE_PTR_ACCESS', idc.GetDisasm(a))); srcval.add(('B',))
                    elif 0x14 <= e <= 0x44:
                        q2s['hits'].append((f, a, 'VARIDX_NEAR(eff=%#x)' % e, idc.GetDisasm(a)))
            # ---- transfer
            if mn == 'push' and ops:
                o = ops[0]; v = set()
                if o.type == ida_ua.o_reg: v = R(o.reg)
                elif o.type == ida_ua.o_imm:
                    iv = o.value & 0xffffffff
                    if gcd <= iv < gcd + cfg['n'] * SLOT: v = {('A', (iv - gcd) % SLOT)}
                else:
                    m = q2s_mem(o)
                    if m and m[0] in (4, 5) and slot_of(m[0], m[2]) is not None: v = st.get(('s', slot_of(m[0], m[2])), set())
                    else: v = srcval
                if v: st[('s', spd - 4)] = set(v)
                else: st.pop(('s', spd - 4), None)
            elif mn == 'pop' and ops and ops[0].type == ida_ua.o_reg:
                setr(ops[0].reg, st.get(('s', spd), set()))
            elif mn == 'call':
                o = ops[0]; tgt = o.addr if o.type in (ida_ua.o_near, ida_ua.o_far) else None
                args = {}
                for i in range(0, 10):
                    v = st.get(('s', spd + 4 * i))
                    if v: args[i] = set(v)
                if R(1): args['ecx'] = set(R(1))
                if R(2): args['edx'] = set(R(2))
                rv = set()
                if tgt is not None:
                    if tgt == getter: rv = {('A', 0)}
                    else:
                        rv = set(q2s['summ'].get(tgt, set()))
                        if args and ida_funcs.get_func(tgt):
                            q2s['queue'].append((tgt, args))
                else:
                    if args: q2s['callargs'].append((f, a, idc.GetDisasm(a), {k: sorted(v) for k, v in args.items()}))
                for r in (0, 1, 2): st.pop(('r', r), None)
                if rv: st[('r', 0)] = rv
            elif mn.startswith('ret'):
                ret |= R(0)
            elif mn == 'mov' and len(ops) == 2:
                d, s = ops
                if d.type == ida_ua.o_reg:
                    v = set()
                    if s.type == ida_ua.o_reg: v = R(s.reg)
                    elif s.type == ida_ua.o_imm:
                        iv = s.value & 0xffffffff
                        if gcd <= iv < gcd + cfg['n'] * SLOT: v = {('A', (iv - gcd) % SLOT)}
                    else:
                        m = q2s_mem(s)
                        if m:
                            base, idx, disp, ab = m
                            if base in (4, 5) and slot_of(base, disp) is not None: v = st.get(('s', slot_of(base, disp)), set())
                            else:
                                v = set(srcval)
                                if base is not None and idx is None:
                                    if disp == cfg['cont']: v.add(('C', 0x1C))
                                    elif disp == cfg['cd']: v.add(('C', 0))
                    setr(d.reg, v)
                else:
                    m = q2s_mem(d)
                    v = R(s.reg) if s.type == ida_ua.o_reg else set()
                    if m:
                        base, idx, disp, ab = m
                        if base in (4, 5) and slot_of(base, disp) is not None:
                            k = ('s', slot_of(base, disp))
                            if v: st[k] = set(v)
                            else: st.pop(k, None)
                        elif v:
                            q2s['esc'].append((f, a, idc.GetDisasm(a), sorted(v)))
            elif mn == 'lea' and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                m = q2s_mem(ops[1]); v = set()
                if m:
                    base, idx, disp, ab = m
                    if ab is not None and gcd <= ab < gcd + cfg['n'] * SLOT: v = {('A', (ab - gcd) % SLOT)}
                    elif base is not None and base in (4, 5) and slot_of(base, disp) is not None: v = set()
                    elif base is not None:
                        for t in R(base):
                            if t[0] in ('S', 'B'): v.add(t)
                            else: v.add((t[0], q2s_off(t, disp) if t[0] == 'A' else t[1] + disp))
                    if idx is not None and not v:
                        for t in R(idx):
                            if t[0] in ('A', 'C'): pass
                        if disp and gcd <= (disp & 0xffffffff) < gcd + cfg['n'] * SLOT: v = {('A', ((disp & 0xffffffff) - gcd) % SLOT)}
                setr(ops[0].reg, v)
            elif mn in ('add', 'sub') and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                t = R(ops[0].reg); nv = set()
                if ops[1].type == ida_ua.o_imm:
                    iv = ops[1].value & 0xffffffff
                    if iv >= 0x80000000: iv -= 1 << 32
                    if mn == 'add' and gcd <= (iv & 0xffffffff) < gcd + cfg['n'] * SLOT: nv = {('A', ((iv & 0xffffffff) - gcd) % SLOT)}
                    else:
                        for x in t:
                            nv.add(x if x[0] in ('S', 'B') else (x[0], x[1] + (iv if mn == 'add' else -iv)))
                elif ops[1].type == ida_ua.o_reg and mn == 'add':
                    nv = set(t) | set(R(ops[1].reg))
                else: nv = set(t)
                setr(ops[0].reg, nv)
            elif mn in ('cmp', 'test', 'nop', 'jmp', 'xchg') or mn.startswith('j'):
                pass
            elif mn in ('movs', 'movsd', 'movsb', 'movsw') or mn.startswith('rep'):
                for r in (6, 7):
                    for t in R(r): q2s['esc'].append((f, a, 'MOVS ' + idc.GetDisasm(a), [t]))
            else:
                if ops and ops[0].type == ida_ua.o_reg and ida_idp.has_insn_feature(insn.itype, ida_idp.CF_CHG1):
                    if mn in ('and', 'or', 'xor', 'shl', 'shr', 'sar', 'imul', 'inc', 'dec', 'neg', 'not', 'movzx', 'movsx', 'cdq', 'cwde'):
                        setr(ops[0].reg, set())
                    else: setr(ops[0].reg, set())
                if mn in ('imul', 'mul', 'idiv', 'div', 'cdq', 'cwde'): st.pop(('r', 0), None); st.pop(('r', 2), None)
            a += l
        for s in b.succs():
            ns, ch = q2s_join(inn.get(s.start_ea), st)
            if s.start_ea not in inn or ch:
                inn[s.start_ea] = ns; work.append(s.start_ea)
    return ret

def q2s_step(budget=6.0):
    t0 = time.time()
    while time.time() - t0 < budget:
        if q2s['phase'] == 0:
            if q2s['pos'] >= len(q2s['funcs']):
                q2s['phase'] = 1; q2s['pos'] = 0; q2s['round'] = q2s.get('round', 0) + 1; continue
            f = q2s['funcs'][q2s['pos']]; q2s['pos'] += 1
            try:
                r = q2s_analyze(f, {})
                if r: q2s['summ'][f] = r
            except Exception as e: q2s['err'].append((hex(f), repr(e)))
        elif q2s['phase'] == 1:
            # re-run sources pass once more to pick up summaries, then drain queue
            if q2s.get('round', 0) < 2: q2s['phase'] = 0; del q2s['hits'][:]; del q2s['esc'][:]; del q2s['callargs'][:]; q2s['queue'][:] = []; continue
            q2s['phase'] = 2
        elif q2s['phase'] == 2:
            if not q2s['queue']: return ('done', len(q2s['hits']), len(q2s['esc']), len(q2s['callargs']), len(q2s['summ']), len(q2s['err']))
            tgt, args = q2s['queue'].pop()
            key = (tgt, tuple(sorted((str(k), tuple(sorted(v))) for k, v in args.items())))
            if key in q2s['done']: continue
            q2s['done'].add(key)
            try:
                r = q2s_analyze(tgt, args)
                if r and not r <= q2s['summ'].get(tgt, set()): q2s['summ'].setdefault(tgt, set()).update(r)
            except Exception as e: q2s['err'].append((hex(tgt), repr(e)))
    return ('run', q2s['phase'], q2s['pos'], len(q2s['funcs']), len(q2s['queue']), len(q2s['hits']))
