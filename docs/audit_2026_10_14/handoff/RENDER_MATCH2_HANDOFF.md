# Round 3 (2026-10-15, same branch; fix commit acca786)

## Fix: D3D9 pixel-centre rule (the rect half-pixel bug)
- IDA (renamed, saved): UNI2 1.42, MBTL and MBAA apply **no** half-pixel correction anywhere. Checked:
  - UNI2: `PatPart_GetGlobalPerspective640x480` 0x4C1860, `CDxCamera_ctor` 0x4AAE30, `CDxCamera_SetPerspectiveFovLH` 0x4AAD10,
    `CgLayer_DrawWithCharaShader` 0x4BE140, shaders `VS_CharaPtDrawV`/`VS_CharaDrawV` (`Shader/sh_chara.txt`).
  - MBTL: `PatPart_GetGlobalPerspective640x480` 0x4A3DE0, `CDxCamera_ctor` 0x48C220.
  - MBAA: `Camera_UpdateMatrices` 0x44BE95.
- So D3D9 samples every pixel at integer window coordinates, half a pixel left/up of GL. Same coverage, different samples.
- Fix: `partrender::D3d9PixelCentreNdc` (+0.5 px) in `Render::UpdateProj` for the whole world view (sprites, parts,
  axes, boxes); `invOrtho` stays the plain ortho inverse. CG UVs start 0.25 texel in (`CgUvStart`, UNI2 0x4BE140).
  Unit check `TestD3d9PixelCentre` in `tests/part_render_math_test.cpp`.
- Side effect: the editor's vertical axis moved one column right (it now sits on the origin pixel); `sheet.py`
  KNOWN_ORIGINS updated to (632, 580) / (842, 680).
- ctest 40/40; part_render_math_test 255 checks.
- Shoot scripts now put the editor window at x = -2000. A user wheel-zoom hit an on-screen shot window earlier.
  All shots were retaken after the final build.

## Results (INDEX "Round 3")
- rect_chr024_p76: R profile identical to the game (mean |d| 0.00), centre 0.00 px, integral x1.000.
  - Open: the game lights one extra row at the top (row -1: R 32, G 66, partial). The editor starts at row 0.
  - The game origin y is 639.99999. Probably a y-only difference or the D3D9 default WRAP address mode at v = 0;
    not resolved.
- CG edge band gone without the band metric: p437 2.31 / 4.4% (was 9.88 / 10.8%), p435 2.06 / 2.1% on the r3
  capture. Floor 1.29 / 0.6%.
- **Vorpal glow** (cyan doll outline, user report): GRD judge win -> `Battle_Std.GRDJudgeWinEffect_Init`
  (`script/btl_MvFunc.txt`) -> `PcAuraEffect_Set` (`SqBind_CharaAura_Set` 0x886810).
  - Aura struct: +0 obj, +8 type, +0xC blendmode, +0x1C color, +0x2C colorB, +0x3C color_chara, +0x4C power,
    +0x50 pulsed glow ARGB, +0x54 prio, +0x60 pulse.
  - `CharaAura_DrawGlow` 0x59D910 draws the object white into a 640x360 RT. `CharaAura_BlurGlowPingPong` 0x59D710
    then runs sh_blur `CharaPointBlur_W`/`_H` (taps 1/.4/.3/.2/.1 at k*power texels). The result is composited over
    the frame under the chara with the +0x50 colour.
  - The `_unlit` hold only zeroes color_chara, so the glow stayed.
  - Modelled in `v2_model.py --glow sq` (alpha squared per pass, i.e. alpha-blend onto a cleared target; 'lin' and
    'sqblur' fit worse).
  - p437 ring 16.02 -> 6.99. The rest is the game's drop shadow, also a game-only pass.
  - Not added to the editor: it is script state (GRD), not .pat/.ha6 data. A "preview Vorpal glow" toggle would
    need an offscreen RT and 2 blur passes in Render. Noted, not done.
  - To capture without it: training GRD/Vorpal options (strings `VORPALMODE`, `GRDMODE`), or poke aura+8 = -1.
- **MBTL light (item 3)**: `Stage_BuildCharaBgColorParams` 0x58E230 (MBTL) is UNI2's BgColor system:
  - Light images: `stage_color.img` (multiplier), `stage_specular.img` (add), `stage_bokashi_alpha.img`.
  - Each is a 2-row strip sampled at the object's x by `Stage_SampleCharaBgStrip` 0x4EFD50: base = bottom row,
    height = top - bottom.
  - The std height is `g_StageCharaStdHeightTbl` 0xCAA174, or **1** when its flag is off, so the gradient is flat
    and a per-channel constant (k, c) is the model. MBTL-IM LightStrength blends these images toward white/none.
  - Not done: reading the strip values for the pinned stage to replace the fitted (k, c) with the image values.
    The MBTL shader is packed, so the k > 1 scale is unexplained.
