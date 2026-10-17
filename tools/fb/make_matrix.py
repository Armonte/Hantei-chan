#!/usr/bin/env python3
"""Generates docs/formats/fb_format_matrix.md from docs/formats/evidence/fb_census.tsv (fbarctool census) + the support table below.
Edit SUPPORT when a cell moves; re-run:  python3 tools/fb/make_matrix.py"""
import collections, os, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CENSUS = os.path.join(ROOT, 'docs/formats/evidence/fb_census.tsv')
OUT = os.path.join(ROOT, 'docs/formats/fb_format_matrix.md')
LEVELS = ['none', 'read', 'edit', 'rt', 'proof']   # rt = byte-exact round trip over every shipped file; proof = also proven in the game
LNAME = {'none': 'none', 'read': 'read', 'edit': 'edit', 'rt': 'byte-exact round trip', 'proof': 'in-game proven'}

def title(p):
    pl = p.lower()
    if '/mb/ac/' in pl: return 'MBAC (Act Cadenza PC)'
    if 'mb/meltyblood' in pl: return 'Melty Blood 2002'
    if '/mb/r/' in pl: return 'ReAct'
    if '/mbaacc' in pl: return 'MBAACC PC 1.07'
    if '/games/gof1/' in pl: return 'GOF1'
    if '/games/gof' in pl: return 'GOF2'
    if '/games/pb/' in pl: return 'PB2K1'
    if 'games/rbo' in pl: return 'RBO'
    if 'rosa' in pl: return 'Rosa (2002/2005)'
    if 'dmp' in pl or 'drill' in pl or 'aquat1c' in pl: return 'dMp'
    if 'yamayuri' in pl: return 'Lilian Fourhand (+ Rosa omake)'
    return 'other'

