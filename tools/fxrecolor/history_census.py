#!/usr/bin/env python3
"""Per-character census of CG (sprite) storage format in older Melty Blood versions vs MBAACC.

Games (read-only, game dirs are never copied; each .DAT is extracted by fbarctool.exe into a scratch dir
under artifacts/fxrecolor_hist/_tmp, only its CG blob is read, then the file is deleted):
  MB2002  /mnt/c/games/MB/MeltyBlood/data03.p  CG blob magic-less, 8 palettes, byte 0x10 == 0xFF -> 24-bit BGR, else 8-bit idx (docs/formats/mb.md s4)
  ReAct   /mnt/c/games/MB/R/01.p               'BMP Cutter2/3' bank in the .DAT (docs/formats/mbr.md s13), image type 0..5
  MBAC    /mnt/c/games/MB/AC/02.p, 05.p        same Hantei4 container + BMP Cutter bank
  MBAACC  /mnt/c/games/mbaacc/data/<char>.cg   reference (tools/fxrecolor/cgparse.py)

Outputs (docs/cg/effect_recolor_data/):
  history_census.csv     game,archive,character,n_images,t0..t5,t_other,idx8,rgb24,note,images ("name=type;...")
  history_name_diff.csv  game,archive,character,name,type_old,type_mbaacc,transition  (name = case-insensitive match
                         in the MBAACC bank of the same character; effect.cg <-> EFFECT.DAT included)
  history_skipped.txt    characters that could not be parsed + reason
Usage: python3 tools/fxrecolor/history_census.py [--games MB2002,ReAct,MBAC] [--keep]
"""
import os, sys, re, csv, struct, subprocess, collections, argparse
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)
import cgparse

FBARC = os.path.join(ROOT, '..', 'fb-formats', 'build', 'fbarctool.exe')
SCRATCH = os.path.join(ROOT, 'artifacts', 'fxrecolor_hist', '_tmp')
OUTDIR = os.path.join(ROOT, 'docs', 'cg', 'effect_recolor_data')
MBAACC_DIR = '/mnt/c/games/mbaacc/data'

GAMES = {
    'MB2002': [('/mnt/c/games/MB/MeltyBlood/data03.p', 'data03')],
    'ReAct': [('/mnt/c/games/MB/R/01.p', '01')],
    'MBAC': [('/mnt/c/games/MB/AC/02.p', '02'), ('/mnt/c/games/MB/AC/05.p', '05')],
}

# stage-2 DAT keys (docs/formats/mb.md s3; tools/gof1/gof1_pack.py)
KEY_HEADER = bytes.fromhex("4d656d6f727982a682e7815b82c182c482b182c682c9835683658349834e")
KEY_BLOB = bytes.fromhex("686982dcc56e6f834a82c982e581482082b28bea984a836982b1546f82be82c982e5")


def xor_key(buf, key):
    n = len(buf)
    if not n:
        return b''
    p = np.arange(n, dtype=np.uint32)
    k = np.frombuffer(key, np.uint8)[p % len(key)]
    return (np.frombuffer(bytes(buf), np.uint8) ^ ((p + k) & 0xFF).astype(np.uint8)).tobytes()


def wpath(p):
    return subprocess.check_output(['wslpath', '-w', p], text=True).strip()


def list_dats(archive):
    out = subprocess.check_output([FBARC, 'ls', wpath(archive)], text=True, errors='replace')
    names = []
    for ln in out.splitlines():
        m = re.match(r'\s*\d+\s+\d+\s+\d+\s+(\S+\.DAT)\s*$', ln, re.I)
        if m:
            names.append(m.group(1))
    return names


def extract(archive, name):
    os.makedirs(SCRATCH, exist_ok=True)
    p = os.path.join(SCRATCH, name)
    if os.path.exists(p):
        os.remove(p)
    subprocess.check_output([FBARC, 'extract', wpath(archive), name, wpath(SCRATCH)], text=True, errors='replace')
    if not os.path.exists(p):
        raise RuntimeError('extract produced no file')
    return p


