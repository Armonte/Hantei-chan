#!/usr/bin/env python3
"""Contact sheets for the S1 render comparison.

  sheet.py <compare dir>

For every case in cases.txt: <name>_old.png | <name>_new.png | (|old-new| x4) from <dir>/editor, plus
<dir>/game/<name>_game.png when present (tools/gamecap writes those). The game shot is a whole game frame
(stage behind the character, unknown camera scale), so it is registered to the new editor shot first:
a scale x offset x mirror search maximising normalised cross-correlation of edge maps (FFT), then the game
region under the editor crop is resampled to the editor crop and |game - new| is measured only on the
editor's content pixels (where the editor shows sprite, not its clear colour/grid).
Writes <dir>/sheets/<name>.png and prints the mean / changed-pixel stats per pair; with a game shot it
also writes <dir>/sheets/game_vs_new.tsv."""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageOps

HERE = os.path.dirname(os.path.abspath(__file__))


def cases():
    for ln in open(os.path.join(HERE, 'cases.txt'), encoding='utf-8'):
        if ln.startswith('#') or not ln.strip(): continue
        f = [x.strip() for x in ln.split('|')]
        yield f[0], f[-1], float(f[5])


def crop_box(imgs, pad=24):
    """Common box around everything that differs from the corner (clear) colour in any image."""
    box = None
    for im in imgs:
        a = np.asarray(im.convert('RGB')).astype(int)
        bg = a[2, 2]
        m = np.abs(a - bg).sum(2) > 24
        # drop the editor's full-length grid lines
        m[m.mean(1) > 0.8, :] = False
        m[:, m.mean(0) > 0.8] = False
        ys, xs = np.nonzero(m)
        if len(xs) == 0: continue
        b = (xs.min(), ys.min(), xs.max(), ys.max())
        box = b if box is None else (min(box[0], b[0]), min(box[1], b[1]), max(box[2], b[2]), max(box[3], b[3]))
    if box is None: return None
    w, h = imgs[0].size
    return (max(0, box[0] - pad), max(0, box[1] - pad), min(w, box[2] + pad), min(h, box[3] + pad))


def diff(a, b):
    x = np.asarray(a.convert('RGB')).astype(int); y = np.asarray(b.convert('RGB')).astype(int)
    d = np.abs(x - y)
    changed = (d.sum(2) > 24).mean() * 100.0
    return Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)), d.mean(), changed


EDITOR_CLEAR = np.array([83, 104, 47])  # the editor's clear colour in --capture-view shots
EDITOR_ORIGIN = (632.0, 580.0)          # where shoot.sh's 1264x861 capture puts the world origin


def content_mask(im):
    """Pixels that differ from the editor clear colour, editor grid lines removed."""
    a = np.asarray(im.convert('RGB')).astype(int)
    m = np.abs(a - EDITOR_CLEAR).sum(2) > 24
    m[m.mean(1) > 0.8, :] = False
    m[:, m.mean(0) > 0.8] = False
    return m


def _corr(img, k, fs):
    """Valid-mode cross-correlation of 2-D k over img via FFT."""
    H, W = img.shape; h, w = k.shape
    return np.fft.irfft2(np.fft.rfft2(img, fs) * np.conj(np.fft.rfft2(k, fs)), fs)[:H - h + 1, :W - w + 1]


def masked_ncc(img, tpl, mask):
    """Zero-mean NCC of an RGB template over an RGB image, counting only template pixels where mask is set
    (so the stage behind the game sprite and the editor clear colour do not take part). Returns a map."""
    H, W, _ = img.shape; h, w, _ = tpl.shape
    if h > H or w > W or mask.sum() < 16: return None
    fs = (H, W); M = mask.astype(np.float64); n = M.sum()
    num = 0.0; ivar = 0.0; tvar = 0.0
    sM = None
    for c in range(3):
        I = img[:, :, c]; T = tpl[:, :, c]
        tz = (T - (T * M).sum() / n) * M
        tvar += (tz * tz).sum()
        num = num + _corr(I, tz, fs)
        s1 = _corr(I, M, fs); s2 = _corr(I * I, M, fs)
        ivar = ivar + np.maximum(s2 - s1 * s1 / n, 0)
    return num / np.sqrt(np.maximum(ivar * tvar, 1e-9))