# (title, ext) -> (level, how / next step). Container rows come from fbarctool; character formats from the Hantei-chan loaders.
SUPPORT = {
 ('MBAC (Act Cadenza PC)', '.DAT'): ('rt', 'HA4 character: ha4tool roundtrip 50/50 byte-identical, 12k edits reload clean (tools/ha4/results); NOT yet in-game proven here'),
 ('MBAC (Act Cadenza PC)', '.EX3'): ('none', 'LLIF compressed BMP: no reader (M4)'),
 ('MBAC (Act Cadenza PC)', '.PAL'): ('edit', 'pal_file.cpp'),
 ('MBAC (Act Cadenza PC)', '.TXT'): ('edit', 'character descriptor / command .txt (M4: byte rt)'),
 ('MBAC (Act Cadenza PC)', '.CT'): ('none', 'collision table (M4)'), ('MBAC (Act Cadenza PC)', '.CPF'): ('none', 'M4'),
 ('MBAC (Act Cadenza PC)', '.WMT'): ('none', 'M4'), ('MBAC (Act Cadenza PC)', '.FNT'): ('none', 'M4'), ('MBAC (Act Cadenza PC)', '.INI'): ('none', 'plain text'),
 ('MBAC (Act Cadenza PC)', '.OGG'): ('read', 'opaque Ogg (BGM preview plays it)'),
 ('MBAACC PC 1.07', '.HA6'): ('rt', 'tools/ha6_regress.sh (round trip, known 31-file box cleanup exit 5)'),
 ('MBAACC PC 1.07', '.DAT'): ('edit', 'stage .DAT ("bgmake"): stage browser / save (tools/bg_regress.sh)'),
 ('MBAACC PC 1.07', '.TXT'): ('edit', 'character descriptors / commands'),
 ('MBAACC PC 1.07', '.BMP'): ('read', 'CG / palettes'), ('MBAACC PC 1.07', '.DDS'): ('read', 'CG textures'), ('MBAACC PC 1.07', '.PNG'): ('read', ''),
 ('MBAACC PC 1.07', '.OGG'): ('read', 'BGM preview'), ('MBAACC PC 1.07', '.WAV'): ('none', ''),
 ('RBO', '.DAT'): ('rt', 'HAN2RBO: han2tool roundtrip/modelrt (patrt-full agent owns the rest)'), ('RBO', '.DT2'): ('rt', 'same'),
 ('RBO', '.IMG'): ('rt', 'han2tool imgrt'), ('RBO', '.FOB'): ('rt', 'han2tool fob section (fob_file); fobdis disassembler'),
 ('RBO', '.CG'): ('rt', 'han2tool cgrt'), ('RBO', '.REP'): ('rt', 'han2tool rep section'), ('RBO', '.RP2'): ('rt', 'rep section'), ('RBO', '.RP3'): ('rt', 'rep section'), ('RBO', '.RP4'): ('rt', 'rep section'),
 ('RBO', '.FNT'): ('rt', 'han2tool fnt section'), ('RBO', '.WAV'): ('rt', 'han2tool audio section'), ('RBO', '.TXT'): ('rt', 'misc section'),
 ('GOF2', '.DT2'): ('rt', 'han2tool'), ('GOF2', '.DAT'): ('rt', 'HAN2RBO .DAT / data archives'), ('GOF2', '.PAT'): ('rt', 'han2tool patrt'), ('GOF2', '.CHP'): ('rt', 'han2tool chp section'),
 ('GOF2', '.IMG'): ('rt', 'han2tool imgrt'), ('GOF2', '.FOB'): ('rt', 'fob section'), ('GOF2', '.FNT'): ('rt', 'fnt section'), ('GOF2', '.WAV'): ('rt', 'audio section'), ('GOF2', '.TXT'): ('rt', 'misc section'),
 ('GOF1', '.DAT'): ('rt', 'framedata_gof1 / han2tool gof1rt'), ('GOF1', '.EX3'): ('rt', 'han2tool ex3 (byte-pair blocks, all tile exactly)'),
 ('GOF1', '.B'): ('rt', 'opaque, proven by the archive rebuild'), ('GOF1', '.CPF'): ('rt', 'opaque, archive rebuild'), ('GOF1', '.CT'): ('rt', 'opaque, archive rebuild'), ('GOF1', '.TXT'): ('rt', 'opaque, archive rebuild'),
 ('Melty Blood 2002', '.DAT'): ('none', '備前長船 triple-XOR character, 116-B frames (M3)'),
 ('ReAct', '.DAT'): ('none', 'ReAct character, 216-B frames (M3)'),
 ('PB2K1', '.DAT'): ('none', '時は来た triple-XOR character (M3) + stage .dat'),
 ('dMp', '.DAT'): ('none', 'proto-RBO data (M3)'), ('dMp', '.FOB'): ('none', 'FOB script VM (fobdis.py exists; M3/M4)'), ('dMp', '.IMG'): ('none', 'obfuscated IMG sheets (M4)'),
 ('Rosa (2002/2005)', '.FOB'): ('none', 'M3'), ('Rosa (2002/2005)', '.IMG'): ('none', 'M4'),
 ('Lilian Fourhand (+ Rosa omake)', '.BGC'): ('none', 'FCHIP/FMAP BG (M4)'), ('Lilian Fourhand (+ Rosa omake)', '.MAP'): ('none', 'M4'), ('Lilian Fourhand (+ Rosa omake)', '.EX3'): ('none', 'M4'),
}
DEFAULT = ('none', '')
for t in ('Melty Blood 2002', 'ReAct', 'PB2K1'):
    for e in ('.EX3',): SUPPORT.setdefault((t, e), ('none', 'LLIF compressed BMP: no reader (M4)'))
for t in ('Melty Blood 2002', 'ReAct', 'PB2K1', 'MBAC (Act Cadenza PC)'):
    for e in ('.CT', '.CPF', '.WMT', '.FNT', '.CCT', '.B', '.BMP', '.H'): SUPPORT.setdefault((t, e), ('none', 'M4'))
    for e in ('.WAV', '.MP3', '.TXT'): SUPPORT.setdefault((t, e), ('read' if e != '.TXT' else 'edit', ''))
for t in ('dMp', 'Rosa (2002/2005)', 'Lilian Fourhand (+ Rosa omake)'):
    for e in ('.WAV', '.MP3', '.TXT', '.REP'): SUPPORT.setdefault((t, e), ('read' if e != '.TXT' else 'edit', ''))
