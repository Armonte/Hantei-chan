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


GREY = None  # --grey: the backdrop level of the <name>_new_g.png shots (shoot_batch.sh: 64)


def load_w(bw, name):
    """The white-backdrop term of E = P + Q * backdrop. With --grey it is extrapolated from the 64-grey shot,
    W = K + (E_g - K) * 255 / 64, as a float array: an additive part clips on white (K + 255 > 255), which made
    Q < 1 and under-predicted every additive pixel over a bright backdrop (pras_chr020_p5f2's .pat layer)."""
    if GREY:
        K = np.asarray(Image.open(os.path.join(bw, name + '_new_k.png')).convert('RGB')).astype(np.float64)
        Eg = np.asarray(Image.open(os.path.join(bw, name + '_new_g.png')).convert('RGB')).astype(np.float64)
        return K + (Eg - K) * 255.0 / GREY
    return Image.open(os.path.join(bw, name + '_new_w.png'))


def zoom_of(name):
    """Editor zoom of a case or control shot (cases.txt, cases_control.txt)."""
    for f in ('cases.txt', 'cases_control.txt'):
        for ln in open(os.path.join(HERE, f), encoding='utf-8'):
            if ln.startswith('#') or not ln.strip(): continue
            x = [v.strip() for v in ln.split('|')]
            if x[0] == name: return float(x[5])
    return 1.0


GRAD_H = 400.0  # g_fCharaBgStandardPixelHeight: the per-stage/per-chara table (uni2 1.42 0xC58C24) or 400 (Stage_BuildCharaBgColorParams 0x5C44E0)


def grad_fit(ctrl, ctrl_game, bw, gd, plate_fallback=None):
    """The stage's chara colour gradient (uni2 1.42 TecCharaDrawBgColorSpecular_V pass, Shader/sh_chara.txt
    PS_CharaDrawBgColorSpecularV: specular += base + height * clamp(h / g_fCharaBgStandardPixelHeight, 0, 1), h = sprite
    px above the object origin; base/height = the stage's chara-colour curves sampled at the object's x,
    Stage_SampleCharaBgSpecular 0x518130). Calibrated on the same session's idle control (its own aura add taken
    out), then applied unchanged to the case: (base, height) per channel."""
    game = Image.open(os.path.join(gd, ctrl_game + '_game.png')).convert('RGB')
    pp = os.path.join(gd, ctrl_game + '_game_plate.png')
    pl = Image.open(pp if os.path.exists(pp) else plate_fallback).convert('RGB')  # chr019/chr027 runs: one shared plate
    meta = json.load(open(os.path.join(gd, ctrl_game + '_game.json')))
    ek = Image.open(os.path.join(bw, ctrl + '_new_k.png')); ew = load_w(bw, ctrl)
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


def layer_weight(bw, cg, pat, full, org):
    """Gradient weight of a frame that mixes CG layers (stage chara gradient, CgLayer_DrawWithCharaShader) and .pat
    layers (Han6Draw pat path, no gradient), from per-layer editor shots (shoot_batch.sh --layer passes). Each layer
    is E_L = P_L + Q_L * backdrop; drawn in order, the frame is E = P_top + Q_top * (P_bot + Q_bot * backdrop). The CG
    output gets + g * (1 - Q_cg) before it is blended, so the weight of g in the frame is (1 - Q_cg) when the CG
    layer is on top and Q_pat * (1 - Q_cg) when the .pat layer is drawn over it. The order is the one whose
    composite reproduces the editor's own all-layer shot. Returns (weight H x W x 3, order, composite error)."""
    def pq(n):
        K = np.asarray(Image.open(os.path.join(bw, n + '_new_k.png')).convert('RGB')).astype(np.float64)
        w = load_w(bw, n)
        Wh = w if isinstance(w, np.ndarray) else np.asarray(w.convert('RGB')).astype(np.float64)
        return K, (Wh - K) / 255.0
    Pc, Qc = pq(cg); Pp, Qp = pq(pat); Pf, Qf = pq(full)
    ox, oy = int(round(org[0])), int(round(org[1]))
    # the editor's additive axis lines are in every shot: compare off the axes only
    off = np.ones(Pf.shape[:2], bool); off[:, ox] = False; off[oy, :] = False
    e_cg_top = np.abs(Pc + Qc * Pp - Pf)[off].mean()
    e_pat_top = np.abs(Pp + Qp * Pc - Pf)[off].mean()
    w = 1 - Qc
    if e_pat_top < e_cg_top: w = Qp * w
    w[:, ox] = 0; w[oy, :] = 0  # axis pixels: not CG coverage
    return np.clip(w, 0, 1), ('pat over CG' if e_pat_top < e_cg_top else 'CG over pat'), min(e_cg_top, e_pat_top)


