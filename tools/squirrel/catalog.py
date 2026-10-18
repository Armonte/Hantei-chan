#!/usr/bin/env python3
"""Script/data catalogue + names dictionary generator.

usage: catalog.py <game> <data dir> [--sqtool path/to/sqtool.exe] [--out docs/squirrel/catalog]
       catalog.py <game> <data dir> --dump-jp out.json   (untranslated JP strings, for the translation pass)

Emits <out>/<game>_files.json, <game>_names.json, <game>_catalog.md. Translations come from
<out>/<game>_tr.json ({jp: en}); every translated string carries "tr":"machine" until reviewed.
Game data is only read, never copied into the output beyond names, values and comments.
Only UNI2 1.42 has load-site rules so far (LOADSITES); other games fall back to "untraced".
"""
import json, os, re, subprocess, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, os.path.join(REPO, 'tools', 'uni'))
import ha6walk  # noqa: E402

JP_RE = re.compile(r'[　-ヿ一-鿿＀-￯]')

def rd(path):
    return open(path, 'rb').read().decode('cp932', 'replace')

def has_jp(s):
    return bool(s and JP_RE.search(s))

# ---------------------------------------------------------------- translation
class Tr:
    def __init__(self, path):
        self.map = json.load(open(path, encoding='utf-8')) if os.path.exists(path) else {}
        self.missing = set()
        self.used = 0
    def __call__(self, jp):
        """Return a dict {jp, en, tr} for a JP string (en None if not translated yet)."""
        jp = (jp or '').strip()
        if not jp:
            return None
        if not has_jp(jp):
            return {'jp': jp, 'en': jp, 'tr': 'none'}
        en = self.map.get(jp)
        if en is None:
            self.missing.add(jp)
            return {'jp': jp, 'en': None, 'tr': None}
        self.used += 1
        return {'jp': jp, 'en': en, 'tr': 'machine'}

# ---------------------------------------------------------------- file kinds
# (regex on the posix path relative to the data dir, kind, subkind, English purpose template)
KINDS = [
    (r'^data/chr\d+\w*/chr\d+_sc_\d+\.txt$', 'character script', 'sc', 'Battle_Status: per-character stats (attack rate, guts, gauge rates, positions)'),
    (r'^data/chr\d+\w*/chr\d+_cmd_\d+\.txt$', 'character script', 'cmd', '<CHR>_CommandTable: command inputs and the moves they start'),
    (r'^data/chr\d+\w*/chr\d+_mv_\d+\.txt$', 'character script', 'mv', '<CHR>_MoveTable: move behaviour (patterns, effects, cancels)'),
    (r'^data/chr\d+\w*/chr\d+_com_ranking_\d+\.txt$', 'character script', 'com', 'CPU AI ranking/priority table'),
    (r'^data/chr\d+\w*/chr\d+_com_\d+\.txt$', 'character script', 'com', 'CPU AI (CCom) behaviour for the character'),
    (r'^data/chr\d+\w*/chr\d+_se_list\.txt$', 'character script', 'se_list', 'sound effect list (pulls in script/btl_SEDefine.txt)'),
    (r'^data/chr\d+\w*/chr\d+_se_category\.txt$', 'character script', 'se_category', 'sound effect categories (Battle_Std.SeTable[chr])'),
    (r'^data/chr\d+\w*/chr\d+_\d+\.txt$', 'list or ini', 'ha6_stack', 'HA6/CG/PAT file stack for the character (ini)'),
    (r'^data/chr\d+\w*/', 'unknown', 'chara_misc', 'character-folder file'),
    (r'^script/', 'shared battle script', 'script', None),
    (r'^data/_combase\.txt$', 'data table', 'combase', 'CPU AI base classes CCom/CComSkill/CComMove'),
    (r'^data/VectorTable\.txt$', 'data table', 'vector', 'global vector (knockback/launch) table, g_VectorTable'),
    (r'^data/BattleInfo\.txt$', 'data table', 'battleinfo', 'global battle tuning values, g_BattleInfo'),
    (r'^data/BoundEff\.txt$', 'data table', 'boundeff', 'wall/ground bounce effect table, g_BoundEffTable'),
    (r'^data/effect\.txt$', 'list or ini', 'ha6_stack', 'system effect HA6/PAT file stack (ini)'),
    (r'^data/_coloredit/', 'data table', 'coloredit', 'palette colour-edit layout for the character (colour edit menu)'),
    (r'^data/_csel/', 'data table', 'csel', 'character select data for the character'),
    (r'^data/', 'unknown', 'data_misc', 'data file'),
    (r'^System/cmd_chr\d+\.txt$', 'data table', 'training_cmdlist', 'training/pause-menu command list display (Training_cmdList)'),
    (r'^System/cmd__common\.txt$', 'data table', 'training_cmdlist', 'common command list display entries'),
    (r'^System/BtlCharaTbl\.txt$', 'list or ini', 'chara_list', 'battle character table (chara list, folders)'),
    (r'^System/str_jp/', 'list or ini', 'wordlist', 'banned word / number list (online names)'),
    (r'^System/Header\w+\.txt$', 'list or ini', 'replay_header', 'replay header template'),
    (r'^System/PlayBonusTbl\.txt$', 'data table', 'playbonus', 'play bonus table'),
    (r'^System/', 'list or ini', 'system', 'system file'),
    (r'^BattleRes/BattleTable/', 'data table', 'battle_table', 'arcade/story stage table (Stage_Table)'),
    (r'^BattleRes/talk/st_bgm', 'data table', 'talk', 'story BGM table'),
    (r'^BattleRes/talk/', 'data table', 'talk', 'story/talk scene script'),
    (r'^BattleRes/win_mes/', 'data table', 'win_mes', 'win quote table'),
    (r'^BattleRes/', 'unknown', 'battleres', 'battle resource file'),
    (r'^Shader/', 'list or ini', 'shader', 'shader source/parameter file'),
    (r'^bg/', 'list or ini', 'bg', 'background list/info'),
]