for e in ('.WAV', '.MP3', '.BMP', '.FNT', '.H', '.PAC', '.WMT'): SUPPORT.setdefault(('GOF1', e), ('rt', 'opaque / media, proven by the archive rebuild'))
# ---- per-title suites (tools/fb/run_fb_suite.sh -> fbchartool <title>) ----
for e, how in (('.EX3', 'fbchartool: LLIF blocks + bit-exact Gage re-encode'), ('.WAV', 'fbchartool: RIFF chunks'), ('.MP3', 'fbchartool: frames validated'), ('.FNT', 'fbchartool: bitmap font'),
               ('.TXT', 'fbchartool: Shift-JIS text'), ('.CT', 'fbchartool: _C.CT command table / CHARASELECT.CT typed (docs/formats/mbr.md)'), ('.CPF', 'fbchartool: CPU script typed (mbr.md)'), ('.WMT', 'fbchartool: win quotes typed (mbr.md)')):
    SUPPORT[('ReAct', e)] = ('rt', how)
SUPPORT[('ReAct', '.DAT')] = ('proof', 'Hantei4 characters (framedata_ha4) + bgmake stages + the MB-format leftover; fbchartool react; in-game: ARC.DAT shift +160 px visible (docs/formats/evidence/react_ingame_*.png)')
SUPPORT[('ReAct', '')] = ('rt', 'MB-format character (stage-2 container, 10.p entry 00): gof1 loader, fbchartool react')
for e, how in (('.EX3', 'fbchartool mb: LLIF blocks + bit-exact Gage re-encode'), ('.WAV', 'fbchartool: RIFF chunks'), ('.MP3', 'fbchartool: frames validated'), ('.FNT', 'fbchartool: bitmap font'),
               ('.TXT', 'fbchartool: Shift-JIS text (incl. VECTOR.TXT)'), ('.CT', 'fbchartool: _C.CT / older CT variants / CHARSEL.CT typed (docs/formats/mb.md)'), ('.CT2', 'older command table variant, typed'), ('.CPF', 'fbchartool: CPU script typed (mb.md)'), ('.WMT', 'fbchartool: win messages typed (mb.md)')):
    SUPPORT[('Melty Blood 2002', e)] = ('rt', how)
SUPPORT[('Melty Blood 2002', '.DAT')] = ('proof', 'GOF1-family container + embedded MB strip sprite bank (view, per-strip import); fbchartool mb; in-game: all characters layer shift +40 visible (docs/formats/evidence/mb_ingame_*.png; +160 crashes mb.exe)')
SUPPORT[('Melty Blood 2002', '')] = ('rt', 'extensionless archive entries: MB-format characters / data blobs handled by fbchartool mb')
for e in ('.EX3', '.CT', '.CCT', '.CPF', '.WMT', '.FNT', '.B', '.BMP', '.H', '.MP3', '.WAV', '.TXT', ''):
    SUPPORT[('PB2K1', e)] = ('rt', 'fbchartool pb2k1: typed / modelled member (docs/formats/pb2k1.md); BGM blobs 02/03/04.dat = inner archives under the name cipher (section 16)')
SUPPORT[('PB2K1', '.DAT')] = ('proof', 'Party Breakers characters (three-section cipher, framedata_pb2k1) + PB strip sprite bank (view, per-strip import) + BGM blobs; in-game: all characters layer shift +40 visible (docs/formats/evidence/pb2k1_ingame_*.png)')
for e, how in (('.FOB', 'han2::dmpfob: dMp dialect, 376,668 instructions decoded, byte-exact (docs/formats/dmp.md)'), ('.IMG', 'raw 16-bit sheets, exact A1R5G5B5 / A4R4G4B4 re-encode'), ('.DAT', 'DEMOnn.DAT replays typed (DmpMatchSetup + 108001 input rows)'), ('.WAV', 'RIFF chunks')):
    SUPPORT[('dMp', e)] = ('rt', 'fbchartool dmp: ' + how)
for e, how in (('.FOB', 'Rosa script bank (dMp VM, opcodes shifted by one), byte-exact, 24k instructions decoded'), ('.IMG', 'v4 enciphered with the file stem / v1 plain, byte-exact (docs/formats/rosa.md)'), ('.WAV', 'RIFF chunks'), ('.MP3', 'frames validated')):
    SUPPORT[('Rosa (2002/2005)', e)] = ('rt', 'fbchartool rosa: ' + how)
ARCH_LEVEL = 'rt'