def register(game, new_c, scales=np.arange(0.40, 1.61, 0.02)):
    """Find scale (game px per editor px), mirror and top-left offset of new_c inside game."""
    G = np.asarray(game.convert('RGB')).astype(np.float64) / 255.0
    D = 4
    G2 = G[::D, ::D]
    mask0 = Image.fromarray((content_mask(new_c) * 255).astype(np.uint8))
    best = (-2, None)
    for mirror in (False, True):
        src = ImageOps.mirror(new_c) if mirror else new_c
        msk = ImageOps.mirror(mask0) if mirror else mask0
        for sc in scales:
            w, h = max(8, int(round(src.width * sc / D))), max(8, int(round(src.height * sc / D)))
            t = np.asarray(src.resize((w, h), Image.BILINEAR)).astype(np.float64) / 255.0
            m = np.asarray(msk.resize((w, h), Image.NEAREST)) > 127
            r = masked_ncc(G2, t, m)
            if r is None: continue
            i = np.argmax(r); y, x = divmod(i, r.shape[1])
            if r[y, x] > best[0]: best = (r[y, x], (sc, mirror, x * D, y * D))
    if best[1] is None: return None
    sc0, mirror, x0, y0 = best[1]
    src = ImageOps.mirror(new_c) if mirror else new_c
    msk = ImageOps.mirror(mask0) if mirror else mask0
    refine = (-2, None)
    for sc in np.arange(sc0 - 0.03, sc0 + 0.0301, 0.0025):
        w, h = int(round(src.width * sc)), int(round(src.height * sc))
        t = np.asarray(src.resize((w, h), Image.BILINEAR)).astype(np.float64) / 255.0
        m = np.asarray(msk.resize((w, h), Image.NEAREST)) > 127
        ys, xs = max(0, y0 - 12), max(0, x0 - 12)
        sub = G[ys:ys + h + 24, xs:xs + w + 24]
        r = masked_ncc(sub, t, m)
        if r is None: continue
        i = np.argmax(r); y, x = divmod(i, r.shape[1])
        if r[y, x] > refine[0]: refine = (r[y, x], (sc, mirror, xs + x, ys + y, w, h))
    return refine if refine[1] else None


# Where the world origin of P1 lands in the game shot. gamecap leaves training at its start positions
# (P1 x = -320 px, P2 x = +320 px, so the camera is centred on 0 at zoom 1), which puts P1's origin at
# (319, 640) of the 1280x720 frame (measured on the zoom-1 chr020 cases, which register by image to 0.3 px).
GAME_ORIGIN = (319.0, 640.0)


def game_transform(meta):
    """P1 origin on screen and screen px per sprite px (x, y) from the gamecap sidecar: the game's own
    world->screen path (pixel = world * ScaleCommon; (x, y) = pixel * (ScaleX, ScaleY); screen = M (x, y, 0, 1)
    with M row-major and divided by w), the same transform its hitbox overlay uses."""
    if 'cam_f32' in meta:
        # MBTL: screen = 640 + (world - cam) * zoom / 128 on the 1280x720 reference (camera element 1, the one
        # GetCameraPosition uses by default: +8 x, +0xC y, +0x14 zoom in a 0x18-byte element)
        e = 6  # element 1 starts at dword 6
        camx, camy = meta['cam_i32'][e + 2], meta['cam_i32'][e + 3]
        zf, zi = meta['cam_f32'][e + 5], meta['cam_i32'][e + 5]
        zoom = zf if 0.05 < abs(zf) < 20 else (zi / 1000.0 if zi else 1.0)
        wx = meta['p1_x'] + meta.get('p1_offx', 0); wy = meta['p1_y'] + meta.get('p1_offy', 0)
        if MBTL_SNAP_ORIGIN:  # render-match17: Han6Object_Draw floors the object position to whole sprite px (sar 7)
            wx, wy = (int(wx) >> 7) * 128, (int(wy) >> 7) * 128
        return (640 + (wx - camx) * zoom / 128.0, 640 + (wy - camy) * zoom / 128.0), (zoom, zoom)
    m = meta['screen_matrix']
    px = (meta['p1_x'] + meta.get('p1_offx', 0)) * meta['scale_common']
    py = (meta['p1_y'] + meta.get('p1_offy', 0)) * meta['scale_common']
    x, y = px * meta['scale_x'], py * meta['scale_y']
    w = x * m[3] + y * m[7] + m[15]
    sx = (x * m[0] + y * m[4] + m[12]) / w
    sy = (x * m[1] + y * m[5] + m[13]) / w
    return (sx, sy), (meta['scale_x'] * m[0] / w, meta['scale_y'] * m[5] / w)


