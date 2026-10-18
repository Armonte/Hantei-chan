"""Catalogue extensions shared by every game (catalog.py calls finish() at the end of main()).

- traced load sites: tools/squirrel/loadsites/<game>.json -> files[].load_site = {fn, addr, when, consumer, traced}
- HA6 stack resolution: resolve_ha6_stack(), the engine rule (UNI2 1.42 CharaProject_LoadHa6DataFiles 0x4C3B70)
- runtime patterns: the sqtool --trace-patterns output merged into each move ("resolved":"runtime")
- traced state map: tools/squirrel/states/<game>.json
- split output: <game>/chrNNN.json + .md per character, <game>_index.json + .md for the shared parts
"""
import collections, json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = 2

# ---------------------------------------------------------------- load sites
TIMING_WORDS = [('boot', 'boot'), ('chara load', 'chara load'), ('battle load', 'battle load'),
                ('character select', 'CSS'), ('replay save', 'replay save'), ('winner demo', 'win demo'),
                ('win demo', 'win demo'), ('talk', 'talk scene'), ('story', 'talk scene'), ('single', 'talk scene')]

def _timing_of(when):
    w = when.lower()
    for k, t in TIMING_WORDS:
        if w.startswith(k) or w.startswith('during a ' + k):
            return t
    for k, t in TIMING_WORDS:
        if k in w:
            return t
    return 'scene'

def _legacy_site(text, timing, purpose):
    """'A 0x.. -> B 0x.. "x"' -> {fn, addr, chain}: the last named function is the loader."""
    pairs = re.findall(r'([A-Za-z_]\w+)\s+(0x[0-9A-Fa-f]+)', text)
    if pairs:
        fn, addr = pairs[-1]
    else:
        m = re.match(r'([\w.]+)', text)
        fn, addr = (m.group(1) if m else text), None
    if fn.startswith('__dofile__'):
        fn = '__dofile__'
    d = {'fn': fn, 'addr': addr, 'when': timing, 'consumer': purpose, 'chain': text, 'traced': True}
    if 'untraced' in text or fn in ('chara', 'character', 'colour', 'battle'):
        d['traced'] = False
        d['reason'] = 'rule from the folder or load order, not an exe trace'
    return d

def apply_loadsites(game, files):
    p = os.path.join(HERE, 'loadsites', f'{game}.json')
    traced = json.load(open(p, encoding='utf-8')) if os.path.exists(p) else {'sites': [], 'untraced': {}}
    sites = [(re.compile(rx), d) for rx, d in traced['sites']]
    n = 0
    for f in files:
        hit = next((d for rx, d in sites if rx.search(f['path'])), None)
        if hit:
            f['load_site'] = dict(hit, traced=True)
            if f['timing'] in ('untraced', 'CSS'):
                f['timing'] = _timing_of(hit['when'])
            n += 1
        elif isinstance(f.get('load_site'), str):
            f['load_site'] = _legacy_site(f['load_site'], f['timing'], f.get('purpose'))
            n += f['load_site']['traced']
        else:
            f['load_site'] = {'fn': None, 'addr': None, 'when': None, 'consumer': None, 'traced': False,
                              'reason': 'no load site traced for this path yet' + (f' ({game} has no loadsites/{game}.json)' if not traced['sites'] else '')}
    return n, traced.get('untraced', {})

# ---------------------------------------------------------------- HA6 stack
def ha6_scan(path, ha6walk):
    """{pid: {code, title, frames, pflg}} for one HA6 file."""
    pats = {}
    buf = open(path, 'rb').read()
    for pid, tag, w, o in ha6walk.walk(buf):
        if pid < 0:
            continue
        e = pats.setdefault(pid, {'frames': 0, 'pflg': 0})
        if tag in ('PTT2', 'PTCN'):
            s = w[1].split(b'\0')[0].decode('cp932', 'replace').strip()
            e['title' if tag == 'PTT2' else 'code'] = s
        elif tag == 'FSTR':
            e['frames'] += 1
        elif tag == 'PFLG':
            e['pflg'] = w[0]
    return pats

