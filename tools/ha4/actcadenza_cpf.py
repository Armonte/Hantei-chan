#!/usr/bin/env python3
"""ActCadenza (Ver.1.03A) CPU AI (.CPF) -> MBAACC moon-3 <stem>_3.cpf.

usage: actcadenza_cpf.py SRC_DIR OUT_DIR [--selftest] [--ct-dir DIR] [--cmd-dir DIR]

SRC_DIR   ActCadenza data dir holding <STEM>.CPF (upper case; 1.00 02.p extract).
          1.03A (06.p) ships no CPF, so 02.p is the 1.03A set. A copy in
          --patch-dir (default .../mbac_v11_english/06) wins if present.
OUT_DIR   receives <stem>_3.cpf (LoadCharacterCPFData 0x450B40 tries
          data\\<File>_<moon>.cpf before data\\<File>.cpf).

File layout (same in both games, checked in IDA):
  193388 B = 188 B header + 100 entries x 1892 B (43 steps x 44 B) + 4000 B trailer
  (100 x 40 B Shift-JIS entry names, editor only). ActCadenza CPU_LoadCpfFile 0x40C030
  reads all 193388 B; MBAACC LoadCPFFile 0x4509D0 clamps the read to 189388 B, so the
  trailer is dropped (harmless: no runtime reader).
  Step (44 B at 188 + 1892*entry + 44*step): byte 0 type; type 0x10 = command, int16 at
  +6 = command id (ActCadenza CommandMove_TryExecute = CT slot; MBAACC
  Command_TryExecuteEntry = _c.txt id). Types 0x12/0x13 exist only in MBAACC
  (AI_ExecuteCpfInputStep 0x451D60); ActCadenza CPU_ApplyCpfStepInput 0x40CE30 ignores them.

Header dword (cpf+0) = base chance % H for the CPU reaction roll, not a count/index:
  ActCadenza CPU_RollDifficultyChance 0x40C440: x = H + 8*d - 16; p = x<0 ? 8 : x+8
  MBAACC AI_RollLevelScaledChance 0x451220:     x = H + 8*L - 24; p = (x<0 ? 0 : x) + 8
  both: H==0 -> never, H==100 -> always, p clamped to 80. Any H is in bounds
  (M_HISUI 0x2A is safe). For the same p at the same CPU level MBAACC needs H+8, so
  0 < H < 100 is rewritten to H+8 (0 and 100 stay). Assumes d (ActCadenza) and L
  (MBAACC g_CpuLevel) are the same option-menu level scale.

Command ids: the converter emits the step list verbatim. That holds because the moon-3
<stem>_3_c.txt is the ActCadenza _C.TXT with its own ids, and the CT slot index equals the
_C.TXT id; --selftest proves CT ids == _3_c.txt ids and every CPF command id is either
defined in both or in neither.
"""
import os
import re
import struct
import sys

STEMS = ["sion", "arc", "ciel", "akiha", "hisui", "kohaku", "shiki", "miyako", "warakia", "nero",
         "v_sion", "warc", "akaakiha", "m_hisui", "nanaya", "satsuki", "len", "neco", "aoko", "wlen",
         "kishima", "nechaos"]

G = '/mnt/c/dev/hantei-chan/gamedata'
PATCH_DIR = G + '/mbac_v11_english/06'
CT_DIR = G + '/mbac_v11_english/06'           # 1.03A CTs (06.p)
CMD_DIR = G + '/mbac_to_cc/actcadenza_moon3/data'  # <stem>_3_c.txt
VANILLA = G + '/mbaacc_ref/vanilla_0002'

FILE_SIZE = 193388
HDR = 188
ENTRY = 1892
NENT = 100
STEP = 44
NSTEP_CMD = 40          # steps 40..42 of an entry hold the entry's conditions
MBAACC_READ = 189388    # LoadCPFFile clamp (0x2E3CC)
CT_RECS = 200


def src_path(src_dir, stem, patch_dir):
    for d in (patch_dir, src_dir):
        if not d or not os.path.isdir(d):
            continue
        for n in os.listdir(d):
            if n.lower() == stem + '.cpf':
                return os.path.join(d, n)
    return None


def map_header(h):
    return h + 8 if 0 < h < 100 else h


def validate(data, name):
    errs = []
    if len(data) != FILE_SIZE:
        errs.append('%s: size %d != %d' % (name, len(data), FILE_SIZE))
        return errs
    if any(data[4:HDR]):
        errs.append('%s: header bytes 4..187 not zero' % name)
    h = struct.unpack_from('<i', data, 0)[0]
    if not 0 <= h <= 100:
        errs.append('%s: header chance %d outside 0..100' % name)
    return errs


def convert(data):
    h = struct.unpack_from('<i', data, 0)[0]
    nh = map_header(h)
    if nh == 100 and h != 100:
        raise ValueError('header %d would become the "always" value 100' % h)
    out = bytearray(data)
    struct.pack_into('<i', out, 0, nh)
    return bytes(out), h, nh


def steps(data):
    """yield (entry, step, type, cmd_id) for every non-empty command step."""
    for e in range(NENT):
        for s in range(NSTEP_CMD):
            o = HDR + ENTRY * e + STEP * s
            rec = data[o:o + STEP]
            if any(rec):
                yield e, s, rec[0], struct.unpack_from('<h', rec, 6)[0]


