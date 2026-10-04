#!/usr/bin/env python3
"""Free palette room per character: indices that no type-0 sprite uses (candidate ramp space for baking),
how many of them are (0,255,0) 'green = unused' fillers in the shipped .pal, and how many slots differ at all.
usage: palfree.py <data_dir> > palfree.csv"""
import sys, os, glob, numpy as np, csv
sys.path.insert(0, os.path.dirname(__file__))
import cgparse, palrange
D = sys.argv[1]
w = csv.writer(sys.stdout); w.writerow(['bank','slots','type0_imgs','idx_used','idx_free','free_green_slot0','free_black_slot0','free_other_slot0','free_idx_varying_across_slots','slots_distinct'])
for f in sorted(glob.glob(D + '/*.cg'), key=str.lower):
    stem = os.path.basename(f)[:-3]
    if stem.lower().startswith(('csel_', 'effect')) or not os.path.exists(f'{D}/{stem}.pal'): continue
    b = cgparse.Bank(f); pk = palrange.read_pal(f'{D}/{stem}.pal')
    h = np.zeros(256, np.int64); n0 = 0
    for m in b.images.values():
        if m.type == 0:
            h += np.bincount(np.frombuffer(b.b, np.uint8, m.blob_len, m.blob_off), minlength=256); n0 += 1
    used = h > 0; used[0] = True
    free = ~used
    p0 = pk[0][:, :3]
    green = free & (p0 == [0, 255, 0]).all(1); black = free & (p0 == 0).all(1)
    nslot = min(pk.shape[0], 64)
    distinct = len({pk[k].tobytes() for k in range(nslot)})
    var = free & np.array([(pk[1:nslot, i, :3] != pk[0, i, :3]).any() for i in range(256)])
    w.writerow([stem, pk.shape[0], n0, int(used.sum()), int(free.sum()), int(green.sum()), int(black.sum()), int((free & ~green & ~black).sum()), int(var.sum()), distinct])