def resolve_ha6_stack(stack):
    """The engine's stack rule, traced in UNI2 1.42 (same shape as MBTL):
    CharaProject_LoadHa6DataFiles 0x4C3B70 loads the [DataFile] list LAST to FIRST; for each incoming pattern
    Han6_LoadPatternIntoSlot 0x4C4800 stores it when the slot is empty or the pattern there has no frames, drops it
    when the pattern there has frames, and when the pattern there has PFLG&1 (a template) merges the incoming frames'
    layers into it (Han6_MergeTemplatePattern_PFLG 0x4C4D60; the template keeps its own frame count, PTCN and flags).
    stack: [(name, {pid: scan})] in [DataFile] order. Returns {pid: {source, code, title, frames, pflg, merged_from,
    shadowed_in}} and the PTCN lookup {code: pid} (CharaObj_FindPatternByCodeName 0x425480: lowest slot whose
    effective pattern has that code)."""
    eff = {}
    for name, pats in reversed(stack):
        for pid, p in pats.items():
            cur = eff.get(pid)
            if cur is None or cur['frames'] == 0:
                shadow = ([cur['source']] + cur['shadowed_in']) if cur else []
                eff[pid] = dict(p, source=name, merged_from=[], shadowed_in=shadow)
            elif cur['pflg'] & 1:
                cur['merged_from'].append(name)
            else:
                cur['shadowed_in'].append(name)
    lookup = {}
    for pid in sorted(eff):
        c = eff[pid].get('code')
        if c and c not in lookup:
            lookup[c] = pid
    return eff, lookup

def apply_ha6_stack(root, names, ha6walk, tr):
    """Re-resolves every character's patterns and move pattern links with resolve_ha6_stack()."""
    cache = {}
    def scan(rel):
        if rel not in cache:
            p = os.path.join(root, rel)
            try:
                cache[rel] = ha6_scan(p, ha6walk) if os.path.exists(p) else {}
            except Exception as ex:
                cache[rel] = {}
        return cache[rel]
    changed = 0
    for cd, c in names['characters'].items():
        stack = [(os.path.basename(rel), scan(rel)) for rel in c['ha6_stack']]
        eff, lookup = resolve_ha6_stack(stack)
        base = {n for n, _ in stack if n.lower() == 'basedata.ha6'}
        plist = []
        for pid in sorted(eff):
            e = eff[pid]
            if e['source'] in base and not e['merged_from'] and not e['shadowed_in']:
                continue   # plain BaseData pattern: listed once in the index (basedata_patterns)
            if not (e.get('code') or e.get('title')):
                continue
            row = {'id': pid, 'pattern_source': e['source'], 'code': e.get('code')}
            t = tr(e.get('title'))
            if t:
                row['title'] = t
            if e['merged_from']:
                row['merged_from'] = e['merged_from']
            if e['shadowed_in']:
                row['shadowed_in'] = e['shadowed_in']
            plist.append(row)
        c['patterns'] = plist
        for m in c['moves']:
            for p in m.get('ha6_patterns', []):
                if p.get('code') is None:
                    continue
                pid = lookup.get(p['code'])
                old = p.get('num')
                p['num'] = pid
                p.pop('ha6', None)
                if pid is not None:
                    p['pattern_source'] = eff[pid]['source']
                changed += old != pid
    return changed

# ---------------------------------------------------------------- runtime patterns
def _move_block(text, line):
    ls = text.splitlines()
    out = []
    for i in range(line - 1, min(len(ls), line + 400)):
        if i > line - 1 and re.match(r'^\s*t\.Mv_\w+\s*<-', ls[i]):
            break
        out.append(ls[i])
    return '\n'.join(out)

def apply_runtime(names, trace, read_mv):
    """Merges sqtool --trace-patterns into the moves. Returns (unresolved_before, unresolved_after, reasons)."""
    before = after = 0
    reasons = collections.Counter()
    for cd, c in names['characters'].items():
        tmoves = trace.get(cd, {})
        mvtext = None
        for m in c['moves']:
            t = tmoves.get(m['name'])
            sets = t.get('sets', []) if t is not None else []
            pats = sorted({(s['num'], s['slot']) for s in sets if not s['native'].endswith('JumpFrameID') and s['num'] >= 0})
            jumps = sorted({s['num'] for s in sets if s['native'].endswith('JumpFrameID')})
            if pats:
                m['runtime_patterns'] = [{'num': n, 'slot': s} for n, s in pats]
            if jumps:
                m['runtime_frame_id_jumps'] = jumps
            if t is not None and t.get('errors'):
                m['runtime_errors'] = t['errors']
            for p in m.get('ha6_patterns', []):
                if p.get('num') is not None:
                    continue
                before += 1
                if pats:
                    nums = sorted({n for n, _ in pats})
                    p['num'] = nums[0] if len(nums) == 1 else None
                    if len(nums) > 1:
                        p['nums'] = nums
                    p['resolved'] = 'runtime'
                elif jumps:
                    p['resolved'] = 'runtime'
                    p['effect'] = 'keeps the current pattern and jumps to frame ID ' + '/'.join(map(str, jumps)) + ' (UpdateFrameID template)'
                elif any(s['num'] < 0 for s in sets):
                    p['resolved'] = 'runtime'
                    p['effect'] = 'SetPattern(-1): the code is in no stack file, so the engine keeps the current pattern and restarts it at frame 0'
                else:
                    if mvtext is None:
                        mvtext = read_mv(cd)
                    blk = _move_block(mvtext, m['line'])
                    if 'def_TmplFlags_ChangeStatusOnly' in blk:
                        p['resolved'] = 'runtime'
                        p['effect'] = 'def_TmplFlags_ChangeStatusOnly: the move changes status only and sets no pattern (Init ran, no SetPattern)'
                    else:
                        why = ('Init/FrameUpdate failed in the sandbox: ' + '; '.join(t['errors'])) if t and t.get('errors') else \
                              ('no Init/FrameUpdate set a pattern with stub engine state (frame counters, hit result and '
                               'opponent read as 0); the pattern is picked on a branch the sandbox cannot reach') if t is not None else \
                              'move not in the built move table'
                        p['unresolved'] = why
                        reasons[why.split(':')[0][:60]] += 1
                        after += 1
    return before, after, reasons