def main():
    rows = collections.OrderedDict()
    arcs = {}
    for l in open(CENSUS, encoding='utf-8'):
        p = l.rstrip('\n').split('\t')
        if len(p) < 6 or p[0] == 'FAIL': continue
        a, kind, ext, magic, n, sz = p[0], p[1], p[2], p[3], int(p[4]), int(p[5])
        t = title(a)
        base = os.path.basename(a).lower()
        key = (t, base)
        arcs.setdefault(key, {}).setdefault((ext, magic), (n, sz, kind, a))
    # dedupe installs: one archive per (title, basename, content signature)
    byTitle = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, set()]))
    archTitle = collections.defaultdict(dict)
    for (t, base), d in arcs.items():
        sig = tuple(sorted((k, v[0], v[1]) for k, v in d.items()))
        archTitle[t].setdefault(sig, (base, next(iter(d.values()))[2], sum(v[0] for v in d.values()), sum(v[1] for v in d.values())))
        if len(archTitle[t]) and archTitle[t][sig][0] != base: pass
    seen = set()
    for (t, base), d in sorted(arcs.items()):
        sig = (t, tuple(sorted((k, v[0], v[1]) for k, v in d.items())))
        if sig in seen: continue
        seen.add(sig)
        for (ext, magic), (n, sz, kind, a) in d.items():
            g = byTitle[t][ext]; g[0] += n; g[1] += sz; g[2].add(magic)
    out = []
    out.append('# French-Bread format matrix (fb-formats branch)\n')
    out.append('Generated by `tools/fb/make_matrix.py` from `docs/formats/evidence/fb_census.tsv` (`fbarctool census` over every archive on this machine; identical installs of the same title counted once). Re-run after every milestone.\n')
    out.append('Support levels: **none** < **read** (parsed / viewable) < **edit** (editable in Hantei-chan) < **byte-exact round trip** (every shipped file parses and re-serialises identically, in `tools/fb/run_fb_suite.sh` or `tools/han2/run_roundtrip.sh`) < **in-game proven** (edited file loaded by the game).\n')
    out.append('## 1. Archive containers (all titles on this machine)\n')
    out.append('Every container below is read, extracted and rebuilt by `src/fbarc` and proven byte-exact over all 118 shipped archives by `tools/fb/run_fb_suite.sh` (edited rebuilds verified for every kind). In-game proof: pending (M6).\n')
    out.append('| Title | Container | Archive files | Entries | Bytes | Level |\n|---|---|---|---|---|---|')
    kinds = {}
    for l in open(CENSUS, encoding='utf-8'):
        p = l.rstrip('\n').split('\t')
        if len(p) < 6 or p[0] == 'FAIL': continue
        kinds.setdefault(title(p[0]), {}).setdefault(os.path.basename(p[0]).lower(), (p[1], 0, 0))
    for t in sorted(archTitle):
        files = sorted(v[0] for v in archTitle[t].values())
        kind = sorted({v[1] for v in archTitle[t].values()})
        out.append('| %s | %s | %s | %d | %s | %s |' % (t, '; '.join(kind), ', '.join(f for f in files)[:160] + (' ...' if len(', '.join(files)) > 160 else ''),
                   sum(v[2] for v in archTitle[t].values()), '{:,}'.format(sum(v[3] for v in archTitle[t].values())), LNAME[ARCH_LEVEL]))
    out.append('\nNot archives (justified n/a): `gof/Data/System00.dat`, `gof/Data/uninst.dat` (installer), `pb/pbex.dat` (trainer table).\n')
    out.append('## 2. File types inside the archives, per title\n')
    out.append('Magic = the first 4 plain bytes after the archive cipher; several magics per extension are listed (most frequent first is not guaranteed). Count / bytes are plain sizes.\n')
    out.append('| Title | Ext | Files | Plain bytes | Level | How / next step |\n|---|---|---|---|---|---|')
    summary = collections.Counter()
    for t in sorted(byTitle):
        for ext, (n, sz, magics) in sorted(byTitle[t].items()):
            lv, how = SUPPORT.get((t, ext), DEFAULT)
            mg = ' '.join(sorted(magics)[:3]) + (' ...' if len(magics) > 3 else '')
            out.append('| %s | %s | %d | %s | %s | %s |' % (t, ext or '(none)', n, '{:,}'.format(sz), LNAME[lv], (how + ' (magic ' + mg + ')').strip()))
            summary[lv] += 1
    out.append('\n## 3. Cell summary\n')
    out.append(', '.join('%s: %d' % (LNAME[l], summary[l]) for l in LEVELS) + ' (title x extension cells in section 2)\n')
    open(OUT, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
    print('wrote', OUT, dict(summary))
main()
