# IDAPython: extract the Sqrat-registered native API of a French-Bread Squirrel-2.2.4 game.
# Run inside IDA (ida-mcp execute_python): exec(open(PATH).read()); extract(seed_name, out_json)
# Pass 1: find the TableBase::Func helper = callee of the call that registers `seed_name`.
# Pass 2: every function that calls a binder is a registration function; every call in it with
#         a string literal is recorded with the function pointers passed beside it.
import re, json, idautils, idc, ida_funcs, ida_hexrays, ida_bytes, ida_nalt

CALL_RE = re.compile(r'(\b[\w:~`\']+)\((.*)\);\s*$')
ASSIGN_RE = re.compile(r'^\s*(v\d+|a\d+)\s*=\s*(.+?);\s*$')
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')

def pseudo(ea):
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception:
        return ''

def find_string_ea(s):
    for st in idautils.Strings():
        if str(st) == s:
            return st.ea
    return None

def split_args(a):
    out, depth, cur = [], 0, ''
    for ch in a:
        if ch in '([': depth += 1
        if ch in ')]': depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip()); cur = ''
        else:
            cur += ch
    if cur.strip(): out.append(cur.strip())
    return out

def scan_func(fea):
    """Return list of (callee, [args]) for calls that carry a string literal, resolving &vN to funcs."""
    txt = pseudo(fea)
    assigned, recs, tables = {}, [], {}
    # join calls that Hex-Rays wrapped over several lines (unbalanced parentheses)
    lines, buf = [], ''
    for raw in txt.split('\n'):
        buf = (buf + ' ' + raw.strip()) if buf else raw
        if buf.count('(') <= buf.count(')'):
            lines.append(buf); buf = ''
    if buf:
        lines.append(buf)
    for line in lines:
        m = ASSIGN_RE.match(line)
        if m and '(' not in m.group(2).replace('(int)', '').replace('(int (__cdecl *)(int))', ''):
            assigned[m.group(1)] = re.sub(r'^\([^)]*\)\s*', '', m.group(2).replace('(int (__cdecl *)(int))', ''))
        m = CALL_RE.search(line.strip())
        if not m or '"' not in m.group(2):
            continue
        callee = m.group(1)
        args = split_args(m.group(2))
        res = []
        for a in args:
            mm = re.match(r'^\([^()]*\)\s*&(v\d+)$|^&(v\d+)$', a)
            if mm:
                v = mm.group(1) or mm.group(2)
                val = assigned.get(v)
                if val and re.match(r'^(sub_[0-9A-F]+|[A-Za-z_]\w*)$', val) and not val.startswith(('v', 'a')):
                    res.append({'ref': v, 'fn': val})
                else:
                    res.append({'ref': v, 'value': val})
            elif a.startswith('"'):
                sm = STR_RE.match(a)
                res.append({'str': sm.group(1) if sm else a})
            else:
                fm = re.search(r'(sub_[0-9A-F]+)', a)
                fn = fm.group(1) if fm else None
                if not fn:
                    nm = re.match(r'^\(int\)\s*([A-Za-z_][\w@?$]*)$', a)
                    if nm and ida_funcs.get_func(idc.get_name_ea_simple(nm.group(1))):
                        fn = nm.group(1)
                res.append({'raw': a, 'fn': fn})
        recs.append({'callee': callee, 'args': res})
    return recs

