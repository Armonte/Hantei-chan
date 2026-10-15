#!/usr/bin/env python3
"""v2 model check: game frame vs the editor's black/white-backdrop shots composited over the game's own backdrop
(the fighters-hidden plate), raw and with the stage light of the draw object.

  v2_model.py <case> [--bw DIR] [--game DIR] [--plate PNG] [--light k,c;k,c;k,c | --light-json F] [--out PNG] [--zoom Z] [--fit | --aura-fit]
              [--band N (default 1)] [--refine R (default 10; 0 = camera geometry only)] [--no-axis-fix] [--json OUT]

The light model is the game's own vertex path (uni2 1.42 PatPart_SetupWorldMatrixAndShader 0x415230):
diffuse = PRCL x DrawObj+0x0C (PatPart_ModulateColorARGB 0x4ACB70) and specular = PRSP + DrawObj+0x10
(PatPart_ComputeSpecularWithObjAdd 0x413BA0, skipped for PRAL 1 parts with no PRSP); PS_CharaPtDrawV:
out = tex*diffuse*palette[PPPA] + specular. Over a backdrop that is k*P + c*(1-Q) + Q*plate.
Prints mean |d| and % px > 24 on the editor footprint; the controls' floor is ~5.5 / 18%."""
import importlib.util, json, os, sys
import numpy as np
from PIL import Image
HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('sheet', os.path.join(HERE, 'sheet.py'))
sh = importlib.util.module_from_spec(spec); spec.loader.exec_module(sh)
D = os.path.abspath(os.path.join(HERE, '../../..'))


def arg(a, k, d=None):
    if k in a: i = a.index(k); v = a[i + 1]; del a[i:i + 2]; return v
    return d


def aura_add(meta):
    """The specular add colour of P1's .pat parts from the sidecar (CharaObj_DrawWithStageLight 0x5CE3C0): the chara
    aura colour (0..1 floats) as int(int(c*255) * strength) when the aura is active (type >= 0), 0 otherwise."""
    if meta.get('aura_type', -1) < 0: return (0, 0, 0)
    return tuple(int(int(c * 255.0) * meta['aura_strength']) for c in meta['aura_rgb'])


def aura_fit(game, pl, ek, ew, zoom, meta, org):
    """c per channel for game = P + c*(1-Q) + Q*plate (k = 1) on the footprint, L1-optimal (weighted median of
    (G - P - Q*plate) / (1-Q), weights 1-Q): the aura add of a capture whose sidecar predates the aura fields."""
    sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    G, B, P, Q, m = sh.game_vs_model.last_arrays
    c = []
    for ch in range(3):
        w = (1 - Q[..., ch])[m]; r = (G[..., ch] - P[..., ch] - Q[..., ch] * B[..., ch])[m]
        k = w > 0.05
        if k.sum() == 0: c.append(0.0); continue
        x = r[k] / w[k]; ww = w[k]; o = np.argsort(x); cw = np.cumsum(ww[o])
        c.append(float(x[o][np.searchsorted(cw, cw[-1] / 2)]))
    return c


def zoom_of(name):
    """Editor zoom of a case or control shot (cases.txt, cases_control.txt)."""
    for f in ('cases.txt', 'cases_control.txt'):
        for ln in open(os.path.join(HERE, f), encoding='utf-8'):
            if ln.startswith('#') or not ln.strip(): continue
            x = [v.strip() for v in ln.split('|')]
            if x[0] == name: return float(x[5])
    return 1.0


GRAD_H = 400.0  # g_fCharaBgStandardPixelHeight: the per-stage/per-chara table (uni2 1.42 0xC58C24) or 400 (Stage_BuildCharaBgColorParams 0x5C44E0)


def grad_fit(ctrl, ctrl_game, bw, gd):
    """The stage's chara colour gradient (uni2 1.42 TecCharaDrawBgColorSpecular_V pass, Shader/sh_chara.txt
    PS_CharaDrawBgColorSpecularV: specular += base + height * clamp(h / g_fCharaBgStandardPixelHeight, 0, 1), h = sprite
    px above the object origin; base/height = the stage's chara-colour curves sampled at the object's x,
    Stage_SampleCharaBgSpecular 0x518130). Calibrated on the same session's idle control (its own aura add taken
    out), then applied unchanged to the case: (base, height) per channel."""
    game = Image.open(os.path.join(gd, ctrl_game + '_game.png')).convert('RGB')
    pl = Image.open(os.path.join(gd, ctrl_game + '_game_plate.png')).convert('RGB')
    meta = json.load(open(os.path.join(gd, ctrl_game + '_game.json')))
    ek = Image.open(os.path.join(bw, ctrl + '_new_k.png')); ew = Image.open(os.path.join(bw, ctrl + '_new_w.png'))
    zoom = zoom_of(ctrl)
    keep = sh.LIGHT, sh.ROW_ADD; sh.LIGHT = None; sh.ROW_ADD = None
    sh.game_vs_model(game, pl, ek, ew, zoom, meta, sh.editor_origin(ek))
    sh.LIGHT, sh.ROW_ADD = keep
    G, B, P, Q, m = sh.game_vs_model.last_arrays; xa, ya, xb, yb, gorg = sh.game_vs_model.last_window
    t = np.clip((gorg[1] - np.repeat(np.arange(ya, yb)[:, None], xb - xa, 1))[m] / GRAD_H, 0, 1)
    aura = aura_add(meta) if 'aura_rgb' in meta else (0, 0, 0)
    out = []
    for c in range(3):
        w = (1 - Q[..., c])[m]; r = (G[..., c] - P[..., c] - Q[..., c] * B[..., c])[m] - w * aura[c]
        co, *_ = np.linalg.lstsq(np.stack([w, w * t], 1), r, rcond=None)
        out.append((float(co[0]), float(co[1])))
    return out