def compact_trace(t):
    """sqtool --trace-patterns output without the slot lists and repeated sets (what the catalogue reads)."""
    o = {}
    for cd, ms in t.items():
        o[cd] = {}
        for m, v in ms.items():
            sets, seen = [], set()
            for x in v.get('sets', []):
                k = (x['slot'], x['native'], x['num'], x.get('code'))
                if k not in seen:
                    seen.add(k)
                    sets.append({'slot': k[0], 'native': k[1], 'num': k[2], **({'code': k[3]} if k[3] else {})})
            o[cd][m] = {**({'sets': sets} if sets else {}), **({'errors': v['errors']} if v.get('errors') else {})}
    return o

# ---------------------------------------------------------------- states
def apply_states(game, names):
    p = os.path.join(HERE, 'states', f'{game}.json')
    if not os.path.exists(p):
        for s in names['states']:
            s.setdefault('traced', False)
            s.setdefault('reason', f'no tools/squirrel/states/{game}.json yet: name match only')
        return 0
    traced = json.load(open(p, encoding='utf-8'))['states']
    n = 0
    for s in names['states']:
        t = traced.get(s['state'])
        if t and t.get('traced'):
            s['name_match'] = {k: s.pop(k) for k in ('engine_consts', 'def_consts', 'mvstatus_fields', 'std_moves') if k in s}
            s.update(t)
            n += 1
        else:
            s['traced'] = False
            s['reason'] = (t or {}).get('reason', 'not traced yet: name match only')
    return n