# The shoot scripts fix the editor window, zoom and pan, so the world origin of a shot depends only on its size
# (measured from the axes on every shot that shows them). A view filled by a full-screen part hides the axes, and the
# edge of such a part was being taken for an axis (pups_chr027 -> (841, 0), mbtl_pral2_chr004 -> (0, 143)).
# render-match2 round 3: the editor's world view follows the D3D9 pixel-centre rule (+0.5 px), so the origin is
# the pixel whose centre is the origin coordinate; the vertical axis moved one column right (631 -> 632 before acca786).
KNOWN_ORIGINS = {(1264, 861): (632.0, 580.0),    # shoot.sh / shoot_bw*.sh: 1280x900 window
                 (1684, 1061): (842.0, 680.0)}   # shoot_v2.sh: 1700x1100 window, zoom 1


def editor_origin(im):
    """The editor draws the world axes as full-length grid lines through the origin."""
    if im.size in KNOWN_ORIGINS: return KNOWN_ORIGINS[im.size]
    a = np.asarray(im.convert('RGB')).astype(int)
    m = np.abs(a - a[2, 2]).sum(2) > 24
    cols = np.where(m.mean(0) > 0.8)[0]; rows = np.where(m.mean(1) > 0.8)[0]
    if len(cols) == 0 or len(rows) == 0: return EDITOR_ORIGIN  # axes hidden (e.g. a full-screen part)
    return float(cols[0]), float(rows[0])


def game_vs_new(game, new_full, box, zoom=1.0, meta=None):
    """Map the editor crop into the game frame by geometry (origin + 1/zoom scale), refine by masked NCC within
    +-6 game px (image matching alone is unreliable: translucent/blended parts composite over the stage
    differently, which is exactly what is being measured). Returns the aligned game crop, diff, stats."""
    org = editor_origin(new_full)
    new_c = new_full.crop(box)
    if org is None: return None
    gorg, gsc = (GAME_ORIGIN, (1.0, 1.0)) if meta is None else game_transform(meta)
    sc = abs(gsc[0]) / zoom
    x0 = gorg[0] + (box[0] - org[0]) * sc
    y0 = gorg[1] + (box[1] - org[1]) * sc
    w, h = int(round(new_c.width * sc)), int(round(new_c.height * sc))
    mask0 = Image.fromarray((content_mask(new_c) * 255).astype(np.uint8))
    t = np.asarray(new_c.resize((w, h), Image.BILINEAR)).astype(np.float64) / 255.0
    m = np.asarray(mask0.resize((w, h), Image.NEAREST)) > 127
    G = np.asarray(game.convert('RGB')).astype(np.float64) / 255.0
    R = 6
    xs, ys = int(round(x0)) - R, int(round(y0)) - R
    # pad the game frame so crops that run off its edge still work
    P = max(w, h) + 2 * R
    Gp = np.pad(G, ((P, P), (P, P), (0, 0)))
    sub = Gp[ys + P:ys + P + h + 2 * R, xs + P:xs + P + w + 2 * R]
    r = masked_ncc(sub, t, m)
    dx = dy = 0; score = float('nan')
    if r is not None:
        i = np.nanargmax(r); yy, xx = divmod(i, r.shape[1])
        score = float(r[yy, xx]); dx, dy = xx - R, yy - R
    gx, gy = int(round(x0)) + dx, int(round(y0)) + dy
    gpad = Image.fromarray((Gp * 255).astype(np.uint8))
    g = gpad.crop((gx + P, gy + P, gx + P + w, gy + P + h)).resize(new_c.size, Image.BILINEAR)
    mask = content_mask(new_c)
    inside = np.zeros_like(mask)  # editor pixels that fall inside the real game frame
    yy_, xx_ = np.mgrid[0:new_c.height, 0:new_c.width]
    gxx = gx + xx_ * w / new_c.width; gyy = gy + yy_ * h / new_c.height
    inside = (gxx >= 0) & (gxx < game.width) & (gyy >= 0) & (gyy < game.height)
    mask = mask & inside
    a = np.asarray(g.convert('RGB')).astype(int); b = np.asarray(new_c.convert('RGB')).astype(int)
    d = np.abs(a - b) * mask[:, :, None]
    n = max(1, mask.sum())
    mean = d.sum() / (n * 3)
    changed = ((d.sum(2) > 24) & mask).sum() * 100.0 / n
    return g, Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)), mean, changed, score, sc, (dx, dy)