GLOW_W = (1.0, 0.4, 0.3, 0.2, 0.1)  # sh_blur.txt g_fTable


def _shift(a, d, axis):
    """a shifted by d texels along axis (clamp at the edges): the bilinear tap at uv + d texels."""
    i0 = int(np.floor(d)); f = d - i0
    def sh(k):
        k = int(k); n = a.shape[axis]
        idx = np.clip(np.arange(n) + k, 0, n - 1)
        return np.take(a, idx, axis=axis)
    return sh(i0) * (1 - f) + sh(i0 + 1) * f if f else sh(i0)


def aura_glow(sil, meta, mode='sq'):
    """The GRD Vorpal / chara aura glow (uni2 1.42 CharaAura_DrawGlow 0x59D910, active while aura_type >= 0; for the
    Vorpal win the script Battle_Std.GRDJudgeWinEffect_Init sets color 0xAA00AAFF -> colorB 0x0000AAFF, power 1,
    blendmode 0). sil = the glowing object's alpha in game px (H x W, 1280x720). The game draws it into a 640x360
    target, blurs it with CharaPointBlur_W then _H (CharaAura_BlurGlowPingPong 0x59D710, sh_blur.txt:
    a' = min(1, sum_k a(uv +- k * power texels) * {1, .4, .3, .2, .1})), and draws the target over the whole screen,
    linear, with the pulsed glow ARGB (aura+0x50) under the character. Returns (alpha H x W, rgb)."""
    H, W = sil.shape
    a = sil[:H - H % 2, :W - W % 2].reshape(H // 2, 2, W // 2, 2).mean((1, 3))
    if mode == 'sq': a = a * a  # every pass alpha-blends (1,1,1,a) onto a cleared target: dst.a = a * a
    pw = float(meta.get('aura_power', 1.0))
    for axis in (1, 0):
        acc = a * GLOW_W[0]
        for k in range(1, 5):
            acc = acc + (_shift(a, k * pw, axis) + _shift(a, -k * pw, axis)) * GLOW_W[k]
        a = np.clip(acc, 0, 1)
        if mode in ('sq', 'sqblur'): a = a * a
    up = np.asarray(Image.fromarray(a.astype(np.float32), ).resize((W, H), Image.BILINEAR))
    argb = int(meta['aura_glow_argb']) & 0xFFFFFFFF
    ga = ((argb >> 24) & 255) / 255.0
    rgb = np.array([(argb >> 16) & 255, (argb >> 8) & 255, argb & 255], dtype=np.float64)
    return np.clip(up, 0, 1) * ga, rgb


def main():
    a = sys.argv[1:]
    bw = arg(a, '--bw', os.path.join(D, 'docs/audit_2026_10_14/evidence/compare/v2/editor_bw'))
    gd = arg(a, '--game', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/v2')
    plate = arg(a, '--plate')
    lj = arg(a, '--light-json', '/mnt/c/dev/hantei-chan/tools/gamecap/captures/s1_compare/light_stage1.json')
    ls = arg(a, '--light')
    out = arg(a, '--out')
    gname = arg(a, '--game-name')
    zarg = arg(a, '--zoom')
    fitl = '--fit' in a
    if fitl: a.remove('--fit')
    afit = '--aura-fit' in a
    if afit: a.remove('--aura-fit')
    sh.BAND = int(arg(a, '--band', '1'))       # +-1 px tolerance band residual, printed next to the plain one
    sh.REFINE = int(arg(a, '--refine', '10'))  # 0 = trust the camera geometry (thin parts: shifts are ambiguous)
    sh.AXIS_FIX = '--no-axis-fix' not in a
    if '--no-axis-fix' in a: a.remove('--no-axis-fix')
    jout = arg(a, '--json')
    gctrl = arg(a, '--grad-ctrl')  # CTRLCASE,CTRLGAME: calibrate the stage chara gradient on that control
    gmode = arg(a, '--glow')  # 'lin' / 'sq': model the aura (GRD Vorpal) glow pass from the sidecar (aura_glow_argb)
    case = a[0]
    zoom = float(zarg) if zarg else zoom_of(case)
    gn = gname or case
    game = Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')
    meta = json.load(open(os.path.join(gd, gn + '_game.json')))
    plate = plate or os.path.join(gd, gn + '_game_plate.png')
    ek = Image.open(os.path.join(bw, case + '_new_k.png')); ew = Image.open(os.path.join(bw, case + '_new_w.png'))
    org = sh.editor_origin(ek)
    if ls: light = [tuple(map(float, x.split(','))) for x in ls.split(';')]
    elif 'aura_rgb' in meta: light = [(1.0, float(c)) for c in aura_add(meta)]
    else: light = json.load(open(lj))
    pl = Image.open(plate).convert('RGB')
    grad = None
    if gctrl:
        cc, cg = gctrl.split(',')
        grad = grad_fit(cc, cg, bw, gd)
        sh.ROW_ADD = lambda c, rows, gorg: grad[c][0] + grad[c][1] * np.clip((gorg[1] - rows) / GRAD_H, 0, 1)
        print('stage chara gradient (base, height) per RGB from %s:' % cg, [tuple(round(v, 1) for v in g) for g in grad])
    sh.LIGHT = None
    r = sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    if r is None: print(case, 'no footprint'); return
    ring = None
    if gmode and meta.get('aura_type', -1) >= 0 and 'aura_glow_argb' in meta:
        # the glowing object's alpha (1 - Q) placed in the full game frame, then the glow under it in the plate
        _, _, _, Qs0, _ = sh.game_vs_model.last_arrays; xa, ya, xb, yb, _g = sh.game_vs_model.last_window
        Gf = np.asarray(game).astype(np.float64); Bf = np.asarray(pl).astype(np.float64)
        sil = np.zeros(Gf.shape[:2]); sil[ya:yb, xa:xb] = np.clip(1 - Qs0.mean(2), 0, 1)
        ga, rgb = aura_glow(sil, meta, gmode)
        Bg = Bf * (1 - ga[..., None]) + rgb[None, None, :] * ga[..., None]
        pl = Image.fromarray(np.clip(np.round(Bg), 0, 255).astype(np.uint8))
        r = sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
        # ring = where the glow is (> 2/255 of colour) outside the silhouette: plate vs plate+glow against the game
        core = sil >= 0.02; dil = core.copy()
        for _ in range(12):  # 12 game px around the silhouette (the blur reaches 4 texels = 8 px, plus the upsample)
            dil = dil | np.roll(dil, 1, 0) | np.roll(dil, -1, 0) | np.roll(dil, 1, 1) | np.roll(dil, -1, 1)
        rm = dil & ~core
        if rm.any():
            ring = (np.abs(Gf - Bf)[rm].mean(), (np.abs(Gf - Bf).sum(2)[rm] > 24).mean() * 100,
                    np.abs(Gf - Bg)[rm].mean(), (np.abs(Gf - Bg).sum(2)[rm] > 24).mean() * 100, int(rm.sum()))
            print('aura glow (%s, ARGB %08x, power %.2f): ring %d px  no glow %.2f / %.1f%%  glow model %.2f / %.1f%%'
                  % (gmode, int(meta['aura_glow_argb']) & 0xFFFFFFFF, meta.get('aura_power', 1), ring[4], ring[0], ring[1], ring[2], ring[3]))
    fit = sh.game_vs_model.last_fit
    if afit:  # the game's own .pat part light with an unrecorded aura: k = 1, c = the aura add (>= 0), least squares
        sh.LIGHT = [(1.0, 0.0)] * 3
        sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
        light = [(1.0, max(0.0, f)) for f in aura_fit(game, pl, ek, ew, zoom, meta, org)]
    if fitl: light = fit  # free least-squares (k, c): the floor for CG sprites, whose shader adds its own stage colour
    sh.LIGHT = light
    r = sh.game_vs_model(game, pl, ek, ew, zoom, meta, org)
    _, lmean, lch, lpred, ldiff = sh.game_vs_model.last_light
    print('light (k, c) per RGB:', light)
    band = sh.game_vs_model.last_band
    print('%s: raw %.2f / %.1f%%  lit %.2f / %.1f%%  band+-%d %s  (%d px, refine %+d,%+d)  free fit k,c = %s'
          % (case, r[3], r[4], lmean, lch, sh.BAND, '%.2f / %.1f%%' % band if band else '-', r[6], r[5][0], r[5][1],
             ', '.join('(%.2f,%.0f)' % f for f in fit)))
    if jout:
        json.dump({'case': case, 'game': gn, 'plate': plate, 'light': light, 'raw': [r[3], r[4]], 'lit': [lmean, lch],
                   'band': band, 'band_px': sh.BAND, 'grad': grad, 'glow': gmode, 'glow_ring': ring, 'px': r[6], 'refine': list(r[5])}, open(jout, 'w'), indent=1)
    if out:
        g = r[0]; w = g.width
        s = Image.new('RGB', (w * 3 + 16, g.height), (30, 30, 30))
        s.paste(g, (0, 0)); s.paste(lpred.resize(g.size), (w + 8, 0)); s.paste(ldiff.resize(g.size), (2 * w + 16, 0))
        s.save(out)


if __name__ == '__main__':
    main()
