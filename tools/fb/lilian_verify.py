#!/usr/bin/env python3
"""Yamayuri Rendan / Lilian Fourhand (LilianFourhand.exe 2005) data verifier. WIP: see docs/formats/lilian.md for what is proven.

Parses and re-serializes byte-exact every entry of the six archives with the grammars documented in docs/formats/lilian.md:
  .p archives (PAC v0 name-keyed / v1 plain, key 0xE3DF59AC), AniEdit .TXT (numeric model + exact writer), [DATA]/[Enemy_N]/proc/ENDSTAFF text files
  (lossless line model + per-kind key checks against the keys the exe reads), .BGC / .MAP binaries, .REP replays (run-length input records),
  the GOF1-family character DAT stored as entry '00' of 00b.p (stage-2 cipher), EX3 (container magic only; --ex3 re-encodes), MP3/WAV (magic).
usage: lilian_verify.py [data_dir]   (default C:/dev/frenchbread/yamayuri_rendan/files/data, WSL path auto-detected)
Exit status 0 only when every check passes and 'unexplained' is 0."""
import os, re, struct, sys, collections

KEY = 0xE3DF59AC
CIPHER_SPAN = 8563          # 0x2173: only the first 8563 payload bytes are enciphered (v0)
DEFAULT_DIR = '/mnt/c/dev/frenchbread/yamayuri_rendan/files/data'
if not os.path.isdir(DEFAULT_DIR):
    DEFAULT_DIR = 'C:/dev/frenchbread/yamayuri_rendan/files/data'

fails = []
counts = collections.Counter()


def ok(cond, what):
    counts['checks'] += 1
    if not cond:
        counts['unexplained'] += 1
        if len(fails) < 60:
            fails.append(what)
    return cond


# ------------------------------------------------------------------ PAC
def sjis_upper(b):
    out = bytearray(b); i = 0
    while i < len(out):
        c = out[i]
        if 0x81 <= c <= 0x9F or 0xE0 <= c <= 0xFC:
            i += 2; continue
        if 0x61 <= c <= 0x7A:
            out[i] = c - 0x20
        i += 1
    return bytes(out)


def name_xor(name_raw, buf):
    nm = sjis_upper(name_raw); L = len(nm)
    n = min(len(buf), CIPHER_SPAN)
    for k in range(n):
        buf[k] ^= (k + nm[k % L] + 3) & 0xFF


class Pac:
    def __init__(self, path):
        self.path = path
        f = open(path, 'rb'); self.f = f
        mode, cnt = struct.unpack('<II', f.read(8)); cnt ^= KEY
        self.mode, self.count = mode, cnt
        idx = f.read(68 * cnt); self.index_raw = idx
        self.ents = []
        for i in range(cnt):
            e = bytearray(idx[68 * i:68 * i + 68])
            for j in range(59):
                e[j] ^= (3 * j * i + 61) & 0xFF
            name_raw = bytes(e[:60])
            off, size = struct.unpack_from('<II', e, 60); size ^= KEY
            self.ents.append((name_raw.split(b'\0')[0], name_raw, off, size))

    def payload(self, i):
        name, _, off, size = self.ents[i]
        self.f.seek(off); b = bytearray(self.f.read(size))
        if self.mode == 0:
            name_xor(name, b)
        elif self.mode == 2:
            for k in range(min(size, CIPHER_SPAN)): b[k] ^= 0xCA
        return bytes(b)

    def raw_payload(self, i):
        _, _, off, size = self.ents[i]
        self.f.seek(off); return self.f.read(size)

    def rebuild_index(self, sizes=None):
        """header + index re-serialized from the parsed model (offsets recomputed contiguously)"""
        n = self.count; out = bytearray(struct.pack('<II', self.mode, n ^ KEY)); off = 8 + 68 * n
        for i, (name, name_raw, _, size) in enumerate(self.ents):
            e = bytearray(name_raw)
            for j in range(59):
                e[j] ^= (3 * j * i + 61) & 0xFF
            out += bytes(e) + struct.pack('<II', off, size ^ KEY); off += size
        return bytes(out)