def extract(seed_name, out_json, max_rounds=3):
    s_ea = find_string_ea(seed_name)
    assert s_ea, 'seed string not found'
    x = next(iter(idautils.XrefsTo(s_ea)))
    f = ida_funcs.get_func(x.frm)
    # seed binder: callee of the first call with seed_name
    binders = set()
    for r in scan_func(f.start_ea):
        if any(a.get('str') == seed_name for a in r['args']):
            binders.add(r['callee'])
    regfuncs, done = {f.start_ea}, set()
    for _ in range(max_rounds):
        for b in list(binders):
            bea = idc.get_name_ea_simple(b)
            if bea == idc.BADADDR: continue
            for c in idautils.CodeRefsTo(bea, 0):
                cf = ida_funcs.get_func(c)
                if cf: regfuncs.add(cf.start_ea)
        # binders = callees that take a string literal in >=3 calls across regfuncs
        cnt = {}
        for rf in regfuncs:
            if rf in done: continue
            for r in scan_func(rf):
                cnt[r['callee']] = cnt.get(r['callee'], 0) + 1
            done.add(rf)
        binders |= {k for k, v in cnt.items() if v >= 3}
    out = {'binders': sorted(binders), 'regfuncs': {}}
    for rf in sorted(regfuncs):
        recs = [r for r in scan_func(rf) if r['callee'] in binders]
        if recs:
            out['regfuncs'][hex(rf)] = {'name': idc.get_func_name(rf), 'calls': recs}
    with open(out_json, 'w', encoding='utf-8') as fp:
        json.dump(out, fp, indent=1, ensure_ascii=False)
    return {'binders': len(binders), 'regfuncs': len(out['regfuncs']),
            'calls': sum(len(v['calls']) for v in out['regfuncs'].values())}


# ---- wrapper signature inference (needs sq_* names applied in the IDB) ----
GET_PATTERNS = [
    (r'sq_getinteger\(a1, (\d+)', 'int'), (r'sq_getfloat\(a1, (\d+)', 'float'),
    (r'sq_getstring\(a1, (\d+)', 'string'), (r'sq_tostring\(a1, (\d+)', 'object'),
    (r'sq_getbool\(a1, (\d+)', 'bool'), (r'Sqrat_ObjectFromStack\((?:[^,]*, )?a1, (\d+)', 'string'),
    (r'sq_getinstanceup\(a1, (\d+)', 'instance'), (r'sq_getuserpointer\(a1, (\d+)', 'userptr'),
]
PUSH_PATTERNS = [('sq_pushinteger', 'int'), ('sq_pushfloat', 'float'), ('sq_pushstring', 'string'),
                 ('sq_pushbool', 'bool'), ('sq_createinstance', 'instance'), ('sq_newarray', 'array'),
                 ('sq_pushobject', 'object')]

def classify_wrapper(name_or_ea, depth=0):
    ea = name_or_ea if isinstance(name_or_ea, int) else idc.get_name_ea_simple(name_or_ea)
    t = pseudo(ea)
    params = {}
    for pat, ty in GET_PATTERNS:
        for m in re.finditer(pat, t):
            i = int(m.group(1))
            if ty == 'instance' and i == 1:
                params[1] = 'this'
                continue
            params.setdefault(i, ty)
    ret = 'void'
    body = t.split('\n')
    for fn, ty in PUSH_PATTERNS:
        if fn + '(' in t:
            ret = ty
            break
    if ret == 'void' and re.search(r'return 1;', t):
        ret = 'value'
    # nested template helpers: classify the single non-sq callee if nothing was found
    if not params and ret in ('void', 'value') and depth < 2:
        callees = [c for c in set(re.findall(r'(sub_[0-9A-F]+)\(', t))]
        if len(callees) == 1:
            return classify_wrapper(callees[0], depth + 1)
    args = [params[k] for k in sorted(params) if params[k] != 'this']
    return {'ret': ret, 'args': args, 'member': params.get(1) == 'this'}

def classify_all(raw_json, out_json):
    d = json.load(open(raw_json, encoding='utf-8'))
    cache = {}
    for rf, v in d['regfuncs'].items():
        for r in v['calls']:
            fns = [a.get('fn') for a in r['args'] if a.get('fn')]
            for f in fns:
                if f not in cache:
                    cache[f] = classify_wrapper(f)
    d['wrappers'] = cache
    with open(out_json, 'w', encoding='utf-8') as fp:
        json.dump(d, fp, indent=1, ensure_ascii=False)
    return len(cache)

# ---- asm-level object attribution (decompiler hides the implicit `this` of chained thiscalls) ----
def _resolve_reg_back(ea, reg, fstart, limit=25):
    cur = ea
    for _ in range(limit):
        cur = idc.prev_head(cur, fstart)
        if cur == idc.BADADDR or cur < fstart:
            return None
        mn = idc.print_insn_mnem(cur)
        if idc.print_operand(cur, 0) == reg and mn in ('lea', 'mov'):
            return idc.print_operand(cur, 1)
    return None

