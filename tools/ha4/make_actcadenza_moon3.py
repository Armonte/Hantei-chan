#!/usr/bin/env python3
"""Build the ActCadenza moon-3 data set (package "P3") from a converted roster and list its files.

usage: make_actcadenza_moon3.py ROSTER MBAACC_VANILLA OUT_DIR TSV_PATH
  ROSTER          ha4tool convert output (roster_v3: <c>/<c>.ha6, <c>.cg, <c>_cc_c.txt, <c>_actcadenza_moon.ini)
  MBAACC_VANILLA  vanilla MBAACC data (0002.p extract: <c>.cg, <c>_0.txt, _type.txt)
  OUT_DIR         gets data\\ (game-relative) and _reports\\
Per shared character <c> (MBAACC File1 stem):
  <c>_3.HA6   ActCadenza HA6 with CG sprite ids rebased onto MBAACC <c>.cg by BMP name (mbac_moon_remap_sprites.py)
  <c>_3.txt   MBAACC <c>_0.txt with ProjectName <c>_3 and <c>_3.HA6 appended to [DataFile] (layered last, so it wins)
  <c>_3_c.txt the converter's <c>_cc_c.txt (1.03A CT guts)
Shared: _type.txt (vanilla + [Type_3], REPLACES the vanilla file), actcadenza_moon.ini (per-character overrides).
G_CHAOS (moon 9) data comes from gamedata/mbac_to_cc/g_chaos/cc_data_v3.
"""
import sys, os, hashlib, subprocess, shutil, glob

HERE = os.path.dirname(os.path.abspath(__file__))
REMAP = '/mnt/c/dev/hantei-chan/gamedata/tools/scripts/mbac_moon_remap_sprites.py'
CHARS = ["sion", "arc", "ciel", "akiha", "hisui", "kohaku", "shiki", "miyako", "warakia", "nero", "v_sion", "warc",
         "akaakiha", "m_hisui", "nanaya", "satsuki", "len", "neco", "aoko", "wlen", "kishima", "nechaos"]
G = '/mnt/c/dev/hantei-chan/gamedata/mbac_to_cc'
BOSS_CHAOS = G + '/g_chaos/cc_data_v3'
BOSS_GAKIHA = G + '/g_akiha_mbac/cc_data_v3'

TYPE3 = """\r
[Type_3] // ActCadenzaStyle (ActCadenza 1.03A moon, Hantei-chan S8b) ---------------------\r
// Constants: docs/mbac_moon/FEASIBILITY.md (mbacPC). Per-character EX-shield windows that differ from\r
// the common 6/6/8 below are in data\\actcadenza_moon.ini (applied by PovertyCaster at load).\r
SparkType = 0\r
MagicCircuitMax = 30000\r
CircuitSpark = 1\r
ReverseBeat = 1\r
\r
AttackRatio_BloodHeat = 1.0\r
AttackRatio_Heat = 1.0\r
AttackRatio_Max = 1.0\r
DefenceRatio_BloodHeat = 1.0\r
DefenceRatio_Heat = 1.0\r
DefenceRatio_Max = 1.0\r
\r
MC_PlusVal = 1.0\r
MC_GuardPlus = 1\r
\r
BloodHeat_HpPlus = 20\r
Heat_HpPlus = 12\r
\r
VSDamageVal = 1.0\r
\r
IsExGuard = 0\r
IsExReduce = 0\r
IsReduce = 1\r
\r
ShieldType = 0\r
ExShieldTime_Stand = 6\r
ExShieldTime_Crouch = 6\r
ExShieldTime_Air = 8\r
ShieldKeepTime = 24\r
ShieldCost = 40\r
\r
// ActCadenza has no guard gauge: uncrushable (the HUD bar is hidden by hook H4)\r
GG_MaxVal = 1000000.0\r
GG_HealVal = 1000.0\r
GG_HealPlusVal = 0.0\r
GG_StopVal = 0\r
GG_RecoverVal = 1\r
GG_CorrectVal = 1.0\r
\r
GQ_MaxVal = 0.0\r
GQ_RecoverVal = 1.0\r
GQ_StopVal = 0\r
\r
DamageVal_Ground = 1.0\r
DamageVal_Air = 0.88\r
DamageVal_Crouch = 1.08\r
GuardVal_Ground = 1.0\r
GuardVal_Air = 1.0\r
GuardVal_Crouch = 1.0\r
"""

def find_ci(d, name):
    for f in os.listdir(d):
        if f.lower() == name.lower(): return os.path.join(d, f)
    return None

def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()

