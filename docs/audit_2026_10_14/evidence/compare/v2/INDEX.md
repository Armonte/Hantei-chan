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

## Round 8 / render-match8 (2026-10-15): arc segment triangle split
- **Single-prop isolation.** `tools/uni/s1_compare/pat_solo_prop.py` writes a chr019.pat whose part set 45 keeps one
  slot on screen (the others moved to PRXY 30000,30000 in place: set 45 has no PRCL to zero). The game loads it as a
  UNI2-IM loose-file override (`gamecap_scripts/v2r8_chr019_solo{,1,2,3,4,8}.gcs`, file_put, removed at quit); the
  editor renders the same file from `gamedata/uni2/data/chr019solo<k>` (cases `flipHV_chr019_p153_solo<k>_z1`).
  Files and diff panels: `round8/`.
- **Result.** Every streak quad (slots 1, 2, 3, 4, 6; Y 220°, PRZM up to 2 x 5) already matched: 0.21-0.42 mean,
  0.0-0.1% px. So u mirroring on back-facing quads and the PRZM/PRAS order are ruled out (IDA confirms both:
  PatPart_BuildTransformMatrix 0x4C10E0 = S·Rz·Rx·Ry·T, _PRASPath 0x4C1580 scales then adds PRAS). The whole residual
  was slot 8, the type-4 arc (shape 91: radius 400, width 160, **3 segments over 288°**): 29.45 / 61.1%.
- **Cause.** Each arc segment is a trapezoid, so the triangle split shows in the texture. uni2
  CPAniPrimRender_DrawQuads 0x4C0F10 queues the segment's corners as inner-cur (u0,v0), outer-cur (u1,v0),
  inner-next (u0,v1), outer-next (u1,v1); the game splits along (u1,v0)-(u0,v1). The editor split along
  (u0,v0)-(u1,v1). The split was established by measurement (solo8 29.45 → 0.28); the index/strip setup of the
  batch renderer was not traced.