- **PRAS** recaptured (r3, same-session plate stepped once): 7.15 / 24.6% (band 3.86 / 13.5%), not at the floor.
  - The footprint mixes CG layers (gradient) and .pat parts (no gradient).
  - Next: split the gradient per layer type (editor layer mask).
- **MBTL PUPS halo** (item 4), not done. Lead: in part set 318, prop 14 is the disc, cut-out 141 with
  **colorSlot 248**, PRAL 1, alpha 153, scale 2.7. The game tints it with palette[248] (PPPA). The faint editor disc
  suggests the palette slot / PUPS palette is resolved differently. Check `paletteTint` for MBTL PUPS patterns before
  modelling a game pass.
- Not done: rebuilding the other 11 sheets (only p435, p437, pras, rect were rebuilt) and re-measuring the S1-plate
  cases.
- uni_regress on the final build: rc=0, byte-exact (`v2/uni_regress_r3.log`). ctest 40/40.

# Render match 2 handoff (2026-10-15, fix/render-match2 in wt/render-match2, from update/parity-1014 362e462)

## Result
Every one of the 15 v2 cases now has a numeric residual against its own control floor
(`docs/audit_2026_10_14/evidence/compare/v2/INDEX.md`, section "Cases (render-match2)"). Sheets were rebuilt; the full
size ones are in the main checkout's `docs/.../compare/v2/`, the half-size copies are in this worktree.

| case | residual | own floor | status |
|---|---|---|---|
| afhk_cg_chr020_p435_t27 (unlit) | 13.32 / 13.2%; band +-1 **2.23 / 2.4%** | chr020 p0 unlit 1.29 / 0.6% (band 1.27 / 0.6%) | edge |
| afhk_cg_chr020_p437_t2 (unlit) | 9.88 / 10.8%; band **2.84 / 5.5%** | same | edge |
| rect_chr024_p76 | line: width 6 = 6, top 0 px, **centre +0.6 px**, R integral x1.27 | chr024 p0 unlit 1.71 / 0.4% | **real, open** |
| mbtl_flipV_chr001_p157f2 | 11.84 / 46.6%; band **5.46 / 23.6%** | MBTL chr001 p0 7.29 / 31.0% (band 6.02 / 27.3%) | floor with the band |
| mbtl_pups_chr011_p309 | silhouette **2.27 / 5.6%**; halo disc 43.7 / 96.8% | 7.29 / 31.0% | silhouette at floor; halo open |
| mbtl_ctrl_chr001_p0f0 | 7.29 / 31.0% with the MBTL stage light (k 1.07/1.04/1.05, c 2/3/5) | (is the floor) | control |
| other 9 | unchanged from the first pass (INDEX) | | floor |

Not 100 % yet. Open items, in priority order:
1. **rect_chr024_p76 is a real editor difference.** It is a half-pixel rasterisation offset of .pat parts.
   - `v2_thin.py` profile (game minus the same-session plate): the game line is centred on the origin pixel with a 1-px
     peak (R 7.6, 28.5, 38, **100**, 38, 28.3).
   - The editor's zoom-1 shot is centred half a pixel to the right, with a 2-px core (R 16.7, 36.3, **100, 100**,
     36.4, 16.8).
   - Width and top endpoint match exactly.
   - Likely cause: D3D9 pixel centres are at integer coordinates and the game's .pat 3D path
     (`PatPart_SetupWorldMatrixAndShader` 0x415230) applies no -0.5. The editor's GL path samples at +0.5.
   - Fix to try: a half-pixel offset in `partrender::PartPerspective` / `Render::UpdateProj`, for parts only.
     - Add a unit check in `tests/part_render_math_test.cpp`: the projected pixel centre of a part at x = 0 must land on
       the origin pixel centre.
     - Re-shoot `rect_chr024_p76_z1` and rerun `v2_thin.py`.
   - Not done here: the time box ran out before the C++ change. No C++ was changed in this branch, so ctest and
     uni_regress are as on update/parity-1014.
2. **CG edge band** (p435, p437): the residual is a 1-px band along every edge of the rotated or scaled
   point-sampled CG. The ±1 px band metric is at the floor. It may be the same half-pixel convention; check after
   item 1.
3. **MBTL PUPS halo:** a soft halo in a disc around the white silhouette. The editor has a faint full disc part
   there; the game shows a much brighter blurred halo.
   - It remains with MBTL-IM `Stages BloomStrength 0`, so it is a game pass, not the mod's bloom.
   - Candidates: the PRAL 3 blur-mask callbacks (MBTL 0x410330 path), or a glow like UNI2's `CharaAura_DrawGlow`.
   - Next: find it in the MBTL IDB.
