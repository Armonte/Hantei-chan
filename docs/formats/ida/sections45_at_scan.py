# AT-record reader scanner (IDAPython, x86-32). exec(open(path).read()); q2_main(at_off, st_off)
# Taint kinds: ('AT',off) pointer to current attack record + off ; ('ST',off) pointer to actor attack state + off.
# Sources: [x+at_off] (actor+0x750 in rbo.exe) -> AT ; lea r,[x+st_off] (actor+0x714) -> ST ; deref ST at +0x3C -> AT.
# Output: q2_reads = [(func, ea, field_off, text)] for every memory operand whose base is a tainted AT pointer.
import idc, idautils, ida_ua, ida_funcs, ida_idp
q2_watch = []; q2_reads = []; q2_esc = []; q2_stfields = []; q2_calls = []; q2_seen = set(); q2_cfg = {}

def q2_mem(op):
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

def q2_an(f, regs0, slots0, depth):
    key = (f, tuple(sorted(regs0.items())), tuple(sorted(slots0.items())))
    if key in q2_seen or len(q2_seen) > 400000: return
    q2_seen.add(key)
    regs = dict(regs0); slots = dict(slots0)
    import ida_gdl
    fn_ = ida_funcs.get_func(f); blocks_ = {b.start_ea: b for b in ida_gdl.FlowChart(fn_)}
    ebp_pos = None
    AT = q2_cfg['at']; ST = q2_cfg['st']
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
            insn = ida_ua.insn_t(); l = ida_ua.decode_insn(insn, a)
            if l == 0: a += 1; continue
            mn = idc.print_insn_mnem(a); spd = idc.get_spd(a) or 0
            ops = [o for o in insn.ops if o.type != ida_ua.o_void]
            if mn == 'mov' and len(ops) == 2 and ops[0].type == ida_ua.o_reg and ops[0].reg == 5 and ops[1].type == ida_ua.o_reg and ops[1].reg == 4 and ebp_pos is None:
                ebp_pos = spd
            def rs(base, disp):
                if base == 4: return spd + disp
                if base == 5 and ebp_pos is not None: return ebp_pos + disp
                return None
            srcval = None
            for op in ops:
                m = q2_mem(op)
                if not m: continue
                base, idx, disp, ab = m
                if base is None: continue
                t = regs.get(base)
                if t is None: continue
                if t[0] == 'AT':
                    q2_reads.append((f, a, t[1] + disp, idc.GetDisasm(a)))
                elif t[0] == 'ST':
                    q2_stfields.append((f, a, t[1] + disp, idc.GetDisasm(a)))
                    if t[1] + disp == q2_cfg['stat']: srcval = ('AT', 0)
            def setreg(r, v):
                if v is None: regs.pop(r, None)
                else: regs[r] = v
            chg1 = ida_idp.has_insn_feature(insn.itype, ida_idp.CF_CHG1) if ops else False
            if mn == 'push' and ops:
                o = ops[0]; v = None
                if o.type == ida_ua.o_reg: v = regs.get(o.reg)
                elif o.type != ida_ua.o_imm:
                    m = q2_mem(o)
                    if m and m[0] in (4, 5) and rs(m[0], m[2]) is not None: v = slots.get(rs(m[0], m[2]))
                    elif srcval: v = srcval
                    elif m and m[0] is not None and regs.get(m[0]) is None and m[2] == AT: v = ('AT', 0)
                if v is None: slots.pop(spd - 4, None)
                else: slots[spd - 4] = v
            elif mn == 'pop' and ops and ops[0].type == ida_ua.o_reg:
                setreg(ops[0].reg, slots.get(spd))
            elif mn == 'call':
                o = ops[0]; tgt = o.addr if o.type in (ida_ua.o_near, ida_ua.o_far) else None
                args = {}
                for i in range(10):
                    v = slots.get(spd + 4 * i)
                    if v is not None: args[4 + 4 * i] = v
                rr = {r: regs[r] for r in (1, 2) if r in regs}
                if tgt is not None:
                    fn = ida_funcs.get_func(tgt)
                    if args and tgt in q2_cfg.get('watch', ()): q2_watch.append((f, a, tgt, dict(args)))
                    if (args or rr) and depth < 8 and fn: q2_an(fn.start_ea, rr, args, depth + 1)
                elif args or rr: q2_calls.append((f, a, idc.GetDisasm(a), args, rr))
                regs.pop(0, None); regs.pop(1, None); regs.pop(2, None)
            elif mn == 'mov' and len(ops) == 2:
                d, s = ops
                if d.type == ida_ua.o_reg:
                    v = None
                    if s.type == ida_ua.o_reg: v = regs.get(s.reg)
                    elif s.type != ida_ua.o_imm:
                        m = q2_mem(s)
                        if m:
                            base, idx, disp, ab = m
                            if base in (4, 5) and rs(base, disp) is not None: v = slots.get(rs(base, disp))
                            elif base is not None and regs.get(base) is None and disp == AT: v = ('AT', 0)
                            elif srcval: v = srcval
                    setreg(d.reg, v)
                else:
                    m = q2_mem(d); v = regs.get(s.reg) if s.type == ida_ua.o_reg else None
                    if m:
                        base, idx, disp, ab = m
                        if base in (4, 5) and rs(base, disp) is not None:
                            k = rs(base, disp)
                            if v is None: slots.pop(k, None)
                            else: slots[k] = v
                        elif v is not None: q2_esc.append((f, a, idc.GetDisasm(a), v))
            elif mn == 'lea' and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                m = q2_mem(ops[1]); v = None
                if m and m[0] is not None:
                    base, idx, disp, ab = m
                    t = regs.get(base)
                    if t: v = (t[0], t[1] + disp)
                    elif base not in (4, 5) and disp == ST: v = ('ST', 0)
                regs.pop(ops[0].reg, None)
                if v: regs[ops[0].reg] = v
            elif mn in ('add', 'sub') and len(ops) == 2 and ops[0].type == ida_ua.o_reg:
                t = regs.get(ops[0].reg)
                if t and ops[1].type == ida_ua.o_imm:
                    v = ops[1].value & 0xffffffff
                    if v >= 0x80000000: v -= 1 << 32
                    regs[ops[0].reg] = (t[0], t[1] + (v if mn == 'add' else -v))
                elif t: pass
                elif mn == 'add' and ops[1].type == ida_ua.o_imm and (ops[1].value & 0xffffffff) == ST: regs[ops[0].reg] = ('ST', 0)
            else:
                if chg1 and ops and ops[0].type == ida_ua.o_reg and mn not in ('cmp', 'test'):
                    regs.pop(ops[0].reg, None)
                if mn in ('imul', 'mul', 'idiv', 'div', 'cdq', 'cwde', 'movs', 'rep', 'cmpxchg'):
                    regs.pop(0, None); regs.pop(2, None)
            a += l

        for s_ in b_.succs(): work_.append((s_.start_ea, regs, slots))
def q2_main(at_off, st_off, stat=0x3C):
    q2_cfg['at'] = at_off; q2_cfg['st'] = st_off; q2_cfg['stat'] = stat
    for L in (q2_reads, q2_esc, q2_stfields, q2_calls): del L[:]
    q2_seen.clear()
    for f in list(idautils.Functions()): q2_an(f, {}, {}, 0)
    return len(q2_reads), len(q2_esc), len(q2_stfields), len(q2_calls)