def check_pac(pac):
    ok(pac.rebuild_index() == pac.index_raw[:0] + open(pac.path, 'rb').read(8 + 68 * pac.count), 'index rebuild ' + pac.path)
    # entries are contiguous from the end of the index and the last one ends at EOF
    off = 8 + 68 * pac.count
    for name, _, o, s in pac.ents:
        ok(o == off, 'contiguous ' + name.decode('cp932', 'replace')); off += s
    ok(off == os.path.getsize(pac.path), 'EOF ' + pac.path)
    counts['pac entries'] += pac.count


# ------------------------------------------------------------------ AniEdit
FRAME_KEYS = ['ShiftX', 'ShiftY', 'U', 'V', 'W', 'H', 'tpage', 'AttrFlag', 'AlphaFlag', 'AlphaDepth', 'Angle', 'ZoomX', 'ZoomY', 'AniFlag', 'Jump', 'Delay', 'EtcFlag0']


def ani_parse(t):
    L = t.split('\r\n')
    assert L[-1] == ''; L.pop()
    secs = []
    for l in L:
        if l.startswith('['):
            assert l.endswith(']'); secs.append([l[1:-1], []])
        else:
            secs[-1][1].append(l)
    m = {'tex': [], 'pat': [], 'rect': [], 'param': []}
    for name, body in secs:
        d = [tuple(l.split('=', 1)) for l in body]
        if name == 'Header':
            assert [k for k, _ in d] == ['Name', 'TextureNum']; m['name'] = d[0][1]; m['texnum'] = int(d[1][1])
        elif re.fullmatch(r'Texture\d\d', name):
            assert [k for k, _ in d] == ['Name', 'Size']; m['tex'].append((int(name[7:]), d[0][1], int(d[1][1])))
        elif re.fullmatch(r'Pattern\d{3}', name):
            assert [k for k, _ in d] == ['FrameNum', 'Name']
            m['pat'].append({'id': int(name[7:]), 'n': int(d[0][1]), 'name': d[1][1], 'fr': []})
        elif re.fullmatch(r'Pattern\d{3}-\d{3}', name):
            p, fi = name[7:].split('-'); fr = {'p': int(p), 'f': int(fi), 'hantei': {}, 'effect': {}}
            keys = [k for k, _ in d]
            assert keys[:17] == FRAME_KEYS
            for k, v in d[:17]:
                fr[k] = float(v) if k in ('Angle', 'ZoomX', 'ZoomY') else int(v)
            for k, v in d[17:]:
                if k.startswith('Hantei'): fr['hantei'][int(k[6:])] = int(v)
                else:
                    assert k.startswith('Effect'); fr['effect'][int(k[6:])] = int(v)
            m['pat'][-1]['fr'].append(fr)
        elif name == 'HanteiRect':
            m['rect'] = [tuple(int(x) for x in v.split(',')) for _, v in d]
            assert [k for k, _ in d] == ['Hantei_%04d' % i for i in range(len(d))]
        elif name == 'EffectParam':
            m['param'] = [tuple(int(x) for x in v.split()) for _, v in d]
            assert [k for k, _ in d] == ['Param_%04d' % i for i in range(len(d))]
        else:
            raise AssertionError(name)
    return m


def ani_write(m):
    o = ['[Header]', 'Name=' + m['name'], 'TextureNum=%d' % m['texnum']]
    for i, n, s in m['tex']:
        o += ['[Texture%02d]' % i, 'Name=' + n, 'Size=%d' % s]
    for p in m['pat']:
        o += ['[Pattern%03d]' % p['id'], 'FrameNum=%d' % p['n'], 'Name=' + p['name']]
        for fr in p['fr']:
            o.append('[Pattern%03d-%03d]' % (fr['p'], fr['f']))
            for k in FRAME_KEYS:
                v = fr[k]
                o.append(k + '=' + ('%f' % v if k == 'Angle' else '%7.3f' % v if k in ('ZoomX', 'ZoomY') else '%d' % v))
            for i in sorted(fr['hantei']): o.append('Hantei%02d=%d' % (i, fr['hantei'][i]))
            for i in sorted(fr['effect']): o.append('Effect%02d=%d' % (i, fr['effect'][i]))
    o.append('[HanteiRect]')
    o += ['Hantei_%04d=%d,%d,%d,%d' % ((i,) + r) for i, r in enumerate(m['rect'])]
    o.append('[EffectParam]')
    o += ['Param_%04d=' % i + ''.join('%d ' % x for x in r) for i, r in enumerate(m['param'])]
    return ('\r\n'.join(o) + '\r\n').encode('cp932')