SCRIPT_PURPOSE = {
    'Battle_Init.txt': 'battle VM init: loads the btl_Define* constants and debug helpers',
    'Battle_Std.txt': 'battle VM std script: Battle_Std/BtlPl/BtlOb/BtlPt tables, pulls in the btl_* std files',
    'btl_ChrTableFunc.txt': 'MakeMoveTable/MakeCommandTable: builds each character table from the std tables',
    'btl_CmdCheck.txt': 'command check helpers (CmdCheck evaluation)',
    'btl_debugFunc.txt': 'debug print helpers',
    'btl_Define.txt': 'system constants: Def_Sys_* version switches, def_* flags, pattern numbers',
    'btl_Define_Chara.txt': 'character numbers (Def_ChrNo_*) and per-character constants',
    'btl_Define_CharaNew.txt': 'additional character constants',
    'btl_Define_NoLocalDebug.txt': 'debug switches for release builds',
    'btl_MvFunc.txt': 'shared move functions (Battle_Std.* used by moves)',
    'btl_MvInitFunc.txt': 'shared move Init helpers',
    'btl_SEDefine.txt': 'sound effect constants (loaded by each character se_list)',
    'btl_StdCommdandTbl.txt': 'GetStdCommandTable: standard commands (dash, jump, guard, system actions)',
    'btl_StdMoveTbl.txt': 'GetStdMoveTable: standard moves (stand, walk, guard, hitstun, knockdown, system moves)',
    'btl_Std_BattleStatusTbl.txt': 'default Battle_Status values shared by characters',
    'cmn_SetPosition.txt': 'round start position helpers',
    'sq_stagebgm.txt': 'stage BGM script (stage_bgm.initround)',
}