# ---------------------------------------------------------------- split output
def _compact_tr(o):
    """Drops the duplicated en of strings without Japanese ({jp, tr:"none"}: en is jp)."""
    if isinstance(o, dict):
        if o.get('tr') == 'none' and o.get('en') == o.get('jp') and set(o) == {'jp', 'en', 'tr'}:
            return {'jp': o['jp'], 'tr': 'none'}
        return {k: _compact_tr(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_compact_tr(v) for v in o]
    return o

def _dump(obj, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(_compact_tr(obj), f, ensure_ascii=False, separators=(',', ':'))
        f.write('\n')

def _en(t):
    if not t:
        return ''
    e = t.get('en') if t.get('tr') != 'none' else t.get('jp')
    return (e or ('[JP] ' + t['jp'])).replace('|', '/')

def _jp(t):
    return (t['jp'] if t and t.get('tr') != 'none' else '').replace('|', '/')

def _site(f):
    s = f['load_site']
    if not s.get('traced'):
        return 'untraced: ' + s.get('reason', '')
    return f"{s['fn']}{' ' + s['addr'] if s.get('addr') else ''}"

def write_split(out, game, files, names, extra):
    sub = os.path.join(out, game)
    chars = names['characters']
    index = {'game': game, 'schema': SCHEMA, 'files': files,
             **{k: names[k] for k in ('consts', 'std_moves', 'std_commands', 'states', 'basedata_patterns')},
             'characters': {cd: {'num': c['num'], 'short': c['short'], 'json': f'{game}/{cd}.json', 'md': f'{game}/{cd}.md',
                                 'moves': len(c['moves']), 'commands': len(c['commands']), 'patterns': len(c['patterns'])}
                            for cd, c in chars.items()},
             **extra}
    _dump(index, os.path.join(out, f'{game}_index.json'))
    for cd, c in chars.items():
        _dump({'game': game, 'schema': SCHEMA, 'chr': cd, **c}, os.path.join(sub, f'{cd}.json'))
        _write_char_md(os.path.join(sub, f'{cd}.md'), game, cd, c)
    _write_index_md(os.path.join(out, f'{game}_index.md'), game, files, names, extra)

def _write_char_md(path, game, cd, c):
    L = [f"# {game.upper()} {cd} ({'/'.join(c['short'])})", '',
         f"Generated by `tools/squirrel/catalog.py`; do not edit. Data: `{cd}.json` (schema {SCHEMA}). Shared parts: `../{game}_index.md`.", '',
         f"HA6 stack ([DataFile] order; the engine loads it last to first): {', '.join(c['ha6_stack'])}. "
         f"{len(c['moves'])} moves, {len(c['commands'])} commands, {len(c['patterns'])} named patterns.", '',
         '## Moves', '', '| move | command | input | HA6 patterns | runtime (sandbox) | move code | EN |', '|---|---|---|---|---|---|---|']
    for m in c['moves']:
        cm = m.get('commands', [])
        pats = []
        for p in m.get('ha6_patterns', []):
            num = p['num'] if p.get('num') is not None else ('/'.join(map(str, p['nums'])) if p.get('nums') else '?')
            tag = ' (runtime)' if p.get('resolved') == 'runtime' else ''
            if p.get('effect'):
                num, tag = '-', ' (' + p['effect'].split(':')[0].split(' (')[0][:40] + ')'
            pats.append(f"{p.get('code') or ''}={num}{tag}")
        rt = ', '.join(f"{r['num']}@{r['slot']}" for r in m.get('runtime_patterns', []))
        if m.get('runtime_frame_id_jumps'):
            rt += (', ' if rt else '') + 'FrameID ' + '/'.join(map(str, m['runtime_frame_id_jumps']))
        L.append(f"| `{m['name']}` | {', '.join(x['cmd'] for x in cm)} | {' '.join(cm[0].get('input', [])[:4]) if cm else ''} | "
                 f"{'; '.join(pats)[:120]}{' (implied)' if m.get('pattern_implied') else ''} | {rt[:60]} | "
                 f"{', '.join(x.replace('def_', '') for x in m.get('movecode', []))[:80]} | {_en(m.get('comment'))} |")
    un = [(m['name'], p) for m in c['moves'] for p in m.get('ha6_patterns', []) if p.get('unresolved')]
    if un:
        L += ['', '## Unresolved pattern links', '', '| move | code | why |', '|---|---|---|']
        L += [f"| `{n}` | {p.get('code') or ''} | {p['unresolved'].replace('|', '/')[:200]} |" for n, p in un]
    L += ['', '## Commands', '', '| entry | num | input | move | EN |', '|---|---|---|---|---|']
    for x in c['commands']:
        L.append(f"| `{x['name']}` | {x.get('num', '')} | {' '.join(x.get('command', []))[:40]} | {x.get('move', '')} | {_en(x.get('comment'))} |")
    L += ['', '## HA6 patterns', '', 'Effective source per number after the stack rule; `merged` = files whose frames merge into a PFLG&1 '
          'template, `shadowed` = files that also define the number but are dropped.', '',
          '| id | source | code | merged | shadowed | EN (JP) |', '|---|---|---|---|---|---|']
    for p in c['patterns']:
        t = p.get('title')
        L.append(f"| {p['id']} | {p['pattern_source']} | {p.get('code') or ''} | {', '.join(p.get('merged_from', []))} | "
                 f"{', '.join(p.get('shadowed_in', []))} | {_en(t)}{' (' + _jp(t) + ')' if _jp(t) else ''} |")
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L) + '\n')