def grid_mask(K, Wh):
    """Editor grid lines: opaque (identical on the black and the white backdrop) and running across the view."""
    opaque = (np.abs(K - Wh).sum(2) == 0)
    m = np.zeros(opaque.shape, bool)
    m[:, opaque.mean(0) > 0.9] = True
    m[opaque.mean(1) > 0.9, :] = True
    return m


AXIS_FIX = False  # v2_model sets it: correct the editor's additive axis lines instead of dropping them
# render-match17: MBTL Han6Object_Draw 0x597070 (MBTL IDB; UNI2 CharaObj_DrawLayersWithLight is the same code) takes the
# draw origin as (obj x >> 7, obj y >> 7): world units floored to whole sprite px before the camera / zoom matrix.
# mbtl_patctrl_chr001_p238f10 (p1_y -500 -> -4 px, origin y 636 at zoom 1, not 636.094) is 0.71 with the unfloored
# origin (a 0.094 px bilinear blur, 4.66 on the opaque tray edges) and 0.26 floored (tray 0.01). Rows with p1_y 0 are
# unchanged.
MBTL_SNAP_ORIGIN = True
EXACT_REG = False  # v2_model --exact-reg (render-match10): registration independent of the footprint bbox
SUBPIX = (0.0, 0.0)  # render-match11: (dx, dy) game px added to the EXACT_REG registration (prediction moves by +d);
                     # fitted on a control (v2_model --subpix-fit) and applied unchanged to that control's cases
REFINE = 10       # +- px of the alignment search (0 = trust the camera geometry)
BAND = 0          # +- px tolerance band for the extra band residual
ROW_ADD = None    # f(channel, game rows (n,1), P1 origin) -> add per row: the stage's chara gradient
POST_ADD_SRC = None  # (P_cg, Q_cg, visibility) editor-space arrays: v2_model --layers + --bokashi (render-match10)
POST_ADD = None   # f(P, Q) -> (H, W, 3) add on top of the lit prediction (v2_model --bokashi: UNI2 chara Bokashi pass)
LIGHT_FN = None   # f(P, Q, plate, game rows, P1 origin) -> prediction: replaces the (k, c) light (v2_model --mbtl-light)
ROW_ADD_W = None  # editor-space (H, W, 3) weight of ROW_ADD per pixel (default 1 - Q: the whole sprite). Mixed CG +
                  # .pat frames: only the CG layers get the gradient (v2_model.py --layers), see layer_weight there
LIGHT = None  # [(k, c)] * 3, set from --light (fit on the idle controls of the pinned stage)


def fit_light(game, plate, ek, ew, zoom, meta, org):
    """Least-squares stage light (k, c) per channel on an idle character sprite (all parts lit, normal blend)."""
    global LIGHT
    keep = LIGHT; LIGHT = None
    r = game_vs_model(game, plate, ek, ew, zoom, meta, org)
    LIGHT = keep
    return game_vs_model.last_fit


EXCLUDE = []  # v2_model --exclude x0,y0,x1,y1[;...]: game-px boxes left out of the footprint