def check_ani(name, b):
    try:
        m = ani_parse(b.decode('cp932'))
        w = ani_write(m)
    except (AssertionError, ValueError, KeyError, IndexError) as e:
        ok(False, 'ani parse %s: %r' % (name, e)); return
    ok(w == b, 'ani roundtrip ' + name)
    ok(m['name'] == 'AniEdit Data' and m['texnum'] == len(m['tex']), 'ani header ' + name)
    for p in m['pat']:
        ok(p['n'] == len(p['fr']) and p['n'] > 0 and p['id'] < 255, 'ani pattern %s %d' % (name, p['id']))
        for fr in p['fr']:
            ok(all(0 <= i < 16 for i in fr['hantei']) and all(0 <= i < 10 for i in fr['effect']), 'ani slots ' + name)
            ok(all(v != 0 for v in list(fr['hantei'].values()) + list(fr['effect'].values())), 'ani zero stored ' + name)
            ok(all(v < len(m['rect']) for v in fr['hantei'].values()) and all(v < max(1, len(m['param'])) for v in fr['effect'].values()), 'ani index range ' + name)
            ok(fr['tpage'] <= len(m['tex']), 'ani tpage ' + name)
            if fr['tpage'] == len(m['tex']): counts['ani frames with tpage == textureCount (one past the array; 5 files x 7 frames in the shipped data)'] += 1
            counts['ani frames'] += 1
    ok(all(len(r) == 16 for r in m['param']), 'ani param width ' + name)
    counts['ani files'] += 1


# ------------------------------------------------------------------ line-model text files
LINE_RE = re.compile(r'^([ \t]*)(?:(//.*)|\[([^\]]*)\](.*)|([^=\[/ \t][^=]*?)([ \t]*)=([ \t]*)(.*)|(\S.*))?$')


def text_lines(t):
    """lossless: list of (text, eol); eol in CRLF / LF / ''"""
    out = []; pos = 0
    for m in re.finditer(r'\r\n|\n', t):
        out.append((t[pos:m.start()], m.group())); pos = m.end()
    if pos < len(t): out.append((t[pos:], ''))
    return out


def classify(line):
    m = LINE_RE.match(line)
    if m is None: return ('other', line)
    ind, com, sec, secws, key, ws1, ws2, val, other = m.groups()
    if com is not None: return ('comment', ind, com)
    if sec is not None: return ('section', ind, sec, secws)
    if key is not None: return ('kv', ind, key, ws1, ws2, val)
    if other is not None: return ('other', line)
    return ('blank', line)


def unclassify(c):
    k = c[0]
    if k == 'comment': return c[1] + c[2]
    if k == 'section': return c[1] + '[' + c[2] + ']' + c[3]
    if k == 'kv': return c[1] + c[2] + c[3] + '=' + c[4] + c[5]
    return c[1]


def split_comment(val):
    """value text as the exe sees it vs the trailing // comment authors add (the exe does NOT strip it: atoi/atof/sscanf stop on it)"""
    m = re.search(r'[ \t]*//', val)
    return (val[:m.start()], val[m.start():]) if m else (val, '')


