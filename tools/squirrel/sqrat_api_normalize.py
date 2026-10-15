#!/usr/bin/env python3
"""Normalize the raw IDA Sqrat extraction (ida_extract_sqrat_api.py) into a per-game API table.

usage: sqrat_api_normalize.py <game>_raw.json <game>_api.json <game>_api.md
Binder kinds are recognised by the names applied in the IDB (see SCOPE.md, "IDB names").
"""
import json, re, sys, collections

KIND = {
    'Sqrat_TableBase_BindFunc': 'func',
    'Sqrat_Class_BindMethod': 'method',
    'Sqrat_Enumeration_ConstInt': 'const',
    'Sqrat_TableBase_SetValueInt': 'const',
}
VAR_PREFIX = 'Sqrat_Class_Var'

def role(callee):
    """Binder role: games built by an older compiler keep one template instance per signature, named <role>_<ADDR> in the IDB (DFCI)."""
    return re.sub(r'_[0-9A-F]{5,8}$', '', callee)
BIND = 'Sqrat_RootTable_BindTable'

def objvar(arg):
    if arg.get('ref'):
        return arg['ref']
    raw = arg.get('raw') or ''
    m = re.search(r'&?(v\d+)\b', raw)
    return m.group(1) if m else None

def asm_attribute(rf, v, W):
    """Attribution with asm operands: an entry belongs to the next BindTable whose bound object
    is the entry's `this` (ecx). Binds themselves are nested the same way (parent table)."""
    calls = [c for c in v['calls'] if any('str' in a for a in c['args'])]
    out = []
    for i, c in enumerate(calls):
        callee, args = c['callee'], c['args']
        sidx = next(j for j, a in enumerate(args) if 'str' in a)
        name = args[sidx]['str']
        if callee == BIND:
            kind = 'table'
        else:
            kind = KIND.get(role(callee)) or ('var' if callee.startswith(VAR_PREFIX) else None)
        if not kind:
            continue
        owner = None
        tobj = objvar(args[0]) if sidx > 0 else None
        if tobj:
            # the decompiler shows the object explicitly: trust it over the asm operand
            for c2 in calls[i + 1:]:
                if c2['callee'] == BIND:
                    j = next(k for k, a in enumerate(c2['args']) if 'str' in a)
                    if len(c2['args']) > j + 1 and objvar(c2['args'][j + 1]) == tobj:
                        owner = next(a['str'] for a in c2['args'] if 'str' in a)
                        break
            if owner is None and kind == 'const':
                owner = '(consttable)'  # Sqrat::ConstTable: global compile-time constants
        elif c.get('ecx'):
            for c2 in calls[i + 1:]:
                if c2['callee'] == BIND and c2.get('bind_obj') == c['ecx']:
                    owner = next(a['str'] for a in c2['args'] if 'str' in a)
                    break
        e = {'kind': kind, 'name': name, 'table': owner or ('' if kind == 'table' else '(root/unbound:%s)' % v['name']), 'regfunc': rf, 'ea': c.get('ea')}
        fns = [a.get('fn') for a in args[sidx + 1:] if a.get('fn')]
        if kind in ('func', 'method') and fns:
            e['native'], e['wrapper'] = fns[0], fns[-1]
            sig = W.get(fns[-1]) or {}
            e['ret'], e['args'] = sig.get('ret', '?'), sig.get('args', [])
        elif kind == 'const':
            vals = [a.get('value') or a.get('raw') for a in args[sidx + 1:]]
            e['value'] = vals[0] if vals else None
        elif kind == 'var':
            vals = [a.get('raw') for a in args[sidx + 1:]]
            e['offset'] = vals[0] if vals else None
            e['vartype'] = callee
        out.append(e)
    return out

def cast_strings(d):
    """(int)\"name\" arguments (older Hex-Rays output for BindTable) count as string literals."""
    for v in d['regfuncs'].values():
        for c in v['calls']:
            for a in c['args']:
                m = re.fullmatch(r'\(int\)"((?:[^"\\]|\\.)*)"', a.get('raw', ''))
                if m and 'str' not in a: a['str'] = m.group(1)