# load site / timing / hot reload, UNI2 1.42 (RE_MBTL_LOAD.md UNI2 section, TRAINING_CHARA_SWITCH.md section 10)
LOADSITES = {
    'uni2': [
        (r'^script/Battle_Init\.txt$', 'Sq_InitBattleVM 0x8808B0 -> Sq_RunScriptFile 0x8A6CD0 "BattleInit"', 'boot (battle VM)', 'reloadstd'),
        (r'^script/Battle_Std\.txt$', 'Sq_InitBattleVM 0x8808B0 -> Sq_RunScriptFile "BattleScript"', 'boot (battle VM)', 'reloadstd'),
        (r'^script/btl_SEDefine\.txt$', '__dofile__ from <chr>_se_list.txt', 'chara load', 'reloadchara (rebuild only)'),
        (r'^script/btl_(Define\w*|debugFunc)\.txt$', '__dofile__ from Battle_Init.txt', 'boot (battle VM)', 'reloadstd'),
        (r'^script/(btl_\w+|cmn_SetPosition|sq_stagebgm)\.txt$', '__dofile__ from Battle_Std.txt', 'boot (battle VM)', 'reloadstd'),
        (r'^data/_combase\.txt$', 'Sq_InitBattleVM "_com_base_"', 'boot (battle VM)', 'reloadstd'),
        (r'^data/VectorTable\.txt$', 'Boot_LoadGlobalResources 0x8DD0F0 -> VectorTable_LoadGlobal 0x529260', 'boot', 'reloadstd'),
        (r'^data/BattleInfo\.txt$', 'Boot_LoadGlobalResources -> BattleInfo_LoadGlobal 0x52FD90', 'boot', 'reloadstd'),
        (r'^data/BoundEff\.txt$', 'Boot_LoadGlobalResources -> BoundEff_LoadGlobal 0x59F790', 'boot', 'reloadstd'),
        (r'^data/chr\d+\w*/chr\d+_sc_\d+\.txt$', 'Chara_LoadBattleScripts 0x645E50 -> CharaDatSq_LoadBattleStatus 0x8905B0', 'chara load', 'reloadchara'),
        (r'^data/chr\d+\w*/chr\d+_cmd_\d+\.txt$', 'Chara_LoadBattleScripts -> Chara_LoadCommandList 0x646F30', 'chara load', 'reloadchara'),
        (r'^data/chr\d+\w*/chr\d+_mv_\d+\.txt$', 'Chara_LoadBattleScripts -> Chara_LoadMoveList 0x646B20', 'chara load', 'reloadchara'),
        (r'^data/chr\d+\w*/chr\d+_com\w*_\d+\.txt$', 'Chara_LoadBattleScripts -> Chara_LoadComScripts 0x645350', 'chara load', 'reloadchara (rebuild only)'),
        (r'^data/chr\d+\w*/chr\d+_se_\w+\.txt$', 'chara load (se_list via mv; se_category via mv __dofile__)', 'chara load', 'reloadchara (rebuild only)'),
        (r'^data/chr\d+\w*/chr\d+_\d+\.txt$', 'chara resource load (HA6 stack ini)', 'chara load', 'reloadchara (rebuild only)'),
        (r'^data/_csel/', 'character select scene (untraced)', 'CSS', 'no'),
        (r'^data/_coloredit/', 'colour edit scene (untraced)', 'CSS', 'no'),
        (r'^data/effect\.txt$', 'battle load (system effect record)', 'battle load', 'reloadchara (rebuild only)'),
    ],
}

def classify(rel):
    for rx, kind, sub, purpose in KINDS:
        if re.search(rx, rel):
            if sub == 'script':
                purpose = SCRIPT_PURPOSE.get(os.path.basename(rel), 'shared battle script')
            return kind, sub, purpose
    return 'unknown', 'unknown', None

def loadsite(game, rel):
    for rx, site, timing, hot in LOADSITES.get(game, []):
        if re.search(rx, rel):
            return site, timing, hot
    return None, 'untraced', 'no'

def header_comment(text):
    """First comment block in the first 15 lines (// or /* */), joined."""
    out = []
    for ln in text.splitlines()[:15]:
        s = ln.strip()
        if s.startswith('//'):
            c = s.lstrip('/').strip(' -=*')
            if c:
                out.append(c)
        elif out:
            break
    return ' / '.join(out[:3])[:200] or None

def sq_compile(sqtool, root, rels):
    if not sqtool or not os.path.exists(sqtool):
        return {}
    res = {}
    for i in range(0, len(rels), 100):
        chunk = rels[i:i + 100]
        p = subprocess.run([sqtool, 'compile'] + chunk, cwd=root, capture_output=True, text=True,
                           encoding='utf-8', errors='replace')
        for ln in (p.stdout + p.stderr).splitlines():
            m = re.match(r'^(ok|FAIL)\s+(\S+?)(?::(\d+):(\d+)\s+(.*)|\s+\(.*)?$', ln.strip())
            if m:
                res[m.group(2)] = (m.group(1) == 'ok', m.group(5))
    return res

# ---------------------------------------------------------------- names
CONST_RE = re.compile(r'^\s*const\s+(\w+)\s*=\s*([^;/]+?)\s*;?\s*(?://\s*(.*))?$')
ENUM_RE = re.compile(r'enum\s+(\w+)\s*\{(.*?)\}', re.S)

def strip_comment(s):
    return s.split('//')[0]

