# IDAPython (Hex-Rays) scan: which byte offsets of the GOF2 attack record (Gof2AtRecord, 236 bytes) are read, and by which function.
# Usage inside GOF2.exe.i64:  exec(open(r"C:\dev\hantei-chan\tools\gof2\ida_at_record_scan.py").read()); at_scan_run()
# Model: the record pointer reaches the hit code ONLY through Obj+0x620 (= CAppHanteiKougeki+0xC, the kougeki object is embedded at Obj+0x614).
#   class R = record pointer (value loaded from [x+0x620], or from [K+0xC]), class K = kougeki object pointer (x+0x614).
# Seeds: every function containing a [reg+614h]/[reg+620h] operand; callees are followed when an R/K variable is passed as a call argument.
# Result: at_cov[offset] = set(function ea). Offsets >= 0xEC are over-propagation noise (obj fields of the attacker passed along) and ignored.
import re, time, collections, idautils, idc
import ida_hexrays as hx
from ida_hexrays import *

def strip(e):
    while e.op == cot_cast: e = e.x
    return e
def psize(t):
    try:
        if t.is_ptr(): return t.get_pointed_object().get_size()
    except Exception: pass
    return 1
def boff(e):
    e = strip(e)
    if e.op == cot_ptr:
        sz = e.type.get_size(); a = strip(e.x)
        if a.op == cot_add and strip(a.y).op == cot_num:
            return (strip(a.x), strip(a.y).numval() * psize(a.x.type), sz)
        if a.op == cot_var: return (a, 0, sz)
    if e.op == cot_idx and strip(e.y).op == cot_num:
        return (strip(e.x), strip(e.y).numval() * e.type.get_size(), e.type.get_size())
    if e.op in (cot_memptr, cot_memref): return (strip(e.x), e.m, e.type.get_size())
    return None
def classify(e, cls):
    e = strip(e)
    if e.op == cot_var: return cls.get(e.v.idx)
    b = boff(e) if e.op in (cot_ptr, cot_idx, cot_memptr, cot_memref) else None
    if b:
        base, off, sz = b
        if off == 0x620: return 'R'
        if base.op == cot_var and cls.get(base.v.idx) == 'K' and off == 12: return 'R'
    if e.op == cot_add and strip(e.y).op == cot_num and strip(e.y).numval() * psize(e.x.type) == 0x614: return 'K'
    if e.op == cot_ref and e.x.op in (cot_ptr, cot_idx, cot_memptr):
        b = boff(e.x)
        if b and b[1] == 0x614: return 'K'
    return None
class Prop(hx.ctree_visitor_t):
    def __init__(s, cls): super().__init__(hx.CV_FAST); s.cls = cls; s.changed = False
    def visit_expr(s, e):
        if e.op == cot_asg:
            l = strip(e.x)
            if l.op == cot_var:
                c = classify(e.y, s.cls)
                if c and s.cls.get(l.v.idx) != c: s.cls[l.v.idx] = c; s.changed = True
        return 0
class Acc(hx.ctree_visitor_t):
    def __init__(s, cls, acc, calls): super().__init__(hx.CV_PARENTS); s.cls = cls; s.acc = acc; s.calls = calls
    def visit_expr(s, e):
        if e.op in (cot_ptr, cot_idx, cot_memptr, cot_memref):
            b = boff(e)
            if b:
                base, off, sz = b
                c = s.cls.get(base.v.idx) if base.op == cot_var else classify(base, s.cls)
                if c == 'R': s.acc.append((off, sz, e.ea))
        if e.op == cot_call:
            for i, a in enumerate(e.a):
                c = classify(a, s.cls)
                if c: s.calls.append((c, i, e.x.obj_ea if e.x.op == cot_obj else -1, e.ea))
        return 0
def argidx(cf):
    lv = cf.get_lvars(); return [i for i in range(len(lv)) if lv[i].is_arg_var]
def analyze(f, init):
    cf = hx.decompile(f); ai = argidx(cf); cls = {}
    for i, c in init:
        if i < len(ai): cls[ai[i]] = c
    for _ in range(6):
        p = Prop(cls); p.apply_to(cf.body, None)
        if not p.changed: break
    acc, calls = [], []
    Acc(cls, acc, calls).apply_to(cf.body, None)
    return acc, calls
def seeds():
    out = {}
    for f in idautils.Functions(0x401000, 0x53C000):
        for h in idautils.Heads(f, idc.find_func_end(f)):
            if re.search(r'\[\w+\+(620h|614h)\]', idc.GetDisasm(h)):
                out[f] = 1; break
    return sorted(out)
at_done = {}; at_cov = {}; at_queue = []
def at_scan_run(budget=8.0):
    global at_queue
    if not at_queue and not at_done: at_queue = [(f, ()) for f in seeds()]
    t = time.time()
    while at_queue and time.time() - t < budget:
        f, init = at_queue.pop(0)
        if (f, init) in at_done: continue
        try: acc, calls = analyze(f, init)
        except Exception as ex: at_done[(f, init)] = None; continue
        at_done[(f, init)] = (acc, calls)
        for o, sz, ea in acc:
            if o < 0xEC: at_cov.setdefault(o, set()).add(f)
        for c, i, tgt, ea in calls:
            if tgt > 0 and tgt != f and (tgt, ((i, c),)) not in at_done: at_queue.append((tgt, ((i, c),)))
    return len(at_queue), len(at_done)
def at_scan_report():
    for o in sorted(at_cov): print(hex(o), [idc.get_func_name(f) for f in sorted(at_cov[o])])
# Not covered by the call-argument propagation (read by hand): HitJudge_ComputeDamage 0x494FF0 takes a struct whose first member is the kougeki pointer
# (reads +0x10, +0x80; sub_495890 reads +0x00, +0x80); sub_4954F0/sub_4952F0 likewise (+0x10).
