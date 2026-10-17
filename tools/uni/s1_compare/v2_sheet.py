#!/usr/bin/env python3
"""v2 full-frame sheets: game (UNI2-IM hitbox overlay) | game clean | ours | sosfiro, same case and frame.

  v2_sheet.py <case> [--v2 DIR] [--game DIR] [--game-name NAME] [--out NAME]
  --game-name: game capture to use (e.g. <case>_unlit, captured with the chara aura held at 0, see v2_light_*.gcs)
  --out: sheet name (default <case>)

The editor shots (shoot_v2.sh, zoom 1, boxes on) are placed on the game's 1280x720 frame by geometry only:
P1's origin in the game frame comes from the capture sidecar (the game's own camera transform, sheet.py
game_transform); the editor's origin is where it draws its world axes. Screen px per sprite px = game scale
(editor zoom 1). Missing columns are left empty. Writes <v2>/<case>.png."""
import importlib.util, json, os, sys
import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('sheet', os.path.join(HERE, 'sheet.py'))
sh = importlib.util.module_from_spec(spec); spec.loader.exec_module(sh)
W, H = 1280, 720


def place(ed, gorg, gsc):
    """Editor shot -> 1280x720 canvas in game pixels, editor origin on the game's P1 origin."""
    org = sh.editor_origin(ed)
    if abs(gsc - 1.0) > 0.01:
        ed = ed.resize((int(round(ed.width * gsc)), int(round(ed.height * gsc))), Image.BILINEAR)
        org = (org[0] * gsc, org[1] * gsc)
    out = Image.new('RGB', (W, H), (0, 0, 0))
    out.paste(ed, (int(round(gorg[0] - org[0])), int(round(gorg[1] - org[1]))))
    return out


def tile(im, text):
    t = Image.new('RGB', (W, H + 26), (24, 24, 24))
    if im is not None: t.paste(im.convert('RGB').resize((W, H)) if im.size != (W, H) else im.convert('RGB'), (0, 26))
    ImageDraw.Draw(t).text((8, 6), text if im is not None else text + ' (missing)', fill=(255, 220, 120))
    return t


def main():
    a = sys.argv[1:]
    v2 = '/mnt/c/dev/hantei-chan/docs/audit_2026_10_14/evidence/compare/v2'
    gd = '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2'
    if '--v2' in a: i = a.index('--v2'); v2 = a[i + 1]; del a[i:i + 2]
    if '--game' in a: i = a.index('--game'); gd = a[i + 1]; del a[i:i + 2]
    gname = None; oname = None
    if '--game-name' in a: i = a.index('--game-name'); gname = a[i + 1]; del a[i:i + 2]
    if '--out' in a: i = a.index('--out'); oname = a[i + 1]; del a[i:i + 2]
    case = a[0]; gname = gname or case; oname = oname or case
    def op(*paths):  # first existing, non-black image (MBTL shots are PrintWindow, which sometimes returns black)
        for p in paths:
            if os.path.exists(p):
                im = Image.open(p).convert('RGB')
                if im.getextrema()[0][1] > 0: return im
        return None
    game = op(os.path.join(gd, gname + '_game.png'), os.path.join(gd, gname + '_game_bb.png'))
    gim = op(os.path.join(gd, gname + '_game_im.png'))
    meta = json.load(open(os.path.join(gd, gname + '_game.json')))
    gorg, gsc = sh.game_transform(meta)
    ours = op(os.path.join(v2, 'ours', case + '_ours.png'))
    sos = op(os.path.join(v2, 'sosfiro', case + '_sosfiro.png'))
    # sosfiro's shoot.sh uses the case's cases.txt zoom (1264x861 view), ours is zoom 1
    szoom = next((z for n, _, z in sh.cases() if n == case), 1.0)
    info = 'p%s f%s t%s' % (meta.get('pattern'), meta.get('frame'), meta.get('tick'))
    if 'aura_rgb' in meta:  # P1's chara aura = the specular add of its .pat parts (CharaObj_DrawWithStageLight 0x5CE3C0)
        add = [int(int(c * 255.0) * meta['aura_strength']) for c in meta['aura_rgb']] if meta.get('aura_type', -1) >= 0 else [0, 0, 0]
        info += ', aura add %s' % add
    tiles = [tile(gim, 'game + %s hitboxes (%s)' % ('MBTL-IM' if case.startswith('mbtl') else 'UNI2-IM', info)), tile(game, 'game clean (%s)' % info),
             tile(place(ours, gorg, abs(gsc[0])) if ours else None, 'ours (Hantei-chan render-match), boxes on'),
             tile(place(sos, gorg, abs(gsc[0]) / szoom) if sos else None, 'sosfiro fork (zoom %g shot, scaled)' % szoom)]
    sheet = Image.new('RGB', (2 * W + 8, 2 * (H + 26) + 8 + 30), (40, 40, 40))
    for i, t in enumerate(tiles):
        sheet.paste(t, ((i % 2) * (W + 8), 30 + (i // 2) * (H + 34)))
    ImageDraw.Draw(sheet).text((8, 8), oname + ': P1 origin %.1f,%.1f, %.3f game px per sprite px' % (gorg[0], gorg[1], gsc[0]),
                               fill=(255, 255, 255))
    os.makedirs(v2, exist_ok=True)
    sheet.save(os.path.join(v2, oname + '.png'), optimize=True)
    print('wrote', os.path.join(v2, oname + '.png'))


if __name__ == '__main__':
    main()