def block_at(text, pos):
    """Text of the {...} block starting at the first '{' after pos."""
    i = text.find('{', pos)
    if i < 0:
        return ''
    d = 0
    for j in range(i, min(len(text), i + 200000)):
        c = text[j]
        if c == '{':
            d += 1
        elif c == '}':
            d -= 1
            if d == 0:
                return text[i:j + 1]
    return text[i:]

def prev_comment(lines, idx):
    for k in range(idx - 1, max(-1, idx - 3), -1):
        s = lines[k].strip()
        if s.startswith('//'):
            return s.lstrip('/').strip(' -=*') or None
        if s:
            return None
    return None

def parse_consts(text, fname, tr):
    out = []
    lines = text.splitlines()
    for i, ln in enumerate(lines):
        m = CONST_RE.match(ln)
        if not m:
            continue
        name, val, com = m.group(1), m.group(2).strip(), (m.group(3) or '').strip()
        if not com:
            com = prev_comment(lines, i) or ''
        e = {'name': name, 'value': val, 'file': fname, 'line': i + 1}
        t = tr(com)
        if t:
            e['comment'] = t
        out.append(e)
    for m in ENUM_RE.finditer(text):
        ename = m.group(1)
        n = 0
        body_start = text[:m.start(2)].count('\n')
        for k, ln in enumerate(m.group(2).splitlines()):
            code, _, com = ln.partition('//')
            for item in [x.strip() for x in code.split(',') if x.strip()]:
                nm, _, v = item.partition('=')
                nm = nm.strip()
                if not re.match(r'^\w+$', nm):
                    continue
                if v.strip():
                    try:
                        n = int(v.strip(), 0)
                    except ValueError:
                        n = v.strip()
                e = {'name': ename + '.' + nm, 'value': str(n), 'file': fname, 'line': body_start + k + 1, 'enum': ename}
                t = tr(com.strip())
                if t:
                    e['comment'] = t
                out.append(e)
                if isinstance(n, int):
                    n += 1
    return out

def pat_refs(block):
    pats = []
    for m in re.finditer(r'(?:SetPattern|SetVorpalPattern|GetPatternNum)\s*\(\s*(?:\{\s*pat\s*=\s*)?("([^"]*)"|-?\d+)', block):
        pats.append(m.group(2) if m.group(2) is not None else int(m.group(1)))
    for m in re.finditer(r'\bpat\s*=\s*"([^"]+)"', block):
        pats.append(m.group(1))
    seen, out = set(), []
    for p in pats:
        if p not in seen:
            seen.add(p)
            out.append(p)
    return out

def mc_refs(block):
    return sorted(set(re.findall(r'\b(def_MC\d?_\w+)', block)))

def parse_table_entries(text, prefix_re):
    """Yield (name, rhs_first_line, block, line) for `X.Name <- ...` style entries."""
    for m in re.finditer(prefix_re, text, re.M):
        name = m.group(1)
        line = text[:m.start()].count('\n') + 1
        rest = text[m.end():m.end() + 400]
        first = strip_comment(rest.split('\n')[0]).strip() or strip_comment(rest.strip().split('\n')[0]).strip()
        if first.startswith('{') or not first:
            blk = block_at(text, m.end())
        else:
            blk = first
        yield name, first, blk, line

def trailing_comment(text, line):
    ln = text.splitlines()[line - 1] if line - 1 < len(text.splitlines()) else ''
    return ln.split('//', 1)[1].strip() if '//' in ln else None

def parse_commands(text, tr, prefix=r'^\s*t\.(\w+)\s*<-'):
    out = []
    lines = text.splitlines()
    entries = list(parse_table_entries(text, prefix))
    if not entries:  # cmd files: `local t = { Skill_X = {...}, ... }`
        entries = list(parse_table_entries(text, r'^\t(\w+)\s*=\s*(?=\{|$)'))
    for name, first, blk, line in entries:
        if not name:
            continue
        e = {'name': name, 'line': line}
        m = re.search(r'\bcommand\s*=\s*\[([^\]]*)\]', blk)
        if m:
            e['command'] = [c.strip().strip('"') for c in strip_comment(m.group(1)).split(',') if c.strip()][:8]
        m = re.search(r'UpdateTable\s*=\s*\{\s*name\s*=\s*"(\w+)"', blk)
        if m:
            e['move'] = m.group(1)
        m = re.search(r'\bnum\s*=\s*(\w+)', blk)
        if m:
            e['num'] = m.group(1)
        m = re.search(r'SkillType\s*=\s*"(\w+)"', blk)
        if m:
            e['skilltype'] = m.group(1)
        c = prev_comment(lines, line - 1)
        t = tr(c)
        if t:
            e['comment'] = t
        out.append(e)
    return out