def game_vs_model(game, plate, ek, ew, zoom, meta, org):
    """Honest game-vs-editor comparison for blended parts.
    The editor view on a black (ek) and a white (ew) backdrop gives, per pixel and channel, E = P + Q*backdrop
    with P = ek, Q = (ew - ek)/255 (normal alpha, additive, subtractive and multiply are all of this form).
    The gamecap plate is the same game frame with the fighters hidden (UNI2-IM harness), i.e. the backdrop the
    game blended onto, so the game pixel should be P + Q*plate. Everything is mapped into game pixels by the
    recorded camera transform; the region is the editor's sprite footprint (Q != 1 or P != 0)."""
    K = np.asarray(ek.convert('RGB')).astype(np.float64)
    # ew: an image, or an (H, W, 3) float array = the white-backdrop shot extrapolated from a grey one (v2_model --grey)
    Wh = ew.astype(np.float64) if isinstance(ew, np.ndarray) else np.asarray(ew.convert('RGB')).astype(np.float64)
    grid = grid_mask(K, np.clip(Wh, 0, 255))
    P = K.copy(); Q = (Wh - K) / 255.0
    ox, oy = int(round(org[0])), int(round(org[1]))
    if AXIS_FIX:
        # The editor's world axes are 1-px additive grey lines drawn under the sprite (K = A, W = 255 where nothing
        # else is drawn). Where a sprite crosses them (thin parts on the origin: rect_chr024_p76's line runs down the
        # y axis), P carries Q*A of axis: take it out and keep those pixels in the footprint instead of dropping the
        # whole column/row (the old behaviour, which removed the line's core).
        for horiz in (False, True):
            for o in ((oy,) if horiz else (ox,)):
                if not (0 <= o < (K.shape[0] if horiz else K.shape[1])): continue
                line_k = K[o, :] if horiz else K[:, o]; line_w = Wh[o, :] if horiz else Wh[:, o]
                bare = (np.abs(line_w - 255).max(1) == 0) & (np.ptp(line_k, axis=1) == 0) & (line_k[:, 0] > 0)
                if bare.mean() < 0.5: continue  # not an axis line in this shot
                A = float(np.median(line_k[bare, 0]))
                if horiz: P[o, :] -= (Q[o, :] * A); grid[o, :] = False
                else: P[:, o] -= (Q[:, o] * A); grid[:, o] = False
        P = np.clip(P, 0, 255)
    foot = ((np.abs(Q - 1).max(2) > 0.02) | (P.max(2) > 4)) & ~grid
    # translucent grid lines (not identical on both backdrops) still run across the whole view
    foot[:, foot.mean(0) > 0.9] = False
    foot[foot.mean(1) > 0.9, :] = False
    if not AXIS_FIX:  # and the 1-px axis lines through the origin
        foot[:, max(0, ox - 1):ox + 2] &= ~(foot[:, max(0, ox - 1):ox + 2].mean(0) > 0.5)[None, :]
        foot[max(0, oy - 1):oy + 2, :] &= ~(foot[max(0, oy - 1):oy + 2, :].mean(1) > 0.5)[:, None]
    ys, xs = np.nonzero(foot)
    if len(xs) == 0: return None
    pad = 8
    x0e, x1e = max(0, xs.min() - pad), min(K.shape[1], xs.max() + pad + 1)
    y0e, y1e = max(0, ys.min() - pad), min(K.shape[0], ys.max() + pad + 1)
    gorg, gsc = (GAME_ORIGIN, (1.0, 1.0)) if meta is None else game_transform(meta)
    sc = abs(gsc[0]) / zoom
    # game-pixel window covering the editor crop
    gx0 = gorg[0] + (x0e - org[0]) * sc; gy0 = gorg[1] + (y0e - org[1]) * sc
    gw = int(round((x1e - x0e) * sc)); gh = int(round((y1e - y0e) * sc))
    # EXACT_REG (render-match10): resample from the exact editor-space box of the integer game window, so the
    # sub-pixel registration follows the camera geometry only. The default maps the footprint's integer crop onto a
    # rounded game size, so it shifts by up to +-0.5 game px (and rescales slightly) whenever the footprint bbox
    # changes; thin-line parts (mbtl_flipV_chr001_p157f2 arcs 4/5/6) swing 3.5 -> 6.6 on that alone.
    if EXACT_REG:
        GX0, GY0 = int(round(gx0)), int(round(gy0))
        sx, sy = SUBPIX  # the box is sampled at -d so the prediction lands at +d game px
        ebox = (org[0] + (GX0 - sx - gorg[0]) / sc, org[1] + (GY0 - sy - gorg[1]) / sc,
                org[0] + (GX0 - sx + gw - gorg[0]) / sc, org[1] + (GY0 - sy + gh - gorg[1]) / sc)
        gx0, gy0 = GX0, GY0
        EP = 16  # the box may run past the shot's edge: pad (P 0, Q 1 = nothing drawn there)
        ebox = tuple(v + EP for v in ebox)
    def to_game(arr, fill=0.0):  # resample an editor-space float array (crop) to game pixels (area-ish, bilinear)
        out = []
        for c in range(arr.shape[2]):
            if EXACT_REG:
                im = Image.fromarray(np.pad(arr[..., c].astype(np.float32), EP, constant_values=fill), mode='F')
                out.append(np.asarray(im.resize((gw, gh), Image.BILINEAR if sc >= 1 else Image.BOX, box=ebox)))
                continue
            im = Image.fromarray(arr[y0e:y1e, x0e:x1e, c].astype(np.float32), mode='F')
            out.append(np.asarray(im.resize((gw, gh), Image.BILINEAR if sc >= 1 else Image.BOX)))
        return np.stack(out, 2)
    Pg = to_game(P); Qg = to_game(Q, 1.0)
    Wg = to_game(ROW_ADD_W) if ROW_ADD_W is not None else None
    if EXACT_REG:
        Fg = np.asarray(Image.fromarray(np.pad(foot.astype(np.uint8) * 255, EP)).resize((gw, gh), Image.NEAREST, box=ebox)) > 127
    else:
        Fg = np.asarray(Image.fromarray(foot[y0e:y1e, x0e:x1e].astype(np.uint8) * 255).resize((gw, gh), Image.NEAREST)) > 127
    G = np.asarray(game.convert('RGB')).astype(np.float64); B = np.asarray(plate.convert('RGB')).astype(np.float64)
    best = None
    for dy in range(-REFINE, REFINE + 1):
        for dx in range(-REFINE, REFINE + 1):
            X = int(round(gx0)) + dx; Y = int(round(gy0)) + dy
            xa, ya = max(0, X), max(0, Y); xb, yb = min(G.shape[1], X + gw), min(G.shape[0], Y + gh)
            if xb - xa < 4 or yb - ya < 4: continue
            sl = (slice(ya - Y, yb - Y), slice(xa - X, xb - X))
            pred = np.clip(Pg[sl] + Qg[sl] * B[ya:yb, xa:xb], 0, 255)
            d = np.abs(G[ya:yb, xa:xb] - pred)
            m = Fg[sl]
            for (ex0, ey0, ex1, ey1) in EXCLUDE:  # game-px boxes left out (HUD text over the footprint)
                m = m.copy(); m[max(0, ey0 - Y):max(0, ey1 - Y), max(0, ex0 - X):max(0, ex1 - X)] = False
            if m.sum() < 16: continue
            mean = d[m].mean()
            if best is None or mean < best[0]:
                best = (mean, dx, dy, (xa, ya, xb, yb), sl, pred, d, m)
    if best is None: return None
    mean, dx, dy, (xa, ya, xb, yb), sl, pred, d, m = best
    # Lighting: the game shades character sprites with the stage light (a per-channel scale k and offset c on the
    # sprite colour before blending). gamecap pins one training stage and measures (k, c) once on idle-sprite
    # controls (fit_light); with E = P + Q*bg the lit prediction is k*P + c*(1-Q) + Q*plate.
    Gs = G[ya:yb, xa:xb]; Bs = B[ya:yb, xa:xb]; Ps = Pg[sl]; Qs = Qg[sl]
    fit = []
    for c in range(3):
        y = (Gs[..., c] - Qs[..., c] * Bs[..., c])[m]
        X = np.stack([Ps[..., c][m], (1 - Qs[..., c])[m]], 1)
        coef, *_ = np.linalg.lstsq(X, y, rcond=None)
        fit.append((float(coef[0]), float(coef[1])))
    game_vs_model.last_fit = fit
    game_vs_model.last_arrays = (Gs, Bs, Ps, Qs, m)
    game_vs_model.last_window = (xa, ya, xb, yb, gorg)
    light = LIGHT if LIGHT is not None else [(1.0, 0.0)] * 3
    lpred = np.empty_like(pred)
    rows = np.arange(ya, yb, dtype=np.float64)[:, None]
    for c in range(3):
        add = light[c][1]
        lpred[..., c] = Ps[..., c] * light[c][0] + (1 - Qs[..., c]) * add + Qs[..., c] * Bs[..., c]
        if ROW_ADD is not None:  # per-row stage chara colour (BgColor specular), on the CG layers' coverage
            w = (1 - Qs[..., c]) if Wg is None else Wg[sl][..., c]
            lpred[..., c] += w * ROW_ADD(c, rows, gorg)
    if LIGHT_FN is not None: lpred = LIGHT_FN(Ps, Qs, Bs, rows, gorg)  # a full per-pixel shading model (MBTL)
    if POST_ADD is not None:
        if POST_ADD_SRC is not None:  # bokashi from the CG layer only, seen through the .pat layer drawn over it
            Pc, Qc, Vc = (to_game(a, f)[sl] for a, f in zip(POST_ADD_SRC, (0.0, 1.0, 1.0)))
            lpred = lpred + POST_ADD(Pc, Qc) * Vc
            game_vs_model.last_cg = (Pc, Qc, Vc)
        else:
            lpred = lpred + POST_ADD(Ps, Qs)
    lpred = np.clip(lpred, 0, 255)
    ld = np.abs(Gs - lpred)
    lmean = ld[m].mean(); lchanged = ((ld.sum(2) > 24) & m).sum() * 100.0 / m.sum()
    # tolerance band: per pixel, the best of the prediction shifted by up to +-BAND px (distance-transform style
    # tolerance for thin geometry, where a sub-pixel edge or AA difference otherwise dominates the mean)
    if BAND > 0:
        Gp = Gs; bd = np.full(Gs.shape[:2], np.inf); bdc = None
        for sy in range(-BAND, BAND + 1):
            for sx in range(-BAND, BAND + 1):
                sh_ = np.roll(np.roll(lpred, sy, 0), sx, 1)
                dd = np.abs(Gp - sh_); ds = dd.sum(2)
                take = ds < bd
                bd = np.where(take, ds, bd)
                bdc = dd if bdc is None else np.where(take[..., None], dd, bdc)
        game_vs_model.last_band = (float(bdc[m].mean()), float(((bd > 24) & m).sum() * 100.0 / m.sum()))
    else:
        game_vs_model.last_band = None
    game_vs_model.last_lpred = lpred  # render-match15: the float prediction (last_light[3] is truncated to uint8)
    game_vs_model.last_light = (light, lmean, lchanged, Image.fromarray(lpred.astype(np.uint8)),
                                Image.fromarray(np.clip(ld * 4 * m[:, :, None], 0, 255).astype(np.uint8)))
    changed = ((d.sum(2) > 24) & m).sum() * 100.0 / m.sum()
    gcrop = Image.fromarray(G[ya:yb, xa:xb].astype(np.uint8))
    pcrop = Image.fromarray(pred.astype(np.uint8))
    dimg = Image.fromarray(np.clip(d * 4 * m[:, :, None], 0, 255).astype(np.uint8))
    up = lambda im: im.resize((int(im.width / sc), int(im.height / sc)), Image.NEAREST) if sc < 1 else im
    return up(gcrop), up(pcrop), up(dimg), mean, changed, (dx, dy), int(m.sum())