def _write_index_md(path, game, files, names, extra):
    L = [f'# {game.upper()} script and data catalogue (index)', '',
         f'Generated by `tools/squirrel/catalog.py`; do not edit. Data: `{game}_index.json` (schema {SCHEMA}); one file per '
         f'character in `{game}/chrNNN.json` / `.md`.', '']
    s = extra.get('stats', {})
    if s:
        L += ['| stat | value |', '|---|---|'] + [f'| {k} | {v} |' for k, v in s.items()] + ['']
    kinds = collections.Counter((f['kind'], f['sub']) for f in files)
    L += ['## Files by kind', '', '| kind | sub | count | Squirrel ok | timing | load sites traced |', '|---|---|---|---|---|---|']
    for (k, sb), n in sorted(kinds.items()):
        fs = [f for f in files if f['kind'] == k and f['sub'] == sb]
        L.append(f"| {k} | {sb} | {n} | {sum(1 for f in fs if f['squirrel'])} | {', '.join(sorted({f['timing'] for f in fs}))} | "
                 f"{sum(1 for f in fs if f['load_site'].get('traced'))} |")
    L += ['', '## Load sites', '', 'One row per distinct loader (first file shown).', '',
          '| first path | files | loader | when | consumer |', '|---|---|---|---|---|']
    seen = collections.OrderedDict()
    for f in files:
        k = json.dumps(f['load_site'], sort_keys=True)
        seen.setdefault(k, [f, 0])[1] += 1
    for k, (f, n) in seen.items():
        s = f['load_site']
        L.append(f"| `{f['path']}` | {n} | {_site(f)} | {(s.get('when') or '').replace('|', '/')} | {(s.get('consumer') or '').replace('|', '/')} |")
    if extra.get('loadsites_untraced'):
        L += ['', 'Not traced yet:', ''] + [f'- {k}: {v}' for k, v in extra['loadsites_untraced'].items()]
    L += ['', '## Non-Squirrel files', '', '| path | kind | error |', '|---|---|---|']
    for f in files:
        if f['squirrel'] is False:
            L.append(f"| `{f['path']}` | {f['sub']} | {(f.get('compile_error') or '').replace('|', '/')[:80]} |")
    L += ['', '## Shared states', '']
    for st in names['states']:
        if st.get('traced'):
            L.append(f"- **{st['state']}** ({st['en']}): " + '; '.join(f"{k}: {', '.join(map(str, v)) if isinstance(v, list) else v}"
                     for k, v in st.items() if k not in ('state', 'en', 'traced', 'name_match')))
        else:
            L.append(f"- **{st['state']}** ({st['en']}): not traced ({st.get('reason', '')}); name match: "
                     f"{', '.join(st.get('mvstatus_fields', [])[:6] + st.get('engine_consts', [])[:4])}")
    L += ['', f"## Constants ({len(names['consts'])})", '']
    byfile = collections.defaultdict(list)
    for c in names['consts']:
        byfile[c['file']].append(c)
    for fn, cs in byfile.items():
        L += [f'### {fn} ({len(cs)})', '', '| name | value | EN | JP |', '|---|---|---|---|']
        L += [f"| `{c['name']}` | `{c['value'][:30].replace('|', '/')}` | {_en(c.get('comment'))} | {_jp(c.get('comment'))} |" for c in cs]
        L.append('')
    L += ['## Std moves', '', '| move | template | HA6 patterns | EN |', '|---|---|---|---|']
    for m in names['std_moves']:
        L.append(f"| `{m['name']}` | {m.get('template', '')} | {', '.join(map(str, m.get('patterns', [])))[:60]} | {_en(m.get('comment'))} |")
    L += ['', '## Std commands', '', '| entry | num | input | move | EN |', '|---|---|---|---|---|']
    for c in names['std_commands']:
        L.append(f"| `{c['name']}` | {c.get('num', '')} | {' '.join(c.get('command', []))[:40]} | {c.get('move', '')} | {_en(c.get('comment'))} |")
    L += ['', '## Characters', '', '| chr | short | moves | commands | patterns |', '|---|---|---|---|---|']
    for cd, c in names['characters'].items():
        L.append(f"| [{cd}]({game}/{cd}.md) | {'/'.join(c['short'])} | {len(c['moves'])} | {len(c['commands'])} | {len(c['patterns'])} |")
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(L) + '\n')

# ---------------------------------------------------------------- entry
def finish(game, root, out, files, names, tr, ha6walk, opt):
    """Applies the extensions and writes the split output. Returns the stats dict."""
    nsites, untraced = apply_loadsites(game, files)
    changed = apply_ha6_stack(root, names, ha6walk, tr)
    trace_path = opt.get('--trace')   # sqtool check <dir> --api <game>_api.json --trace-patterns out.json
    stats = {'load sites traced': f"{nsites}/{len(files)} files", 'pattern links changed by the stack rule': changed}
    if trace_path and os.path.exists(trace_path):
        trace = compact_trace(json.load(open(trace_path, encoding='utf-8')))
        def read_mv(cd):
            p = os.path.join(root, 'data', cd, f'{cd}_mv_0.txt')
            return open(p, 'rb').read().decode('cp932', 'replace') if os.path.exists(p) else ''
        b, a, reasons = apply_runtime(names, trace, read_mv)
        stats['unresolved pattern links before runtime trace'] = b
        stats['unresolved pattern links after runtime trace'] = a
        for k, v in reasons.items():
            stats['unresolved: ' + k] = v
    stats['states traced'] = f"{apply_states(game, names)}/{len(names['states'])}"
    write_split(out, game, files, names, {'stats': stats, 'loadsites_untraced': untraced})
    return stats
