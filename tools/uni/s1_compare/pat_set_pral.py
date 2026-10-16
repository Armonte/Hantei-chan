#!/usr/bin/env python3
"""render-match17: set PRAL (blend byte) of some slots of one .pat part set, in place (length-preserving; the slot must
already carry PRAL). Used to build PRAL 3 (blur mask) cases: UNI2 1.42 ships no PRAL 3 part.
Usage: pat_set_pral.py IN.pat SET SLOTS(comma) VALUE OUT.pat"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from pat_solo_prop import scan

b = bytearray(open(sys.argv[1], 'rb').read()); want = int(sys.argv[2])
slots = {int(x) for x in sys.argv[3].split(',')}; val = int(sys.argv[4])
for r in scan(b, want):
    if r['slot'] in slots:
        if 'PRAL' not in r: raise SystemExit('slot %d has no PRAL' % r['slot'])
        b[r['PRAL'][0]] = val
open(sys.argv[5], 'wb').write(b); print('wrote', sys.argv[5], 'PRAL', val, 'slots', sorted(slots))