def ct_ids(path):
    d = open(path, 'rb').read()
    ids = set()
    for i in range(CT_RECS):
        r = d[4 + STEP * i:4 + STEP * i + STEP]
        if len(r) == STEP and r[0] != 0xFF:
            if r[0] != i & 0xFF:
                raise ValueError('%s: CT slot %d holds id byte %d' % (path, i, r[0]))
            ids.add(i)
    return ids


def cmd_ids(path):
    txt = open(path, 'rb').read().decode('cp932', 'replace')
    return {int(m.group(1)) for m in re.finditer(r'^[ \t]*(\d+)[ \t]', txt, re.M)}


def find_ci(d, name):
    if not os.path.isdir(d):
        return None
    for n in os.listdir(d):
        if n.lower() == name.lower():
            return os.path.join(d, n)
    return None


def selftest(src_dir, out_dir, patch_dir, ct_dir, cmd_dir):
    fails, notes = [], []
    # MBAACC reference: same size, header is a plain chance value.
    for n in ('arc.cpf', 'arc_1.cpf', 'b_m_hisui_p.cpf'):
        p = find_ci(VANILLA, n)
        if p:
            fails += validate(open(p, 'rb').read(), 'vanilla ' + n)
    for stem in STEMS:
        p = src_path(src_dir, stem, patch_dir)
        if not p:
            fails.append('%s: no source CPF' % stem)
            continue
        data = open(p, 'rb').read()
        fails += validate(data, stem)
        if len(data) != FILE_SIZE:
            continue
        conv, h, nh = convert(data)
        if conv[4:] != data[4:]:
            fails.append('%s: body changed' % stem)
        if struct.unpack_from('<i', conv, 0)[0] != nh or (0 < h < 100 and nh != h + 8):
            fails.append('%s: header rule' % stem)
        outp = os.path.join(out_dir, stem + '_3.cpf')
        if os.path.exists(outp) and open(outp, 'rb').read() != conv:
            fails.append('%s: %s differs from a fresh conversion' % (stem, outp))
        st = list(steps(data))
        bad = sorted({t for _, _, t, _ in st if t > 0x13})
        if bad:
            fails.append('%s: unknown step types %s' % (stem, bad))
        mb_only = [(e, s, t) for e, s, t, _ in st if t in (0x12, 0x13)]
        if mb_only:
            fails.append('%s: MBAACC-only step types 0x12/0x13 at %s (ActCadenza ignores them)' % (stem, mb_only[:4]))
        used = {c for _, _, t, c in st if t == 0x10}
        ctp = find_ci(ct_dir, stem + '_c.ct')
        cmp_ = find_ci(cmd_dir, stem + '_3_c.txt')
        if not ctp or not cmp_:
            fails.append('%s: missing %s' % (stem, 'CT' if not ctp else '_3_c.txt'))
            continue
        ct, cm = ct_ids(ctp), cmd_ids(cmp_)
        if ct != cm:
            fails.append('%s: CT ids vs _3_c.txt ids differ: only CT %s, only _c %s'
                         % (stem, sorted(ct - cm), sorted(cm - ct)))
        split = sorted(c for c in used if (c in ct) != (c in cm))
        if split:
            fails.append('%s: CPF command ids %s resolve in one game only' % (stem, split))
        dead = sorted(c for c in used if c not in ct)
        if dead:
            notes.append('%s: CPF command ids %s are in neither table (no-op in both games)' % (stem, dead))
        notes.append('%s: header %d -> %d, %d command steps, %d distinct ids' % (stem, h, nh, sum(1 for x in st if x[2] == 0x10), len(used)))
    for n in notes:
        print('note: ' + n)
    for f in fails:
        print('FAIL: ' + f)
    print('selftest: %s (%d failures)' % ('ok' if not fails else 'FAILED', len(fails)))
    return 0 if not fails else 1


def main(argv):
    args = [a for a in argv if not a.startswith('--')]
    opts = {}
    it = iter(argv)
    for a in it:
        if a in ('--ct-dir', '--cmd-dir', '--patch-dir'):
            opts[a] = next(it)
            args.remove(opts[a])
    if len(args) != 2:
        print(__doc__)
        return 2
    src_dir, out_dir = args
    patch_dir = opts.get('--patch-dir', PATCH_DIR)
    os.makedirs(out_dir, exist_ok=True)
    for stem in STEMS:
        p = src_path(src_dir, stem, patch_dir)
        if not p:
            print('error: %s: no %s.CPF in %s or %s' % (stem, stem.upper(), patch_dir, src_dir))
            return 1
        data = open(p, 'rb').read()
        errs = validate(data, stem)
        if errs:
            print('error: ' + '; '.join(errs))
            return 1
        conv, h, nh = convert(data)
        with open(os.path.join(out_dir, stem + '_3.cpf'), 'wb') as f:
            f.write(conv)
        print('%-9s %s  header %d -> %d' % (stem, p, h, nh))
    if '--selftest' in argv:
        return selftest(src_dir, out_dir, patch_dir, opts.get('--ct-dir', CT_DIR), opts.get('--cmd-dir', CMD_DIR))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
