#!/usr/bin/env python3
"""before | game | after triples per stage (docs/bg_research/accuracy/triples/).

  triples.py <capture dir> <out dir> <bg dir (WSL)> <bg_render.exe> <bg_test.exe> <bg_render_old.exe> [ids...]

before = the renderer at 9d751c6 (update/ex-mbac, tools/stage_capture/before/bg_render_old.cpp built against
that commit), drawing the stage after T sim ticks where T is the first tick whose pool equals the game's c0
pool (bg_test --find-tick); after = this branch's bg_render with the game's c0 state imported. Both at game
camera (0,0), zoom 1, 640x480. Metrics against the game capture: exact / close(<=8) / mae.
"""
import sys, os, subprocess, json, re
import numpy as np
from PIL import Image, ImageDraw
cap, out, bgdir, rnd, tst, old = sys.argv[1:7]
ids = sys.argv[7:] or sorted(int(d[1:]) for d in os.listdir(cap) if re.fullmatch(r's\d\d', d))
os.makedirs(out, exist_ok=True)
w = lambda p: subprocess.check_output(['wslpath', '-w', p]).decode().strip()
def metr(a, b):
    D = np.abs(np.asarray(a, np.int16) - np.asarray(b, np.int16)).max(2)
    return dict(exact=round(float((D == 0).mean() * 100), 2), close=round(float((D <= 8).mean() * 100), 2),
                mae=round(float(np.abs(np.asarray(a, np.int16) - np.asarray(b, np.int16)).mean()), 3))
res = []
for i in map(int, ids):
    d = f'{cap}/s{i:02d}'
    if not os.path.exists(f'{d}/c0.png') or not os.path.exists(f'{d}/c0.pool'):
        continue
    ini = open(f'{bgdir}/BgList.ini', 'rb').read().decode('latin1').replace('\r', '')
    m = re.search(r'\[Bg_%03d\][^\[]*?DataFile\s*=\s*(\S+)' % i, ini)
    df = m.group(1) if m else f'bg{i:02d}'
    dat = w(f'{bgdir}/{df}.dat')
    ft = subprocess.run([tst, dat, '--find-tick', w(f'{d}/c0'), '20000'], capture_output=True, text=True).stdout
    mt = re.search(r'MATCH tick=(\d+)', ft)
    T = int(mt.group(1)) if mt else None
    bt = re.search(r'best tick=(\d+)', ft)
    Tb = T if T is not None else (int(bt.group(1)) if bt else 0)
    subprocess.run([old, dat, w(f'{out}/_b.png'), str(Tb)], capture_output=True)
    subprocess.run([rnd, dat, '--out', w(f'{out}/_a.png'), '--state', w(f'{d}/c0')], capture_output=True)
    g = Image.open(f'{d}/c0.png').convert('RGB')
    b = Image.open(f'{out}/_b.png').convert('RGB') if os.path.exists(f'{out}/_b.png') else Image.new('RGB', g.size)
    a = Image.open(f'{out}/_a.png').convert('RGB')
    mb, ma = metr(g, b), metr(g, a)
    tri = Image.new('RGB', (1920, 498)); dr = ImageDraw.Draw(tri)
    for k, (im, lab) in enumerate([(b, 'BEFORE (9d751c6) exact %.2f%% close %.2f%%' % (mb['exact'], mb['close'])),
                                   (g, 'GAME s%02d %s c0 cam(0,0)' % (i, df)),
                                   (a, 'AFTER (feat/stage) exact %.2f%% close %.2f%%' % (ma['exact'], ma['close']))]):
        tri.paste(im, (k * 640, 18)); dr.text((k * 640 + 4, 3), lab, fill=(255, 255, 0))
    tri.save(f'{out}/s{i:02d}_before_game_after.png')
    for f in ('_a.png', '_b.png'):
        if os.path.exists(f'{out}/{f}'): os.remove(f'{out}/{f}')
    r = dict(id=i, file=df, simTick=T, before=mb, after=ma); res.append(r); print(json.dumps(r), flush=True)
json.dump(res, open(f'{out}/triples.json', 'w'), indent=1)
