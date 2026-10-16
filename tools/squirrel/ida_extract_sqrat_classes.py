# IDAPython: extract the Sqrat classes of a French-Bread Squirrel game and which class each instance-returning wrapper pushes.
# Run inside IDA (ida-mcp execute_python): exec(open(PATH).read()); r = extract_classes(); json.dump(r, open(OUT, "w"))
#
# Method (docs/squirrel/RE_MBTL_LOAD.md, "Sqrat classes"):
#  * a class registration is `mov [ebp+x], offset Sqrat::Class<T,...>::vftable` ... `push offset <ClassObject static>` ...
#    `call Sqrat_CreateClass`, followed by Sqrat_Class_Var*(name, offset) / BindFunc(name, ...) calls and ended by a
#    `*_BindTable("<ScriptName>")` call;
#  * an instance wrapper (calls sq_createinstance) pushes `dword <ClassObject static>` right before;
#  * a field's type is read from the getter its binder installs (Sqrat_SetClosure target): *(_DWORD*) int, *(__int16*) short,
#    unsigned __int8 bool, float float.
# Needs the binder names (Sqrat_CreateClass, Sqrat_Class_Var*, Sqrat_*BindTable, Sqrat_TableBase_BindFunc, Sqrat_SetClosure)
# applied in the IDB first.
import re, idautils, idc, ida_hexrays, ida_funcs

def _name_ea(n):
    ea = idc.get_name_ea_simple(n)
    return None if ea == idc.BADADDR else ea

def _pseudo(ea):
    try:
        return str(ida_hexrays.decompile(ea))
    except Exception:
        return ''

def _push_imm_before(ea, n=12):
    """Values pushed before the call at ea, nearest first: [(kind, value)] kind = 'str' | 'imm' | 'off'."""
    out, h = [], ea
    for _ in range(n):
        h = idc.prev_head(h)
        if h == idc.BADADDR:
            break
        m = idc.print_insn_mnem(h)
        if m == 'call':
            break
        if m == 'push':
            t = idc.get_operand_type(h, 0)
            v = idc.get_operand_value(h, 0)
            fn = ida_funcs.get_func(v) if t == idc.o_imm else None
            if fn is not None and fn.start_ea == v:
                out.append(('imm', v))   # a function pointer argument, never a string
                continue
            s = idc.get_strlit_contents(v, -1, idc.STRTYPE_C) if t in (idc.o_imm, idc.o_mem) else None
            if s:
                out.append(('str', s.decode('cp932', 'replace')))
            elif t == idc.o_imm:
                out.append(('imm', v))
            else:
                out.append(('other', idc.print_operand(h, 0)))
    return out

_TYPE_CACHE = {}
def _getter_type(binder_ea):
    """Field type from the getter closure a Sqrat_Class_Var* binder installs (first Sqrat_SetClosure target)."""
    if binder_ea in _TYPE_CACHE:
        return _TYPE_CACHE[binder_ea]
    t = _pseudo(binder_ea)
    m = re.search(r'(?:Sqrat_SetClosure|sq_newclosure)\([^,]+, (?:\(int\))?(\w+),', t)
    ty = '?'
    if m:
        g = _pseudo(_name_ea(m.group(1)) or 0)
        if m.group(1) == 'Sqrat_Getter_StdString':
            ty = 'string'
        elif 'sq_pushfloat' in g or '(float *)' in g:
            ty = 'float'
        elif '__int16 *)' in g:
            ty = 'short'
        elif 'unsigned __int8 *)' in g or 'unsigned __int8)' in g:
            ty = 'bool'
        elif 'sq_pushinteger' in g:
            ty = 'int'
        elif 'sq_pushstring' in g:
            ty = 'string'
    _TYPE_CACHE[binder_ea] = ty
    return ty

def _vftable_class(ea, back=40):
    """C++ class T from the nearest preceding `offset Sqrat::Class<T,...>::vftable` operand."""
    h = ea
    for _ in range(back):
        h = idc.prev_head(h)
        if h == idc.BADADDR:
            break
        for op in (0, 1):
            v = idc.get_operand_value(h, op)
            n = idc.get_name(v) or ''
            dn = idc.demangle_name(n, idc.get_inf_attr(idc.INF_SHORT_DEMNAMES)) or n
            m = re.search(r'Sqrat::Class<([^,<>]+(?:<[^<>]*>)?),', dn)
            if m:
                return m.group(1)
    return None

_BINDER_CACHE = {}
def _is_var_binder(ea):
    """A field binder by shape: installs a getter and a setter closure carrying the field offset in a stack slot."""
    if ea not in _BINDER_CACHE:
        t = _pseudo(ea)
        closures = t.count('Sqrat_SetClosure') + t.count('sq_newclosure')
        _BINDER_CACHE[ea] = closures >= 2 and ('Sqrat_GetStackSlot' in t or 'sq_newuserdata' in t) and 'sq_pushstring' in t
    return _BINDER_CACHE[ea]

def _closure_type(fn_ea):
    """Field type from a getter closure body; None when the closure is a setter."""
    g = _pseudo(fn_ea)
    if 'sq_push' not in g:
        return None
    if 'sq_pushfloat' in g or '(float *)' in g:
        return 'float'
    if '__int16 *)' in g:
        return 'short'
    if 'unsigned __int8' in g:
        return 'bool'
    if 'sq_pushstring' in g:
        return 'string'
    return 'int'