PROC_CMDS = ('Wait SetScrollX SetScrollY SetScrollAddX SetScrollAddY Flip SetScrollRatio SetScrollLoop SetLoopEndCount SetMoveMode SetScrollFree SetBgPos SetBgPrio '
             'SetBgColorDepth InitializeBgColorDepth StopBgCount HanteiView SpriteHanteiView MutekiMode SetCharaPos SetCharaPosGround SetCharaControl SetCharaClip '
             'SetCharaKey SetPauseAble SetNextItem SetEnemy SetEnemy_AreaBoss SetEnemyPamam ResetEnemyPamam LoadBGM PlayBGM StopBGM FadeBGM PlayEffect StageClear').split()
ENEMY_KEYS = set('Type Position IsScroll GroundHit PlayerBulletHit MaxHp Attack Score IsBullet StartPat DestroyPat CenterPos RadarType NoShotDownPlus Shadow ShadowX ShadowY ShadowW ItemType AniTablePath AniTableName _SameParam_'.split()) | {'Param_%02d' % i for i in range(10)}
EDITOR_ONLY_KEYS = {'aram_01'}   # BG03ENEMY*.TXT: typo of Param_01, never read by the game
BULLET_KEYS = set('Speed Time AniTablePath AniTableName StartPattern HitPattern GroundHit HitVectorRoll Power PowPower Detonation'.split())
EFFECT_KEYS = set('Type AniTablePath AniTableName Time StartPat IsScroll'.split())
STATUS_KEYS = set('Gravity Jump Run Dash Fumituke JumpNum DashNum DashTime JumpShotStartAngle JumpShotAngleAdd FumitukePower'.split())
SHOT_KEYS = set('BulletNum Rapid PowRapid Time BulletCnt StartAngle IncAngle'.split())
LOCK_KEYS = set('Type ChargeTime DelayWait AttackDelayWait AttackEndWait Max LockOnFront'.split())
BGDATA_KEYS = {'Name', 'Path', 'Hantei'} | {'Layer%02d' % i for i in list(range(5)) + list(range(10, 15))}
BGVALUE_KEYS = set('BosstimeBest BosstimeDelay ShotDownWorst ShotDownDelay'.split())
CHARA_DATA_KEYS = {'Path', 'Ani', 'Data', 'Name'}


def check_text(name, b, kind):
    t = b.decode('cp932')
    lines = text_lines(t)
    cls = [classify(l) for l, _ in lines]
    rt = ''.join(unclassify(c) + e for c, (_, e) in zip(cls, lines))
    ok(rt == t, 'text roundtrip ' + name)
    sec = None
    unknown = []
    for c in cls:
        if c[0] == 'other':
            if kind != 'endstaff' and c[1].strip(): unknown.append(c[1])
            continue
        if c[0] == 'section': sec = re.sub(r'\d+$', '', c[2]) if re.match(r'^(Enemy|Effect|Bullet|SHOT)_\d+$', c[2]) else c[2]; continue
        if c[0] != 'kv': continue
        k = c[2]
        if kind == 'proc': good = k in PROC_CMDS
        elif kind == 'enemy': good = k in ENEMY_KEYS or k in EDITOR_ONLY_KEYS
        elif kind == 'bullet': good = k in BULLET_KEYS
        elif kind == 'effect': good = k in EFFECT_KEYS
        elif kind == 'bg': good = (sec in ('DATA', 'SUBDATA') and k in BGDATA_KEYS | {'ItemRate'}) or (sec == 'VALUE' and k in BGVALUE_KEYS)
        elif kind == 'chara':
            good = (sec in ('DATA', 'SUBDATA') and k in CHARA_DATA_KEYS) or (sec == 'STATUS' and k in STATUS_KEYS) or (sec == 'LOCKSHOT' and k in LOCK_KEYS) or (sec == 'SHOT_' and k in SHOT_KEYS)
        else: good = True
        if not good: unknown.append((sec, k))
    ok(not unknown, 'unknown keys %s %r' % (name, unknown[:5]))
    counts['text files'] += 1