def label(im, text):
    out = Image.new('RGB', (im.width, im.height + 22), (20, 20, 20))
    out.paste(im, (0, 22))
    ImageDraw.Draw(out).text((6, 4), text, fill=(240, 240, 240))
    return out


def main():
    # sheet.py <compare dir> [--game DIR] [--small]
    #   --game: where the full game frames are (gamecap writes them to tools/gamecap/captures/s1_compare/game,
    #           outside git); JSON sidecars are read from there too, falling back to <compare dir>/game
    #   --small: palette-quantize the sheets (evidence kept small in git)
    args = sys.argv[1:]
    small = '--small' in args
    if small: args.remove('--small')
    gd = None
    if '--game' in args:
        i = args.index('--game'); gd = args[i + 1]; del args[i:i + 2]
    root = args[0]
    ed = os.path.join(root, 'editor'); od = os.path.join(root, 'sheets')
    if gd is None:
        gd = os.path.join(root, 'game')
    os.makedirs(od, exist_ok=True)
    tsv = []
    for name, note, zoom in cases():
        po, pn = os.path.join(ed, name + '_old.png'), os.path.join(ed, name + '_new.png')
        if not (os.path.exists(po) and os.path.exists(pn)):
            print(name, 'missing editor shots'); continue
        old, new = Image.open(po).convert('RGB'), Image.open(pn).convert('RGB')
        box = crop_box([old, new]) or (0, 0, old.width, old.height)
        if box[2] - box[0] < 8 or box[3] - box[1] < 8: box = (0, 0, old.width, old.height)
        old_c, new_c = old.crop(box), new.crop(box)
        d, mean, changed = diff(old_c, new_c)
        mv = None
        tiles = [label(old_c, 'old build (395a56b)'), label(new_c, 'new build (S1)'), label(d, '|old - new| x4')]
        pg = os.path.join(gd, name + '_game.png')
        if os.path.exists(pg):
            game = Image.open(pg).convert('RGB')
            who = 'MBTL' if name.startswith('mbtl_') else 'UNI2'
            pk, pw = os.path.join(ed, name + '_new_k.png'), os.path.join(ed, name + '_new_w.png')
            pp = os.path.join(gd, name + '_game_plate.png')
            pm = os.path.join(gd, name + '_game.json')
            meta = json.load(open(pm)) if os.path.exists(pm) else None
            mv = None
            if os.path.exists(pk) and os.path.exists(pw) and os.path.exists(pp):
                mv = game_vs_model(game, Image.open(pp).convert('RGB'), Image.open(pk), Image.open(pw), zoom, meta, editor_origin(new))
            gv = None if mv else game_vs_new(game, new, box, zoom, meta)
            if mv:
                gcrop, pcrop, dimg, gmean, gchanged, (dx, dy), npx = mv
                model_tiles = [label(gcrop, 'game (%s), refine %+d,%+d' % (who, dx, dy)),
                               label(pcrop, 'new editor over the game backdrop'), label(dimg, '|game - predicted| x4')]
                print('%-28s game vs new-editor model: mean |d| %.2f, %.2f%% of %d sprite px differ by >24 (refine %+d,%+d)'
                      % (name, gmean, gchanged, npx, dx, dy))
                tsv.append('%s\t%.3f\t%.2f\t%d\t%d\t%d\tmodel' % (name, gmean, gchanged, npx, dx, dy))
            elif gv is None:
                tiles.insert(0, label(game, 'game (%s), registration failed' % who))
                print('%-28s game: registration failed' % name)
            else:
                g_al, gd_im, gmean, gchanged, score, sc, (dx, dy) = gv
                tiles.insert(0, label(g_al, 'game (%s) x%.3f, refine %+d,%+d px, ncc %.2f' % (who, sc, dx, dy, score)))
                tiles.append(label(gd_im, '|game - new| x4 (sprite px)'))
                print('%-28s game->new mean |d| %.2f, %.2f%% sprite pixels differ (scale %.3f, refine %+d,%+d, ncc %.2f)'
                      % (name, gmean, gchanged, sc, dx, dy, score))
                tsv.append('%s\t%.3f\t%.2f\t%d\t%d\t%d\traw(ncc %.2f)' % (name, gmean, gchanged, -1, dx, dy, score))
        rows = [tiles] + ([model_tiles] if mv else [])
        W = max(sum(t.width for t in r) + 8 * (len(r) - 1) for r in rows)
        Hs = [max(t.height for t in r) for r in rows]
        sheet = Image.new('RGB', (W, sum(Hs) + 24 + 8 * (len(rows) - 1)), (32, 32, 32))
        y = 24
        for r, h in zip(rows, Hs):
            x = 0
            for t in r:
                sheet.paste(t, (x, y)); x += t.width + 8
            y += h + 8
        ImageDraw.Draw(sheet).text((6, 6), '%s: %s' % (name, note), fill=(255, 220, 120))
        if small:
            sheet = sheet.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        sheet.save(os.path.join(od, name + '.png'), optimize=True)
        print('%-28s old->new mean |d| %.2f, %.2f%% pixels changed' % (name, mean, changed))
    if tsv:
        with open(os.path.join(od, 'game_vs_new.tsv'), 'w') as f:
            f.write('case\tmean_abs_diff\tpct_sprite_px_diff_gt24\tsprite_px\trefine_dx\trefine_dy\tmethod\n' + '\n'.join(tsv) + '\n')


if __name__ == '__main__':
    main()