def _offset_before(h, n=14):
    """The field offset a UNI2-style bind passes by reference: the last `mov [ebp+var], imm` before the call."""
    for _ in range(n):
        h = idc.prev_head(h)
        if idc.print_insn_mnem(h) == 'call':
            break
        if idc.print_insn_mnem(h) == 'mov' and idc.get_operand_type(h, 0) == idc.o_displ and idc.get_operand_type(h, 1) == idc.o_imm:
            return idc.get_operand_value(h, 1)
    return None

def _walk_bindings(start, rec, var_binders, bind_table, bind_func, create):
    """Forward from `start` in its function: field binders, method binds, up to the *_BindTable call that names the class."""
    f = ida_funcs.get_func(start)
    h = start
    while h != idc.BADADDR and h < f.end_ea:
        h = idc.next_head(h, f.end_ea)
        if idc.print_insn_mnem(h) != 'call':
            continue
        tgt = idc.get_operand_value(h, 0)
        pushed = _push_imm_before(h)
        strs = [v for k, v in pushed if k == 'str']
        if tgt not in var_binders and strs and tgt != bind_func and tgt not in bind_table and _is_var_binder(tgt):
            var_binders[tgt] = idc.get_name(tgt)
        if idc.get_name(tgt) == 'Sqrat_Class_BindVarClosure' and strs:
            # UNI2: one call per closure (getter into the get table, setter into the set table); record the field on its getter
            fns = [v for k, v in pushed if k == 'imm' and ida_funcs.get_func(v) is not None and ida_funcs.get_func(v).start_ea == v]
            ty = _closure_type(fns[0]) if fns else None
            if ty and not any(f['name'] == strs[0] for f in rec['fields']):
                rec['fields'].append({'name': strs[0], 'offset': _offset_before(h), 'ea': hex(h), 'type': ty, 'binder': 'Sqrat_Class_BindVarClosure'})
            continue
        if tgt in var_binders and strs:
            imms = [v for k, v in pushed if k == 'imm']
            rec['fields'].append({'name': strs[0], 'offset': imms[0] if imms else None, 'ea': hex(h),
                                  'type': _getter_type(tgt), 'binder': var_binders[tgt]})
        elif tgt == bind_func and strs:
            wrappers = [v for k, v in pushed if k == 'imm' and idc.get_name(v).startswith('SqratWrap_')]
            rec['methods'].append({'name': strs[0], 'ea': hex(h), 'wrapper': idc.get_name(wrappers[0]) if wrappers else None})
        elif tgt in bind_table and strs:
            rec['name'] = strs[0]
            rec['bind_ea'] = hex(h)
            return
        elif tgt == create:
            return

def extract_classes():
    create = _name_ea('sq_newclass') or _name_ea('Sqrat_CreateClass')
    var_binders = {ea: n for ea, n in idautils.Names() if n.startswith('Sqrat_Class_Var')}
    bind_table = {ea for ea, n in idautils.Names() if n.endswith('_BindTable')}
    bind_func = _name_ea('Sqrat_TableBase_BindFunc')
    classes = {}      # static ea -> class record
    for x in idautils.XrefsTo(create):
        if not x.iscode:
            continue
        call = x.frm
        # the ClassObject static: `push offset dword_X` among the next instructions (SetClassParent/GetStackValue/AddRef)
        static, h = None, call
        for _ in range(8):
            h = idc.next_head(h)
            if idc.print_insn_mnem(h) == 'push' and idc.get_operand_type(h, 0) == idc.o_imm:
                v = idc.get_operand_value(h, 0)
                if idc.get_name(v).startswith(('dword_', 'unk_', 'g_')) or ida_funcs.get_func(v) is None:
                    static = v
                    break
        if static is None:
            continue
        cpp = _vftable_class(call)
        rec = {'static': hex(static), 'cpp': cpp, 'name': None, 'parent_table': None, 'fields': [], 'methods': [],
               'site': hex(call)}
        _walk_bindings(call, rec, var_binders, bind_table, bind_func, create)
        if rec['name'] is None:
            # UNI2 layout: the class is created in a per-class constructor; its caller binds the fields and the table
            f = ida_funcs.get_func(call)
            for cx in idautils.XrefsTo(f.start_ea):
                if cx.iscode:
                    _walk_bindings(cx.frm, rec, var_binders, bind_table, bind_func, create)
                    if rec['name']:
                        rec['site'] = hex(cx.frm)
                        break
        classes[static] = rec
    # instance wrappers -> class static
    returns = {}
    sq_ci = _name_ea('sq_createinstance')
    for x in idautils.XrefsTo(sq_ci):
        f = ida_funcs.get_func(x.frm)
        if not f:
            continue
        wname = idc.get_func_name(f.start_ea)
        h = x.frm
        for _ in range(24):
            h = idc.prev_head(h)
            if idc.print_insn_mnem(h) == 'push' and idc.get_operand_type(h, 0) == idc.o_mem:
                v = idc.get_operand_value(h, 0)
                if v in classes and classes[v]['name']:
                    returns[wname] = classes[v]['static']
                    break
    # sqstdlib's own classes (blob, stream) register without a *_BindTable name: not game API
    classes = {k: v for k, v in classes.items() if v['name']}
    unnamed = sorted(n for n in var_binders.values() if not n.startswith('Sqrat_'))
    return {'classes': list(classes.values()), 'wrapper_returns': returns, 'unnamed_binders': unnamed}