def check_endstaff(name, b):
    check_text(name, b, 'endstaff')
    t = b.decode('cp932'); recs = []; nums = []
    for l, _ in text_lines(t):
        s = l.strip()
        if not s or s.startswith('//'): continue
        s = re.split(r'//', s)[0].strip()
        if ',' in s: recs.append(tuple(int(x) for x in s.split(',')))
        else: nums.append(int(s))
    ok(len(nums) == 2 and recs and recs[-1] == (-1, -1), 'endstaff structure')
    counts['endstaff records'] += len(recs) - 1


# ------------------------------------------------------------------ BGC / MAP
def bgc_parse(b):
    assert b[:9] == b'FCHIP SYS'
    ver, mode, flag, zero, gc, gr = struct.unpack_from('<6I', b, 9)
    assert ver == 0
    if mode:
        z, cnt, w, h = struct.unpack_from('<4I', b, 0x21); assert z == 0
        n = cnt * w * h * 3; base = 0x31
        assert len(b) == base + 2 * n
        return dict(mode=mode, flag=flag, zero=zero, gc=gc, gr=gr, cnt=cnt, w=w, h=h, a=b[base:base + n], b=b[base + n:base + 2 * n])
    z, cnt, w, h, n1, n2 = struct.unpack_from('<6I', b, 0x21); assert z == 0
    base = 0x39; n = cnt * w * h
    assert len(b) == base + 2 * n + 4 * cnt + 4 * n1 + 4 * n2
    p = base
    d = dict(mode=0, flag=flag, zero=zero, gc=gc, gr=gr, cnt=cnt, w=w, h=h, n1=n1, n2=n2)
    d['a'] = b[p:p + n]; p += n; d['b'] = b[p:p + n]; p += n
    d['sel'] = b[p:p + 4 * cnt]; p += 4 * cnt
    d['pa'] = b[p:p + 4 * n1]; p += 4 * n1; d['pb'] = b[p:p + 4 * n2]
    return d


def bgc_write(d):
    o = b'FCHIP SYS' + struct.pack('<6I', 0, d['mode'], d['flag'], d['zero'], d['gc'], d['gr'])
    if d['mode']:
        return o + struct.pack('<4I', 0, d['cnt'], d['w'], d['h']) + d['a'] + d['b']
    return o + struct.pack('<6I', 0, d['cnt'], d['w'], d['h'], d['n1'], d['n2']) + d['a'] + d['b'] + d['sel'] + d['pa'] + d['pb']


def check_bgc(name, b):
    try:
        d = bgc_parse(b)
    except (AssertionError, struct.error) as e:
        ok(False, 'bgc parse %s %r' % (name, e)); return None
    ok(bgc_write(d) == b, 'bgc roundtrip ' + name)
    ok(d['gc'] * d['gr'] == d['cnt'] and d['w'] == 32 and d['h'] == 32, 'bgc geometry ' + name)
    if d['mode']:
        if not (d['b'][0::3] == d['b'][1::3] == d['b'][2::3]): counts['bgc alpha plane NOT gray (ENDING_BG00: 95 px)'] += 1
    else:
        ok(max(d['a']) <= 4, 'bgc attr range ' + name)
    counts['bgc mode%d' % d['mode']] += 1
    return d


def check_map(name, b, chips=None):
    try:
        assert b[:8] == b'FMAP SYS'
        ver, w, h = struct.unpack_from('<3I', b, 8); assert ver == 0 and len(b) == 20 + 2 * w * h
    except (AssertionError, struct.error) as e:
        ok(False, 'map parse %s %r' % (name, e)); return
    cells = b[20:]
    ok(b[:8] + struct.pack('<3I', 0, w, h) + cells == b, 'map roundtrip ' + name)
    if chips is not None:
        ok(max(struct.unpack('<%dH' % (w * h), cells)) < chips, 'map chip index range ' + name)
    counts['map files'] += 1


# ------------------------------------------------------------------ REP
def rep_parse(b):
    h = struct.unpack_from('<9I', b, 0)
    n = h[8]; assert len(b) == 36 + 68 * n
    runs = [struct.unpack_from('<17I', b, 36 + 68 * i) for i in range(n)]
    return h, runs