4. **MBTL stage light:** a per-channel k of about 1.05 measured on the control. Not yet found in the MBTL IDB; MBTL-IM
   has `Stages LightStrength 100`. Try `LightStrength 0` and look for the stage chara colour path, as in UNI2 below.
5. **pras_chr020_p5f2** and the other first-pass UNI2 cases use the old S1 plates and no gradient model. Rerun them in
   same-session v2_light sessions with `--grad-ctrl`; the chr020 floor is now 1.29.

## Findings
- **UNI2 stage chara gradient** (IDA uni2 1.42, renamed and saved):
  - `Stage_BuildCharaBgColorParams` 0x5C44E0 builds the CG layer BgColor block.
  - `Stage_SampleCharaBgSpecular` 0x518130 supplies base (bottom) and height (top − bottom) from the stage curve
    `g_StageCharaSpecCurve` 0xC58ABC at the object's x.
  - The std pixel height comes from `g_StageCharaStdHeightTbl` 0xC58C24, or 400.
  - `DrawObj_DrawLayers` draws each CG layer three times through `CgLayer_DrawWithCharaShader` 0x4BE140:
    `TecCharaDrawBgColor_V`, `…Specular_V` and `…Bokashi_V`. The shader text is `gamedata/uni2/Shader/sh_chara.txt`
    (main checkout, outside git).
  - The specular pass adds base + height × clamp(h/H, 0, 1).
  - Other renames: `CgLayer_SetBgColorLocalParams` 0x4BEBE0, `CharaAura_DrawGlow` 0x59D910 (the cyan aura outline
    around chr020's doll), `CharaObj_DrawAfterimages` 0x59C710, `SqBind_SetBgColor`/`SqBind_SetBgSpeculer` with
    `g_BgColorIndex`/`g_BgSpeculerIndex`, `g_StageCharaLightSlot`, `g_StageCharaLightOn`, `g_StageCharaBokashiCurve`.
- **Model:** `v2_model.py --grad-ctrl CTRL,CTRLGAME` calibrates (base, height) on the same session's idle control and
  applies it to the case unchanged.
  - chr020: base (10.3, 9.1, 11.5), height (11.9, 3.2, 13.7).
  - The idle control goes from 14.04 / 97.7% to 1.29 / 0.6%.
  - .pat parts do not use it; they go through the Han6Draw pat path.
- **Aura glow:** the lit CG captures also carry the chara aura glow over the sprite (p437 lit band 11.5 / 75.9%). The
  numeric cases therefore use the `_unlit` captures (aura RGB held 0).
- **Metric changes** (`sheet.py`):
  - `AXIS_FIX` subtracts the editor's additive axis lines instead of dropping their columns. This kept the rect's
    core.
  - `REFINE` sets the alignment search radius.
  - `BAND` adds the ±N px tolerance residual.
  - `ROW_ADD` is a per-row add (the gradient).
- **New script:** `v2_thin.py` reports line geometry and colour for thin parts.

## Captures (tools/gamecap, outside git; copies in v2/gamecap_scripts, sidecars in v2/game_meta)
- **UNI2:** `cases/v2_light_chr020.gcs` and `v2_light_chr024.gcs`, each with lit and unlit control and case.
  - Includes: `games/uni2_v2_ctrl_plate.gcs`, `uni2_v2_case_aura_plate.gcs`.
  - The plate is `hidefighters on` plus **one UNI2-IM F6 step**. A paused UNI2-IM replays the frozen frame, so a grab
    without the step equals the frame. The first chr020/chr024 runs had that bug; they were rerun.
- **MBTL:** `cases/v2p_mbtl_ctrl_chr001.gcs`, `v2p_mbtl_chr011.gcs`, `v2p_mbtl_chr004.gcs`, with
  `games/mbtl_v2_case_plate.gcs`.
  - The plate holds P1/P2's draw-alpha override (obj+0x648 = 0, +0x654 = 9999) for one MBTL-IM step, then pokes it back
    (0 / 255).
    - Leaving +0x654 at 9999 hides the fighters in the next case of the session.
  - MBTL-IM's own "Hide the characters" hotkey, bound to F9 via the ini, did not follow the toggle reliably under
    gamecap (runs 1–4), so it is no longer used.
  - Runs used `Stages BloomStrength 0`; the outputs are in `captures/v2/nobloom/`, staged as `captures/v2/*_nb_*`.
  - The backbuffer `capture` (`_bb`) is the reliable frame. PrintWindow (`capture_window`) is often black or stale.
  - chr004 (pral2) was not rerun with the fixed plate; its full-frame number stands.
- **sosfiro:** `afhk_cg_chr020_p435_t27_sosfiro.png` was captured and copied to the main checkout's `v2/sosfiro/`.

## Commits (fix/render-match2)
- WIP commit: the tool changes.
- Final commit: INDEX, sheets, sidecars, scripts and this handoff.

No push or merge.