# ---------------------------------------------------------------- MB 2002 CG blob
def parse_mb2002(path):
    """-> (list of (name, type, w, h)), info dict. type = 'idx8' | 'rgb24'."""
    with open(path, 'rb') as f:
        hdr = bytearray(f.read(0x444))
        hdr = bytearray(xor_key(hdr, KEY_HEADER))
        pat_end, parts, cg_off, cg_size = struct.unpack_from('<IIII', hdr, 0x14)
        fsize = os.fstat(f.fileno()).st_size
        if cg_size == 0 or cg_off + cg_size > fsize or cg_size > 64 << 20:
            raise ValueError('implausible CG offset/size %#x/%#x (header decrypt failed?)' % (cg_off, cg_size))
        f.seek(cg_off)
        c = xor_key(f.read(cg_size), KEY_BLOB)
    true_color = c[0x10] == 0xFF
    offs = struct.unpack_from('<3000i', c, 0x14)
    pal = np.frombuffer(c, np.uint8, 8192, 0x2EF4).reshape(8, 256, 4)
    ndist = len({pal[i].tobytes() for i in range(8)})
    imgs = []
    for i, o in enumerate(offs):
        if o < 0:
            continue
        if o + 52 > len(c):
            raise ValueError('image %d offset out of range' % i)
        name = c[o:o + 36].split(b'\0')[0].decode('cp932', 'replace')
        x1, y1, x2, y2 = struct.unpack_from('<4h', c, o + 0x24)
        imgs.append((name, 'rgb24' if true_color else 'idx8', x2 - x1 + 1, y2 - y1 + 1))
    return imgs, {'true_color': true_color, 'distinct_palettes': ndist, 'bytes': len(c)}


# ---------------------------------------------------------------- ReAct / MBAC Hantei4 .DAT CG bank
def parse_h4dat(path):
    """-> (list of (name, type int, w, h)), info dict."""
    with open(path, 'rb') as f:
        h = f.read(0x24)
        if h[:7] != b'Hantei4':
            raise ValueError('not Hantei4 (magic %r)' % h[:8])
        cg_off, cg_size = struct.unpack_from('<II', h, 0x1C)
        fsize = os.fstat(f.fileno()).st_size
        if cg_size == 0:
            raise ValueError('cgBlobSize is 0')
        if cg_off + cg_size > fsize:
            raise ValueError('CG blob out of file')
        f.seek(cg_off)
        c = f.read(cg_size)
    mg = c[:12].split(b'\0')[0]
    if not mg.startswith(b'BMP Cutter'):
        raise ValueError('CG blob magic %r' % mg)
    u = lambda o: struct.unpack_from('<I', c, o)[0]
    nimg = u(0x2014 + 12)
    idx = struct.unpack_from('<3000I', c, 0x2014 + 48)
    imgs = []
    for i, o in enumerate(idx):
        if o == 0xFFFFFFFF:
            continue
        if o + 72 > len(c):
            raise ValueError('image %d offset out of range' % i)
        name = c[o:o + 32].split(b'\0')[0].decode('cp932', 'replace')
        t, w, hh = struct.unpack_from('<iII', c, o + 32)
        imgs.append((name, t, w, hh))
    return imgs, {'magic': mg.decode('ascii', 'replace'), 'header_nimg': nimg}


def parse_mbaacc(char):
    p = os.path.join(MBAACC_DIR, char.lower() + '.cg')
    if not os.path.exists(p):
        return None
    bank = cgparse.Bank(p)
    return [(m.name, m.type, m.w, m.h) for _, m in sorted(bank.images.items())]


ALIAS = {'EFFECT_': 'effect'}  # MB2002 EFFECT_.DAT (never loaded by the game) is compared with effect.cg


