# IDAPython: DFCI (and other older-compiler French-Bread builds) Sqrat API extraction, on top of ida_extract_sqrat_api.py.
# Older MSVC kept ONE template instance per bound signature (UNI2/MBTL fold them into Sqrat_TableBase_BindFunc + a wrapper
# pointer), so binders are recognised by call shape and named <role>_<ADDR>, and argument types come from each native's own
# prototype (the native is passed directly). Run in IDA after exec(open(ida_extract_sqrat_api.py).read()):
#   extract('GetPlayerSide', RAW)            # finds the registration functions and the frequent binders
#   name_binders_by_shape(RAW)               # every (name, fn) / (name, int) / (obj, name, int) callee gets a role name
#   extract_named(RAW); attach_objects(RAW, RAW); native_prototypes(RAW)
# then on the host: fix (int)"name" casts to strings (see below) and run sqrat_api_normalize.py (role() strips _<ADDR>).
import re, json, idc, idautils, ida_funcs, ida_hexrays

def _regfuncs(raw):
    regs = {int(k, 16) for k in json.load(open(raw, encoding='utf-8'))['regfuncs']}
    bt = idc.get_name_ea_simple('Sqrat_RootTable_BindTable')
    for x in idautils.CodeRefsTo(bt, 0):
        f = ida_funcs.get_func(x)
        if f: regs.add(f.start_ea)
    return regs

def _isthis(fn):
    try: return '__thiscall' in str(ida_hexrays.decompile(idc.get_name_ea_simple(fn))).split('\n')[0]
    except Exception: return False

def name_binders_by_shape(raw):
    renamed = {}
    for rf in sorted(_regfuncs(raw)):
        for r in scan_func(rf):
            c, a = r['callee'], r['args']
            if not c.startswith('sub_'): continue
            k = ['str' if 'str' in x else ('fn' if x.get('fn') else ('int' if re.fullmatch(r'-?\d+|0x[0-9A-Fa-f]+', x.get('raw', '')) else 'obj')) for x in a]
            if k == ['str', 'fn']: role = 'Sqrat_Class_BindMethod' if _isthis(a[1]['fn']) else 'Sqrat_TableBase_BindFunc'
            elif k == ['str', 'int']: role = 'Sqrat_Class_VarA'
            elif len(k) == 3 and k[1] == 'str' and k[2] == 'int': role = 'Sqrat_TableBase_SetValueInt'
            else: continue
            ea = idc.get_name_ea_simple(c)
            if c not in renamed:
                renamed[c] = '%s_%X' % (role, ea)
                idc.set_name(ea, renamed[c], idc.SN_NOWARN)
    return renamed

def extract_named(raw):
    regs = _regfuncs(raw)
    out, B = {'binders': [], 'regfuncs': {}}, set()
    for rf in sorted(regs):
        recs = [r for r in scan_func(rf) if r['callee'].startswith('Sqrat_')]
        B |= {r['callee'] for r in recs}
        if recs: out['regfuncs'][hex(rf)] = {'name': idc.get_func_name(rf), 'calls': recs}
    out['binders'] = sorted(B)
    json.dump(out, open(raw, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    return len(B), sum(len(v['calls']) for v in out['regfuncs'].values())

def _ty(t):
    t = t.strip()
    if re.search(r'char\s*\*', t): return 'string'
    if 'float' in t or 'double' in t: return 'float'
    if t in ('bool', '_BOOL1'): return 'bool'
    if '*' in t: return 'instance'
    return 'void' if t == 'void' else 'int'

def _proto(fn):
    try: head = next(l for l in str(ida_hexrays.decompile(idc.get_name_ea_simple(fn))).split('\n') if '(' in l and not l.startswith('//'))
    except Exception: return None
    m = re.match(r'^(.*?)\s*(?:__\w+\s+)?\**[\w:@?$]+\((.*)\)\s*$', head)
    if not m: return None
    member = '__thiscall' in head
    ps = [p.strip() for p in m.group(2).split(',')] if m.group(2).strip() not in ('', 'void') else []
    if member: ps = ps[1:]
    args = [_ty(re.sub(r'\b\w+$', '', re.sub(r'@<\w+>', '', p)).strip() or p) for p in ps]
    return {'ret': _ty(re.sub(r'__\w+', '', m.group(1)).strip()), 'args': args, 'member': member, 'proto': head.strip()}

def native_prototypes(raw):
    d = json.load(open(raw, encoding='utf-8'))
    W = {}
    for v in d['regfuncs'].values():
        for c in v['calls']:
            for a in c['args']:
                f = a.get('fn')
                if f and f not in W:
                    p = _proto(f)
                    if p: W[f] = p
    d['wrappers'] = W
    json.dump(d, open(raw, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    return len(W)
