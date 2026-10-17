# Taint scanner used for docs/formats/ida/sections45_ex.md (IDAPython, x86-32, run via exec(open(path).read())).
# Usage:  q1_main(gcd=<addr of g_CharData array>, getter=<CharData_GetSlot ea>)
# Taint kinds: ('C',off) = pointer to CharData slot base + off (slot size 0x734C),
#              ('A',off) = pointer into the g_CharData array (slot unknown),
#              ('S',n)   = pattern-area section n pointer (n=4 or 5 are the targets).
# Sources: [x+7F0h] = pattern container = CharData+0x1C ; [x+7ECh] = CharData ; call CharData_GetSlot ; imm == array.
# Section n pointer = CharData+0x1C+4+4n -> sec4 = +0x30, sec5 = +0x34.
import idc, idautils, ida_ua, ida_funcs, ida_idp
SLOT = 0x734C
q1_hits = []      # (func, ea, kind, text)
q1_esc = []       # tainted pointer stored to non-stack memory
q1_callargs = []  # tainted args passed to indirect calls
q1_summ = {}      # func -> return taint
q1_seen = set()
q1_cfg = {}

def q1_mem(op):
    """-> (base, index, disp, absaddr) for a memory operand"""
    if op.type == ida_ua.o_mem:
        idx = None
        if op.specflag1 & 1:
            sib = op.specflag2 & 0xff
            i = (sib >> 3) & 7
            idx = None if i == 4 else i
        return (None, idx, 0, op.addr & 0xffffffff)
    if op.type in (ida_ua.o_displ, ida_ua.o_phrase):
        disp = (op.addr & 0xffffffff) if op.type == ida_ua.o_displ else 0
        if disp >= 0x80000000: disp -= 1 << 32
        if op.specflag1 & 1:
            sib = op.specflag2 & 0xff
            b = sib & 7; i = (sib >> 3) & 7
            return (b, None if i == 4 else i, disp, None)
        return (op.phrase, None, disp, None)
    return None

def q1_eff(t, disp):
    if t[0] == 'A': return (t[1] + disp) % SLOT
    return t[1] + disp

