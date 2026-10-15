# v2 game-vs-editor index (2026-10-15, fix/render-match2; first pass fix/render-match)

The sheets are `<case>.png` in this folder (half size here, full size in the main checkout's copy of the same folder). Each sheet has four tiles: game with the Improvement Mod hitboxes, game clean, ours, and sosfiro. The sosfiro shot is scaled by its own zoom.

The game frame in every sheet is verified: pattern, frame and tick are asserted from memory in the capture script and shown in the tile title.

## How the residual is measured
**Metric.** Mean |d| per channel over the editor's sprite footprint, and the % of footprint pixels with |d| > 24.
- Prediction = editor black/white shots composited over the game's backdrop plate (`tools/uni/s1_compare/v2_model.py`).
- Full-view parts use a straight per-pixel diff of the placed editor shot (`v2_fullframe.py`).

**Light model (from IDA, uni2 1.42).** Every .pat part of P1 gets the chara aura as a specular add: `+int(int(aura.rgb*255) * strength)`.
- Code path: `CharaObj_DrawWithStageLight` 0x5CE3C0 → `PatPart_ComputeSpecular_PRSPPlusObjAdd` 0x413BA0.
- The aura is `g_CharaAura` 0xC7A688, slot `[obj+0x8C0]`. The character script sets it (`SqBind_CharaAura_Set` 0x886810).
- It pulses as a triangle wave over time (`CharaAura_UpdatePulse` 0x59E8A0).
- Diffuse is not lit (k = 1).

**"aura" column.**
- New captures record the aura in the sidecar ("sidecar"). Old captures without that record get the add fitted under k = 1, c ≥ 0 ("fit").
- CG-layer sprites also get the stage colour from the TecCharaDraw BgColor shader.

**Noise floor** (idle controls, geometry identical; the residual is edge AA and stage animation):
| control | residual |
|---|---|
| chr019 p0, aura held at 0 | 5.20 / 10.7% |
| chr019 p0, lit (k, c fitted) | 6.87 / 19.0% |
| chr025 p0 / chr000 p0 (S1) | 5.6 / 5.2 at 18% |
| chr027 p0, small sprite (unlit / lit) | 9.50 / 21.4%, 11.5 / 36% |

**Floor used below: ≈ 5–7 mean, ≈ 10–20%.**

## Cases (render-match2, 2026-10-15: every case numeric, against its own control)
Columns: residual = mean |d| per channel / % footprint px with |d| > 24 (summed RGB); "band" = the same with a +-1 px
tolerance band (per pixel, best of the prediction shifted by up to 1 px; `v2_model.py --band 1`). Floor = the idle
control of the same character, same session, same metric. Status: **floor** = at or below the control floor;
**edge** = at the floor only with the +-1 px band (residual confined to a 1-px edge); **real** = a real editor
difference, open.

| case | game capture used | residual (plain) | band +-1 | own control floor (plain / band) | status |
|---|---|---|---|---|---|
| flipHV_chr019_p153 | `_unlit` (aura held 0) | 6.75 / 19.5% | - | chr019 p0 unlit 5.20 / 10.7% | floor (previous pass, unchanged) |
| pups_chr027_p449 | `_unlit` | 0.00 / 0% | - | chr027 p0 unlit 9.50 / 21.4% | floor (previous pass) |
| mbtl_pral2_chr004_p420f4 | v2 | full frame 0.69 / 0.6% | - | full-frame (no sprite) | floor (previous pass) |
| pral2_chr025_p133f1 | v2 + S1 plate | 4.68 / 13.8% | - | chr025 p0 5.6 / 18% | floor (previous pass) |
| flipH_chr000_p96f1 | v2 + S1 plate | 0.76 / 0.5% | - | chr000 p0 5.2 / 18% | floor (previous pass) |
| flipV_chr000_p383f1 | v2 + S1 plate | 3.43 / 10.2% | - | chr000 p0 5.2 / 18% | floor (previous pass) |
| afhk_pat_chr000_p72_t2 | v2 + S1 plate | 3.83 / 14.4% | - | chr000 p0 5.2 / 18% | floor (previous pass) |
| afhk_pat_chr019_p62_t7 | v2 + S1 plate | 3.04 / 6.0% | - | chr019 p0 5.20 / 10.7% | floor (previous pass) |
| pras_chr020_p5f2 | v2 + S1 plate | 5.45 / 13.5% | - | chr020 p0 unlit 1.29 / 0.6% (new) | above the new chr020 floor; old plate, not re-run with the gradient model (see handoff) |
| **afhk_cg_chr020_p435_t27** | `_unlit` + same-session plate, stage gradient model | 13.32 / 13.2% | **2.23 / 2.4%** | ctrl_chr020_p0_unlit **1.29 / 0.6%**, band 1.27 / 0.6% | edge (1-px edge band of the AFAZ-rotated point-sampled CG) |
| **afhk_cg_chr020_p437_t2** | `_unlit` + same-session plate, stage gradient model | 9.88 / 10.8% | **2.84 / 5.5%** | as above | edge (AFZM-scaled CG, same edge band) |
| **rect_chr024_p76** | `_unlit` + same-session plate; `v2_thin.py` on the zoom-1 shot | line metric: width 6 = 6 px, top endpoint 0 px, **centre +0.6 px**, R integral x1.27, R profile mean |d| 15.4 | 13.30 / 74.2% (pixel metric, zoom 1.5 shot) | ctrl_chr024_p0_unlit 1.71 / 0.4% | **real**: the editor rasterises the .pat line half a pixel right with a 2-px core; the game's is centred on the origin pixel with a 1-px core |
| **mbtl_ctrl_chr001_p0f0** | `_nb` (MBTL-IM bloom 0) + same-session plate (P1/P2 draw alpha 0) | 13.02 / 70.7% raw; **7.29 / 31.0%** with the MBTL stage light (k 1.07/1.04/1.05, c 2/3/5) | 6.02 / 27.3% | (is the MBTL floor) | control |
| **mbtl_flipV_chr001_p157f2** | `_nb` + same-session plate, control light | 11.84 / 46.6% | **5.46 / 23.6%** | 7.29 / 31.0%, band 6.02 / 27.3% | floor with the band (plain is the arc's AA over a 35k px translucent footprint) |
| **mbtl_pups_chr011_p309** | `_nb` + same-session plate, control light | silhouette (Q < 0.5, 42.7k px) **2.27 / 5.6%**; translucent disc 43.7 / 96.8% | 22.35 / 53.6% (all) | 7.29 / 31.0% | silhouette at floor; the soft halo around it in the game is not in the editor (unmodelled glow/blur pass, see handoff) |

**Stage chara gradient (new, all UNI2 CG character sprites).** IDA uni2 1.42 (renamed, saved):
`Stage_BuildCharaBgColorParams` 0x5C44E0 builds a BgColor block (base and height colours from
`Stage_SampleCharaBgSpecular` 0x518130 at the object's x, std pixel height from `g_StageCharaStdHeightTbl` 0xC58C24 or 400)
and `DrawObj_DrawLayers` draws each CG layer three times through `CgLayer_DrawWithCharaShader` 0x4BE140
(`TecCharaDrawBgColor_V`, `TecCharaDrawBgColorSpecular_V`, `TecCharaDrawBgColorBokashi_V`; shader
`Shader/sh_chara.txt`): specular += base + height x clamp(h / H, 0, 1), h = sprite px above the origin. The model
(`v2_model.py --grad-ctrl CTRL,CTRLGAME`) calibrates (base, height) on the same session's idle control and applies it to
the case unchanged. chr020 on the pinned stage: base (10.3, 9.1, 11.5), height (11.9, 3.2, 13.7) at H = 400. It takes
the chr020 idle control from 14.04 / 97.7% to 1.29 / 0.6%.

## Round 3 (2026-10-15, after acca786: D3D9 pixel-centre rule in the editor)
Editor shots retaken with the fixed build (window off-screen at x = -2000). The editor origin is now (632, 580) in
the 1264x861 shots (`sheet.py` KNOWN_ORIGINS). Metrics in `r3_json/`. "r3" captures: `gamecap_scripts/v2r3_chr020.gcs`
(chr020 unlit, same-session plates, aura glow fields in the sidecar).

| case | capture | residual (plain) | band +-1 | own floor | status |
|---|---|---|---|---|---|
| rect_chr024_p76 | `_unlit`, `v2_thin.py` on the zoom-1 shot | R profile **identical** (7.6 28.5 38 100 38 28.3; mean abs d 0.00), centre d 0.00 px, width 6 = 6, R integral x1.000; top endpoint d +1 row (see handoff) | - | ctrl_chr024_p0 1.71 / 0.4% | **fixed** (x); 1 top row open |
| afhk_cg_chr020_p435_t27 | `_r3` + gradient (ctrl r3) | **2.06 / 2.1%** | 2.01 / 2.0% | ctrl_chr020 1.29 / 0.6% | floor without the band (old `_unlit` capture: 5.50 / 5.5%, band 2.06) |
| afhk_cg_chr020_p437_t2 | `_r3` + gradient | **2.31 / 4.4%** | 2.30 / 4.4% | 1.29 / 0.6% | edge band gone (was 9.88 / 10.8%); same on the old `_unlit` capture |
| afhk_cg_chr020_p437_t2 Vorpal glow ring (12 px around the doll) | `_r3`, `--glow sq` | no glow 16.02 / 37.9% -> **glow model 6.99 / 19.0%** | - | ctrl r3 ring: 11.22 / 41.9% -> 7.20 / 13.4% | modelled (ring includes the game's drop shadow) |
| pras_chr020_p5f2 | `_r3` same-session plate + gradient + glow | 7.15 / 24.6% | 3.86 / 13.5% | 1.29 / 0.6% | open: mixed CG + .pat footprint, the gradient applies to the CG layers only |
| mbtl_ctrl_chr001_p0f0 | `_nb` | 7.30 / 31.0% (k 1.07/1.04/1.05, c 2/3/5) | 6.02 / 27.3% | control | MBTL light = stage_color/specular strips (handoff) |
| mbtl_flipV_chr001_p157f2 | `_nb` | 9.51 / 40.8% | 5.24 / 22.2% | 7.30 / 31.0% | floor with the band |
| mbtl_pups_chr011_p309 | `_nb` (all px) | 27.78 / 61.7% | 22.38 / 53.5% | 7.30 / 31.0% | halo open (silhouette at floor) |

The other first-pass cases (flipHV, pups_chr027, pral2, flipH, flipV, afhk_pat) were not re-measured in round 3:
their S1 plate paths are not in captures/v2.

**Vorpal glow.** The cyan outline on chr020's doll is the GRD Vorpal aura: `btl_MvFunc.txt`
`Battle_Std.GRDJudgeWinEffect_Init` sets `PcAuraEffect_Set { type 0, power 1.0, color 0xAA00AAFF, colorB 0x0000AAFF,
color_chara 0x00001A2A, delay 60, blendmode 0 }` when P1 wins the GRD judge. `CharaAura_DrawGlow` 0x59D910 draws it
whenever aura type >= 0, independent of `color_chara` (the `_unlit` hold zeroes only that). Model:
`v2_model.py --glow sq` (see the `aura_glow` docstring).

## Reproduce
```
CASES=<re> OUTDIR=docs/.../v2/editor_bw tools/uni/s1_compare/shoot_bw2.sh
python3 tools/uni/s1_compare/v2_model.py <case> --plate <plate> [--game-name <capture>] [--aura-fit]
python3 tools/uni/s1_compare/v2_fullframe.py <case> --rect x0,y0,x1,y1 [--game-name N] [--aura]
python3 tools/uni/s1_compare/v2_sheet.py <case> [--game-name N --out NAME]
# render-match2
python3 tools/uni/s1_compare/v2_model.py afhk_cg_chr020_p435_t27 --game-name afhk_cg_chr020_p435_t27_unlit --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_unlit
python3 tools/uni/s1_compare/v2_model.py mbtl_flipV_chr001_p157f2 --game-name mbtl_flipV_chr001_p157f2_nb --light "1.07,2;1.04,3;1.05,5"
python3 tools/uni/s1_compare/v2_thin.py rect_chr024_p76_z1 --game-name rect_chr024_p76_unlit
# round 3
python3 tools/uni/s1_compare/v2_model.py afhk_cg_chr020_p437_t2 --game-name afhk_cg_chr020_p437_t2_r3 --grad-ctrl ctrl_chr020_p0,ctrl_chr020_p0_r3 --glow sq
```
Captures: `cases/v2_light_chr020.gcs`, `v2_light_chr024.gcs` (UNI2, plate = one UNI2-IM step after `hidefighters on`,
because a paused UNI2-IM replays the frozen frame), `v2p_mbtl_*.gcs` (MBTL, plate = P1/P2 draw-alpha override held 0
for one MBTL-IM step; MBTL-IM's own "Hide the characters" key did not follow the toggle reliably under gamecap).
Game captures: `tools/gamecap/run.sh cases/v2_light_*.gcs`. Copies are in `gamecap_scripts/`, sidecars in `game_meta/`.

## Round 4 / render-match3 (2026-10-15, fix/render-match3): final table
All editor views came from one hidden `--headless --batch` process (`tools/uni/s1_compare/shoot_batch.sh`, 72 shots).
The batch output was byte-identical to the old per-process shots before the C++ change. Every number comes from
`tools/uni/s1_compare/v2_round4.sh`; the log is `round4.log` and the per-run JSON is in `r4_json/`.

Method changes:
- **`--grey`**: the second backdrop is now 64 grey instead of white. An additive part clips on white, which gave
  Q < 1 and under-predicted every additive pixel over a bright plate. That was most of pras.
- **`--mbtl-light`**: the real MBTL CG shading model (below) replaces the fitted k > 1.
- **`--layers`**: per-layer CG / .pat gradient weight.
- UNI2 controls: chr024, chr019 and chr027 now each carry their stage gradient, calibrated on themselves.

Columns: residual is mean |d| / % px with |d| > 24; band is the ±1 px tolerance residual.

| case | residual | band ±1 | own control floor (plain / band) | status |
|---|---|---|---|---|
| flipHV_chr019_p153 | 9.23 / 27.4% | 3.85 / 10.3% | ctrl_chr019 unlit 3.39 / 8.1% (1.64 / 3.4%) | above the floor. The residual is the brown band reported in the first pass; not investigated this round |
| pups_chr027_p449 | no editor footprint | - | ctrl_chr027 4.48 / 9.7% | floor: neither side draws anything visible (layer 0 already selects bank 1) |
| mbtl_pral2_chr004_p420f4 | full frame 1.23 / 0.9% (rect 200,100,1080,600) | - | full-frame | floor |
| pral2_chr025_p133f1 | 2.72 / 4.5% | 2.25 / 3.9% | ctrl_chr025 2.48 / 0.7% | floor (plain, at the % level only with the band) |
| flipH_chr000_p96f1 | 0.84 / 1.1% | 0.35 / 0.0% | ctrl_chr000 2.66 / 4.1% | floor |
| flipV_chr000_p383f1 | 1.66 / 2.6% | 0.96 / 0.6% | 2.66 / 4.1% | floor |
| afhk_pat_chr000_p72_t2 | 3.07 / 9.1% | 1.55 / 2.0% | 2.66 / 4.1% | floor with the band |
| afhk_pat_chr019_p62_t7 | 1.61 / 3.1% | 0.79 / 0.8% | ctrl_chr019 3.39 / 8.1% | floor |
| afhk_cg_chr020_p435_t27 | 2.06 / 2.1% | 2.01 / 2.0% | ctrl_chr020 r3 1.29 / 0.6% (1.27 / 0.6%) | ~floor: +0.8 mean, a sub-pixel edge on the AFAZ-rotated point-sampled CG |
| afhk_cg_chr020_p437_t2 | 2.31 / 4.4% (glow ring 6.99 / 19.0%) | 2.30 / 4.4% | 1.29 / 0.6% | ~floor. The ring holds the game's drop shadow, a game-only pass |
| **pras_chr020_p5f2** | **4.29 / 8.5%** (was 7.15 / 24.6%) | **1.63 / 1.4%** (was 3.86 / 13.5%) | 1.29 / 0.6% (1.27 / 0.6%) | floor with the band. The per-layer split (CG over .pat) gives 4.33 / 8.7%, no better. The fix was `--grey`: the .pat layer is additive (PRAL 1, slot 237) and its add clipped on white |
| rect_chr024_p76 | thin metric: profile identical (mean abs d 0.00), centre d 0.00, width 6 = 6, top +1 row | pixel metric (492 px) 11.85 / 62.0%, band 5.64 / 25.6% | ctrl_chr024 1.71 / 0.4% | x fixed; one top row open (handoff) |
| **mbtl_ctrl_chr001_p0f0** | **0.27 / 0.0%** with the IDA model (was 7.30 / 31.0% with the fitted k, c) | 0.24 / 0.0% | (is the floor) | control |
| mbtl_flipV_chr001_p157f2 | 4.03 / 13.5% | 2.20 / 4.7% | 0.27 / 0.0% (sprite control; this frame is .pat only) | above the sprite floor. It is AA on a 35k px translucent arc; at the old pat floor with the band |
| **mbtl_pups_chr011_p309** | **0.91 / 1.9%** (was 27.78 / 61.7%) | 0.36 / 0.4% | 0.27 / 0.0% | **fixed**: the PPPA tint now reads the base palette (editor fix, below) |

- **MBTL PUPS halo, an editor bug, now fixed.**
  - IDA MBTL 25646393, renamed and saved: `PatPart_SetupDraw_ShaderWorldMtx` 0x411C10 sets
    g_fPtLastColor = drawParams+0x2C[PPPA].rgb / 255. Alpha is unused.
  - `Han6Object_Draw` 0x597070 points that at `CCharaDataSet+0x118`.
  - `CharaDataSet_ApplyColorBuildPalTexAndTintCopy` 0x593030 fills that copy from palette slot 0, which is the base
    `.pal` of the selected colour.
  - PUPS only moves the sprite's palette texture row. The editor tinted from the PUPS bank (`chr011_p1.pal`[248] =
    (54, 54, 63)); the game uses `chr011.pal`[248] = (193, 188, 255).
  - Fix: `partrender::PartTintPalette` in `src/parts/parts.cpp`. Unit check: `TestPppaTintPalette`.
- **MBTL light, the real formula** (IDA MBTL, renamed and saved). `Han6Draw_DrawFrameLayers_2to0` 0x4A0750 draws each
  CG layer three times through `Han6Draw_SubmitCgSprite_ParentMtx` 0x4A0C40 (MBTL `Shader/sh_chara.txt`):
  1. colour pass, normal blend: pal·vcol·(base + height·t)
  2. specular pass, additive: spec_base + spec_height·t
  3. bokashi pass, additive: bokashi_alpha · mean9x9(pow(pal·vcol, 4))
  - t = clamp(px above the feet / StdH, 0, 1), with StdH 330 for chr001 (`bg/bg_chara_height.csv`).
  - The strips are read at column 2048 + (x >> 7) of `bg/bg007/stage_color.img`, `stage_specular.img` and
    `stage_bokashi_alpha.img` (alpha 0.40).
  - k > 1 is the pow(tex, 4) bokashi add. It is not a gain or a gamma.
  - MBTL-IM BloomStrength only drives the separate `Stage_ScreenBloomPass` 0x4EEDA0.
  - Check script: `mbtl_stage_light_check.py`.
- ctest 41/41 (`ctest_r4.log`). uni_regress: `uni_regress_r4.log`.

## Round 5 / render-match4 (2026-10-15, fix/render-match4): the four open cases
Numbers come from `tools/uni/s1_compare/v2_round5.sh` (log `round5.log`, JSON in `r5_json/`). New game captures:
`gamecap_scripts/v2r5_chr019.gcs`, `v2r5_chr027.gcs`, `v2r5_mbtl_chr001.gcs` (per-process, UNI2-IM / MBTL-IM
pause/step, same-session plates; run log `gamecap_r5.log`). New editor views (one `--headless --batch` process,
`CASES=... shoot_batch.sh`) are the new rows at the end of `cases_control.txt`. No C++ change this round.

| case | residual | band ±1 | own control floor (plain / band) | status |
|---|---|---|---|---|
| flipHV_chr019_p153 (`_r5`, same-session plate) | **7.37 / 22.0%** (was 9.23 / 27.4%) | **2.35 / 5.0%** (was 3.85 / 10.3%) | ctrl_chr019 r5 3.08 / 7.3% (1.28 / 2.5%) | floor in % with the band (5.0% < 7.3%); plain mean still 2x. See below |
| flipHV_chr019_p153_z1 (zoom 1 shot, no resample) | 9.20 / 29.5% (102k px: the whole streak fan, partly off-view at 1.5) | 3.24 / 9.3% | ctrl_chr019_p0_z1 1.17 / 2.0% (1.15 / 2.0%) | open: streak edges |
| rect_chr024_p76 | thin metric unchanged: profile identical, centre d 0.00, width 6 = 6, top +1 row | - | ctrl_chr024 1.71 / 0.4% | open: 1 top row (no proven cause; see handoff) |
| **mbtl_patctrl_chr001_p238f10** (new .pat-only control, set 275) | 4.55 / 11.1% | 2.75 / 7.8% | (is a floor) | control |
| **mbtl_patctrl_chr001_p277f0** (new .pat-only control, set 211) | 4.83 / 13.7% | 2.21 / 6.3% | (is a floor) | control |
| mbtl_flipV_chr001_p157f2 | 4.03 / 13.5% | 2.20 / 4.7% | .pat-only control 4.55-4.83 / 11.1-13.7% (2.21-2.75 / 6.3-7.8%) | **floor** (at or below both .pat-only controls, plain and band) |
| **pups_chr027_p449f4** (`_r5`; CG 869 + .pat 839, cutout 427 PPPA 254 visible) | **4.17 / 14.5%** (no gradient: 13.48 / 99.5%) | 3.15 / 11.9% | ctrl_chr027 r5 4.67 / 10.2% (2.29 / 3.1%) | **floor** on the mean; +4% px over the floor at the additive PRAL 1 .pat edge |
| pups_chr027_p449f6 (`_r5`; CG 870 twice, one AFZM-zoomed alpha 128, + .pat 840 on two layers) | 6.16 / 22.0% | 4.39 / 11.1% | 4.67 / 10.2% (2.29 / 3.1%) | above: the zoomed alpha-128 CG copy (afhk_cg-type sub-pixel edge) |

- **flipHV.** Most of the round-4 residual was the plate. `v2_light_chr019_027.gcs` grabbed it after unpausing,
  with P1's body still on screen under the streaks. The new same-session plate takes the case to 7.37 / 22.0% (band
  2.35 / 5.0%). The rest sits on the edges of the six PRFL 1 (linear) streak parts of set 45 (PRZM up to 2 x 5).
  The game edges are stair-stepped, the editor edges smooth. A zoom-1 shot does not remove it, so it is not the
  1.5x resample. Not modelled.
- **MBTL flipV.** It now has a .pat-only floor of its own and sits at it.
- **pups chr027.** p449 f0 draws nothing on either side (set 690 is off-view). Frame 4 puts the PUPS-bank PPPA part
  on screen, and the case matches its control on the mean. It needs the stage gradient for its CG layer.

## Round 6 notes (2026-10-15): flipHV, ruled-out causes
- **Not AFHK.** p153 frame 0 has interp on, toward part set 46, which shortens the streaks and fades props 2/6/8.
  The game capture is in-frame tick 1. The editor's case tick counts from 0 (game tick 28 = editor 27; game frame-1
  tick 5 = editor 7), so editor tick 0 is the match, and that is what the case renders. Re-shot at editor tick 1 the
  residual goes 7.37 → 16.86 (zoom 1: 9.20 → 21.12), which confirms the game shows no lerp at its tick 1
  (progress (tick-1)/duration = 0).
- **Not the type-4 ribbon.** Prop 8 (shape 91, flip 3) is tessellated from `vtx` in both the editor and the game
  (`parts.cpp` type 4 branch).
- **Where the residual is.** Per 60 px cell at zoom 1, the lower streak tips improve from 9.8 → 2.8 and 7.2 → 1.7
  mean when the editor is shifted 1 px vertically, so those are placed 1 px off. The upper fan keeps 11–19 at any
  shift: different thin-streak content, not a placement error.
- **Code.** UNI2 `PatPart_DrawPlaneQuad` 0x4BFCB0 builds the quad at (-cx,-cy)..(-cx+w,-cy+h) and is never mirrored;
  PRRV flips only the UVs (u += w-1, w = -w). The editor matches that.
- Open: the 1 px tip offset on the 1×5-zoomed props 1–4 and 6.
- **pups_chr027_p449f6 (6.16 / 22.0%, control 4.67 / 10.2%) is at the floor without the game-only darkening.**
  - 51.7% of its |d| > 24 pixels are pixels where the game is clearly darker than the prediction (luma < 0.8×). That is
    the floor drop shadow under the zoomed alpha-128 CG copy, a game-only pass like the ring in afhk_cg_chr020_p437.
  - Excluding those pixels, the mean |d| is 4.46, below the control.
  - Re-measured with the round 5 inputs (identical numbers): v2_model `--grey`, `--grad-ctrl ctrl_chr027_p0,ctrl_chr027_p0_r5`.
- **Process note (2026-10-15).** A `sed` with `\|` (alternation in GNU sed) emptied `cases_control.txt` in the
  working copy for a while. Without the case rows every zoom fell back to 1, and the controls read 16–64. It was
  restored from git; nothing with the empty file was committed.
- **Edge-tie bias 1/64 → 0.005 px (render-match6).** The bias must exceed the 1/256 px snap, and 1/64 also broke the
  ties. But it moved every world pixel, and the rotated point-sampled CG of afhk_cg_chr020_p435_t27 picked other
  texels along its edges.
  - Measured with fresh editor shots: 0 → 2.06 / 2.1% (rect top row wrong), 0.004 → 2.22, 0.005 → 2.22 (rect top row
    exact), 1/64 → 2.56 / 2.5%.
  - Now 2.22 / 2.2% (band 2.01 / 2.0%) against its 1.29 floor.
  - The CG UV start was also re-checked on this case: 0.25 texel (the game's `CgLayer_DrawWithCharaShader` value) is
    best (0 → 7.27, 0.5 → 7.79, 0.75 → 12.84).
  - Edge sharpness matches, so both sides are point-sampled. The remaining ~0.9 is which texel a rotated edge pixel
    lands on.

## Round 6 / render-match6 (2026-10-15): part perspective FOV
- **Cause.** `kPartFovY` was 1.3045008 rad. Both games pass 0x3FA78D36 = 1.3089969 rad (75°) to
  D3DXMatrixPerspectiveFovLH (uni2 0x4C1860, MBTL 0x4A3DE0), which puts the part eye at 1024 × cot(37.5°) = 1334.5 px
  instead of 1340.7. Deep 3D-rotated parts land up to ~0.7 px off at their far ends (the flipHV tip offset above).
- **Fix.** `src/parts/part_render_math.h`; the test pins PartEyeDistance() = 1334.50. The UNI2 IDB comment was
  corrected and saved.
- **Re-measured.** Fresh editor shots, same game captures and commands as rounds 4/5:

| case | before | after |
|---|---|---|
| flipHV_chr019_p153 | 7.37 / 22.0% (band 2.35 / 5.0%) | **6.32 / 18.5%** (band 2.53 / 6.7%) |
| flipHV_chr019_p153_z1 | 9.20 / 29.5% | 8.85 / 28.6% |
| mbtl_flipV_chr001_p157f2 | 4.03 / 13.5% | **3.57 / 11.3%** |
| pups_chr027_p449f4 | 4.17 / 14.5% | **3.81 / 12.9%** |
| pups_chr027_p449f6 | 6.16 / 22.0% | 6.06 / 21.8% |
| flipH_chr000_p96f1 | 0.84 / 1.1% | 0.76 / 0.5% |
| flipV_chr000_p383f1 | 1.66 / 2.6% | 1.61 / 2.3% |
| afhk_pat_chr000_p72_t2 | 3.07 / 9.1% | 3.29 / 10.4% (band 1.58 / 2.1%) |
| afhk_pat_chr019_p62_t7 | 1.61 / 3.1% | 1.65 / 3.3% |
| pral2_chr025_p133f1 | 2.72 / 4.5% | 2.75 / 4.7% |

- **flipHV** is now within 2× of its 3.10 / 7.4% control on the mean, and under it in % with the band. The rest is the
  fan content described in the round 6 notes.
- **The three small increases are within each case's band floor.** Their shots also carry the tie-bias change above.

## Round 6b (2026-10-15): part UV bias
- **Cause.** All three games add a fixed bias to each part quad's UVs, (uv / 256) + bias per corner: uni2
  PatPart_DrawPlaneQuad 0x4BFCB0 and sub_4C0430, MBTL 0x4A2710 / 0x4A2DD0, MBAA PatPart_RenderQuad.
  - Start-side biases: u0 = 0x3951B717 (2.0e-4), v0 = 0x394C78EA (1.95e-4). End side: 0.
  - Only on an AMD adapter (VendorId 0x1002: uni2 sub_4B0480, MBTL sub_491E00) does the game switch to u0 = 0,
    u1 = −2.0e-4.
  - Read live on the capture machine (NVIDIA RTX 3070 Ti): gamecap `uni2_uvbias_peek.gcs`, `mbtl_uvbias_peek.gcs`.
  - The editor applied none of it. On stretched streaks that is a pixel-sized shift of the texture.
- **Fix.** `partrender::PartUvBiasU/V`, applied to plane and rect quads. The editor's other shapes map u/v onto their
  own axes, and biasing them moved MBTL flipV from 3.57 to 5.48, so they stay unbiased.

| case | round 6 | round 6b |
|---|---|---|
| flipHV_chr019_p153 | 6.32 / 18.5% | 6.17 / 17.7% (band 2.74 / 7.2%) |
| flipHV_chr019_p153_z1 | 8.85 / 28.6% | **5.74 / 15.8%** (band 2.45 / 7.5%) |
| flipH_chr000_p96f1 | 0.76 / 0.5% | **0.26 / 0.1%** |
| afhk_pat_chr000_p72_t2 | 3.29 / 10.4% | **2.71 / 7.6%** |
| afhk_pat_chr019_p62_t7 | 1.65 / 3.3% | 1.48 / 2.9% |
| mbtl_flipV_chr001_p157f2 | 3.57 / 11.3% | 3.55 / 11.3% |
| flipV_chr000_p383f1 | 1.61 / 2.3% | 1.62 / 2.7% |
| pral2_chr025_p133f1 | 2.75 / 4.7% | 2.72 / 4.4% |

- **flipHV at zoom 1** improved 9.20 → 5.74 over rounds 6 and 6b, but is still above ctrl_chr019_p0_z1 (1.17 / 2.0%).
  At zoom 1.5 it is 6.17 against a 3.10 control. Still open.

## Round 7 (2026-10-15): probes, no render change
No render change this round. Every lead tried either matched what the editor already does or made the cases worse.
So the shots in `editor_bw` are unchanged and every number below equals round 6b. The experiments used env switches
in throwaway builds; none of them are in the tree.

| case | round 6b | round 7 | control |
|---|---|---|---|
| flipHV_chr019_p153 | 6.17 / 17.7% | 6.17 / 17.7% (band 2.74 / 7.2%) | ctrl_chr019_p0 3.10 / 7.4% |
| flipHV_chr019_p153_z1 | 5.74 / 15.8% | 5.74 / 15.8% (band 2.45 / 7.5%) | ctrl_chr019_p0_z1 1.17 / 2.0% |
| afhk_cg_chr020_p435_t27 | 2.22 / 2.2% | 2.22 / 2.2% (band 2.01 / 2.0%) | ctrl_chr020_p0 1.29 / 0.6% |
| other .pat / CG cases | round 6b table | unchanged (no code change) | |

### Part draw order (uni2 1.42, IDB renamed and saved)
- **What the game does.**
  - `PatPartSet_BuildDrawOrderByPriority` 0x4C19E0 builds the part set's draw list at +0x164 (count at +0x160).
  - It holds (slot, PRPR) pairs, `std::sort` ascending by PRPR (comparator `PatPartDrawOrder_LessByPriority`
    0x4C1CC0). That sort is a stable insertion sort for 32 or fewer parts.
  - PRPR is stored at slot +4 by the loader (0x4A7989).
  - `Han6Draw_DrawPatLayerParts_3D` 0x4BF9A0 walks the list in order. It queues each part through
    `PrimQueue_LinkPrimIntoBucket` 0x4BC570, which links at the bucket head.
- **Ascending order is worse.** Drawing ascending in the editor makes flipHV much worse: z1 5.74 → 18.62, zoom 1.5
  6.17 → 23.03. The dark PPPA-0xD6 band (slot 4, cut-out 56) ends up on top. So the editor's highest-PRPR-first
  order is what reaches the screen, which fits the head insertion reversing the list.
- **Tie order makes no difference.** Reversing ties (slots 1/2 at PRPR 40, 4/6 at PRPR 45) gives exactly the same
  numbers, so draw order is ruled out for the fan.

### flipHV texture source
- **The texture matches.** PPTP 1 is PGST 1 `tsu_ef00.dds`: 2048², DXT5, PGTE 0x00080008. The cut-outs are
  51 (384,0) 96×176, 52 (480,0), 54 (624,0) 48×192 and 56 (288,0), in /256 units × 2048, so 1 texel per quad unit
  before PRZM.
- **Both sides decode DXT5 on the GPU.** The editor uploads the DXT5 blocks as they are
  (`glCompressedTexImage2D`, no mips, no resize). The game samples the same blocks through D3D9 on the same GPU.
  A decoder rounding difference is therefore not expected.
- **Not minification.** The 3D rotation does not turn these quads edge-on (|cos Y| 0.77, cos X 0.94), so the fan is
  magnified about 1.5× across and 4× along the streak.
- **Game-only content in the measured region.** The capture has a game-only blue/white arc and a cyan streak near
  the lower fan, at z1 (300..330, 75..140) and the blue blob in the diff panel. They are not part of pattern 153 and
  add a small fixed error.
- **Axis lines are not noise.** The editor's axis crosshair is already corrected in the metric (`sheet.py`
  AXIS_FIX).

### p435: rotation and pivot are already at the minimum
Probes on the CG layer matrix (AFRT=false path), against the 2.22 / 2.2% baseline:

| probe | result |
|---|---|
| AFAZ + 0.0002 turn (0.07°) | 3.18 |
| AFAZ ± 0.001 turn | 7.4 |
| pivot ± 0.5 px in x or y | 13–14.6 |
| AFHK weight ± 0.006 | 11.4 |
| neighbouring ticks 26 / 28 | 27.5 / 27.1 |

- **The geometry is right.** Rotation angle, pivot and lerp weight are all at a sharp minimum, so a pixel-centre
  pivot shift is ruled out.
- **The remaining gap is not geometry.** p435 is 2.22 against a 1.29 floor; with the ±1 px band it is 2.01. What is
  left sits on the sprite edges and in the aura ring (no glow 4.33, sq glow 3.70 on the ring).
- **Next experiment.** Test the glow model (ring radius and falloff, power), not the transform.

### Still open (flipHV)
- **Inside the stretched streak bands.** The residual is the line structure inside the bands. The line angles agree,
  but the lateral position and thickness of the fine lines differ.
- **Best next experiment.** Capture the game with a single streak prop (freeze at set 45 and zero PRCL alpha on all
  other slots through gamecap poke). Compare one prop against the editor's single-prop render, then check the u
  mapping across the width (mirrored or offset on the back-facing Y = 220° quads) and PRZM applied before or after
  PRAS.