def parse_moves(text, tr, prefix):
    out = []
    lines = text.splitlines()
    for name, first, blk, line in parse_table_entries(text, prefix):
        e = {'name': name, 'line': line}
        if not first.startswith('{'):
            m = re.match(r'([\w.]+)\s*\(', first)
            if m:
                e['template'] = m.group(1)
            elif first:
                e['alias'] = first.rstrip(';')[:80]
        p = pat_refs(blk)
        if p:
            e['patterns'] = p
        mc = mc_refs(blk)
        if mc:
            e['movecode'] = mc
        c = trailing_comment(text, line) or prev_comment(lines, line - 1)
        t = tr(c)
        if t:
            e['comment'] = t
        out.append(e)
    return out

def ha6_patterns(path):
    pats = {}
    try:
        buf = open(path, 'rb').read()
        for pid, tag, w, o in ha6walk.walk(buf):
            if tag in ('PTT2', 'PTCN'):
                s = w[1].split(b'\0')[0].decode('cp932', 'replace').strip()
                pats.setdefault(pid, {})['title' if tag == 'PTT2' else 'code'] = s
    except Exception as ex:  # keep going; report
        return pats, str(ex)
    return pats, None

def ha6_stack(chrdir, inifile):
    files = []
    if os.path.exists(inifile):
        sec = None
        for ln in rd(inifile).splitlines():
            ln = ln.strip()
            if ln.startswith('['):
                sec = ln
            elif sec == '[DataFile]' and re.match(r'File\d+=', ln):
                files.append(os.path.normpath(os.path.join(chrdir, ln.split('=', 1)[1].strip())))
    return files

# ---------------------------------------------------------------- shared state names
STATES = [
    ('stand', 'standing', ['Stand']),
    ('crouch', 'crouching', ['Crouch', 'Sit']),
    ('air', 'airborne', ['Air', 'Jump', 'Landing']),
    ('ground', 'grounded', ['Ground']),
    ('guard', 'guarding', ['Guard']),
    ('shield', 'shield (UNI2 GRD shield)', ['Shield']),
    ('hitstun', 'being hit (damage)', ['Damage', 'Hit', 'Yarare']),
    ('knockdown', 'knocked down', ['Down', 'Fall']),
    ('bound', 'wall/ground bounce', ['Bound']),
    ('throw', 'throw / being thrown', ['Throw', 'Nage']),
    ('dash', 'dash / backdash', ['Dash', 'BackStep']),
    ('counter', 'counter hit', ['Counter']),
    ('vorpal', 'Vorpal', ['Vorpal', 'VP']),
    ('veiloff', 'Veil Off / Cross Cast', ['VeilOff', 'CrossCast', 'Veil']),
    ('chainshift', 'Chain Shift', ['ChainShift']),
    ('exs', 'EXS gauge', ['EXS', 'Exs']),
]

def state_map(api, consts, std_moves):
    api_c = [x for x in api if x.get('kind') == 'const' and re.match(r'_(PosState|State|Status|GuardFlag|MvStFlag)_', x['name'])]
    mvst = [x for x in api if x.get('kind') == 'var' and x.get('table') == 'MvStatus']
    out = []
    for key, en, words in STATES:
        rx = re.compile('|'.join(words))
        e = {'state': key, 'en': en}
        e['engine_consts'] = sorted({f"{x.get('qualified_table', x.get('table'))}.{x['name']}={x.get('value')}" for x in api_c if rx.search(x['name'])})[:20]
        e['def_consts'] = sorted({c['name'] for c in consts if rx.search(c['name'])})[:40]
        e['mvstatus_fields'] = sorted({x['name'] for x in mvst if rx.search(x['name'])})
        e['std_moves'] = sorted({m['name'] for m in std_moves if rx.search(m['name'])})[:40]
        out.append(e)
    return out