def mbtl_light(stage, std_h, xpos):
    """MBTL CG character shading (IDA MBTL 25646393, renamed and saved): Han6Draw_DrawFrameLayers_2to0 0x4A0750 draws
    each CG layer three times through Han6Draw_SubmitCgSprite_ParentMtx 0x4A0C40 (Shader/sh_chara.txt):
      TecCharaDrawBgColor_V      normal blend:  pal * vcol * (base + height * t)
      ...Specular_V              additive:      spec_base + spec_height * t
      ...Bokashi_V               additive:      bokashi_alpha * mean_9x9(pow(pal * vcol, 4))   (g_Power 4, g_Blightness 1)
    t = clamp(px above the feet / StdH, 0, 1), StdH from bg/bg_chara_height.csv (chr001: 330). base / height = the
    bottom row and top - bottom of bg/<stage>/stage_color.img at the object's strip column 2048 + (x >> 7)
    (Stage_SampleCharaBgStrip 0x4EFD50), likewise stage_specular.img; bokashi alpha = stage_bokashi_alpha.img byte 0.
    This replaces the fitted k > 1: the brightening is the bokashi pow(tex, 4) add (MBTL-IM BloomStrength only
    drives the separate screen bloom, Stage_ScreenBloomPass 0x4EEDA0)."""
    def strip(name):
        b = open('/mnt/c/dev/hantei-chan/gamedata/mbtl/bg/%s/%s' % (stage, name), 'rb').read()
        w = int.from_bytes(b[12:16], 'little'); h = int.from_bytes(b[16:20], 'little')
        return np.frombuffer(b[20:20 + w * h * 4], np.uint8).reshape(h, w, 4).astype(np.float64)
    xi = 2048 + (int(xpos) >> 7)
    col = strip('stage_color.img')[:, xi]; spc = strip('stage_specular.img')[:, xi]
    bk = strip('stage_bokashi_alpha.img')[0, xi, 0] / 255.0
    from numpy.lib.stride_tricks import sliding_window_view
    def boxmean(a, r=4): return sliding_window_view(np.pad(a, r), (2 * r + 1, 2 * r + 1)).mean((-1, -2))
    def fn(P, Q, B, rows, gorg):
        t = np.clip((gorg[1] - rows) / std_h, 0, 1)
        op = 1 - Q.mean(2); opm = op > 0.5; cnt = boxmean(opm.astype(np.float64))
        out = np.empty_like(P)
        for c in range(3):
            C = col[1, c] / 255 + (col[0, c] - col[1, c]) / 255 * t; S = spc[1, c] + (spc[0, c] - spc[1, c]) * t
            e = P[..., c] / np.maximum(op, 1e-3) / 255
            p4 = boxmean(np.where(opm, np.clip(e, 0, 1) ** 4, 0)) / np.maximum(cnt, 1e-6)
            out[..., c] = P[..., c] * C + op * S + Q[..., c] * B[..., c] + bk * p4 * 255 * np.where(opm, 1, cnt)
        return out
    return fn


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
    global GREY
    if '--grey' in a: a.remove('--grey'); GREY = 64.0
    gctrl = arg(a, '--grad-ctrl')  # CTRLCASE,CTRLGAME: calibrate the stage chara gradient on that control
    layers = arg(a, '--layers')  # CGSHOT,PATSHOT: per-layer editor shots; the gradient goes on the CG layers only
    ml = arg(a, '--mbtl-light')  # STAGE,STDH: the MBTL BgColor three-pass shading (mbtl_light)
    gmode = arg(a, '--glow')  # 'lin' / 'sq': model the aura (GRD Vorpal) glow pass from the sidecar (aura_glow_argb)
    case = a[0]
    zoom = float(zarg) if zarg else zoom_of(case)
    gn = gname or case
    game = Image.open(os.path.join(gd, gn + '_game.png')).convert('RGB')
    meta = json.load(open(os.path.join(gd, gn + '_game.json')))
    plate = plate or os.path.join(gd, gn + '_game_plate.png')
    ek = Image.open(os.path.join(bw, case + '_new_k.png')); ew = load_w(bw, case)
    org = sh.editor_origin(ek)
    if ls: light = [tuple(map(float, x.split(','))) for x in ls.split(';')]
    elif 'aura_rgb' in meta: light = [(1.0, float(c)) for c in aura_add(meta)]
    else: light = json.load(open(lj))
    pl = Image.open(plate).convert('RGB')
    grad = None
    if gctrl:
        cc, cg = gctrl.split(',')
        grad = grad_fit(cc, cg, bw, gd, plate)
        sh.ROW_ADD = lambda c, rows, gorg: grad[c][0] + grad[c][1] * np.clip((gorg[1] - rows) / GRAD_H, 0, 1)
        print('stage chara gradient (base, height) per RGB from %s:' % cg, [tuple(round(v, 1) for v in g) for g in grad])
        if layers:
            lc, lp = layers.split(',')
            sh.ROW_ADD_W, order, err = layer_weight(bw, lc, lp, case, org)
            print('per-layer gradient: CG %s, .pat %s, %s (layer composite vs all-layer shot: mean |d| %.3f)' % (lc, lp, order, err))
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
    if ml:
        st, sth = ml.split(','); sh.LIGHT_FN = mbtl_light(st, float(sth), meta.get('p1_x', 0))
        sh.LIGHT = [(1.0, 0.0)] * 3; light = 'mbtl %s StdH %s' % (st, sth)
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