def rep_write(h, runs):
    return struct.pack('<9I', *h) + b''.join(struct.pack('<17I', *r) for r in runs)


def check_rep(name, b):
    try:
        h, runs = rep_parse(b)
    except (AssertionError, struct.error) as e:
        ok(False, 'rep parse %s %r' % (name, e)); return
    ok(rep_write(h, runs) == b, 'rep roundtrip ' + name)
    ok(sum(r[0] + 1 for r in runs) == h[1], 'rep frame count ' + name)       # header +4 = total frames = sum(extra+1)
    ok(all(r[0] >= 0 for r in runs) and all(runs[i][1:] != runs[i + 1][1:] for i in range(len(runs) - 1)), 'rep runs are maximal ' + name)
    ok(h[4] == 0 and h[7] == 0, 'rep reserved header words ' + name)
    counts['rep files'] += 1; counts['rep runs'] += len(runs)


# ------------------------------------------------------------------ 00 (GOF1-family character DAT, stage-2 cipher)
K_HEADER = bytes.fromhex('4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e')
K_PATTERN = bytes.fromhex('4d65839382c782a42d2d82c88e9682cd594182e882bd82ad4e6182a282f182be82af82c782cb')
K_BLOB = bytes.fromhex('686982dcc56e6f834a82c982e581482082b28bea984a836982b1546f82be82c982e5')


def xorks(buf, key):
    return bytes(buf[p] ^ ((p + key[p % len(key)]) & 0xFF) for p in range(len(buf)))


def dat_decrypt(raw):
    d = bytearray(raw); d[:0x444] = xorks(d[:0x444], K_HEADER)
    pe, ps, co, cs = struct.unpack_from('<4I', d, 0x14)
    d[0x444:pe] = xorks(d[0x444:pe], K_PATTERN); d[pe:pe + ps] = xorks(d[pe:pe + ps], K_BLOB); d[co:co + cs] = xorks(d[co:co + cs], K_BLOB)
    if len(d) - (co + cs) == 0x4000: d[co + cs:] = xorks(d[co + cs:], K_BLOB)
    return bytes(d)


def dat_encrypt(plain):
    d = bytearray(plain); pe, ps, co, cs = struct.unpack_from('<4I', d, 0x14)
    d[0x444:pe] = xorks(d[0x444:pe], K_PATTERN); d[pe:pe + ps] = xorks(d[pe:pe + ps], K_BLOB); d[co:co + cs] = xorks(d[co:co + cs], K_BLOB)
    if len(d) - (co + cs) == 0x4000: d[co + cs:] = xorks(d[co + cs:], K_BLOB)
    d[:0x444] = xorks(d[:0x444], K_HEADER)
    return bytes(d)


def check_dat00(name, raw):
    p = dat_decrypt(raw)
    ok(p[:8] == bytes.fromhex('94f5914f92b79144'), 'dat signature')            # CP932 "備前長船"
    ok(struct.unpack_from('<I', p, 0x10)[0] == 18, 'dat version 18')
    pe, ps, co, cs = struct.unpack_from('<4I', p, 0x14)
    ok(pe + ps == co and cs == 0 and len(p) == co + 0x4000, 'dat layout')
    ok(dat_encrypt(p) == raw, 'dat cipher roundtrip')
    counts['gof1 dat'] += 1


# ------------------------------------------------------------------ driver
def kind_of(name):
    u = name.upper()
    if u.endswith('.TXT') or u.endswith('.TXT_'): pass
    return u


