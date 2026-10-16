#!/usr/bin/env python3
"""Builds the UNI2 in-game proof files for a NEW PUPS palette bank (chr000_p1.pal) - E19 / S5.

Output folder (default work/s45/uni2_pups_proof/___St/data/chr000/):
  chr000_p1.pal  the file `cgmtool pal-new-pups chr000.cg 1` writes (a copy of chr000.pal), then every colour of every
                 palette with red and blue swapped (entry 0 kept), so the bank is unmistakable on screen
  chr000.ha6     the shipped file with one tag added: pattern 0 (Hyde's stand, no BaseData template) gets PUPS 1,
                 which makes the game draw that pattern with chr000_p1.pal (UNI2_MBTL_DATA.md: PUPS n = _pn.pal)

Deploying them needs the loader's override folder, <game>\\UNI2-IM\\Mods\\data\\chr000\\ (fpan-im / UNI2-IM; the CG manager's
"Save to mod folder" writes there): UNI2 1.42 opens a loose ___St\\data file but the archive copy still wins (S3 test, 0x4B9971). The output folder name is only a staging layout. Usage: make_uni2_pups_proof.py [--cgmtool build/cgmtool.exe] [--out DIR]
"""
import argparse, hashlib, os, shutil, struct, subprocess, sys

GD = '/mnt/c/dev/hantei-chan/gamedata/uni2/data/chr000'
ap = argparse.ArgumentParser()
ap.add_argument('--cgmtool', default=os.path.join(os.path.dirname(__file__), '../../build/cgmtool.exe'))
ap.add_argument('--out', default='/mnt/c/dev/hantei-chan/work/s45/uni2_pups_proof/___St/data/chr000')
a = ap.parse_args()
os.makedirs(a.out, exist_ok=True)

# 1. new PUPS bank through the CG manager's own code path (CreatePupsBank), in a scratch copy of the character folder
stage = os.path.join(os.path.dirname(a.out.rstrip('/')), '_stage')
os.makedirs(stage, exist_ok=True)
for f in ('chr000.cg', 'chr000.pal'):
    shutil.copyfile(os.path.join(GD, f), os.path.join(stage, f))
p1 = os.path.join(stage, 'chr000_p1.pal')
if os.path.exists(p1):
    os.remove(p1)
r = subprocess.run([a.cgmtool, 'pal-new-pups', subprocess.check_output(['wslpath', '-w', os.path.join(stage, 'chr000.cg')]).decode().strip(), '1'],
                   capture_output=True, text=True)
print(r.stdout.strip())
if r.returncode or not os.path.exists(p1):
    sys.exit('pal-new-pups failed: ' + r.stdout + r.stderr)
d = bytearray(open(p1, 'rb').read())
assert d == open(os.path.join(GD, 'chr000.pal'), 'rb').read(), 'the new PUPS bank is not a copy of the base .pal'
count = struct.unpack_from('<I', d, 12)[0]
assert 16 + count * 1024 == len(d), 'not a UNI-layout .pal'
for p in range(count):
    for e in range(1, 256):
        o = 16 + p * 1024 + e * 4
        d[o], d[o + 2] = d[o + 2], d[o]
open(os.path.join(a.out, 'chr000_p1.pal'), 'wb').write(d)

# 2. PUPS 1 on pattern 0: insert the tag right after "PSTR" + pattern number
h = open(os.path.join(GD, 'chr000.ha6'), 'rb').read()
i = h.find(b'PSTR')
assert i >= 0 and struct.unpack_from('<I', h, i + 4)[0] == 0, 'pattern 0 not found'
seq_end = h.find(b'PEND', i)
assert h.find(b'PUPS', i, seq_end) < 0, 'pattern 0 already has PUPS'
h2 = h[:i + 8] + b'PUPS' + struct.pack('<I', 1) + h[i + 8:]
open(os.path.join(a.out, 'chr000.ha6'), 'wb').write(h2)
shutil.rmtree(stage)
for f in ('chr000_p1.pal', 'chr000.ha6'):
    print(f, hashlib.sha1(open(os.path.join(a.out, f), 'rb').read()).hexdigest())