# ---------------------------------------------------------------- main
def main():
    a = sys.argv[1:]
    if len(a) < 2:
        print(__doc__)
        sys.exit(2)
    game, root = a[0], os.path.abspath(a[1])
    opt = dict(zip(a[2::2], a[3::2]))
    out = opt.get('--out', os.path.join(REPO, 'docs', 'squirrel', 'catalog'))
    sqtool = opt.get('--sqtool') or next((p for p in [os.path.join(REPO, 'build', 'sqtool.exe'),
                                                         os.path.join(REPO, 'build', 'sqtool')] if os.path.exists(p)), None)
    os.makedirs(out, exist_ok=True)
    tr = Tr(os.path.join(out, f'{game}_tr.json'))
    api_path = os.path.join(REPO, 'docs', 'squirrel', 'api', f'{game}_api.json')
    api = json.load(open(api_path)) if os.path.exists(api_path) else []

    rels = []
    for dp, dn, fn in os.walk(root):
        dn.sort()
        for f in sorted(fn):
            if f.lower().endswith('.txt'):
                rels.append(os.path.relpath(os.path.join(dp, f), root).replace(os.sep, '/'))
    comp = sq_compile(sqtool, root, rels)

    files = []
    for rel in rels:
        text = rd(os.path.join(root, rel))
        kind, sub, purpose = classify(rel)
        # content check: a "unknown" file that defines a MoveTable/CommandTable is a character script
        if kind == 'unknown' and re.search(r'_MoveTable|_CommandTable|Battle_Status\s*<-', text):
            kind = 'character script'
        site, timing, hot = loadsite(game, rel)
        ok = comp.get(rel)
        hdr = header_comment(text)
        row = {'path': rel, 'kind': kind, 'sub': sub, 'bytes': len(text.encode('cp932', 'replace')),
               'squirrel': (ok[0] if ok else None), 'load_site': site, 'timing': timing, 'hot_reload': hot,
               'purpose': purpose}
        if ok and not ok[0]:
            row['compile_error'] = ok[1]
        if hdr:
            row['header'] = tr(hdr)
        files.append(row)

    # ---- names: shared consts/enums
    sdir = os.path.join(root, 'script')
    consts = []
    shared = [os.path.join('script', f) for f in sorted(os.listdir(sdir))] if os.path.isdir(sdir) else []
    if os.path.exists(os.path.join(root, 'data', '_combase.txt')):
        shared.append('data/_combase.txt')
    for rel in shared:
        consts += parse_consts(rd(os.path.join(root, rel)), rel.replace(os.sep, '/'), tr)

    def rf(rel):
        p = os.path.join(root, rel)
        return rd(p) if os.path.exists(p) else ''
    std_moves = parse_moves(rf('script/btl_StdMoveTbl.txt'), tr, r'^\s*Std_MoveTable\.(\w+)\s*<-')
    std_cmds = parse_commands(rf('script/btl_StdCommdandTbl.txt'), tr)

    # chr number -> short names (Def_ChrNo_Xxx aliases)
    chrnames = collections.defaultdict(list)
    for c in consts:
        m = re.match(r'Def_ChrNo_(\w+)$', c['name'])
        if m and re.match(r'^\d+$', c['value']) and not m.group(1).startswith('Chr'):
            chrnames[int(c['value'])].append(m.group(1))

    # ---- per character
    chars = {}
    base_pats = {}
    ddir = os.path.join(root, 'data')
    for cd in sorted(os.listdir(ddir)) if os.path.isdir(ddir) else []:
        if not re.match(r'^chr\d{3}$', cd):
            continue
        cpath = os.path.join(ddir, cd)
        num = int(cd[3:])
        stack = ha6_stack(cpath, os.path.join(cpath, f'{cd}_0.txt'))
        pats_by_code, pat_list, errs = {}, [], []
        for hf in stack:
            if not os.path.exists(hf):
                continue
            isbase = os.path.basename(hf).lower() == 'basedata.ha6'
            if isbase and base_pats:
                pp = base_pats
            else:
                pp, err = ha6_patterns(hf)
                if err:
                    errs.append(f'{os.path.basename(hf)}: {err}')
                if isbase:
                    base_pats = pp
            for pid in sorted(pp):
                code = pp[pid].get('code')
                if code and code not in pats_by_code:
                    pats_by_code[code] = (pid, os.path.basename(hf))
                if not isbase and (pp[pid].get('title') or code):
                    t = tr(pp[pid].get('title'))
                    pat_list.append({'id': pid, 'ha6': os.path.basename(hf), 'code': code,
                                     **({'title': t} if t else {})})
        mv = rf(f'data/{cd}/{cd}_mv_0.txt')
        cmd = rf(f'data/{cd}/{cd}_cmd_0.txt')
        moves = parse_moves(mv, tr, r'^\s*t\.(Mv_\w+)\s*<-')
        cmds = parse_commands(cmd, tr)
        by_move = collections.defaultdict(list)
        for c in cmds:
            mvn = c.get('move') or ('Mv_' + c['name'] if c['name'].startswith('Skill_') else None)
            if mvn:
                by_move[mvn].append(c)
        for m in moves:
            cs = by_move.get(m['name'])
            if cs:
                m['commands'] = [{'cmd': c['name'], **({'input': c['command']} if 'command' in c else {})} for c in cs]
            codes = list(m.get('patterns', []))
            if m['name'].startswith('Mv_Skill_') and not codes:
                codes = [m['name'][len('Mv_Skill_'):]]
                m['pattern_implied'] = True
            res = []
            for p in codes:
                if isinstance(p, int):
                    res.append({'code': None, 'num': p})
                else:
                    hit = pats_by_code.get(p)
                    res.append({'code': p, 'num': hit[0] if hit else None, **({'ha6': hit[1]} if hit else {})})
            if res:
                m['ha6_patterns'] = res
            m.pop('patterns', None)
        sc = rf(f'data/{cd}/{cd}_sc_0.txt')
        chars[cd] = {'num': num, 'short': chrnames.get(num, []), 'ha6_stack': [os.path.relpath(h, root).replace(os.sep, '/') for h in stack],
                     'moves': moves, 'commands': cmds, 'patterns': pat_list,
                     **({'ha6_errors': errs} if errs else {})}
        if sc:
            chars[cd]['sc_fields'] = sorted(set(re.findall(r'^\s*(\w+)\s*=', sc, re.M)))

    base_list = [{'id': pid, 'code': v.get('code'), **({'title': tr(v['title'])} if v.get('title') else {})}
                 for pid, v in sorted(base_pats.items())]
    states = state_map(api, consts, std_moves)

    if '--dump-jp' in opt:
        json.dump(sorted(tr.missing), open(opt['--dump-jp'], 'w', encoding='utf-8'), ensure_ascii=False, indent=0)
        print(f'{len(tr.missing)} untranslated JP strings -> {opt["--dump-jp"]}')
        return

    names = {'game': game, 'schema': 1,
             'consts': consts, 'std_moves': std_moves, 'std_commands': std_cmds,
             'states': states, 'basedata_patterns': base_list, 'characters': chars}
    def dump(obj, name):
        with open(os.path.join(out, name), 'w', encoding='utf-8') as f:
            json.dump(obj, f, ensure_ascii=False, separators=(',', ':'))
            f.write('\n')
    dump({'game': game, 'schema': 1, 'files': files}, f'{game}_files.json')
    dump(names, f'{game}_names.json')
    write_md(out, game, files, names, tr)
    kinds = collections.Counter(f['kind'] for f in files)
    print('files', len(files), dict(kinds))
    print('squirrel ok', sum(1 for f in files if f['squirrel']), 'fail', sum(1 for f in files if f['squirrel'] is False))
    print('consts', len(consts), 'std_moves', len(std_moves), 'std_cmds', len(std_cmds),
          'moves', sum(len(c['moves']) for c in chars.values()), 'cmds', sum(len(c['commands']) for c in chars.values()),
          'patterns', sum(len(c['patterns']) for c in chars.values()), 'base', len(base_list))
    print('translated (machine)', tr.used, 'untranslated JP', len(tr.missing))

