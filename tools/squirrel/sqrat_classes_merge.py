#!/usr/bin/env python3
"""Merge the Sqrat class extraction (ida_extract_sqrat_classes.py) into a game's API table.

usage: sqrat_classes_merge.py <game>_api.json <game>_classes.json [out.json]   (out defaults to the api file)

* every native whose wrapper pushes a class instance gets "class": the class's qualified script name (e.g. "BMvTbl.MvStatus");
* every class field gets "type" (int/short/bool/float/string) and "offset", matched to the api's var entries by call address;
  fields the api table does not have yet are added;
* one {"kind": "class"} entry per class: name, qualified name, C++ type.
"""
import json, sys

def main(api_path, cls_path, out_path):
    api = json.load(open(api_path, encoding='utf-8'))
    cx = json.load(open(cls_path, encoding='utf-8'))
    by_ea = {e.get('ea'): e for e in api if e.get('ea')}
    # qualified name of each class: the api's table entry bound at the class's BindTable call ("BMvTbl" + "MvStatus")
    qual = {}
    for c in cx['classes']:
        t = by_ea.get(c.get('bind_ea'))
        parent = t.get('table', '') if t else ''
        qual[c['static']] = (parent + '.' if parent and not parent.startswith('(') else '') + c['name']
    api = [e for e in api if e.get('kind') != 'class']
    have = {(e.get('kind'), e.get('ea')) for e in api}
    added = typed = 0
    for c in cx['classes']:
        q = qual[c['static']]
        api.append({'kind': 'class', 'name': c['name'], 'qualified': q, 'cpp': c['cpp'], 'table': q.rsplit('.', 1)[0] if '.' in q else ''})
        for f in c['fields']:
            e = by_ea.get(f['ea'])
            if e is None or e.get('kind') != 'var':
                e = {'kind': 'var', 'name': f['name'], 'table': c['name'], 'qualified_table': q, 'ea': f['ea'], 'vartype': f['binder']}
                api.append(e)
                added += 1
            e['type'] = f['type']
            e['offset'] = f['offset']
            e['qualified_table'] = q
            typed += 1
    rets = 0
    for e in api:
        s = cx['wrapper_returns'].get(e.get('wrapper') or '')
        if s and s in qual:
            e['class'] = qual[s]
            rets += 1
    json.dump(api, open(out_path, 'w', encoding='utf-8'), indent=1, ensure_ascii=False)
    print(f'{len(cx["classes"])} classes, {typed} fields typed ({added} new), {rets} natives return a known class -> {out_path}')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[1])
