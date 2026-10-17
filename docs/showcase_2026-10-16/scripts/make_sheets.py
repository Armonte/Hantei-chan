#!/usr/bin/env python3
"""make_sheets.py: one side-by-side sheet per case (game | ours | sosfiro | gonp, then the |d| x4 diff row over the
shared footprint), at most 1600 px wide, from work/m/<id>*.png + .json (showcase_metric.py). Also writes manifest.json."""
import json, os, sys
from PIL import Image, ImageDraw, ImageFont
S = '/home/teo/dev/hantei-chan-wt/parity/docs/showcase_2026-10-16'; M = S + '/work/m'
CASES = json.load(open(S + '/scripts/cases.json'))
TOOLS = [('ours', 'Hantei-chan (ours, update/parity-1014)'), ('sosfiro', "sosfiro's fork (v3.0.0_alpha10)"), ('gonp', 'gonp upstream (gonpgonp/Hantei-chan 7e97e19)')]
try:
    F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 15); FB = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
except Exception:
    F = FB = ImageFont.load_default()


def cell(im, w, h, head, sub, col=(235, 235, 235)):
    c = Image.new('RGB', (w, h + 46), (22, 22, 26)); d = ImageDraw.Draw(c)
    if im is not None:
        s = min(w / im.width, h / im.height); r = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.LANCZOS)
        c.paste(r, ((w - r.width) // 2, 46 + (h - r.height) // 2))
    d.text((6, 3), head, font=FB, fill=col); d.text((6, 25), sub, font=F, fill=(200, 200, 200))
    return c


def blank(w, h, lines):
    im = Image.new('RGB', (w, h), (60, 20, 20)); d = ImageDraw.Draw(im)
    y = 10
    for ln in lines:
        d.text((10, y), ln, font=F, fill=(255, 220, 220)); y += 20
    return im


man = {'generated': '2026-10-16', 'metric': 'mean |d| per channel (0-255) and % of pixels with |d| summed over RGB > 24, game capture vs each tool\'s render composited over the game\'s fighters-hidden plate with the same light model and registration; scored on the union of all tools\' footprints (showcase_metric.py)', 'cases': []}
for c in CASES:
    cid = c['id']; j = json.load(open(f'{M}/{cid}.json'))
    g = Image.open(f'{M}/{cid}_game.png'); W = 1600 // 4 - 4; H = int(W * g.height / g.width); H = min(H, 520)
    top = [cell(g, W, H, 'GAME (ground truth)', c['game_capture'])]
    bot = [cell(None, W, H, '', '')]
    ImageDraw.Draw(bot[0]).multiline_text((8, 60), 'diff rows: |game - tool| x4\non the shared footprint\n(black = match)', font=F, fill=(200, 200, 200))
    entry = {'id': cid, 'title': c['title'], 'game': c['game'], 'pattern_frame': c['pattern_frame'], 'game_capture': c['game_capture'],
             'notes': c.get('notes', ''), 'caveat': c.get('caveat', ''), 'metrics': {}, 'union_px': j['union_px'], 'v2_model_args': j['args']}
    for t, tname in TOOLS:
        r = j['tools'].get(t); st = c.get('status', {}).get(t, '')
        if r is None:
            msg = c.get('cant_open', {}).get(t, 'no render')
            top.append(cell(blank(W, H, ["CAN'T OPEN / NO RENDER"] + [msg[i:i + 44] for i in range(0, len(msg), 44)]), W, H, tname.split(' (')[0], 'n/a', (255, 150, 150)))
            bot.append(cell(None, W, H, '', '')); entry['metrics'][t] = None; continue
        good = t == 'ours'
        top.append(cell(Image.open(f'{M}/{cid}_{t}.png'), W, H, tname.split(' (')[0], 'mean |d| %.2f   px>24 %.1f%%' % (r['mean_abs_d'], r['pct_px_gt24']), (150, 255, 150) if good else (255, 210, 120)))
        bot.append(cell(Image.open(f'{M}/{cid}_{t}_diff.png'), W, H, '', ''))
        entry['metrics'][t] = {'mean_abs_d': r['mean_abs_d'], 'pct_px_gt24': r['pct_px_gt24'], 'native_footprint': r.get('native_footprint'), 'status': st}
    row = lambda cs: (lambda im: ([im.paste(x, (i * (W + 4), 0)) for i, x in enumerate(cs)], im)[1])(Image.new('RGB', (4 * (W + 4) - 4, cs[0].height), (10, 10, 10)))
    a, b = row(top), row(bot)
    title = Image.new('RGB', (a.width, 78), (10, 10, 10)); d = ImageDraw.Draw(title)
    d.text((8, 4), '%s  [%s, %s]' % (c['title'], c['game'], c['pattern_frame']), font=FB, fill=(255, 255, 255))
    t = c.get('caveat') or c.get('notes', ''); d.text((8, 32), t[:175], font=F, fill=(190, 190, 190)); d.text((8, 52), t[175:350], font=F, fill=(190, 190, 190))
    sheet = Image.new('RGB', (a.width, title.height + a.height + b.height), (10, 10, 10))
    sheet.paste(title, (0, 0)); sheet.paste(a, (0, title.height)); sheet.paste(b, (0, title.height + a.height))
    os.makedirs(S + '/sheets', exist_ok=True)
    p = f'sheets/{cid}.jpg'; sheet.save(f'{S}/{p}', quality=88)
    entry['images'] = {'sheet': p}
    man['cases'].append(entry)
json.dump(man, open(S + '/work/manifest_cases.json', 'w'), indent=1)  # merged into manifest.json by hand-written notes
print('\n'.join('%-28s ' % e['id'] + '  '.join('%s %s' % (t, ('%.2f/%.1f%%' % (m['mean_abs_d'], m['pct_px_gt24'])) if m else 'n/a') for t, m in e['metrics'].items()) for e in man['cases']))