def tname(t):
    return t if isinstance(t, str) else 't%d' % t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--games', default='MB2002,ReAct,MBAC')
    ap.add_argument('--keep', action='store_true', help='keep extracted DATs')
    a = ap.parse_args()
    os.makedirs(OUTDIR, exist_ok=True)
    census, diff, skipped, extra = [], [], [], []
    mb_cache = {}

    def mbaacc_bank(char):
        k = char.lower()
        if k not in mb_cache:
            try:
                mb_cache[k] = parse_mbaacc(k)
            except Exception as e:
                skipped.append(('MBAACC', k, 'cgparse failed: %r' % e))
                mb_cache[k] = None
        return mb_cache[k]

    def add_census(game, arch, char, imgs, note=''):
        cnt = collections.Counter(tname(t) for _, t, _, _ in imgs)
        row = dict(game=game, archive=arch, character=char, n_images=len(imgs))
        for t in range(6):
            row['t%d' % t] = cnt.get('t%d' % t, 0)
        row['t_other'] = sum(v for k, v in cnt.items() if k not in ('t0', 't1', 't2', 't3', 't4', 't5', 'idx8', 'rgb24'))
        row['idx8'] = cnt.get('idx8', 0)
        row['rgb24'] = cnt.get('rgb24', 0)
        row['note'] = note
        row['images'] = ';'.join('%s=%s' % (n, tname(t)) for n, t, _, _ in imgs)
        census.append(row)

    # MBAACC reference census for every character that is compared
    compared = set()

    for game in a.games.split(','):
        for archive, arch in GAMES[game]:
            try:
                names = list_dats(archive)
            except Exception as e:
                skipped.append((game, arch, 'cannot list archive: %r' % e))
                continue
            for dn in names:
                char = dn[:-4]
                path = None
                try:
                    path = extract(archive, dn)
                    if game == 'MB2002':
                        imgs, info = parse_mb2002(path)
                        note = 'true_color=%s distinct_palettes=%d' % (info['true_color'], info['distinct_palettes'])
                    else:
                        imgs, info = parse_h4dat(path)
                        note = 'magic=%s' % info['magic']
                        if info['header_nimg'] != len(imgs):
                            note += ' header_nimg=%d' % info['header_nimg']
                except Exception as e:
                    skipped.append((game, '%s/%s' % (arch, dn), 'parse failed: %r' % e))
                    imgs = None
                finally:
                    if path and os.path.exists(path) and not a.keep:
                        os.remove(path)
                if imgs is None:
                    continue
                add_census(game, arch, char, imgs, note)
                new = mbaacc_bank(ALIAS.get(char.upper(), char))
                if new is None:
                    skipped.append((game, '%s/%s' % (arch, dn), 'no MBAACC bank %s.cg (name diff skipped; census kept)' % char.lower()))
                    continue
                compared.add(char.lower())
                nm = collections.defaultdict(list)
                for n, t, _, _ in new:
                    nm[n.lower()].append(t)
                om = collections.defaultdict(list)
                for n, t, _, _ in imgs:
                    om[n.lower()].append(t)
                for n, ts in om.items():
                    if n in nm:
                        to, tn = ts[0], nm[n][0]
                        dup = '' if len(ts) == 1 and len(nm[n]) == 1 else 'dup'
                        diff.append(dict(game=game, archive=arch, character=char, name=n, type_old=tname(to),
                                         type_mbaacc=tname(tn), transition='%s->%s' % (tname(to), tname(tn)), dup=dup))
                print('%-7s %-4s %-12s n=%d %s' % (game, arch, char, len(imgs), note), flush=True)

    for c in sorted(compared):
        b = mbaacc_bank(c)
        if b:
            add_census('MBAACC', 'cg', c, b)

    cols = ['game', 'archive', 'character', 'n_images'] + ['t%d' % t for t in range(6)] + ['t_other', 'idx8', 'rgb24', 'note', 'images']
    with open(os.path.join(OUTDIR, 'history_census.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, cols)
        w.writeheader()
        w.writerows(census)
    with open(os.path.join(OUTDIR, 'history_name_diff.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, ['game', 'archive', 'character', 'name', 'type_old', 'type_mbaacc', 'transition', 'dup'])
        w.writeheader()
        w.writerows(diff)
    with open(os.path.join(OUTDIR, 'history_skipped.txt'), 'w') as f:
        for s in skipped:
            f.write('\t'.join(s) + '\n')
    print('wrote %d census rows, %d diff rows, %d skipped' % (len(census), len(diff), len(skipped)))


if __name__ == '__main__':
    main()