- **Fix.** `src/parts/parts.cpp` kSplitX/kSplitY on the ARC branch, UNI2 only (`DrawParams::uni2ArcSplit`, set from
  the layer's patApplyPras flag in `render.cpp`). With the split MBTL flipV p157 (also an arc) went 3.55 → 4.62, so
  MBTL keeps the old split. Ring / sphere stay unchanged (no case isolates them).
- **Also.** Arc shrink is now geometric as in the game (CPAniPrimRender_Donuts_Setup 0x414590 / _NextEdge 0x4148D0:
  one matrix scales x/y by 1 - (dR/r)/N per segment, width included): `partrender::ArcEdgeScale`. No measured case
  has dR != 0, so no number moved.
- **IDB.** Donuts setup/edge/corner getters, PrimQueue_DrawLinePrimUP 0x4AC190, RenderBatch_LockVBIB 0x410B30 renamed
  and commented; saved.
- **Numbers.** `tools/uni/s1_compare/v2_round8.sh [BWDIR]` (pass the old editor_bw to compare).

| case | round 7 | round 8 | control |
|---|---|---|---|
| flipHV_chr019_p153 | 6.17 / 17.7% (band 2.74 / 7.2%) | **3.20 / 10.1%** (band 1.88 / 4.4%) | ctrl_chr019_p0 3.10 / 7.4% |
| flipHV_chr019_p153_z1 | 5.74 / 15.8% (band 2.45 / 7.5%) | **0.32 / 0.1%** (band 0.31 / 0.1%) | ctrl_chr019_p0_z1 1.17 / 2.0% |
| flipHV solo slot 8 (arc), z1 | 29.45 / 61.1% | **0.28 / 0.0%** | |
| flipHV solo slots 1/2/3/4/6, z1 | 0.37 / 0.34 / 0.34 / 0.21 / 0.42 | unchanged | |
| flipV_chr000_p383f1 (`--aura-fit`, plate) | lit 3.26 / 9.2% | **lit 2.64 / 6.1%** | |
| mbtl_flipV_chr001_p157f2 | 3.55 / 11.3% | 3.55 / 11.3% (unchanged: MBTL keeps the old split) | .pat-only 4.55-4.83 |
| mbtl_pups_chr011_p309, afhk_pat_chr019_p62_t7, pras_chr020_p5f2, rect_chr024_p76 | | within ±0.02 / unchanged | |
| afhk_cg_chr020_p435_t27 | 2.22 | not worked this round (no change to its path) | 1.29 |

- **flipHV is at its floor:** the z1 shot is below its control (0.32 vs 1.17). At zoom 1.5 it is 3.20 against 3.10,
  and lower in % with the band.
- **Open.** p435 glow-ring model (round 7 next experiment). Also MBTL's arc split: MBTL flipV keeps the old split.
  Check MBTL's DrawQuads corner order (MBTL_25646393 IDB) before changing it.

## Round 9 / render-match9 (2026-10-15): the UNI2 chara Bokashi pass; MBTL arc split from code
No editor (C++) change this round. The p435 gap was a game shading pass that the measurement model left out, not a
render difference, so the fix is in `v2_model.py`. Numbers: `tools/uni/s1_compare/v2_round9.sh` (log `round9.log`).
Evidence: `round9/`.

### p435: where the residual was
- `tools/uni/s1_compare/v2_decomp.py` splits the lit residual by editor coverage and by distance to the edge.
  - On p435, 74% of |d| sits in the interior, 3 or more px from any edge; it is not on the edges.
  - The game is brighter there: about +1.3 / +2.4 / +2.4 RGB on average.
- **The glow ring is not the cause.** The aura glow (`CharaAura_DrawGlow` 0x59D910) is drawn under the doll. Its
  0x10 alpha cannot reach opaque pixels, so `--glow sq` leaves the in-footprint number at exactly 2.22. A glow drawn
  over the doll is ruled out by the signs: it would add +10 G and +16 B to the dark pixels (lerp: R −7), but the dark
  pixels measure only +1.4.
- **What the error looks like.** For a single flat palette colour (74,72,73; 1147 px), the game value varies with a
  standard deviation of 5.7. Pixels near the white eye/ear areas are brighter, and pixels next to the dark outline are
  darker (`round9/p435_palette74_game_minus_pred.png`). That pattern is a blur of the sprite's own colours added on
  top of it.

### Cause: TecCharaDrawBgColorBokashi_V
- **Where the game draws it.** `CgLayer_DrawWithCharaShader` 0x4BE140 draws each CG layer a third time with
  `TecCharaDrawBgColorBokashi_V` (UNI2 Shader/sh_chara.txt `PS_CharaDrawBgColorBokashiV`).
- **What the pass computes.**
  - It averages pow(pal × vcol, g_Power 4) over the opaque samples of a 9×9 texel window. g_BlurRadius is 40.5, so
    all 81 samples are used.
  - Output alpha is `g_fCharaBgBokashiAlpha`, taken from stage_bokashi_alpha.img byte 0. Training stage 1 = bg001,
    which is 20/255 at every column.
  - On a transparent source pixel the alpha is multiplied by count/81.
- **Model.** `v2_model.py --bokashi 0.0784` adds the pass (additive) on top of the lit prediction (`sheet.POST_ADD`).
  `grad_fit` subtracts the same add from the control before it calibrates the stage gradient.
- **The alpha is the stage's value, not a fit.** A free fit on p435 gives 0.08 (r = 4), and bg001's 20/255 is
  0.0784. Additive fits; alpha-blend made both cases worse (`round9/bok.py`).

| case | round 8 | round 9 (`--bokashi 0.0784`) | floor |
|---|---|---|---|
| ctrl_chr020_p0 (r3) | 1.29 / 0.6% (band 1.27) | **0.71 / 0.0%** (band 0.59) | is the floor |
| **afhk_cg_chr020_p435_t27** (r3, `--glow sq`) | 2.22 / 2.2% (band 2.01) | **0.67 / 0.2%** (band 0.25) | 0.71: **at the floor** |
| afhk_cg_chr020_p435_t27 (old `_unlit` capture) | 5.54 / 5.5% (band 2.06) | 4.00 / 3.6% (band 0.30) | |
| afhk_cg_chr020_p437_t2 (r3, `--glow sq`) | 2.31 / 4.4% | **0.30 / 0.0%** | 0.71 |
| pras_chr020_p5f2 (r3, `--glow sq --grey`) | 4.30 / 8.5% (band 1.63) | 3.84 / 8.6% (band 1.08) | |
| ctrl_chr027_p0 (r5) | 4.67 / 10.2% (band 2.29) | **4.20 / 9.4%** (band 1.69) | is the floor |
| pups_chr027_p449f4 (r5) | 3.81 / 12.9% (band 2.97) | **2.46 / 3.1%** (band 1.58) | 4.20: below |
| round-8 list (.pat cases, MBTL) | | unchanged (same shots, no bokashi) | |

- **Glow ring.** The ring metric of p435 is unchanged at no glow 4.33 → glow model 3.70; `lin` gives 4.33 and `sqblur`
  3.74. That ring sits outside the footprint and does not enter the 2.22. The pass also puts a faint (count/81) halo
  just outside the sprite, which the ring metric does not model yet.
- **Use `--bokashi 0.0784` on every UNI2 CG case** captured on training stage 1. .pat layers do not get the pass.
- **The editor does not draw stage shading.** It has no BgColor, specular or bokashi pass, and that is by design: the
  editor shows the raw sprite.

### MBTL arc split (MBTL_25646393, read-only; no names or comments written)
- **MBTL's arc code matches UNI2's.** Every piece of the arc path below is the same in both games:
  - The Donuts setup, edge step and advance.
  - The corner getters and the UV assignment.
  - The batch index pattern.
- **So both games split along (u1,v0)-(u0,v1).** Both turn the 4-vertex prim into the triangles (0,1,2),(3,2,1).
  MBTL's DrawQuads difference is CPU-side: it pre-transforms the corners to XYZRHW with a perspective divide and
  queues them as command type 1 (FVF 0x1C4). UNI2 uses command type 2.
- **The measured regression is real.** Rebuilt with the UNI2 split forced on for MBTL (throwaway probe, reverted), the
  case gives:
  - mbtl_flipV_chr001_p157f2: 3.55 → 4.62 (auto-refine moved +1,0), or 3.55 → 6.55 with `--refine 0`.
  - mbtl_pups_chr011_p309: unchanged at 0.64.
- **Conclusion.** The old diagonal is not what the game draws. The p157 residual has another arc-related cause that
  the old split partly hides. Next: a solo-prop isolation of the p157 arc in MBTL (the round 8 method, through MBTL-IM
  loose-file override), before switching MBTL to the split.

| MBTL address | current name | suggested name |
|---|---|---|
| 0x4A31B0 | PatPart_DrawPrimitive_DispatchByShapeType | (keep) |
| 0x4A3480 | sub_4A3480 | CPAniPrimRender_DrawQuads (pre-transformed XYZRHW, command type 1) |
| 0x410F60 | sub_410F60 | CPAniPrimRender_Donuts_Setup |
| 0x410ED0 | sub_410ED0 | CPAniPrimRender_Donuts_GetSegmentCount |
| 0x410EE0 / 0x410F00 / 0x410F20 / 0x410F40 | sub_* | CPAniPrimRender_Donuts_GetCorner0_InnerCur / 1_OuterCur / 2_InnerNext / 3_OuterNext |
| 0x4112A0 / 0x411330 | sub_* | CPAniPrimRender_Donuts_NextEdge / _AdvanceEdge |
| 0x4106E0 | sub_4106E0 | CPAniPrimRender_GetCutoutUvFlipped (PRRV bit 1 / 2 flips u / v) |
| 0x410910 / 0x410AF0 / 0x411430 / 0x4117C0 | sub_* | CPAniPrimRender_Rect / Ring / Sphere / Corn_Setup |
| 0x48D260 | RenderList_Execute | (keep); its table 0x8A1E80 holds the per-command-type batchers |
| 0x48DA80 | sub_48DA80 | RenderList_BatchPrim_Quad4Tri3Line2 |
| 0x40DBA0 | sub_40DBA0 | RenderBatch_AppendQuad_Idx012_321 |
| 0x40DBF0 | sub_40DBF0 | RenderBatch_AppendTri_Idx012 |
| 0x40DD30 | sub_40DD30 | RenderBatch_Flush (DrawIndexedPrimitive) |

- **UNI2 IDB (renamed, commented, saved).**
  - Renamed: PrimQueue_ExecuteBucketList 0x4AB7F0, PrimQueue_BatchPrim_Quad4Tri3Line2 0x4ABEA0,
    RenderBatch_AppendQuad_Idx012_321 0x410CC0, RenderBatch_AppendTri_Idx012 0x410D10,
    RenderBatch_AppendVertsIndices 0x410BE0.
  - Commented: the Bokashi pass on CgLayer_DrawWithCharaShader 0x4BE140.
- **Checks.**
  - The round-8 list in `round9.log` reproduces the round 8 numbers.
  - Its last line, `ctrl_chr019_p0` at 15.48, is already that value with the round 8 tools. The line has no stage
    gradient and no bokashi model, and chr019 p0 is a CG sprite, so it is not a regression.
  - Full ctest: 67/67 passed (`ctest_r9.log`, `--timeout 900`).

## Round 10 / render-match10 (2026-10-15): MBTL arc split, metric registration, pras layers
Numbers: `tools/uni/s1_compare/v2_round10.sh` (log `round10.log`: every line with the round-9 registration and with
`--exact-reg`). Full ctest 67/67 (`ctest_r10.log`, `--timeout 900`).

### mbtl_flipV_chr001_p157f2: single-prop isolation
- Part set 21 holds four arcs (VEST type 4, radius 256, 24 segments): slots 4/5/6 (widths 64/56/40, thin-line
  texture, PRRV 0) and slot 8 (width 100, 54%, **PRRV 2**, PRAL 1, PRCL 0x99), plus planes 7/11/21/22/24.
- Solo files: `pat_solo_prop.py chr001.pat 21 8` and `... 21 4,5,6` (`tools/gamecap/round10/chr001_solo21_s8.pat`
  sha256 453f517c..., `_s456.pat` 72ecf84f...; 50 MB each, not committed, regenerate with the script). Game:
  `gamecap_scripts/v2r10_mbtl_chr001_solo_s{8,456}.gcs` (MBTL-IM `Mods/data/chr001/chr001.pat` override, removed at
  quit). The s8 window plate came out black: measure both against `_game_plate_bb.png`. Editor: cases
  `mbtl_flipV_chr001_p157f2_solo_s8 / _s456` (`gamedata/mbtl/data/chr001solos8`, `chr001solos456`).

| probe | old split | game split (0,1,2),(3,2,1) |
|---|---|---|
| solo slot 8 (PRRV 2 arc) | 1.73 / 4.6% | **1.06 / 2.5%** (refine 0: 1.73 → 1.32) |
| solo slots 4/5/6 | 12.17 / 54.6% (band 4.91) | 12.10 / 54.3% (band 4.91): neutral |
| full p157, round-9 registration | 3.55 / 11.3% | 4.62 / 17.9% |
| full p157, `--exact-reg` | 5.11 / 19.9% (band 4.34) | **4.62 / 17.9%** (band 4.22) |
| full p157 `--exact-reg`, split on slot 8 only / on 4,5,6 only | | 4.63 / 5.11 |

- **Why the game split measured worse.** Not the render. `sheet.game_vs_model` mapped the editor crop (footprint
  bbox + 8 px) onto a rounded game-pixel size, so the sub-pixel registration and scale moved whenever the footprint
  bbox moved. The split changes slot 8's faint footprint (~135 px), which re-registered the thin-line arcs 4/5/6; a
  sub-pixel shift scan of the s456 solo shot swings 10.4 → 33.7 over ±0.75 editor px. The old 3.55 was crop luck.
- **Metric.** `v2_model --exact-reg` resamples from the exact editor-space box of the integer game window (the camera
  geometry alone sets the registration; the shot is padded with P 0 / Q 1 at its edges). Opt-in: it is not better
  everywhere (ctrl_chr027_p0 4.20 → 7.14, pups_chr027_p449f4 2.46 → 3.35, flipHV_chr019_p153 3.20 → 4.63 but band
  1.88 → 1.50), so those round-9 numbers carried the same crop luck the other way. ctrl_chr020, p435, p437 and the
  z1 / solo flipHV cases are identical either way.
- **MBTL code (IDB renamed/commented, saved).** No other arc difference: CPAniPrimRender_DrawQuads 0x4A3480 divides
  by w and sets rhw = 1/w (perspective-correct, as UNI2's type 2); corners inner-cur (u0,v), outer-cur (u0+du,v),
  inner-next, outer-next as in the editor; no UV bias on these shapes; segment count is the VEST int, angle per
  segment length/10000 · 360/N, shrink geometric (as round 8); GetCutoutUvFlipped 0x4106E0 = the editor's FlipUv.
  New names: CPAniPrimRender_GetUvU0 0x4107E0, _GetUvVCur 0x4107F0, _GetUvDu 0x410800, _GetUvDvStep 0x410810,
  CPAniPrimRender_Donuts_ScalarDeletingDtor 0x411370.
- **Fix.** `src/parts/parts.cpp`: the arc uses the game split for every uniDialect game (UNI2 and MBTL);
  `DrawParams::uni2ArcSplit` is gone. MBAACC keeps the old split (not traced). `tool_windows_impl.h` now sets
  `patUniDialect` on its layers (it left the default true, so MBAACC would have taken the split there).
- **Floor.** With `--exact-reg` the MBTL .pat-only controls are p238f10 4.29 / 9.9%, p277f0 4.75 / 14.5%: p157 at
  4.62 / 17.9% is at the floor on the mean. mbtl_pups_chr011_p309 0.64 (0.92 with --exact-reg).

### pras_chr020_p5f2
- A mixed frame: a CG layer (gets the Bokashi pass) and a .pat layer (does not). Round 9 applied `--bokashi` to the
  whole footprint. With `--layers pras_chr020_p5f2_cg,pras_chr020_p5f2_pat` the add now comes from the CG layer only
  (`sheet.POST_ADD_SRC`, through Q_pat when the .pat is on top; here CG is over .pat): 3.84 → 3.83, so the .pat leak
  was negligible.
- With `--exact-reg`: **2.43 / 5.0% (band 0.83 / 0.6%)**; the round-9 crop had moved it by (0,-1). Floor
  ctrl_chr020_p0 0.71 / 0.0% (band 0.59), unchanged by `--exact-reg`.
- **What is left** (`v2_decomp.py`, now with a CG / .pat split under `--layers` + `--bokashi`): 1886 px (3%) with
  |d| > 20 carry 49% of the residual, in 550 scattered 1–16 px components.
  - CG-partial pixels (577 px, mean 42.5, 15% share; game −13 R): CG sprite edges over the cyan .pat flame.
  - CG interior (mean 2.8): rotated/zoomed CG edge texel choice (as p435 in rounds 6/7).
  - .pat-only pixels: mean 1.63 (at the floor).
- **UNI2 IDB** untouched (read-only this round); no new addresses. Lead for the CG-over-.pat edge:
  CgLayer_DrawWithCharaShader 0x4BE140 (CG edge alpha cut / sampling) vs Han6Draw_DrawPatLayerParts_3D 0x4BF9A0.

### Next
- Decide whether `--exact-reg` becomes the default; if so, recheck ctrl_chr027 / pups_chr027 / flipHV at zoom 1.5
  with a sub-pixel registration fit instead of the integer refine.
- pras: the CG edge alpha cut on a CG-over-.pat edge.

## Round 11 / render-match11 (2026-10-15): control-registered metric, zoom-1 shots
No editor (C++) change this round. Numbers: `tools/uni/s1_compare/v2_round11.sh` (log `round11.log`). Each case is
printed three ways: round-9 registration, `--exact-reg`, and control-registered. Full ctest 69/69 (`ctest_r11.log`,
`--timeout 900`).

### Metric: the control sets the registration
- **`v2_model --subpix-fit OUT.json`** (run on a control).
  - The registration is the camera geometry (`--exact-reg`, no integer refine) plus a bounded sub-pixel offset,
    |dx|, |dy| ≤ 1.5 game px.
  - The offset is the one that minimises the control's own lit residual. The search is coarse to fine: steps of
    0.5, then 0.125, then 1/32 game px.
- **`v2_model --subpix OUT.json`** (or `--subpix dx,dy`) applies that offset unchanged to the control's cases. It
  implies `--exact-reg --refine 0`, so a case cannot pick its own best shift.
- **Code.** `sheet.SUBPIX` shifts the exact editor-space box. `v2_model.main` became `measure()` (it resets the
  sheet globals on each call) plus `subpix_fit()`.
- **Fitted offsets** (`round11/reg/*.json`):

| control | zoom | offset (game px) |
|---|---|---|
| ctrl_chr019_p0 (r5) | 1.5 | +0.125, −0.375 |
| ctrl_chr027_p0 (r5) | 1.5 | +0.25, −0.375 |
| mbtl_patctrl_chr001_p238f10 (nb) | 1.5 | +0.25, 0 |
| ctrl_chr020_p0 (r3), ctrl_chr019_p0_z1, ctrl_chr027_p0_z1, ctrl_chr024_p0 | 1 | 0, 0 |

- **What the offsets show.**
  - Zoom 1.5 shots need about +¼, −⅜ game px, and the two independent UNI2 r5 controls agree.
  - Zoom 1 needs none: the camera geometry is exact. At sc = 1 the area resample is pixel-exact, so only whole-pixel
    offsets could register, and the fit picks 0.
  - **Cross-check:** ctrl_chr027_p0 at zoom 1.5 with its offset gives 1.45, and the same control shot at zoom 1 gives
    1.46. pups f4 gives 1.89 both ways.
- **Default.** Report the control-registered number (this round's `three` third line) for every case. Where a zoom-1
  shot exists, prefer it: it needs no sub-pixel registration at all. `--exact-reg` on its own is retired for
  zoom-1.5 cases.
  - The offset is per zoom and per session. rect_chr024_p76 (zoom 1.5) against the zoom-1 chr024 control is
    invalid (19.22). Use rect_chr024_p76_z1.
  - MBTL is reported on raw (as before). Its fit minimises the lit line, which for MBTL with the identity light
    differs from raw. A raw-objective fit is a follow-up.

| case | old (round-9 reg) | exact-reg | **new (control-registered)** | floor (its control, new) |
|---|---|---|---|---|
| ctrl_chr020_p0 | 0.71 | 0.71 | **0.71** | is the floor |
| afhk_cg_chr020_p435_t27 | 0.67 | 0.67 | **0.67** | 0.71: at floor |
| afhk_cg_chr020_p437_t2 | 0.30 | 0.30 | **0.30** | 0.71: at floor |
| pras_chr020_p5f2 (zoom 1.5, `--layers`) | 3.83 | 2.43 | 2.43 | 0.71 |
| **pras_chr020_p5f2_z1** (new zoom-1 shot) | | 1.32 | **1.32** | 0.71: +0.61 (see below) |
| ctrl_chr027_p0 | 4.20 | 7.14 | **1.45** | is the floor |
| ctrl_chr027_p0_z1 | | 1.46 | **1.46** | is the floor (zoom 1) |
| pups_chr027_p449f4 | 2.46 | 3.35 | **1.89** | 1.45: +0.44 |
| pups_chr027_p449f4_z1 | | 1.89 | **1.89** | 1.46: +0.43 |
| ctrl_chr019_p0 | 3.56 | 5.04 | **1.58** | is the floor |
| flipHV_chr019_p153 | 3.20 | 4.63 | **3.65** (band 1.83) | 1.58 (CG control; the zoom-1 pair is the fair one) |
| ctrl_chr019_p0_z1 | 1.58 | 1.58 | **1.58** | is the floor |
| flipHV_chr019_p153_z1 | 0.32 | 0.32 | **0.32** | 1.58: below |
| flipHV_chr019_p153_solo8_z1 | 0.28 | 0.28 | **0.28** | below |
| ctrl_chr024_p0 | 0.86 | 0.86 | **0.86** | is the floor |
| rect_chr024_p76 (zoom 1.5) | 11.62 | 12.65 | (19.22: zoom mismatch, invalid) | |
| rect_chr024_p76_z1 | | 0.33 | **0.33** | 0.86: below |
| mbtl_patctrl_chr001_p238f10 (raw) | 4.85 | 4.29 | **3.40** | is the floor |
| mbtl_patctrl_chr001_p277f0 (raw) | 4.89 | 4.75 | **1.66** | control |
| mbtl_flipV_chr001_p157f2 (raw) | 4.62 | 4.62 | **4.46** | 3.40 / 1.66: +1.06 over p238 |
| mbtl_flipV_chr001_p157f2_solo_s8 (raw) | 1.07 | 1.06 | **1.03** | below |
| mbtl_flipV_chr001_p157f2_solo_s456 (raw) | 12.10 | 12.26 | 12.10 (band 5.03) | thin-line arcs |
| mbtl_pups_chr011_p309 (raw, chr001 offset: no chr011 control) | 0.64 | 0.92 | **0.59** | below |

- **The rounds 9–10 swings were crop luck.**
  - ctrl_chr027 went 4.20 → 7.14 → 1.45, and pups 2.46 → 3.35 → 1.89. Both controls, chr019 and chr027 at
    zoom 1.5, now sit at about 1.5 (the old 4–7 "floors" were misregistration).
  - pups_chr027_p449f4 was "below its floor" in round 9. It is now correctly measured as +0.44 above it.

### pras_chr020_p5f2: the CG edge was the 1.5× shot resample
- **Draw order is right.** `DrawObj_DrawLayer` submits each AF layer immediately, in layer order: .pat goes to
  `Han6Draw_DrawPatLayerParts_3D`/`_2D`, CG goes to `CgLayer_DrawWithCharaShader`/`_NoCharaShader`. A pat-over-CG
  composite built from the per-layer shots measures 21.91, against 2.43 for CG-over-pat, so the editor's order
  matches the game.
- **No edge alpha cut.** `PS_CharaDrawBgColorV` (sh_chara.txt) point-samples the index texture and returns 0 where
  palette alpha == 0. That is the editor's rule.
- **Zoom 1 fixes the CG edge.** A zoom-1 shot (`pras_chr020_p5f2_z1` plus `_z1_cg` / `_z1_pat` layer shots;
  `shoot_batch.sh` now resolves LAYER_SHOTS bases from cases_control.txt too) takes the case from 2.43 to **1.32**.
  The CG-visible pixels are at **0.65**, at the 0.71 floor. The CG-edge-over-flame residual of round 10 was the 1.5×
  editor-shot resample of hard CG edges against the bright flame. It was not a render difference.
- **What is left (.pat-only, mean 2.0): a smooth R-only add over the saturated additive flame.**
  - The flame has P_G = P_B = 255, Q = 0, P_R = 0, Q_R = 1.
  - Where it is saturated, game − prediction is R +3.2 at the edge of the saturated area, rising to +5.0 at a depth
    of 16 px. It decays over about 6 px outside (+3.2, then +1.5, then 0). G and B are clipped, so they read 0.
  - That profile is a blur-like add, not a per-part colour. Not the cause:
    - `StageBloom_Apply`: stage only, drawn in `Stage_DrawFrame` before the fighters.
    - The aura glow: its colour 0x00AAFF has no R.
    - The bokashi pass: CG only.
  - **Open.** Find a post-fighter screen pass in UNI2 (bloom/glare over the battle target) or rule it out. Do not
    change the editor until it is found.
- **pups_chr027_p449f4_z1 (+0.43).** The interior has a uniform R bias of +4.4 (G +0.8, B +1.0) over 118k px. The
  edge pixels (474 px, mean 40) are darker in the game. The R bias has the same sign and size as the pras flame
  add. That suggests the same unknown pass, so investigate the two together.

### UNI2 IDB (renamed, commented, saved)
- **Renamed:**
  - AfLayer_BuildTransformMatrix_Interp 0x4BDE20
  - CgLayer_LookupTextureChip 0x4BF1B0
  - Han6Draw_DrawPatLayerParts_2D 0x4C01D0
  - CgLayer_Draw_NoCharaShader 0x4BED20
  - ShaderCmd_QueueSetTextureParam 0x4AE840
  - ShaderCmd_NewEffTag 0x4AF400
  - ShaderCmd_PushEffTag 0x4AF2D0
- **Commented:** DrawObj_DrawLayer (layer order, measured), CgLayer_DrawWithCharaShader 0x4BE140 (round 11
  finding), StageBloom_Apply (stage only).

### Next
- UNI2: find the R add over saturated additive .pat (pras flame, pups interior). Start from the battle-frame draw
  after the fighters (callers of Stage_DrawFrame / Battle_FrameUpdate 0x52740B).
- MBTL p157: refit the chr001 offset on raw. Its +1.06 over p238 is the remaining MBTL gap.
- Reshoot the remaining zoom-1.5 cases at zoom 1 (flipHV_chr019_p153 is already done as `_z1`).