def main():
    roster, van, out, tsv = sys.argv[1:5]
    data = os.path.join(out, 'data'); rep = os.path.join(out, '_reports')
    os.makedirs(data, exist_ok=True); os.makedirs(rep, exist_ok=True)
    rows, inject, notes = [], [], []
    def add(name, src=None, lst=rows):
        p = os.path.join(data, name)
        if src: shutil.copyfile(src, p)
        lst.append(('data\\' + name, sha(p), os.path.getsize(p)))
    ini = ["; ActCadenza moon-3 per-character overrides (format: docs/actcadenza_parity/ACTCADENZA_MOON_SIDECAR.md).",
           "; Sections are MBAACC File1 names. A key that is absent means the [Type_3] value applies.",
           "[Format]", "Version = 1", "Moon = 3", ""]
    for c in CHARS:
        cdir = os.path.join(roster, c)
        ha6 = os.path.join(cdir, c + '.ha6')
        mcg = os.path.join(cdir, c + '.cg')
        ccg = find_ci(van, c + '.cg')
        l0 = find_ci(van, c + '_0.txt')
        if not (os.path.exists(ha6) and ccg and l0):
            notes.append('%s: missing input (ha6 %s, MBAACC cg %s, _0.txt %s)' % (c, os.path.exists(ha6), ccg, l0)); continue
        o3 = os.path.join(data, c + '_3.HA6')
        r = subprocess.run([sys.executable, '-I', REMAP, ha6, mcg, ccg, o3, os.path.join(rep, c + '_remap.txt')],
                           capture_output=True, text=True)
        if r.returncode: notes.append('%s: remap failed: %s' % (c, r.stderr.strip()[-300:])); continue
        add(c + '_3.HA6')
        # <c>_3.txt from MBAACC <c>_0.txt
        t = open(l0, 'rb').read().decode('cp932').replace('\r\n', '\n').rstrip('\n').split('\n')
        o, sec, last, n = [], None, -1, 0
        for line in t:
            s_ = line.strip()
            if s_.startswith('['): sec = s_
            if s_.lower().startswith('projectname'): line = 'ProjectName=%s_3' % c
            if sec == '[DataFile]' and s_.replace(' ', '').lower().startswith('filenum='):
                n = int(s_.split('=')[1]); line = 'FileNum= %d' % (n + 1)
            o.append(line)
            if sec == '[DataFile]' and s_.lower().startswith('file') and not s_.lower().startswith('filenum'): last = len(o)
        o.insert(last, 'File%02d=%s_3.HA6' % (n, c))
        open(os.path.join(data, c + '_3.txt'), 'wb').write(('\r\n'.join(o) + '\r\n').encode('cp932'))
        add(c + '_3.txt')
        cc = os.path.join(cdir, c + '_cc_c.txt')
        add(c + '_3_c.txt', cc)
        sc = os.path.join(cdir, c + '_actcadenza_moon.ini')
        if os.path.exists(sc):
            ini += [l for l in open(sc, 'rb').read().decode('cp932').replace('\r', '').split('\n') if l and not l.startswith(';')] + [""]
    # _type.txt: vanilla bytes + [Type_3]
    vt = open(find_ci(van, '_type.txt'), 'rb').read()
    tt = vt + (b'' if vt.endswith(b'\r\n') else b'\r\n') + TYPE3.encode('cp932')
    open(os.path.join(data, '_type.txt'), 'wb').write(tt)
    add('_type.txt')
    open(os.path.join(data, 'actcadenza_moon.ini'), 'wb').write('\r\n'.join(ini).encode('cp932'))
    add('actcadenza_moon.ini')
    # G_CHAOS: charaselect entry 27 exists in vanilla (GiantFlag 1, no data); files are new.
    for f in sorted(os.listdir(BOSS_CHAOS)):
        add(f, os.path.join(BOSS_CHAOS, f))
    # G_AKIHA_MBAC: needs the S7 charaselect entry 36 (schema 2 inject) -> listed separately.
    for f in sorted(os.listdir(BOSS_GAKIHA)):
        add(f, os.path.join(BOSS_GAKIHA, f), inject)
    vanilla = {f.lower() for f in os.listdir(van)}
    with open(tsv, 'w', newline='\n') as w:
        w.write('# ActCadenza moon-3 package (P3), class sim. path\\tsha256\\tsize. Built by tools/ha4/make_actcadenza_moon3.py.\n')
        w.write('# replaces_vanilla = 1: the path exists in vanilla MBAACC 0002.p data\\ (ships WHOLE).\n')
        w.write('path\tsha256\tsize\treplaces_vanilla\n')
        for p, h, s in sorted(rows, key=lambda r: r[0].lower()):
            w.write('%s\t%s\t%d\t%d\n' % (p, h, s, int(p[5:].lower() in vanilla)))
    inj = tsv.replace('.tsv', '_NEEDS_SCHEMA2_INJECT.tsv')
    with open(inj, 'w', newline='\n') as w:
        w.write('# G_AKIHA_MBAC (charaselect CharaNo_036, GiantFlag 2): needs schema 2 "inject" (S7). Not in the schema-1 package.\n')
        w.write('path\tsha256\tsize\treplaces_vanilla\n')
        for p, h, s in sorted(inject, key=lambda r: r[0].lower()):
            w.write('%s\t%s\t%d\t%d\n' % (p, h, s, int(p[5:].lower() in vanilla)))
    open(os.path.join(rep, 'notes.txt'), 'w').write('\n'.join(notes) + '\n')
    print('files %d (+%d inject-only), notes %d' % (len(rows), len(inject), len(notes)))
    for n in notes: print('  ' + n)

if __name__ == '__main__':
    main()
