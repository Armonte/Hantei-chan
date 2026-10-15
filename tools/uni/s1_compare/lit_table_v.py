#!/usr/bin/env python3
"""Old S1 metric (stage-lit blend model, sheet.py game_vs_model) for one set of editor black/white shots.
  lit_table_v.py <editor _new_k/_new_w dir>   -> case, lit mean |d|, % px > 24
Game frames/plates/sidecars: tools/gamecap/captures/s1_compare/game (direct-forced frame, same frame/tick as v2)."""
import importlib.util, json, os, sys
from PIL import Image
H = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('sheet', os.path.join(H, 'sheet.py')); sh = importlib.util.module_from_spec(spec); spec.loader.exec_module(sh)
sh.LIGHT = json.load(open('/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare/light_stage1.json'))
G = '/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare/game/'
E = sys.argv[1].rstrip('/') + '/'
for n, _, z in sh.cases():
    if n.startswith('mbtl'): continue
    try:
        m = json.load(open(G + n + '_game.json'))
        r = sh.game_vs_model(Image.open(G + n + '_game.png'), Image.open(G + n + '_game_plate.png'), Image.open(E + n + '_new_k.png'),
                             Image.open(E + n + '_new_w.png'), z, m, sh.EDITOR_ORIGIN)
    except FileNotFoundError as e:
        print('%s\tmissing' % n); continue
    if r is None: print('%s\tno footprint' % n); continue
    L = sh.game_vs_model.last_light
    print('%s\t%.1f\t%.0f' % (n, L[1], L[2]))