def en(t):
    if not t:
        return ''
    return (t['en'] or ('[JP] ' + t['jp'])).replace('|', '/')

def jp(t):
    return (t['jp'] if t and t['tr'] != 'none' else '').replace('|', '/')

def write_md(out, game, files, names, tr):
    L = [f'# {game.upper()} script and data catalogue', '',
         f'Generated by `tools/squirrel/catalog.py {game} <data dir>`; do not edit by hand. JSON: `{game}_files.json`, '
         f'`{game}_names.json`. Translations are machine (`"tr":"machine"`) until reviewed; JP is kept beside them.', '']
    kinds = collections.Counter((f['kind'], f['sub']) for f in files)
    L += ['## Files by kind', '', '| kind | sub | count | Squirrel ok | timing | hot reload |', '|---|---|---|---|---|---|']
    for (k, s), n in sorted(kinds.items()):
        fs = [f for f in files if f['kind'] == k and f['sub'] == s]
        ok = sum(1 for f in fs if f['squirrel'])
        tim = ', '.join(sorted({f['timing'] for f in fs}))
        hot = ', '.join(sorted({f['hot_reload'] for f in fs}))
        L.append(f'| {k} | {s} | {n} | {ok} | {tim} | {hot} |')
    L += ['', '## Shared files', '', '| path | Squirrel | load site | timing | hot reload | purpose | header (EN) |', '|---|---|---|---|---|---|---|']
    for f in files:
        if f['kind'] in ('shared battle script', 'data table') and not f['path'].startswith(('BattleRes/', 'System/cmd', 'data/_csel', 'data/_coloredit')):
            L.append(f"| `{f['path']}` | {'yes' if f['squirrel'] else 'no'} | {f['load_site'] or ''} | {f['timing']} | {f['hot_reload']} | {f['purpose'] or ''} | {en(f.get('header'))} |")
    L += ['', '## Non-Squirrel files', '', 'Files sqtool could not compile (ini, csv-like lists, shader source).', '',
          '| path | kind | error |', '|---|---|---|']
    for f in files:
        if f['squirrel'] is False:
            L.append(f"| `{f['path']}` | {f['sub']} | {(f.get('compile_error') or '').replace('|', '/')[:80]} |")
    L += ['', '## Shared state names', '', '| state | engine consts | def_ consts | MvStatus fields | std moves (first) |', '|---|---|---|---|---|']
    for s in names['states']:
        L.append(f"| {s['state']} ({s['en']}) | {', '.join(s['engine_consts'][:6])} | {', '.join(s['def_consts'][:6])} | "
                 f"{', '.join(s['mvstatus_fields'][:6])} | {', '.join(s['std_moves'][:6])} |")
    L += ['', '## Constants (script/*)', '', f"{len(names['consts'])} constants and enum members; full list in `{game}_names.json` `consts`.", '']
    byfile = collections.defaultdict(list)
    for c in names['consts']:
        byfile[c['file']].append(c)
    for fn, cs in byfile.items():
        L += [f'### {fn} ({len(cs)})', '', '| name | value | EN | JP |', '|---|---|---|---|']
        for c in cs:
            L.append(f"| `{c['name']}` | `{c['value'][:30].replace('|', '/')}` | {en(c.get('comment'))} | {jp(c.get('comment'))} |")
        L.append('')
    L += ['## Std moves (btl_StdMoveTbl)', '', '| move | template | patterns | EN |', '|---|---|---|---|']
    for m in names['std_moves']:
        L.append(f"| `{m['name']}` | {m.get('template', '')} | {', '.join(map(str, m.get('patterns', [])))[:60]} | {en(m.get('comment'))} |")
    L += ['', '## Std commands (btl_StdCommdandTbl)', '', '| entry | num | input | move | EN |', '|---|---|---|---|---|']
    for c in names['std_commands']:
        L.append(f"| `{c['name']}` | {c.get('num', '')} | {' '.join(c.get('command', []))[:40]} | {c.get('move', '')} | {en(c.get('comment'))} |")
    L += ['', '## Characters', '']
    for cd, c in names['characters'].items():
        L += [f"### {cd} ({'/'.join(c['short'])})", '',
              f"HA6 stack: {', '.join(c['ha6_stack'])}. {len(c['moves'])} moves, {len(c['commands'])} commands, {len(c['patterns'])} named patterns.", '',
              '| move | command | input | HA6 patterns | move code | EN |', '|---|---|---|---|---|---|']
        for m in c['moves']:
            cm = m.get('commands', [])
            pats = ', '.join(f"{p['code'] or ''}={p['num'] if p['num'] is not None else '?'}" for p in m.get('ha6_patterns', []))
            L.append(f"| `{m['name']}` | {', '.join(x['cmd'] for x in cm)} | {' '.join(cm[0].get('input', [])[:4]) if cm else ''} | "
                     f"{pats[:80]}{' (implied)' if m.get('pattern_implied') else ''} | {', '.join(x.replace('def_', '') for x in m.get('movecode', []))[:80]} | {en(m.get('comment'))} |")
        L += ['', '<details><summary>HA6 pattern names</summary>', '', '| id | ha6 | code | EN (JP) |', '|---|---|---|---|']
        for p in c['patterns']:
            t = p.get('title')
            L.append(f"| {p['id']} | {p['ha6'].replace('.ha6', '')} | {p.get('code') or ''} | {en(t)}{' (' + jp(t) + ')' if jp(t) else ''} |")
        L += ['', '</details>', '']
    with open(os.path.join(out, f'{game}_catalog.md'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(L) + '\n')

if __name__ == '__main__':
    main()