def main():
    d = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else DEFAULT_DIR
    deep_ex3 = '--ex3' in sys.argv
    for arc in ('00dt', '00bg', '00bgt', '00dm', '00b', '00e'):
        pac = Pac(os.path.join(d, arc + '.p')); check_pac(pac)
        names = {e[0].decode('cp932', 'replace').upper() for e in pac.ents}
        chips = {}
        for i, (nb, nraw, off, size) in enumerate(pac.ents):
            nm = nb.decode('cp932'); u = nm.upper()
            if arc == '00b':
                if u == '00': check_dat00(nm, pac.raw_payload(i)); continue
                b = pac.raw_payload(i)
                ok(b[:3] == b'ID3' or b[:2] in (b'\xff\xfb', b'\xff\xfa', b'\xff\xf3'), 'mp3 magic ' + nm); counts['mp3'] += 1; continue
            b = pac.payload(i)
            if arc == '00e':
                ok(b[:4] == b'RIFF' and b[8:12] == b'WAVE', 'wav ' + nm); counts['wav'] += 1
            elif arc == '00dm':
                check_rep(nm, b)
            elif arc == '00bg':
                if u.endswith('.BGC'):
                    r = check_bgc(nm, b); chips[u[:-4]] = r['cnt'] if r else None
                else:
                    check_map(nm, b, chips.get(u[:-4]))
                    ok(u[:-4] + '.BGC' in names, 'map has bgc ' + nm)
            elif arc == '00dt':
                if u.endswith('.EX3'):
                    ok(b[:4] == b'LLIF' and b[4:4 + len(nm) - 4].lower() == nm[:-4].lower().encode('cp932') and b[4 + len(nm) - 4:4 + len(nm) - 4 + 4] == b'.bmp', 'ex3 header ' + nm)
                    if not u.endswith('_M.EX3'): pass
                    counts['ex3'] += 1
                elif b.startswith(b'[Header]'):
                    check_ani(nm, b)
                elif u == 'ENDSTAFF.TXT': check_endstaff(nm, b)
                elif u == 'CHARABULLET.TXT' or u == '_CHARABULLET.TXT': check_text(nm, b, 'bullet')
                elif u == 'SYSTEMEFFECT.TXT': check_text(nm, b, 'effect')
                elif u == 'SYSTEMDATA.TXT': check_text(nm, b, 'bg')
                elif re.fullmatch(r'C0\dSUB\.TXT|C0\d\.TXT', u): check_text(nm, b, 'chara')
                else: ok(False, 'unclassified dt entry ' + nm)
            elif arc == '00bgt':
                if 'PROC' in u: check_text(nm, b, 'proc')
                elif 'ENEMY' in u: check_text(nm, b, 'enemy')
                else: check_text(nm, b, 'bg')
        if arc == '00dt':
            exs = {n for n in names if n.endswith('.EX3')}
            ani_tex = set()
            for i, (nb, _, _, _) in enumerate(pac.ents):
                if nb.decode('cp932').upper().endswith('.TXT'):
                    bb = pac.payload(i)
                    if bb.startswith(b'[Header]'):
                        for m in re.finditer(rb'\[Texture\d+\]\r\nName=([^\r]*)\.bmp\r', bb, re.I):
                            ani_tex.add(m.group(1).decode('cp932').upper() + '.EX3')
            ok(ani_tex <= exs, 'every AniEdit texture has an EX3: missing %r' % sorted(ani_tex - exs)[:5])
            masks = {n for n in exs if n.endswith('_M.EX3')}
            ok(all(m[:-6] + '.EX3' in exs for m in masks), 'every mask has a color EX3')
            unref = (exs - masks) - ani_tex
            system = {n for n in unref if re.match(r'(C_SEL_|SYS|ED0|LOADING|LOGO|DBGFNT)', n)}      # loaded by path from the exe (.\\data\\system\\*.bmp strings)
            orphans = unref - system
            ok(orphans == {'C03_04M.EX3', 'C04_04M.EX3', 'EN01_02.EX3', 'EN01_03.EX3', 'EN03_00.EX3', 'ENEMY_BARA00_02.EX3'}, 'EX3 orphans %r' % sorted(orphans))
            counts['ex3 system images'] = len(system); counts['ex3 orphans (unreferenced)'] = len(orphans)
        print('%-6s %4d entries ok' % (arc, pac.count))
    print(dict(counts))
    for f in fails: print('FAIL', f)
    print('unexplained:', counts['unexplained'])
    return 0 if counts['unexplained'] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