def q1_analyze(f, regs0, slots0, depth):
    key = (f, tuple(sorted(regs0.items())), tuple(sorted(slots0.items())))
    if key in q1_seen or len(q1_seen) > 400000: return None
    q1_seen.add(key)
    gcd = q1_cfg['gcd']; getter = q1_cfg['getter']
    regs = dict(regs0); slots = dict(slots0)
    import ida_gdl
    fn_ = ida_funcs.get_func(f); blocks_ = {b.start_ea: b for b in ida_gdl.FlowChart(fn_)}
    a = f
    ebp_pos = None
    ret = None
    work_ = [(f, regs, slots)]; vis_ = set()
    while work_:
        bs_, regs, slots = work_.pop()
        sig_ = (bs_, tuple(sorted(regs.items())), tuple(sorted(slots.items())))
        if sig_ in vis_: continue
        vis_.add(sig_)
        b_ = blocks_.get(bs_)
        if b_ is None: continue
        regs = dict(regs); slots = dict(slots)
        a = b_.start_ea; fe = b_.end_ea
        while a < fe:
            insn = ida_ua.insn_t()
            l = ida_ua.decode_insn(insn, a)
            if l == 0: a += 1; continue
            mn = idc.print_insn_mnem(a)
            spd = idc.get_spd(a) or 0
            ops = [o for o in insn.ops if o.type != ida_ua.o_void]
            if mn == 'mov' and len(ops) == 2 and ops[0].type == ida_ua.o_reg and ops[0].reg == 5 and ops[1].type == ida_ua.o_reg and ops[1].reg == 4 and ebp_pos is None:
                ebp_pos = spd
            # --- memory operand checks / address resolution
            srcval = None   # taint of value loaded by a memory source
            def res_slot(base, disp):
                if base == 4: return spd + disp
                if base == 5 and ebp_pos is not None: return ebp_pos + disp
                return None
            for oi, op in enumerate(ops):
                m = q1_mem(op)
                if m is None: continue
                base, idx, disp, ab = m
                txt = idc.GetDisasm(a)
                if ab is not None:
                    if gcd <= ab < gcd + 64 * SLOT:
                        e = (ab - gcd) % SLOT
                        if 0x20 <= e <= 0x3c and e % 4 == 0:
                            q1_hits.append((f, a, 'ABS_SEC%d_PTR' % ((e - 0x20) // 4), txt))
                            srcval = ('S', (e - 0x20) // 4)
                    continue
                if base is None: continue
                t = regs.get(base)
                if t is None: continue
                if t[0] == 'S':
                    q1_hits.append((f, a, 'DEREF_SEC%d' % t[1], txt)); continue
                e = q1_eff(t, disp)
                if 0x20 <= e <= 0x3c and e % 4 == 0 and idx is None:
                    q1_hits.append((f, a, 'SEC%d_PTR_LOAD' % ((e - 0x20) // 4), txt))
                    srcval = ('S', (e - 0x20) // 4)
                elif idx is not None and 0x14 <= e <= 0x44 and t[0] != 'A':
                    q1_hits.append((f, a, 'VARIDX_NEAR_SEC(eff=%#x)' % e, txt))
            # --- also non-tainted base: stack slot / source loads
            # --- transfer function
            def setreg(r, v):
                if v is None: regs.pop(r, None)
                else: regs[r] = v
            chg1 = ida_idp.has_insn_feature(insn.itype, ida_idp.CF_CHG1) if ops else False
            if mn == 'push' and ops:
                v = None
                o = ops[0]
                if o.type == ida_ua.o_reg: v = regs.get(o.reg)
                elif o.type == ida_ua.o_imm:
                    if gcd <= (o.value & 0xffffffff) < gcd + 64 * SLOT: v = ('A', (o.value - gcd) % SLOT)
                else:
                    m = q1_mem(o)
                    if m and m[0] in (4, 5):
                        k = res_slot(m[0], m[2]); v = slots.get(k) if k is not None else None
                        if v is None and srcval: v = srcval
                    elif srcval: v = srcval
                if v is None: slots.pop(spd - 4, None)
                else: slots[spd - 4] = v
            elif mn == 'pop' and ops and ops[0].type == ida_ua.o_reg:
                setreg(ops[0].reg, slots.get(spd))
            elif mn == 'call':
                o = ops[0]
                tgt = None
                if o.type in (ida_ua.o_near, ida_ua.o_far): tgt = o.addr
                args = {}
                for i in range(0, 10):
                    v = slots.get(spd + 4 * i)
                    if v is not None: args[4 + 4 * i] = v     # entry-relative: arg at esp0+4+4i
                rr = {r: regs[r] for r in (1, 2) if r in regs}
                if tgt is not None:
                    if tgt == getter: rv = ('C', 0)
                    else:
                        rv = q1_summ.get(tgt)
                        if (args or rr) and depth < 6 and ida_funcs.get_func(tgt):
                            s0 = {k: v for k, v in args.items()}
                            r2 = q1_analyze(ida_funcs.get_func(tgt).start_ea, rr, s0, depth + 1)
                    regs.pop(0, None); regs.pop(1, None); regs.pop(2, None)
                    if rv: regs[0] = rv
                else:
                    if args or rr: q1_callargs.append((f, a, idc.GetDisasm(a), args, rr))
                    regs.pop(0, None); regs.pop(1, None); regs.pop(2, None)
            elif mn.startswith('ret'):
                if 0 in regs: ret = regs[0]
            elif mn == 'mov' and len(ops) == 2:
                d, s = ops
                if d.type == ida_ua.o_reg:
                    v = None
                    if s.type == ida_ua.o_reg: v = regs.get(s.reg)
                    elif s.type == ida_ua.o_imm:
                        iv = s.value & 0xffffffff
                        if gcd <= iv < gcd + 64 * SLOT: v = ('A', (iv - gcd) % SLOT if iv >= gcd else 0)
                    else:
                        m = q1_mem(s)
                        if m:
                            base, idx, disp, ab = m
                            if base in (4, 5) and res_slot(base, disp) is not None:
                                v = slots.get(res_slot(base, disp))
                            elif base is not None and disp == q1_cfg['cont'] and regs.get(base) is None: v = ('C', 0x1C)
                            elif base is not None and disp == q1_cfg['cd'] and regs.get(base) is None: v = ('C', 0)
                            elif srcval: v = srcval
                    setreg(d.reg, v)
                else:
                    m = q1_mem(d)
                    v = regs.get(s.reg) if s.type == ida_ua.o_reg else None
                    if m:
                        base, idx, disp, ab = m
                        if base in (4, 5) and res_slot(base, disp) is not None:
                            k = res_slot(base, disp)
                            if v is None: slots.pop(k, None)
                            else: slots[k] = v
                        elif v is not None and v[0] in ('C', 'A') or (v is not None and v[0] == 'S'):
                            q1_esc.append((f, a, idc.GetDisasm(a), v))
            elif mn == 'lea' and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                m = q1_mem(ops[1]); v = None
                if m:
                    base, idx, disp, ab = m
                    if base is not None and regs.get(base) and regs[base][0] != 'S':
                        t = regs[base]; v = (t[0], t[1] + disp)
                        if t[0] == 'A': v = ('A', (t[1] + disp))
                    elif base in (4, 5) and res_slot(base, disp) is not None: v = None
                    elif ab is not None and gcd <= ab < gcd + 64 * SLOT: v = ('A', (ab - gcd) % SLOT)
                    if idx is not None and v is None and regs.get(idx): pass
                regs.pop(ops[0].reg, None)
                if v: regs[ops[0].reg] = v
            elif mn in ('add', 'sub') and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                t = regs.get(ops[0].reg)
                if t and t[0] != 'S' and ops[1].type == ida_ua.o_imm:
                    v = ops[1].value & 0xffffffff
                    if v >= 0x80000000: v -= 1 << 32
                    regs[ops[0].reg] = (t[0], t[1] + (v if mn == 'add' else -v))
                elif t and mn == 'add' and ops[1].type == ida_ua.o_reg:
                    pass   # base + index: keep taint (index ignored)
                elif t and mn == 'add':
                    pass
                else:
                    if mn == 'add' and ops[1].type == ida_ua.o_reg and regs.get(ops[1].reg):
                        regs[ops[0].reg] = regs[ops[1].reg]
                    elif t: regs.pop(ops[0].reg, None)
            elif mn == 'xchg':
                pass
            else:
                if chg1 and ops and ops[0].type == ida_ua.o_reg and mn not in ('cmp', 'test'):
                    regs.pop(ops[0].reg, None)
                if mn in ('imul', 'mul', 'idiv', 'div', 'cdq', 'cwde', 'movs', 'rep', 'cmpxchg'):
                    regs.pop(0, None); regs.pop(2, None)
            a += l
        for s_ in b_.succs(): work_.append((s_.start_ea, regs, slots))
    if ret is not None and ret != q1_summ.get(f):
        q1_summ[f] = ret
    return ret

def q1_main(gcd, getter, cd_off=0x7EC, cont_off=0x7F0, rounds=3):
    q1_cfg['gcd'] = gcd; q1_cfg['getter'] = getter; q1_cfg['cd'] = cd_off; q1_cfg['cont'] = cont_off
    for r in range(rounds):
        before = dict(q1_summ)
        q1_seen.clear(); del q1_hits[:]; del q1_esc[:]; del q1_callargs[:]
        for f in list(idautils.Functions()):
            q1_analyze(f, {}, {}, 0)
        if before == q1_summ: break
    return len(q1_hits), len(q1_esc), len(q1_callargs), len(q1_summ)