def ecx_obj(call_ea, known=None):
    """`this` of a thiscall. Sqrat's chained API (Func(..).Func(..)) passes the previous call's
    return value (eax == previous this), so follow the chain through `known` call sites."""
    f = ida_funcs.get_func(call_ea)
    cur = call_ea
    for _ in range(60):
        cur = idc.prev_head(cur, f.start_ea)
        if cur == idc.BADADDR or cur < f.start_ea:
            return None
        mn = idc.print_insn_mnem(cur)
        if mn == 'call':
            # ecx not set since the previous call: chained on its return value
            return (known or {}).get(cur, 'chain@' + hex(cur))
        if idc.print_operand(cur, 0) == 'ecx' and mn in ('lea', 'mov'):
            op = idc.print_operand(cur, 1)
            if op == 'eax':
                c2 = cur
                for _ in range(10):
                    c2 = idc.prev_head(c2, f.start_ea)
                    if idc.print_insn_mnem(c2) == 'call':
                        return (known or {}).get(c2, 'chain@' + hex(c2))
            if op in ('esi', 'edi', 'ebx'):
                return _resolve_reg_back(cur, op, f.start_ea) or op
            return op
    return None

def nth_push(call_ea, n):
    """operand of the n-th push before call_ea (0 = first argument), register pushes resolved."""
    f = ida_funcs.get_func(call_ea); cur = call_ea; k = -1
    for _ in range(40):
        cur = idc.prev_head(cur, f.start_ea)
        if cur == idc.BADADDR or cur < f.start_ea:
            return None
        if idc.print_insn_mnem(cur) == 'push':
            k += 1
            if k == n:
                op = idc.print_operand(cur, 0)
                if op in ('eax', 'ebx', 'ecx', 'edx', 'esi', 'edi'):
                    return _resolve_reg_back(cur, op, f.start_ea) or op
                return op
    return None

def site_string(call_ea):
    f = ida_funcs.get_func(call_ea); cur = call_ea
    for _ in range(12):
        cur = idc.prev_head(cur, f.start_ea)
        if idc.print_insn_mnem(cur) == 'push':
            x = idc.get_operand_value(cur, 0)
            b = idc.get_strlit_contents(x) if x else None
            if b:
                return b.decode('cp932', 'replace')
    return None

def attach_objects(raw_json, out_json, bind_name='Sqrat_RootTable_BindTable'):
    """Pair each text call with its code call site (same callee, address order) and record obj operands."""
    d = json.load(open(raw_json, encoding='utf-8'))
    stats = {'paired': 0, 'mismatch': 0}
    for rf, v in d['regfuncs'].items():
        fstart = int(rf, 16)
        f = ida_funcs.get_func(fstart)
        sites = {}
        for h in idautils.FuncItems(f.start_ea):
            if idc.print_insn_mnem(h) == 'call':
                sites.setdefault(idc.get_name(idc.get_operand_value(h, 0)), []).append(h)
        known = {}
        bycallee = {}
        for c in v['calls']:
            bycallee.setdefault(c['callee'], []).append(c)
        for callee, calls in bycallee.items():
            s = sites.get(callee, [])
            if len(s) == len(calls):
                for c, ea in zip(calls, s):
                    c['ea'] = hex(ea)
                    stats['paired'] += 1
                continue
            # counts differ: pair by the string literal pushed at each site
            stats['mismatch'] += 1
            pool = {}
            for c in calls:
                st = next((a['str'] for a in c['args'] if 'str' in a), None)
                pool.setdefault(st, []).append(c)
            for ea in s:
                st = site_string(ea)
                if st in pool and pool[st]:
                    pool[st].pop(0)['ea'] = hex(ea)
                    stats['paired'] += 1
        for c in v['calls']:
            if c['callee'] == bind_name and 'ea' in c:
                c['bind_obj'] = nth_push(int(c['ea'], 16), 1)
        # resolve `this` in address order so chains inherit the previous binder's object
        paired = sorted((int(c['ea'], 16), c) for c in v['calls'] if 'ea' in c)
        for ea, c in paired:
            c['ecx'] = ecx_obj(ea, known)
            known[ea] = c['ecx']
    with open(out_json, 'w', encoding='utf-8') as fp:
        json.dump(d, fp, indent=1, ensure_ascii=False)
    return stats