def main(raw, out_json, out_md):
    d = json.load(open(raw, encoding="utf-8"))
    cast_strings(d)
    W = d.get('wrappers', {})
    entries = []
    for rf, v in d['regfuncs'].items():
        if any('ecx' in c for c in v['calls']):
            entries.extend(asm_attribute(rf, v, W))
            continue
        pending = []
        for c in v['calls']:
            callee, args = c['callee'], c['args']
            sidx = next((i for i, a in enumerate(args) if 'str' in a), None)
            if sidx is None:
                continue
            name = args[sidx]['str']
            obj = objvar(args[0]) if sidx > 0 else None
            if callee == BIND:
                ref = args[sidx + 1].get('ref') if len(args) > sidx + 1 else None
                ref = ref or objvar(args[sidx + 1]) if len(args) > sidx + 1 else None
                keep = []
                for e in pending:
                    if e['_obj'] is None or e['_obj'] == ref:
                        e['table'] = name
                    else:
                        keep.append(e)
                pending = keep
                entries.append({'kind': 'table', 'name': name, 'table': '', 'regfunc': rf, '_obj': None})
                continue
            kind = KIND.get(role(callee)) or ('var' if callee.startswith(VAR_PREFIX) else None)
            if not kind:
                continue
            e = {'kind': kind, 'name': name, 'table': None, 'regfunc': rf, '_obj': obj}
            fns = [a.get('fn') for a in args[sidx + 1:] if a.get('fn')]
            if kind in ('func', 'method') and fns:
                native, wrapper = fns[0], fns[-1]
                e['native'] = native
                e['wrapper'] = wrapper
                sig = W.get(wrapper) or {}
                e['ret'] = sig.get('ret', '?')
                e['args'] = sig.get('args', [])
            elif kind == 'const':
                vals = [a.get('value') or a.get('raw') for a in args[sidx + 1:]]
                e['value'] = vals[0] if vals else None
            elif kind == 'var':
                vals = [a.get('raw') for a in args[sidx + 1:]]
                e['offset'] = vals[0] if vals else None
                e['vartype'] = callee
            pending.append(e)
            entries.append(e)
        for e in pending:
            e['table'] = e['table'] or '(root/unbound:%s)' % v['name']
    for e in entries:
        e.pop('_obj', None)
    # qualify nested tables/classes: Parent.Child
    parent = {e['name']: e['table'] for e in entries if e['kind'] == 'table' and e['table']}
    def qual(t):
        seen = set()
        while t in parent and parent[t] and t not in seen and not parent[t].startswith('('):
            seen.add(t); t = parent[t] + '.' + t.split('.')[-1] if False else parent[t] + '.' + t
            break
        return t
    for e in entries:
        if e['kind'] != 'table' and e['table'] and not e['table'].startswith('('):
            e['qualified_table'] = qual(e['table'])
    json.dump(entries, open(out_json, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    by = collections.OrderedDict()
    for e in entries:
        if e['kind'] == 'table':
            continue
        by.setdefault(e['table'], []).append(e)
    cnt = collections.Counter(e['kind'] for e in entries)
    with open(out_md, 'w', encoding='utf-8') as fp:
        fp.write('# Native Squirrel API (generated from the IDB, do not edit)\n\n')
        fp.write('Source: `%s`. Counts: %s.\n\n' % (raw.split('/')[-1], ', '.join('%s %d' % kv for kv in sorted(cnt.items()))))
        fp.write('Types are inferred from the Sqrat wrapper (`sq_get*`/`sq_push*` calls); `value` = pushes a non-primitive, `string` may also accept an int via Sqrat::Object.\n\n')
        for t, es in by.items():
            fp.write('## %s (%d)\n\n' % (t, len(es)))
            for e in es:
                if e['kind'] in ('func', 'method'):
                    fp.write('- `%s %s(%s)` %s native `%s` wrapper `%s`\n' % (e.get('ret', '?'), e['name'], ', '.join(e.get('args', [])), e['kind'], e.get('native'), e.get('wrapper')))
                elif e['kind'] == 'const':
                    fp.write('- const `%s = %s`\n' % (e['name'], e.get('value')))
                else:
                    fp.write('- var `%s` @%s (%s)\n' % (e['name'], e.get('offset'), e.get('vartype')))
            fp.write('\n')
    print(dict(cnt), 'tables:', len(by))

if __name__ == '__main__':
    main(*sys.argv[1:4])
