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

## Round 12 / render-match12 (2026-10-15): the "R add" was the metric, MBTL thin arcs at zoom 1
No editor (C++) change this round: all three open gaps were measurement errors. Numbers:
`tools/uni/s1_compare/v2_round12.sh` (log `round12.log`, control-registered line only, offsets refitted into
`round12/reg/`) and `v2_round12_mbtl_z1.sh` (log `round12_mbtl_z1.log`). Full ctest 69/69 (`ctest_r12.log`, `--timeout 900`).

### pras_chr020_p5f2: the glow model darkened R under the additive flame
- **Not a game pass.** UNI2 1.42 has nothing after the fighters that would add red over them:
  - `Stage_DrawFrame` renders the stage into its own MSAA target and does bloom and FXAA there (`StageBloom_Apply`,
    the `g_BiassR/G/B` bloom). Battle_FrameUpdate's call to it comes after the chara loop, but only to composite the
    stage RT at priority 250.
  - `Stage_SubmitScreenTintQuads` 0x52B3D0 (new name) draws full-screen tint quads and the chara-light darkening quad.
    These alpha-blend, so they would pull G and B down over the flame; they cannot add R.
  - `sh_paniblur.txt` TecCharaPtBlurDrawW/H (`CharaPtBlur_DrawBlurWH_Passes` 0x87F9A0) is a 9-tap blur of .pat parts.
    It runs only for parts with blend byte 3 (`PatPart_PrepareDraw_ColorSpecBlendMatrix` 0x4150E0, 2D path).
    The flame is PRAL 1, so this pass is not it either.
- **What the metric did.** `--glow sq` built the glow silhouette from the whole frame's 1 − Q. With `--grey`, the
  saturated cyan flame clips G and B on the 64-grey shot, so its 1 − Q came out at about 2/3 instead of 0. The model
  therefore put the 0x16-alpha glow lerp (towards 00AAFF) under the flame, which lowered the predicted R by
  a·R_plate, about 4 to 5. G and B were already clipped at 255, so only R showed the error. Evidence, on the
  saturated-flame px (11.6k):
  - Glow on the full silhouette: game − pred R = **+4.13**, and it rises with plate R (3.6 / 3.9 / 4.8 at plate R
    20–40 / 40–60 / 60–90). That is the a·R signature.
  - No glow: **−0.44**.
  - Glow silhouette from the CG layer only: **−0.12**.
- **Model.** `v2_model --glow-sil cg` (with `--layers`) builds the glow silhouette from the CG layer shot alone. Additive
  parts carry no alpha coverage. Commented on `CharaAura_DrawGlow` 0x59D910.
- **What is left on pras_z1, .pat-only px only.** Mean 1.6, signed about 0. It grows with the local gradient
  (|d| ≈ 0.2 × |∇|). It is not a shift: the best ±0.75 px shift is 0,0. It is not blur either: the game and the
  prediction have the same gradient energy, and blurring either one makes it worse. So this is texel-level detail in
  the flame texture sampling, with no post pass involved.

### pups_chr027_p449f4: the "+4.4 R" was a stage-gradient fit biased by clipped px
- `grad_fit` least-squares fitted the stage chara gradient (base + height·t) on every control footprint px. 14% of
  ctrl_chr027_p0's R channel is game-clipped (≥ 254), where the add cannot show. Those px pulled the R base down to
  2.4. The unclipped px of the control itself read R +3.9, and pups read +4.4, so this was the same bias, not a
  pups-specific pass.
- **Fix (metric).** `grad_fit` now leaves out px with game ≥ 254, per channel. The chr027 R base goes 2.4 → 7.9. It
  agrees with a fit on pups itself (7.7) and with chr020's 8.4. Every CG control drops, because they all had some
  clipped px.

### mbtl_flipV_chr001_p157f2: thin-line arcs were the 1.5× shot resample
- **Raw refit.** `v2_model --subpix-obj raw` (new) on p238f10 gives the same offset as round 11: +0.25, 0. So p157
  stays at 4.46 at zoom 1.5. The round-11/12 scripts pass `"$L0"` as one argument, so the MBTL lit line used the stage
  light JSON, not the identity. raw is unaffected. `v2_round12_mbtl_z1.sh` passes `--light` correctly (lit = raw).
- **Where it is.** `v2_split_by_shot.py` (new) splits a case's residual by the coverage of other shots with the same
  framing. At 1.5, the thin-line arcs (slots 4/5/6, solo s456 coverage, 12.9k px) carry 73% of p157's raw residual
  (mean 8.97); the rest is 1.86. The s456 solo case (12.10) has the right energy (game/pred 1.03) but is sharper in the
  game: gradient energy is +13% and the peak is 217 vs 180. Blur makes it worse and the best shift gives only 9.1. That
  is the resample softening thin lines, as with the pras CG edges in round 11.
- **Zoom-1 shots** (new rows in cases_control.txt, `*_z1`, against the same `_nb` game captures, camera geometry 0,0):

| case | zoom 1.5 (raw) | **zoom 1 (raw)** |
|---|---|---|
| mbtl_patctrl_chr001_p238f10 (control) | 3.40 | **3.25** |
| mbtl_flipV_chr001_p157f2 | 4.46 | **1.28** (s456 px 0.71, rest 1.48) |
| mbtl_flipV_chr001_p157f2_solo_s456 | 12.10 | **0.75** |
| mbtl_flipV_chr001_p157f2_solo_s8 | 1.03 | **0.79** |
| mbtl_patctrl_chr001_p277f0 | 1.66 | invalid (0.00: the editor window falls outside the capture, so the game side is the P pad) |

### Table (control-registered; zoom-1 rows preferred)

| case | round 11 | **round 12** | floor (its control, round 12) |
|---|---|---|---|
| ctrl_chr020_p0 | 0.71 | **0.28** | is the floor |
| afhk_cg_chr020_p435_t27 | 0.67 | **0.63** | 0.28: +0.35 |
| afhk_cg_chr020_p437_t2 | 0.30 | **0.27** | 0.28: at floor |
| pras_chr020_p5f2 (zoom 1.5) | 2.43 | 2.12 | 0.28 |
| **pras_chr020_p5f2_z1** | 1.32 | **1.01** | 0.28: +0.73 (.pat flame texel detail, see above) |
| ctrl_chr027_p0 / _z1 | 1.45 / 1.46 | **0.49 / 0.50** | is the floor |
| pups_chr027_p449f4 / _z1 | 1.89 / 1.89 | **0.34 / 0.59** | 0.49 / 0.50: below / +0.09 (474 edge px) |
| ctrl_chr019_p0 / _z1 | 1.58 / 1.58 | **0.28 / 0.28** | is the floor |
| flipHV_chr019_p153 (zoom 1.5) | 3.65 | 3.65 | (no gradient; use _z1) |
| flipHV_chr019_p153_z1 / _solo8_z1 | 0.32 / 0.28 | **0.32 / 0.28** | 0.28: at floor |
| ctrl_chr024_p0 | 0.86 | **0.27** | is the floor |
| rect_chr024_p76_z1 | 0.33 | **0.33** | 0.27: at floor |
| mbtl_patctrl_chr001_p238f10 (raw) | 3.40 | 3.40 (z1 **3.25**) | is the floor |
| mbtl_flipV_chr001_p157f2 (raw) | 4.46 | 4.46 (z1 **1.28**) | 3.25: below at zoom 1 |
| mbtl_pups_chr011_p309 (raw) | 0.59 | 0.59 | below |

### UNI2 IDB (renamed, commented, saved)
- Renamed: CharaPtBlur_DrawBlurWH_Passes 0x87F9A0, CharaPtBlur_BeginMaskTarget 0x87FBA0, CharaPtBlur_Callback_Blur
  0x87FC50, CharaPtBlur_RegisterCallbacks 0x87FC70, PatPart_PrepareDraw_ColorSpecBlendMatrix 0x4150E0,
  PatPartBlurCallbacks_SetSlot 0x4C0BB0, g_PatPartBlurCb_Blur 0xA54CA8, g_PatPartBlurCb_Begin 0xA54CAC,
  Stage_SubmitScreenTintQuads 0x52B3D0.
- Commented: CharaAura_DrawGlow (round-12 finding), the three functions above. MBTL IDB untouched (not needed).

### Next
- pras_z1 .pat flame (+0.73 over the new 0.28 floor): texel-level. Compare the flame texture's sampling (filter,
  UV bias, mip) per part, e.g. a solo-part shot of slot 237.
- afhk p435 is now +0.35 over the lower floor: recheck it with `--glow-sil cg` style care (it is CG only) and look at
  the glow ring.
- MBTL p277 zoom-1 registration (window outside the capture); reshoot MBTL pups_chr011 at zoom 1.
- Fix the `"$L0"` quoting in v2_round11/12.sh (MBTL lit line).

## Round 13 / render-match13 (2026-10-15): pras flame solos, MBTL zoom 1, the L0 quoting
No editor (C++) change this round. Numbers: `tools/uni/s1_compare/v2_round13.sh` (log `round13.log`, offsets refitted
into `round13/reg/`). Full ctest 69/69 (`ctest_r13.log`, `--timeout 900`).

### pras_chr020_p5f2_z1: the .pat layer matches; the residual is an extra animated game object
- **Set 991 props** (the pras .pat layer, AFGX layer 0): additive point-sampled slots 1,2 (PRID 921, no PRFL); additive
  PRFL 1 slots 4,5,14,15,27,28; normal-blend slots 10,25 (PRCL alpha 0). "Slot 237" in the round-4 note is not a set-991 slot.
- **UNI2 sampler state** (IDB): `PatPart_SetBlendState_PRAL` 0x413900 sets `g_2DFilterMode` 2 (LINEAR) for PRFL == 1,
  else 1 (POINT); `D3D_SetTexFilterFromTable` applies MIN/MAG only (no mip filter); `sh_chara` `TecCharaPtDraw_V`
  samples `TexSample_EmptyNF` (no sampler_state, so the device state rules) and returns 0 where the texel alpha is 0.
  The UV bias globals are now named (`g_PartUvBiasU0/U1/V0/V1`); `D3D_QueryDeviceCaps_SetAmdPartUvBias` 0x4B0480 swaps
  them only on AMD (VendorId 0x1002). Everything matches `src/parts` (PRFL filter, plane-quad bias). No render change.
- **Solo shots** (`pat_solo_prop.py`, UNI2-IM loose override, `gamecap_scripts/v2r13_chr020_solo_{s12,slin,s1025,snone}.gcs`;
  editor `gamedata/uni2/data/chr020solo<k>`, cases `pras_chr020_p5f2_solo_<k>_z1` + `_cg`/`_pat` layer shots):

| solo | lit | .pat-only px |
|---|---|---|
| s12 (point-sampled additive 1,2) | **0.42** | 0.66 |
| slin (PRFL 1 additive) | **0.41** | 0.55 |
| s1025 (normal blend, alpha 0: editor draws nothing) | **0.28** | (none) |

- **`pat_solo_prop.py` fix.** Moving a prop to PRXY 30000 is not enough for PRA3-rotated props: UNI2 still projected
  slots 27/28 on screen (first s12 shot: 1.29, B +8.7). KEEP mode now also zeroes PRZM.
- **The extra object.** With *no* set-991 prop left (`snone`) the game still draws blue/cyan arcs near the hat; they
  are hidden by hidefighters (character-owned), have no hitboxes in the IM view, and move between captures. A fresh
  full capture (`v2r13_chr020_full.gcs`, `pras_chr020_p5f2_r13`) gives **0.88**, and `v2_split_additive.py` (new: splits
  the residual by where each additive solo shot draws) puts 71% of it on the s12-only px (mean 4.16, the arcs) while
  slin-only / both / neither sit at 0.28 / 0.38 / 0.40. Against the round-3 capture the same split is s12 1.17,
  slin 1.63: the residual moves with the arcs, not with a prop. The "texel-level detail" of round 12 was this object.

### afhk_cg_chr020_p435_t27 (0.63 vs 0.28)
- 4421 px. 8 px (0.2%) with |d| > 5 carry 28% (the rotated-CG texel choice of rounds 6/7); the rest is a uniform
  +0.2..+0.3 interior bias (stage-gradient extrapolation from the idle control to the airborne sprite). Band 0.21 is
  below the floor. The glow ring is outside the footprint (ring 7161 px: no glow 4.33, glow model 3.70). No change.

### MBTL
- **p277_z1 is not a registration failure.** Game = prediction exactly (raw 0.00 over 9876 px, game ≠ plate there by
  71/px); a ±1 px subpix gives 5.2-6.5. The part is an unrotated opaque point-sampled quad at scale 1, so zoom 1 is
  pixel-exact. Round 12's "window outside the capture" was wrong.
- **mbtl_pups_chr011_p309_z1** (new zoom-1 editor shot against the `_nb` capture, camera geometry 0,0): **0.14**.
- **`"$L0"`**: v2_round10/11/12.sh passed `--light 1,0;1,0;1,0` as one argument, so v2_model never saw `--light`. L0 is
  now a bash array (`"${L0[@]}"`). Effect: MBTL lit = raw again; the UNI2 r5 lines are unchanged (their gradient
  calibration dominates); the chr019_r5 offset refits to +0.25, −0.375 (was +0.125, −0.375), same numbers.

### Table (control-registered; zoom-1 rows preferred)

| case | round 12 | **round 13** | floor |
|---|---|---|---|
| ctrl_chr020_p0 | 0.28 | **0.28** | is the floor |
| afhk_cg_chr020_p435_t27 | 0.63 | **0.63** | 0.28 (8 rotated-texel px = 28%; band 0.21) |
| afhk_cg_chr020_p437_t2 | 0.27 | **0.27** | at floor |
| pras_chr020_p5f2_z1 (r3 capture) | 1.01 | **1.01** | 0.28: extra arc object, not the .pat layer |
| pras_chr020_p5f2_z1 (r13 recapture) | | **0.88** | non-arc px 0.28-0.40 |
| pras solos s12 / slin / s1025 | | **0.42 / 0.41 / 0.28** | at floor |
| ctrl_chr027_p0 / _z1 | 0.49 / 0.50 | **0.49 / 0.50** | is the floor |
| pups_chr027_p449f4 / _z1 | 0.34 / 0.59 | **0.34 / 0.59** | 0.49 / 0.50 |
| ctrl_chr019_p0 / _z1 | 0.28 / 0.28 | **0.28 / 0.28** | is the floor |
| flipHV_chr019_p153_z1 / _solo8_z1 | 0.32 / 0.28 | **0.32 / 0.28** | at floor |
| ctrl_chr024_p0 / rect_chr024_p76_z1 | 0.27 / 0.33 | **0.27 / 0.33** | at floor |
| mbtl_patctrl_chr001_p238f10_z1 | 3.25 | **3.25** | MBTL .pat control |
| mbtl_patctrl_chr001_p277f0_z1 | invalid | **0.00** | MBTL .pat control (pixel-exact) |
| mbtl_flipV_chr001_p157f2_z1 | 1.28 | **1.28** | below p238 |
| mbtl_flipV solo s8 / s456 _z1 | 0.79 / 0.75 | **0.79 / 0.75** | |
| mbtl_pups_chr011_p309 / _z1 | 0.59 | **0.59 / 0.14** | below |

### UNI2 IDB (renamed, commented, saved)
- Renamed: D3D_QueryDeviceCaps_SetAmdPartUvBias 0x4B0480, PartUvBias_SetAmdValues 0x4B11B0, PatPart_DrawPlaneQuad_2D
  0x4C0430, PatPart_DrawByShapeType_2D 0x4C08F0, CPAniPrimRender_Ring_Setup 0x414120, g_PartUvBiasU0 0x9A3C5C,
  g_PartUvBiasU1 0xA0361C, g_PartUvBiasV0 0x9A3C58, g_PartUvBiasV1 0xA0374C.
- Commented: PatPart_SetBlendState_PRAL (round-13 solos), the two new functions, the four bias globals.

### Next
- Identify the blue arc object on chr020 pattern 5 (character-owned, hidden by hidefighters, no boxes, animates
  per session): find what draws it (chara effect / aura particles / pattern-start script) and either hide it in the
  harness or mask it out of the pras comparison. Until then pras is at the floor off the arcs.
- MBTL p238f10 (3.25 at zoom 1) is now the outlier among MBTL .pat controls (p277 0.00): split it by prop.

## Round 14 / render-match14 (2026-10-16): the pras "extra object" is set-991's ring props; MBTL p238 is the HUD
No editor (C++) change this round. Numbers: `tools/uni/s1_compare/v2_round14.sh` (log `round14.log`; offsets reused
from `round13/reg`). Every round-13 line is unchanged (identical `: raw` lines). Full ctest 69/69 (`--timeout 900`).

### pras_chr020_p5f2: the blue arcs are set-991 slots 41/44/45, and they change from run to run in the game
- **Round 13 missed three slots.** Set 991 also has slots **41, 44, 45**: additive PRID 947/952/936, cut-outs on
  tex 3 with shapes 47/51 (type 4, donut) and 38 (type 3, ring), tilted in 3D (PRA3 x/y non-zero), alpha 0x99/0x33/0x99.
  41 and 44 have **no PRZM**, so `pat_solo_prop.py` could only move them to PRXY 30000 and could not collapse them.
  Every round-13 solo (s12/slin/s1025/snone) therefore still had them in the game. That is why "snone" still showed arcs.
- **Not a spawned effect.** Pattern 5 (しゃがみ中攻撃, `Mv_Atk_CroB`, an empty table in chr020_mv_0.txt) spawns nothing
  before frame 3 (`EF type 9` at frame 3 only). Frame 2 has no EF.
- **Proof.** `round14/chr020_991_norings.pat` is the original file with only slots 41/44/45 at PRCL alpha 0
  (`cases/v2r14_chr020_norings.gcs`, capture `pras_chr020_p5f2_norings`). In that capture the arcs are gone. On the
  game side the ring props are the only source of the arcs.
- **Runtime-varying.** Two runs of the same ring-only file (`round14/chr020_solo991_srings.pat`, `v2r14_chr020_solo_srings{,_b}.gcs`)
  differ by 5013 px. Only the thin arcs (and the timer-driven aura glow on the hat) differ; the plates are identical.
  snone vs s1025 (byte-identical 41/44/45 data) differ the same way. The ring setup
  (`CPAniPrimRender_Ring_Setup` / `_Donuts_Setup`) and the AFHK weight (`DrawObj_DrawLayers` → `Easing_ApplyCurve_AFHK`)
  have no timer, so the time source is still unknown. Candidates not yet excluded: `CharaObj_DrawAfterimages` (it
  redraws history records through `DrawObj_DrawLayers`) and the aura timer.
- **The editor draws them as wide soft bands.** Editor shot `pras_chr020_p5f2_solo_srings_z1_pat` (new case row).
  Sign flips of PRA3 x/y/z do not turn them into thin arcs. Where only the editor draws them, the game agrees: the
  game draws the same soft band there, and **removing them from the full game frame takes lit from 0.88 to 6.40**.
- **Mask (metric, documented).** `v2_mask_rings.py` (new) masks out the union of where the editor draws 41/44/45 and
  where the game frame differs from the norings capture by more than 12, dilated by 2 px. Split on the r13 capture:

| px set | px | mean | share |
|---|---|---|---|
| all footprint | 63496 | 0.98 | 100% |
| editor-rings-only px | 24958 | **0.32** | 13% |
| game-rings-only px (thin arcs) | 1296 | 31.65 | **66%** |
| off the ring mask | 21647 | **0.27** | 9% |

  pras is at the floor everywhere except the 1296 thin-arc px, which are runtime game output the editor cannot
  reproduce statically.

### MBTL mbtl_patctrl_chr001_p238f10_z1 (3.25): the MAGIC CIRCUIT HUD draws over the tray
- Set 275 has three props: 4 (additive glow), 5 (opaque tray, rot 0.75), 18 (translucent, tilted x 0.879). Editor-only
  solo shots (`mbtl_patctrl_chr001_p238f10_solo_s{4,5,18}_z1`, data `gamedata/mbtl/data/chr001solo`, other slots
  PRID −1) were fed to `v2_split_by_shot.py`. The tray (s5) carries 67% at mean 7.52. Its best shift is 0,0 (7.43; ±1 px
  14.6-19.9), so this is not registration.
- `v2_mbtl_hud_split.py` (new): within the opaque tray footprint, 767 game px equal the plate. These are the MAGIC CIRCUIT
  gauge (bottom-left HUD, which is in the plate) drawn on top of the tray, in rows 123-137 of the window. The model
  composites the tray over the plate, so the gauge is "missing" there. Rows from 123 down: 10473 px at mean 7.61
  (70%). **Off the HUD rows: 1.43.** This is the harness, not the renderer. Next: capture MBTL with the HUD hidden, or
  mask those rows.

### afhk_cg_chr020_p435_t27 (0.63): not decided this round (time cap)
- The round-13 split stands: 8 rotated-texel px carry 28%, ±1 band 0.21. Whether the uniform +0.2..0.3 is the
  gradient-fit extrapolation is still open. Next: fit the gradient on p435 itself, leaving out its own footprint ring,
  and compare that against the idle-control fit.

### Table

| case | round 13 | **round 14** | floor |
|---|---|---|---|
| ctrl_chr020_p0 | 0.28 | **0.28** | is the floor |
| afhk_cg_chr020_p435_t27 | 0.63 | **0.63** | 0.28 (open) |
| afhk_cg_chr020_p437_t2 | 0.27 | **0.27** | at floor |
| pras_chr020_p5f2_z1 (r13 capture) | 0.88 | 0.88; **0.27 off the ring mask** (editor-ring px 0.32) | at floor off the 1296 runtime-arc px |
| ctrl / pups chr027, chr019, chr024 rows | as round 13 | **unchanged** | |
| mbtl_patctrl_chr001_p238f10_z1 | 3.25 | 3.25; **1.43 off the HUD rows** | HUD occlusion (harness) |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | **0.00** | |
| mbtl_flipV_chr001_p157f2_z1 | 1.28 | **1.28** | |
| mbtl_pups_chr011_p309_z1 | 0.14 | **0.14** | |

### UNI2 IDB (renamed, commented, saved)
- Renamed: CPAniPrimRender_Sphere_Setup 0x414A60, CPAniPrimRender_Corn_Setup 0x414DF0, PatPart_SubmitPrimQueue_2D
  0x4C0BC0 (named from its call site; the body is not traced).
- Commented: Han6Draw_DrawPatLayerParts_2D (ctx layout: current/next set, AFHK weight), CPAniPrimRender_Donuts_Setup
  (the round-14 runtime arcs), CharaObj_DrawAfterimages. MBTL IDB not needed.

### Next
- Find the time source of the ring-prop arcs: watch slots 41/44 draw calls live (break in `CPAniPrimRender_Donuts_Setup`
  for PRID 947 and dump the matrix in two runs), or check `CharaObj_DrawAfterimages` records for P1.
- MBTL: recapture p238 with the HUD hidden (or mask the gauge rows in v2_model) so the control drops to about 1.4.
- afhk p435: the gradient-fit question above.

## Round 15 / render-match15 (2026-10-16): the ring arcs vary inside the game renderer; p435 has no uniform bias
No editor (C++) change. The round-14 numbers stand (same build inputs). Full ctest 69/69 (`ctest_r15.log`, `--timeout 900`). Logs: `round15_afhk_bias.log`; gamecap runs in
`tools/gamecap/round15/` (main checkout).

### pras set-991 ring slots 41/44/45: what makes them differ between runs
- **Probe at the grab** (`cases/v2r15_chr020_srings_probe.gcs` with `games/uni2_v2_case_aura_plate_probe.gcs`, two runs
  `_p1` / `_p2`). Identical in both runs: tick 1, subtick 0, speed 128, AFHK progress (+0x48) **0.0**, AFHK target
  (+0x40/+0x44) = pattern 5 frame 3, and aura timer 253. The arcs still differ by 5602 px.
- **Not UNI2-IM's pause/replay.** `cases/v2r15_chr020_srings_freeze.gcs` captures through gamecap's own freeze and the
  real backbuffer (`games/uni2_force_pattern.gcs`, no F5/F6, no harness grab). Its two runs (`pras_chr020_p5f2_srings_frz_{a,b}`)
  also differ by 3980 px on the arcs. So the variation happens inside UNI2's own renderer, not in the harness.
- **What the draw path reads** (traced this round): the AFHK weight comes from `Easing_ApplyCurve_AFHK(type, progress)`
  (animstate +0x1C / +0x10), and progress is the same in both runs. The ring and donut setups take only shape fields
  and that weight. `PatPart_QueueShapeSegments_2D` (body traced, see below) has no timer or global counter either. No
  timer, frame counter or afterimage input was found that differs. The afterimage pass (`CharaObj_DrawAfterimages`)
  redraws history records through `DrawObj_DrawLayers`, but it draws whole-object ghosts and these runs do not differ
  in history. Not proven either way.
- **Decision: the round-14 mask is the final answer** for these px. The editor draws the deterministic soft band and the
  game agrees with it there (0.32). The thin arcs change from run to run with byte-identical data and identical object
  state, so no static renderer can match them. The likely cause is uninitialised or stale state in the game's prim
  path; that would need a live debugger to confirm, and gamecap has no breakpoints.

### MBTL p238: HUD-free recapture not done
- MBTL has no harness HUD switch. A lead was found and named in the MBTL IDB: the tutorial script tag `@GAUGEVIEW@<n>`
  (`TutorialTalk_ParseTag_GAUGEVIEW` 0x7BC800 builds a `CTutorialTalk_ItemGAUGEVIEW`, item type 5) toggles the battle
  gauges. Its executor (the global it writes) is not traced yet. The HUD-row workaround of round 14 stays (1.43 off
  the gauge rows).

### afhk_cg_chr020_p435_t27: the +0.2..0.3 is not a gradient-fit bias
- `v2_bias_split.py` (new) prints the **signed** residual from the float prediction. It needed `sheet.game_vs_model.last_lpred`
  (new): `last_light[3]` is truncated to uint8, which adds a fake +0.48 to every channel.

| case (grad fitted on) | lit | signed R/G/B (footprint) | interior signed | without the 8 \|d\|>5 px |
|---|---|---|---|---|
| ctrl_chr020_p0 (itself) | 0.28 | +0.00 −0.00 +0.00 | +0.00 | 0.28 |
| p435 (idle control) | 0.63 | −0.05 −0.08 −0.11 | −0.09 −0.11 −0.14 | **0.43** |
| p435 (**fitted on p435 itself**) | 0.65 | +0.05 +0.05 +0.05 | −0.01 0.00 −0.00 | 0.44 |
| p437 (idle control) | 0.27 | +0.05 −0.11 +0.01 | | 0.27 |
| p437 (itself) | 0.26 | 0 0 0 | | 0.26 |

- There is no uniform signed bias (at most 0.1, and negative). Fitting the gradient on p435 itself does not lower it
  (0.65 vs 0.63). Removing the best constant makes it worse. So the extra 0.15 above the floor (after the 8 rotated-texel
  px) is unsigned per-pixel noise of the rotated, point-sampled CG (texel choice along rotated rows, rounds 6/7), not
  the metric. Round 13's "+0.2..0.3 interior bias" described |d|, not a signed offset. **No metric change.**

### Table vs floors

| case | round 14 | **round 15** | floor / note |
|---|---|---|---|
| ctrl_chr020_p0 | 0.28 | **0.28** | is the floor |
| afhk p435 | 0.63 | **0.63** (0.43 without 8 rotated-texel px) | texel noise, not metric |
| afhk p437 | 0.27 | **0.27** | at floor |
| pras_z1 (r13) off ring mask / editor band px | 0.27 / 0.32 | **0.27 / 0.32** | at floor; arcs nondeterministic in game |
| MBTL p238_z1 | 3.25 (1.43 off HUD rows) | **same** | HUD recapture pending |
| other rows | | unchanged | |

### IDBs (renamed, commented, saved)
- UNI2: `PatPart_SubmitPrimQueue_2D` → **`PatPart_QueueShapeSegments_2D`** 0x4C0BC0 (body traced: per segment it allocates
  a prim, transforms the 4 corners by the part matrix with the part perspective, divides by w on the CPU, stores 1/w,
  zeroes z, sets UVs and colours, links the prim into its bucket), `PrimQueue_AllocPrim` 0x413110. Comment added to
  `CPAniPrimRender_Donuts_Setup` (the round-15 freeze-capture result).
- MBTL: `TutorialTalk_ParseTag_GAUGEVIEW` 0x7BC800, `CTutorialTalk_Item_ScalarDeletingDtor` 0x440840.

### Next
- MBTL: trace the CTutorialTalk_ItemGAUGEVIEW executor (vtable 0x9E17C8 users, item type 5) to the gauge-visible
  global, `hold` it in `mbtl_to_training.gcs`, recapture p238 and drop the HUD-row split.
- pras arcs: a live debugger session (a breakpoint in `CPAniPrimRender_Donuts_Setup` for PRID 947, then dump the 4
  corners from `PatPart_QueueShapeSegments_2D` in two runs) would show which input differs.

## Round 16 / render-match16 (2026-10-16): MBTL captures without the battle HUD
No editor (C++) change. Numbers: `tools/uni/s1_compare/v2_round16.sh` (log `round16.log`). Full ctest 69/69 (`ctest_r16.log`, `--timeout 900`).

### The gauge-visible flag (MBTL IDB, renamed and saved)
- `TutorialTalk_ParseTag_GAUGEVIEW` 0x7BC800 builds an item of type 5 whose value is the tag number. The runner
  **`TutorialTalk_ExecuteItems`** 0x7BB0B0 (`switch` on item +4) handles case 5:
  - value ≠ 0 → **`BattleHudFader_Show`** 0x5463E0 (fade in from alpha 0, or straight to alpha 10000).
  - value 0 → `*g_BattleHudFader`: state +8 = 2 (hidden), alpha +16 / +20 = 0, +24 = 0, +28 (i16) = 0.
- **`g_BattleHudFader`** 0xAFC3C8 (was `g_BattleObjPtrAFB3E0_Unverified`) points at that fader. State +8: 0 shown,
  1 fading in, 2 hidden.
- **Harness.** `cases/v2r16_mbtl_chr00{1,11}_nohud.gcs` hold `[g_BattleHudFader]+8 = 2`, `+16 = 0` and `+20 = 0` after
  `mbtl_to_training.gcs`. Otherwise they are the round-5 case files (bloom 0, same-session plates). Every MBTL gauge
  (life, MAGIC CIRCUIT, portraits) is gone from both the shot and the plate. The training input/frame overlay text and
  the combo counter stay. Raw shots are in `captures/v2/nohud/`, copied as `*_nh_game*` next to the `_nb` ones.
- **The stage is random.** The `_nh` session landed on different stages (chr001: dojo, chr011: a fountain stage), and
  stage choice is not pinned (`CBtlStageTableMakeRandom` / `MakeBgNo`, not traced).

### MBTL rows (raw = lit, identity light)

| case | _nb (round 5) | **_nh (HUD hidden)** | note |
|---|---|---|---|
| mbtl_patctrl_chr001_p238f10_z1 (control) | 3.25 | **0.71** | the gauge was over the tray; the HUD-row split is no longer needed |
| mbtl_patctrl_chr001_p238f10 (zoom 1.5) | 3.40 | **1.08** | |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | **0.00** | pixel-exact |
| mbtl_patctrl_chr001_p277f0 (zoom 1.5) | 1.66 | **1.17** | |
| mbtl_flipV_chr001_p157f2_z1 | 1.28 | **1.05** | |
| mbtl_flipV_chr001_p157f2 (zoom 1.5) | 4.46 | **4.23** | thin arcs at 1.5 (round 12) |
| mbtl_pups_chr011_p309_z1 | **0.14** | 0.85 | `_nh` landed on the fountain stage; the residual is the animated spray (plate one step later). Keep `_nb` (no HUD over the footprint) |
| mbtl_pups_chr011_p309 (zoom 1.5) | **0.59** | 1.02 | same |
| mbtl_flipV solos s8 / s456 _z1 | 0.79 / 0.75 | not recaptured | own `.pat` captures |

- Zoom 1.5 uses the round-12 chr001 offset (+0.25, 0; same camera) for both captures. A refit on the `_nh` p238 alone
  gives +0.25, +0.125. That makes p277 (3.67) and p157 (6.61) worse, so the y term is ambiguous on p238 and the refit
  is printed for reference only.
- **Preferred rows from now on:** chr001 rows against `_nh`, chr011 against `_nb`. The MBTL .pat control floor is now
  **0.71** at zoom 1 (p238) and 0.00 (p277). The round-14 `v2_mbtl_hud_split.py` workaround is retired (kept for the
  `_nb` history).

### Next
- Pin the MBTL training stage (trace `CBtlStageTableMakeRandom` / the BgNo the training scene picks), recapture
  chr011 HUD-free on a still stage, and recapture the p157 s8/s456 solos HUD-free.
- What is left on p238 _nh at zoom 1 (0.71): split by prop again (`v2_split_by_shot.py` with the round-14 solo shots).

## Round 17 / render-match17 (2026-10-16): MBTL stage pinned, draw origin floored, p238 at its floor
No editor (C++) change. Numbers: `tools/uni/s1_compare/v2_round17.sh` (log `round17/round17.log`). Full ctest 69/69, exit 0 (`ctest_r17.log`, `--timeout 900`).

### Stage pin (MBTL IDB, renamed and saved)
- **The pin never worked because the symbol was misnamed.** 0xAFC04C is the stage index itself, not a pointer
  (`mov eax, ds:0xAFC04C` in `BattleStageSel_Get`; `mov ds:0xAFC04C, 5Ah` in the CSS). Renamed **`g_BattleStageSel`**
  (u32). The draft harness held `[g_pBattleStageSel]`, a write through the value, so the real field stayed 0.
- **0 = random.** `Scene15_VsScreen_Update` calls **`BattleStage_PickRandom`** 0x609DB0 (was sub_) when the field is 0.
  The CSS writes 0 or 90. **`Stage_ApplyVariantRedirect`** 0x7C11F0 can swap in a stage's alternate index (+524);
  **`Stage_IsAlreadyLoaded`** 0x4F2BC0 compares against **`g_LoadedStageName`** 0xAFC050.
- **Harness.** `games/mbtl_to_training_stage.gcs` holds `u32 g_BattleStageSel ${STAGE}` from before the CSS decide
  until the battle starts. Every run printed `stage_loaded = 21`: chr011, chr001 full and both solos (logs in
  `round17/`). Stage 21 is the night alley; it has no animated props. The earlier tries loaded 21 (STAGE 0, random)
  and 27 (STAGE 21 written through the value).
- **New cases** (copies in `gamecap_scripts/`): `v2r17_mbtl_chr011_stage.gcs`, `v2r17_mbtl_chr001_stage.gcs` (p238,
  p277, p0, p157) and `v2r17_mbtl_chr001_solo_{s8,s456}_stage.gcs`. All are HUD-free (g_BattleHudFader). Shots are in
  `captures/v2/nohud_st`, copied as `*_st_game*`.
- **Capture caveat.** `capture_window` (PrintWindow) sometimes returns an all-black image on these runs: p277/p0 and
  the p157 plate in the first chr001 run, then the s456 shot or its plate. The `_bb` backbuffer does not replace it:
  it differs from the window image in about 217k px. So the black sessions were re-run and only all-clean sessions
  were copied.

### MBTL draw origin: whole sprite px (MBTL IDB)
- **`Han6Object_Draw`** 0x597070 (commented) takes the draw origin as `this+0x10 + (obj x >> 7)`, `(obj y >> 7)`. That
  is world units floored to whole sprite px, before the camera / zoom matrix. The matrix uses `g_BtlCamera_RcpHalfW/H`
  (0xCABCFC / 0xCABD00, renamed) and `AnimState_FillDrawInterpCtx` 0x42A630 (renamed). UNI2's
  `CharaObj_DrawLayersWithLight` is the same code.
- **p238f10 is the only MBTL row with a fractional origin.** Its p1_y is −500, which floors to −4 px, so the origin y is
  636, not 636.094. `sheet.game_transform` (MBTL branch) now floors the same way (`sheet.MBTL_SNAP_ORIGIN`). The
  unfloored origin had resampled the prediction 0.094 px off, a bilinear blur that cost 4.66 on the opaque tray edges.
  The rows with p1_y 0 (p277, p157, p0, chr011) do not change.

### p238_z1 split by prop (`v2_split_by_shot.py`, SPLIT_EXCL=1: exclusive regions, edge = within 1 px of a boundary)

| region (editor solo shots s4 / s5 / s18) | round 16 (0.71) | **round 17 floored, _nh (0.26)** | _st (0.23) |
|---|---|---|---|
| s5 tray (opaque, rot 0.75) edge, 1447-1607 px | 4.66, 28% of the residual | **0.01** | 0.01 |
| s5 tray inner, 8.3-8.5k px | 0.87, 31% | **0.01** | 0.01 |
| s18 translucent only, inner, 19.8k px | 0.40, 32% | **0.36** (signed +0.05) | 0.32 (+0.03) |
| s18 translucent only, edge, 3.1k px | 0.44 | **0.40** (+0.12) | 0.30 (+0.08) |
| s4 additive glow | 252 px at 2.31 (blur bleed) | 0 px | 0 px |

- **What is left on p238 is the floor.** The opaque tray is pixel-exact (0.01). The last 0.23–0.26 sits on the
  translucent s18 plane and has no sign (|d| 0.3–0.4, signed ≤ +0.1, no px > 8). That is 8-bit rounding of an
  alpha-blended layer: the game blends into an X8R8G8B8 backbuffer (capture fmt 22), while the prediction
  P + Q·B is float from 8-bit editor shots. The split's `lit` column reads `last_light[3]`, truncated to uint8
  (+0.48, round 15), so the regions are reported on raw.

### Rows (raw = lit, identity light; zoom 1 at camera geometry 0,0, zoom 1.5 at the round-13 chr001 offset)

| case | round 16 | **round 17 _nh** | **round 17 _st (stage 21)** | floor / note |
|---|---|---|---|---|
| mbtl_patctrl_chr001_p238f10_z1 (control) | 0.71 | **0.26** | **0.23** | at floor: s18 8-bit blend rounding |
| mbtl_patctrl_chr001_p238f10 (zoom 1.5) | 1.08 | 1.68 | 1.67 | see below |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | 0.00 | **0.00** | pixel-exact |
| mbtl_patctrl_chr001_p277f0 (zoom 1.5) | 1.17 | 1.17 | 1.24 | 1.5× resample |
| mbtl_flipV_chr001_p157f2_z1 | 1.05 | 1.05 | **0.99** | +0.76 over p238: the slot-8 arc, below |
| mbtl_flipV_chr001_p157f2 (zoom 1.5) | 4.23 | 4.23 | 4.88 | thin arcs at 1.5 (round 12) |
| mbtl_pups_chr011_p309_z1 | 0.14 (_nb) / 0.85 (_nh, fountain) | 0.85 | **0.15** | floor; the round-16 0.85 was the fountain spray |
| mbtl_pups_chr011_p309 (zoom 1.5) | 0.59 (_nb) / 1.02 | 1.02 | **0.59** | |
| mbtl_flipV solo s8 _z1 | 0.79 (round 10) | | **0.83** | the slot-8 arc, below |
| mbtl_flipV solo s456 _z1 | 0.75 (round 10) | | not re-measured | thin-line arcs; every `_st` session so far had a black shot or plate (the last re-run was refused: an MBAA.exe from another session was running outside the lock) |

- **Preferred rows from now on:** `_st` for every MBTL row (HUD-free, same still stage). chr011 pups is back at its
  `_nb` value without the HUD, so the floor there is confirmed.
- **p238 at zoom 1.5 (1.08 → 1.67).** The shared chr001 1.5× offset (+0.25, 0) was fitted with the unfloored origin.
  On the floored p238, a refit gives +0.25, +0.25 (1.08; `round17/reg/mbtl_chr001_nh_snap.json`). p277 and p157
  (origin y 640) are best at y 0 (3.52 / 6.61 at +0.25). The y scan is flat in 1/8 px steps at 1.5×
  (y 0.125 = y 0.25), which matches the editor-shot resample grid. This is the round-16 "y ambiguous" result again,
  now with the game origin known exactly. It is the zoom-1.5 resample, not the game: the zoom-1 rows are exact.
- **p157_z1: 0.76 above p238, localised but not fixed.** With SPLIT_EXCL, the uncovered region carries 84% of the
  residual at **signed +1.26 ≈ |d| 1.27**, so the game is uniformly brighter there. The s8 solo game case shows the
  same thing on its own (inner 21.3k px, signed +1.28; free fit gain k 1.01–1.02). So the excess is the slot-8 arc
  (PRRV 2, PRAL 1, 54%, PRCL 0x99). The split listed it under "none" because an additive part has Q = 1, so 1 − Q
  coverage cannot see it. A ~1.5% gain on an additive part points at how MBTL turns the part's alpha % / PRCL into
  the vertex colour. **Next:** trace that conversion in `PatPart_DrawByShapeType` / the PRAL 1 blend state, and compare
  it with the editor's `54 / 100` and colour path.

### IDBs (saved)
- MBTL: `g_BattleStageSel` (was `g_pBattleStageSel`, now typed u32), `BattleStage_PickRandom`,
  `Stage_ApplyVariantRedirect`, `Stage_IsAlreadyLoaded`, `g_LoadedStageName`, `g_BtlCamera_RcpHalfW/H`,
  `AnimState_FillDrawInterpCtx`, plus a comment on `Han6Object_Draw`.
- UNI2: nothing kept. A comment that went to the wrong instance (the ida-mcp current target switched to UNI2 when
  another session opened it) was reverted and the IDB saved.

## Round 18 / render-match18 (2026-10-16): additive coverage in the split, stage-21 lamp flicker, s456 re-measured
No editor (C++) change. Numbers: `tools/uni/s1_compare/v2_round18.sh` (log `round18/round18.log`). Full ctest 69/69, exit 0.

### MBTL colour / blend path for an additive part (MBTL IDB, renamed and saved)
- `PatPart_SetupDraw_ShaderWorldMtx` 0x411C10: diffuse = `PatPart_LerpDiffuseColor_PRCL` (PRCL ARGB, byte lerp, float
  truncation), then `Color_ModulateARGB_Clamped` with DrawObj+0x0C only when that is not −1 (c·k/255, truncated, so it
  can only darken). Specular for PRAL 1 with no PRSP stays 0. The four vertices get that D3DCOLOR as is
  (`PatPart_SubmitPrimByShapeType` 0x4A2710, `CPAniPrimRender_SubmitShapeQuads` 0x4A37D0, `RenderList_AllocPrimEntry`
  0x49DBA0). The pixel shader is `out = tex·diffuse; out.rgb *= g_fPtLastColor (PPPA/255); out += spec`.
- Blend mode 2 (PRAL 1) in `g_BlendModeStateTable` 0xA1CB80: ADD, SRCALPHA, ONE. Filter 2 (PRFL 1) in
  `g_TexFilterStateTable` 0xA1CB44 via `D3D_SetTexFilterFromTable`: LINEAR mag / LINEAR min, no mip filter. The .pat
  DDS go through `Texture_CreateFromFileInMemory_NoMips` 0x492FA0 (MipLevels 1, filter NONE).
- **No 255 vs 256 divisor, no premultiply, no rounding gain.** The editor's `bgra / 255` vertex colour, its
  SRC_ALPHA/ONE blend and its PRFL linear filter are the same thing. Nothing in the path can give +1.5%.
- **MBTL's stage bloom ruled out.** `Stage_ScreenBloomPass` 0x4EEDA0 adds blur(pow(screen, power) · bias RGB) · alpha
  with blend 2 when `g_StageBloomEnabled` 0xA1CD1A is set (`Stage_ApplyBloomParams` 0x4EF3E0, from the stage's +0xB8..).
  Stage 21 is on: alpha 0.5, power 3.5, bias 0.4 / 0.3 / 1.3. That per-channel bias looked like the excess (B
  strongest). But with the flag held at 0 (`v2r18_mbtl_chr001_solo_s8_nobloom.gcs`), the game frame is the same as the
  `_st` frame (mean signed −0.001), so under MBTL-IM `BloomStrength 0` the pass draws nothing.

### The p157 / s8 excess is the stage-21 door lamp, not the arc
- Stage 21 is **not** still. The door lamp above the alley door flickers between an off state (probe 96) and several
  on states (124.8 / 127.2 / 129.6), and it lights the wall and floor at x 560–840, y 360–680 by up to +8 levels
  (`plate_b0 − plate` of the s8 session). The s8 / s456 arcs and the p157 slot-8 arc sit inside that pool.
- The game shot and its plate are different frames: window and backbuffer shots of one session are 3 frames apart
  (MBTL-IM frame counter 280 / 283, `round18/s456_st_win_bb_plate_montage.png`), and the lamp state differs
  between them. The energy gain of the s8 solo (sum (G − plate) / sum P) is R 1.011, G 1.015, **B 1.038**. It is
  channel-dependent, which an alpha or divisor error cannot be, and bluish, like the lamp light. Its sign follows
  the session: `_st` +0.82, the no-bloom session −0.63.
- `v2_lamp_match.py` fits `plate + λ·(plate_b0 − plate)` on the backdrop around the footprint. A single lamp map does
  not model it well: s8 0.83 → 0.98, p157 0.99 → 0.85 with the signed value flipping. So the lamp has more than one
  light pattern. Either way the excess is backdrop, not the editor's arc.

### Split tool: additive coverage
- `v2_split_by_shot.py` took coverage from 1 − Q, and an additive part has Q = 1, so it was never covered. Coverage is
  now max(1 − Q, P ≥ 2) from the shot's black-backdrop image. p157_z1 split by the s8 / s456 solos: s8 inner 20.9k px
  signed +0.98, s8 edge +0.61, s8+s456 +0.03 to +0.17. The "(none)" 5.2k px (+2.5) are the other additive props of set
  21 (slots 7, 21, 22, 24; no solo shots), also in the lamp pool. On p238 the s4 additive glow is now covered
  (23.6k px). Its exclusive regions are signed +0.02, so an additive part on a still backdrop is exact.

### s456 solo re-measured (stage 21, `round18/run_s456_stage.log`)
- The capture came out clean (no black frames). `capture_window` now retries an all-black PrintWindow result up to 8×
  and logs it (`round18/gamecap_printwindow_retry.diff`; gamecap lives outside the repo). This run needed no retry.
- Window pair (`_st`): **7.73**. The window game shot is a different frame from the backbuffer one (lamp 96 against
  129.6; −14 in the arc area) and sits at a different lamp state from its plate. Backbuffer pair (`_stbb` = `game_bb`
  + `plate_bb`, lamp 129.6 / 129.6): **0.72** (round 10: 0.75). That is the s456 number. On s8 the backbuffer pair
  is the bad one (4.23, lamp 96 / 127.2) and the window pair is good (0.83). Neither source is always right. **The pair
  to use is the one whose lamp states match.**

| case | round 17 | **round 18** | note |
|---|---|---|---|
| mbtl_flipV_chr001_p157f2_z1 | 0.99 | 0.99 | slot 8 / 7 / 21–24 excess = lamp flicker between game frame and plate; no engine gap found |
| mbtl_flipV solo s8 _z1 | 0.83 | 0.83 (`_st`), 2.29 (`_nbl`, other lamp phase) | same |
| mbtl_flipV solo s456 _z1 | not measured | **0.72** (`_stbb`, matched lamp) | at the round-10 floor |
| mbtl_patctrl_chr001_p238f10 (zoom 1.5) | 1.67 | 1.67 | not worked this round |

### Next
- **Get rid of the lamp, don't model it.** Either hold the stage-21 lamp object's state (find its animation in the
  stage script), or capture the game shot and the plate from one source with the lamp state matched (retry until the
  probe at x 690–750, y 295–325 agrees), or move the MBTL rows to a stage without a light source near the fighters.
- p238 at zoom 1.5: the zoomed-draw rule (camera scale about the floored origin) is still open. The offset is still
  the round-13 fit.

## Round 19 / render-match19 (2026-10-16): stage anime held, the p157 / s8 excess is the stage-21 shadow, zoom-1.5 rows are a resample
No editor (C++) change. Numbers: `tools/uni/s1_compare/v2_round19.sh` (log `round19/round19.log`). Full ctest exit 0.

### Stage-21 lamp: held through the stage anime clock (MBTL IDB, renamed and saved)
- The battle stage is one FBX scene (`Stage_LoadByIndex` 0x4F24F0: `bg/bg021/bg.fbx.bin/.json`, `g_StageFbxScene`
  0xA2FF60). `FbxScene_DrawNodeTree` 0x585DA0 takes each node matrix from its anime keys at
  `g_StageAnimFrame % keycount` (`g_StageAnimFrame` 0xA300A0 = scene+0x140; bg021 has 7999-key nodes).
- `Stage_RenderSceneAndAdvanceAnim` 0x4B0AA0 does `++g_StageAnimFrame` right before `FbxScene_Draw` 0x586A40 on every
  rendered frame where `Stage_GetAnimHoldState` 0x4F3160 >= 0. That is −1 only for `g_TrainingMode_UpdateFlag` /
  `g_BattlePauseState`, and MBTL-IM's pause sets neither. So the stage kept animating while a case was paused, and
  the window shot, the backbuffer shot and each plate caught different lamp frames.
- gamecap `hold i32 g_StageAnimFrame 0` (cases `v2r19_mbtl_chr001_{solo_s8,solo_s456,}_stagehold.gcs`, suffix `_sa`;
  diff `round19/gamecap_round19_cases.diff`). Every `_sa` shot and plate now has the same lamp probe (127.2). Before
  the hold the probe read 96 to 129.6 across the shots of one session.
- **The lamp was not the p157 / s8 excess.** With the lamp matched, p157_z1 stays at 0.99 and s8 at 0.83 against the
  window plate. The game shot's wall and floor pool (x 560–840, y 360–680) is the same in `_st` and `_sa` (mean
  difference −0.002). Round 18's "lamp pool" was `plate_b0 − plate`, and that difference is the shadow below.

### The excess is the stage-21 character shadow, which the window plate removes
- The residual sits entirely on y 600–680 under the fighters: signed RGB +6.4 / +6.5 / +15.5 on s8 and
  +4.3 / +3.4 / +8.4 on p157. That matches a bluish ellipse that `plate_b0` has and `plate` does not:
  +6.6 / +6.7 / +16.1 at the ellipse core (`round19/p157_sa_floor_shadow_game_b0_plate_bb_diffx8.png`, panels game,
  plate_b0, plate, plate_bb, diff ×8).
- `plate_b0` is taken with the P1/P2 draw-alpha override at 0 and no tick, so the fighters are gone but the shadow
  is still drawn. `plate` / `plate_bb` come one F6 tick later, and by then the shadow is gone too. The game shot has
  the shadow, so `plate_b0` is the right backdrop.
- Stage 21 in `bg/BgList.txt` (read by `Stage_LoadBgListParams` 0x4F38B0) has `ShadowLightType = 1`
  ("texture-colour shadow") and one `ShadowLightStatus` light. The PatPart path has a texture-shadow technique
  (`PatPart_SetupDraw_ShaderWorldMtx` draw mode 0x65 → `TecCharaPtDraw_ShadowV_TEX`). The link from
  ShadowLightType to mode 0x65, and why the shadow follows the override one tick late, are not traced.
- The editor does not draw MBTL's stage shadow. Use `--plate *_sa_game_plate_b0.png` for MBTL stage rows.

### p238 at "zoom 1.5": not a zoomed engine draw
- The game shots are at camera zoom 1.0 (sidecar `cam_f32[11]` = 1). The "zoom 1.5" rows take the editor's 1.5×
  raster and box-downsample it to the 1× game (sheet `sc = 0.667`). The same game shot is exact against the zoom-1
  editor shot (p277 0.00, p238 0.23). So the 1.67 / 1.24 / 4.52 are the 1.5×→1× resample, not MBTL. A refit (+0.25 /
  +0.25 on p238, but y 0 on p277 / p157) cannot fix a re-rasterisation.
- D3D9 half-pixel rule, checked in IDA: `CBtlCamera_UpdateMatrices` 0x509240 builds a plain `D3DXMatrixLookAtLH`
  (0xCABD08) and `D3DXMatrixPerspectiveFovLH` (0xCABD88). The chara-part VS (`Shader/sh_chara.txt`
  `Chara_Transform`) is world·view·proj, and `Han6Object_Draw` 0x597070 passes those two matrices as g_mView / g_mProj.
  There is no −0.5 px term, so D3D9's pixel-centre rule applies unchanged: pixel i is shaded at screen x = i. At 1×
  with integer-floored origins, a point-filtered texel sampled at an exact texel boundary is floor()ed to the same
  texel the editor draws, so the rule does not show at 1× (z1 rows pixel-exact). A zoomed game capture would be
  needed to see it. None exists, so no zoom model was added.

| case | round 18 | **round 19** | note |
|---|---|---|---|
| mbtl_flipV solo s8 _z1 | 0.83 (`_st`) | **0.01** (`_sa`, plate_b0) | was the stage shadow |
| mbtl_flipV_chr001_p157f2_z1 | 0.99 | **0.18** (`_sa`, plate_b0) | under the p238 control (0.23) |
| mbtl_flipV solo s456 _z1 | 0.72 (`_stbb`) | **0.60** (`_sa`, plate_b0); 0.72 window plate | thin-line arcs; below the round-10 0.75 |
| mbtl_patctrl_chr001_p238f10_z1 | 0.23 | 0.23 (all plates) | floor: s18 8-bit blend rounding (round 17) |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | 0.00 | pixel-exact |
| ~~mbtl_patctrl_chr001_p238f10 (editor 1.5)~~ | 1.67 | 1.67 | **retired in round 20**: 1.5×→1× resample of the editor shot, game at zoom 1 (see round 20) |

### Next
- MBTL stage rows: report `_sa` + plate_b0 (or recapture the plate before any tick). Optional: trace the
  ShadowLightType → draw mode 0x65 link and the one-tick shadow lag.
- Retire the MBTL "zoom 1.5" rows, or capture a real zoomed game frame (camera zoom ≠ 1) to test the D3D9 rule.

## Round 20 / render-match20 (2026-10-16): real camera zoom 1.5 captures; the fake zoom-1.5 rows are retired
No editor (C++) change. Numbers: `tools/uni/s1_compare/v2_round20.sh` (log `round20/round20.log`). Full ctest exit 0.
Gamecap case `tools/gamecap/cases/v2r20_mbtl_chr001_zoom15.gcs` (copy + run log in `round20/`), suffix `_z15sa`.

### MBTL battle camera zoom (MBTL IDB, renamed and saved)
- `g_CameraElements` 0xCABA90 is four 0x18-byte elements (+8 x, +0xC y, +0x14 zoom). `CBtlCamera_Update` 0x50B7F0
  (called each battle tick by `CBtlCamera_TickGatherObjectsAndUpdate` 0x508020, skipped on training-update /
  battle-pause) eases elem1's zoom `g_CamElem1_Zoom` 0xCABABC (sidecar `cam_f32[11]`) toward the half width that
  `g_CamElem2_ZoomTarget` 0xCABAD4 asks for (81920 / z), copies elem1 into elem0 (`g_CamElem0_RenderZoom` 0xCABAA4),
  applies the effect-camera lerps, then calls `CBtlCamera_UpdateMatrices` 0x509240. The matrices use
  elem0 zoom × `g_CamElem3_ZoomMul` 0xCABAEC (eye distance ∝ 1 / zoom: a dolly, not a viewport scale).
- The case holds all three zooms (elem0, elem1, elem2) at 1.5, so the ease has nothing to do and the tick's matrices
  see 1.5. MBTL-IM's pause only suppresses ticks, so the matrices refresh on the case's F6 step. Every shot: zoom0
  1.5 before and after, stage anime 0 (run log). Sidecars: `cam_f32[5]` = `cam_f32[11]` = 1.5.
- Side finding: `BtlCamera_IsZoomedIn` 0x58E590 and `Han6Object_Draw` pass `elem0 zoom > 1.0` as a byte into
  `Stage_BuildCharaBgColorParams` 0x58E230's param block (+4, read via the draw context +0x84 by
  `Han6Draw_DrawLayer`). Its consumer is not traced. The texture filter is not it: `D3D_SetTexFilterFromTable`
  0x48CD20 sets MAG/MIN from the part's PRFL only.

### Numbers (editor zoom 1.5 vs game, plate_b0, `--subpix 0,0`, refine found +0,+0 on all three)
| case | round 19 (fake: editor 1.5 → game zoom 1) | **round 20 (real game zoom 1.5)** | band ±1 (r19 → r20) |
|---|---|---|---|
| mbtl_patctrl_chr001_p238f10 | 1.67 | **3.12** | 0.56 → **0.27** |
| mbtl_patctrl_chr001_p277f0 | 1.24 | **3.50** | 0.39 → **0.03** |
| mbtl_flipV_chr001_p157f2 | 4.52 | **8.22** | 1.95 → 1.87 |
The raw mean is higher because the real 1.5× shot is compared at full resolution (no box filter averaging the edges
away; 1.8–2.3× more pixels in the footprint). The band ±1 numbers fall: on p277 everything but 0.03 is a 1-px edge.

### What remains: 1-px texel-edge lines (sheets `round20/*_z15sa_sheet.png`: game, model, |diff|)
- The residual is thin lines along part edges and along texel boundaries inside parts (p277 chair rungs/slats).
  That is the signature of point sampling under magnification: at 1.5× each texel covers 1.5 screen px, so texels
  land on 2, 1, 2, 1… pixels, and which pixel gets the extra texel depends on where the pixel is sampled.
- The plain D3D9 pixel-centre rule (no −0.5 term, pixel i shaded at screen i, round 19) predicts a uniform half-pixel
  shift against a GL raster. Tested as a prediction, not a fit: `--subpix -0.5,-0.5` → p238 3.95, p277 5.40, p157 7.40;
  `0.5,0.5` → 4.71 / 5.00 / 9.17. Both are worse than 0,0 on the controls (p238 / p277), so the residual is not a
  uniform half-pixel offset. p157 prefers y −0.5 (5.38) while the controls do not, so no single offset explains all
  three; nothing was fitted.
- Not traced yet: where each part's vertices land in sub-pixel terms at 1.5× (Han6Object_Draw floors the object
  position to whole sprite px, then the 1.5× dolly puts that origin on a 1.5-px grid, so the texel phase alternates
  with the object x), versus the editor's quad placement at 1.5×. That per-part phase, not a global shift, is the next
  thing to model from IDA. p157's PRRV thin-line arcs add on top (as at zoom 1, round 10).

### Retired: the MBTL "zoom 1.5" rows of rounds 4–19
The rows `mbtl_patctrl_chr001_p238f10` / `p277f0` / `mbtl_flipV_chr001_p157f2` measured against a zoom-1 game shot
(sidecar `cam_f32[11]` = 1) box-downsampled the editor's 1.5× raster to 1× (sheet `sc = 0.667`). They measured the
resample, not MBTL, which is how p238 sat at 1.67 while its z1 row was 0.23. They are retired: zoom-1 parity is the
`_z1` rows, zoom-1.5 parity is the `_z15sa` rows above.

## Round 21 / render-match21 (2026-10-16): the editor's world origin was half a pixel off at 1.5x; zoom 1.5 now at the zoom-1 floor
Editor (C++) change: `Render::BeginPass` (src/render.cpp). Full ctest 69/69, exit 0 (`round21/ctest_r21.log`). Numbers: `tools/uni/s1_compare/v2_round21.sh` (log
`round21/round21.log`; the same rows on the round-20 shots: `round21/round21_before.log`). The three zoom-1.5 editor shot
pairs in `editor_bw/` (`*_new_k/_g`) were re-shot with this build; the zoom-1 shots came out pixel-identical. No new game
capture: the round-20 `_z15sa` captures are used unchanged.

### The cause: the editor truncated its pan to whole world px before zooming
- `BeginPass` set the world pan as `x = (int)(originX / zoom)` and the world matrices draw `scale(zoom) * translate(x)`.
  With the shoot window's origin 632 that is `1.5 * 421 = 631.5`: the world origin, and every sprite px k at
  `631.5 + 1.5k`, was half a screen px left of the analysis origin (632, `sheet.KNOWN_ORIGINS`). The axis column
  moved 632 -> 631 at 1.5x (zoom-1 shots: 632). Exact registration (`--subpix`) then resampled the editor image by
  half a pixel, and the texel phase of every point-sampled magnified part was off by 0.5 px: the 1-px lines along part
  edges and texel boundaries of round 20.
- The engine (MBTL IDB, unchanged names): `Han6Object_Draw` 0x597070 floors the object to whole sprite px;
  `CBtlCamera_UpdateMatrices` 0x509240 zooms by moving the eye (1 / zoom) and its NDC->px matrix (+824) maps the camera
  centre to the fixed pixel (640, 360) at every zoom; there is no -0.5 px term (round 19) and no snapping of the camera to
  a zoomed grid. So sprite px k lands at `centre + zoom * k` with the centre on a whole pixel. p238/p277/p157 at 1.5x:
  origin (160, 634 / 640), whole pixels.
- Fix: the remainder `floor(origin) - zoom * x` (screen px) is a translate folded into `projection` in front of every
  world matrix (sprites, parts, axes, boxes). It is 0 at zoom 1 and at any integer zoom, so zoom-1 and zoom-2 images do
  not change. This replaces nothing fitted: the round-20 half-pixel tests shifted the resample (a blur), not the raster.

### Numbers (editor zoom 1.5 vs REAL game zoom 1.5 `_z15sa`, plate_b0, `--subpix 0,0`)
| case | round 20 | **round 21** | band ±1 (r20 -> r21) | zoom-1 `_z1` vs `_sa` (unchanged) |
|---|---|---|---|---|
| mbtl_patctrl_chr001_p238f10 | 3.12 | **0.24** | 0.27 -> 0.19 | 0.23 |
| mbtl_patctrl_chr001_p277f0 | 3.50 | **0.00** | 0.03 -> 0.00 | 0.00 |
| mbtl_flipV_chr001_p157f2 | 8.22 | **0.19** | 1.87 -> 0.18 | 0.18 |
| mbtl_flipV_chr001_p157f2_solo_s8 | - | - | - | 0.01 |
What is left at 1.5x is the zoom-1 floor: p238's s18 translucent residual (round 17) and p157's stage-21 shadow (round 19).

### The zoom > 1 flag (MBTL IDB, commented and saved)
`Stage_BuildCharaBgColorParams` 0x58E230 stores its a6 (`BtlCamera_IsZoomedIn` / `Han6Object_Draw`: elem0 zoom > 1)
at param block +4; the block goes to draw context +0x84 and from `Han6Draw_DrawLayer` into the CG sprite record +0x4C.
Its only reader is `Han6Draw_SubmitCgSprite_ParentMtx` 0x4A0C40: 0x4A0F68 picks `TecCharaDrawBgColorFilter_V` over
`TecCharaDrawBgColor_V` (colour pass 0) and 0x4A0F2A picks `TecCharaDrawBgColorSpecularFilter_V` over
`TecCharaDrawBgColorSpecular_V` (specular pass 1). So it changes the shader technique of zoomed-in CG sprites only.
.pat parts draw through `TecCharaPtDraw_V`, which has no Filter variant, so the three .pat cases here are not affected.
The Filter techniques' shader body is not in the repo yet; a CG-sprite row at zoom 1.5 is the place to measure it.

## Round 22 (2026-10-16): remainder closure
Docs only. No editor change, no build, no game run. Static trace in the MBTL IDB (renamed, commented, saved) plus
floor-strip statistics on the existing round-19 `_sa` captures. The `v2_round19.sh` rows reproduce unchanged
(s456 0.60 / band 0.58, p238 0.23 / band 0.21).

### The stage-21 character shadow path (MBTL IDB)
- `Stage_LoadBgListParams` 0x4F38B0 reads `ShadowLightType` into the BgList record at +0xFC (0x4F52F5; -1 = none,
  1 = texture colour), the `ShadowLightStatus` count (max 8) into +0x100, and the lights into +0x104 (0x1C bytes each).
  The record array was named `g_StageNames`. It is renamed `g_StageBgListRecords` 0xAFC170 (0x234-byte records).
- `Stage_HasCharaShadow` 0x4F1DC0 returns record+0xFC >= 0. `BattleProc_UpdateFrame` also copies that test into
  `g_CharaShadowRT_Enabled` 0xC7BE60 and calls `CharaShadowRT_RenderAndComposite` 0x4B6540 on the draw pass. That
  function renders the 1280x720 RT `g_CharaShadowRT` 0xC7BDF8, whose list is `g_CharaShadowRT_DrawList` 0xC7BE18, and
  draws it back.
- `Object_DrawWithStageShadow` 0x597B90 is the per-object draw. Its callers are `BattleProc_UpdateFrame` and
  `ObjectList_DrawAllWithShadows` 0x5371C0 (new function). It does three things:
  1. The main `Han6Object_Draw` 0x597070. Its alpha is the draw-alpha override obj+0x648 while obj+0x654 > 0
     (0x597CEC). That is what gamecap's plate hide holds at 0.
  2. Then the shadow pass. Gates: `Stage_HasCharaShadow`, stage index >= 0, `byte_C0B906`, a full-screen fade value
     (`Stage_GetMaxFadeOverlay` 0x5003E0, a guessed name) < 1, and the shadow option at `dword_3642AF8`+0x10AFC != 0.
     The mode is 1 for players (`PLAYER_DATA` vtbl+8 = `PlayerData_GetObjType_Ret1` 0x40D780). Other objects need
     `_ObjFlags_RenderShadow` 0x10000 in obj+0x7C. The pass draws the same object again into the shadow list, with:
     - alpha forced to 255 (0x5984F8), so it does **not** read obj+0x648;
     - shadow params {type = record+0xFC, y squash = +0x224, alpha = +0x228} and the per-light colour and x fade
       (`CharaShadow_ApplyStageShadowLight` 0x596D60, one draw per ShadowLightStatus light);
     - draw mode 100 + type in `Han6Object_Draw`, so type 1 gives 0x65 = `TecCharaPtDraw_ShadowV_TEX`. This is the
       ShadowLightType → mode-0x65 link left open in round 19.
  3. A third, reflection draw from `ShadowReflexColor` (+0x1E4..+0x1F0) when its strength is > 0.

### Why the plate_b0 extra is there and why it fades one step later
- The hide cannot remove the shadow pass. The plate hide only changes obj+0x648, and the shadow pass forces alpha 255.
  This is the engine-backed reason that P1/P2 vanish from `plate_b0` while their secondary draws stay.
- The p157 floor light is **not** P1's shadow silhouette. Floor-strip means (y 660–705, x 500–900, minus the p277
  `plate_b0`) on the existing `_sa` shots:

| case | game_bb | plate_b0 | plate_bb (one F6 tick) |
|---|---|---|---|
| p0f0 / p238f10 / p277f0 | 0.00 | 0.00 | 0.00 (b0 = bb pixel-identical outside the lamp) |
| p157f2 full | +4.9 / +8.1 / +6.8 (fighter in strip) | +0.36 / +0.27 / +1.11 | +0.19 / +0.14 / +0.60 |
| p157f2 solo s8 | (fighter) | +0.36 / +0.27 / +1.11 | +0.19 / +0.14 / +0.60 |
| p157f2 solo s456 | +0.36 / +0.28 / +1.11 | +0.36 / +0.27 / +1.11 | +0.19 / +0.14 / +0.60 |

  - The three non-p157 controls show no floor term at all, so P1's stage-21 shadow pass leaves nothing visible on this
    floor. The term is the same with the full .pat and with the s8 and s456 solo .pats, which share pattern 157 but
    have different frame-2 layers. A silhouette of P1's parts would change between them. So the term comes from
    state that pattern 157 sets up outside P1's drawn frame, such as an effect object or a light it spawns. The plate
    hide covers only P1 and P2, not effect objects.
  - The term roughly halves after one F6 tick (blue +1.11 → +0.60). It does not vanish outright. So it is
    tick-driven state that decays as the move advances. It is not a stale draw list: b0 is taken after 8 draw-only frames, and the shadow pass is
    issued by `Object_DrawWithStageShadow` on each draw pass. It is not the hide flag either.
  - Not traced: which object or light pattern 157 spawns. The probe, if it is ever needed, is one gamecap peek of
    `g_ObjectListCount` and the effect entries at `plate_b0` against `plate_bb`. No run was made: the verdict below does
    not depend on it.
- **Verdict: the plate_b0 method is correct.** `plate_b0` is taken on the same battle tick as the game shot (no F6
  between them; sidecar tick 1, `tick_plate_b` 2 only after the step). Every non-P1/P2 draw is in the same state,
  including this term and the stage anime (held). The editor draws only P1's pattern frame, so everything else belongs
  in the backdrop. The s456 solo game shot equals its `plate_b0` in the floor strip to 0.01, which shows that the game
  shot's term is the `plate_b0` term. Use `plate_b0` for every MBTL stage row. `plate` / `plate_bb` (one tick later)
  is wrong for p157 and identical for the rest.
- **Follow-up (not implemented):** the editor draws no stage shadow. When a stage with `ShadowLightType >= 0` is
  previewed, the engine model is a second draw of the frame into a shadow layer: alpha 255, y scaled by record+0x224,
  alpha × record+0x228, and per-light colour / x fade from ShadowLightStatus. Mode 0x64 is `TecCharaPtDraw_ShadowV`
  and mode 0x65 is `_TEX`. Then the reflection draw from ShadowReflexColor. The shader bodies are not in the repo.
  Measuring this needs a case where the shadow is visible. None of the current rows shows one.

### Final table (zoom 1 `_sa` + plate_b0, zoom 1.5 `_z15sa` + plate_b0; `--subpix 0,0`)
| case | residual | band ±1 | px > 24 | floor reason |
|---|---|---|---|---|
| mbtl_patctrl_chr001_p238f10_z1 | **0.23** | 0.21 | 0.0% | 8-bit rounding of the s18 translucent plane (round 17); tray 0.01 |
| mbtl_patctrl_chr001_p277f0_z1 | **0.00** | 0.00 | 0.0% | opaque point-sampled parts: pixel-exact |
| mbtl_flipV_chr001_p157f2_z1 | **0.18** | – | 0.0% | under the p238 control; the additive props and the s8 arc are on the same blend-rounding floor |
| mbtl_flipV_chr001_p157f2_solo_s8_z1 | **0.01** | – | 0.0% | one opaque part: exact once the backdrop is plate_b0 |
| mbtl_flipV_chr001_p157f2_solo_s456_z1 | **0.60** | 0.58 | 0.0% | see below |
| mbtl_patctrl_chr001_p238f10 (zoom 1.5) | **0.24** | 0.19 | – | the zoom-1 floor (round 21) |
| mbtl_patctrl_chr001_p277f0 (zoom 1.5) | **0.00** | 0.00 | – | pixel-exact |
| mbtl_flipV_chr001_p157f2 (zoom 1.5) | **0.19** | 0.18 | – | the zoom-1 floor (round 21) |

**s456 at 0.60 is at its floor.**
- Its whole 13,156 px footprint is the three additive thin-line arcs (slots 4/5/6). No pixel is over 24, and the
  ±1 band changes nothing (0.60 → 0.58), so none of it sits on an edge or comes from registration.
- The game shot and plate_b0 agree on the backdrop (lamp 127.2 / 127.2, floor strip to 0.01). What remains is the
  same 8-bit blend-rounding floor as p238: each blend into the X8R8G8B8 backbuffer rounds, while the prediction
  P + Q·B is float.
- p238's single translucent layer gives 0.32–0.40 per pixel. s456 stacks three additive arcs per pixel (each rounds
  once), so its per-pixel floor is about √3 × 0.35 ≈ 0.6. This is an estimate from the p238 split, not a new fit.
- The earlier s456 figures (0.75 in round 10, 0.72 in round 18) were measured against later-tick plates. They carried
  part of the p157 term described above (+0.6 blue in the strip), which is how the window plate still gives 0.72.

**Status: every MBTL row is at its floor. Nothing is left untraced on the parity side except the identity of the
p157 spawned floor term, which plate_b0 already covers by construction.**

## Round 23 (2026-10-16): the p157 floor term is a puddle effect (p158); s456 / p238 floors measured in 8 bits
No editor (C++) change, so no build. One MBTL run (`round23/v2r23_mbtl_chr001_solo_s456_efpeek.gcs`, log
`round23/run_v2r23_efpeek.log`; shots in gamecap `captures/v2/r23`, suffix `_sa23`). Numbers:
`tools/uni/s1_compare/v2_round23.sh` (log `round23/round23.log`). New tool `v2_quant.py` (8-bit model). The float path is
unchanged, and both are reported. MBTL IDB commented, renamed and saved.

### What pattern 157 spawns: effect pattern 158, "水たまり" (puddle)
- chr001.ha6 (MBTL data008) p157 frame 2 has one EF entry: `EFTP 1`, `EFNO 158`, `EFPR {420, 0, 64}`. Pattern 158's
  PTT2 name is Shift-JIS 水たまり ("puddle"). It has 13 frames, each one part set (AFGX layer 0: sets 302, 303, 304,
  306 ... 316), and every frame has AFHK 1 (interpolate to the next frame).
- Engine path (MBTL IDB): `Han6_EF_Dispatch` 0x582420 (case 1 at 0x582452) → `EffectPool_AllocSlot` 0x42CAE0
  (`g_EffectPool` 0xCCE700, 2000 slots × 0x868, in-use byte +0x864) → `Effect_RegisterSpawned` 0x581BD0 (EFPR word 0 =
  x offset <<7, mirrored by facing; word 1 = y; words 2|3<<16 → obj+0x7C, here 0x40; pattern = EFNO for type 1) → the
  object goes into `g_ActiveEffectList_Objs` 0xCCC7A4 (renamed from `dword_CCC7A4`) and is drawn from `g_ObjectList`
  0xCAAD04 by `ObjectList_DrawAllWithShadows` 0x5371C0 after P1 and P2. Effect objects have no draw-alpha override, so
  the plate hide (P1/P2 obj+0x648) leaves it drawn. 0x40 is not `_ObjFlags_RenderShadow` (0x10000), so it gets no stage
  shadow pass.
- Live peek (game shot, `plate_b0`, `plate_bb`): `g_ObjectListCount` = 3 every time: P1 0x10F9340, P2 0x10F9F84 and
  effect slot 0 (0xCCE700). Slot 0: in use, owner side 0 (P1), pattern **158**, frame 0, duration 3, obj+0x7C 0x40.
  Its tick is 1 at the game shot and `plate_b0`, and 2 at `plate_bb`. No other effect slot is in use.
  `g_CharaShadowRT_Enabled` is 1 throughout. There is no other light or stage object.
- The part set (chr001.pat): set 302 slot 30 is part 1, PRCL bytes ff 66 66 33 (B 255, G 102, R 102, **A 0x33**),
  zoom 3.0 × 0.8, normal alpha blend, priority 255. That is a flat bluish ellipse on the floor to the right of P1, which
  matches the floor term's colour (blue strongest). Set 303 slot 30: A 0x00, zoom 2.7 × 0.7. With AFHK the frame-0 draw
  lerps 302 → 303 as its tick advances. So the term fades within frame 0, and that is the "halving" one F6 step later.
  Measured: blue summed over y 600–720 is 172,654 at `plate_b0` and 76,799 at `plate_bb` (ratio 0.44). The strip mean goes
  +1.11 → +0.60, the same as round 19 / 22 to 0.01. Interpolation weights tick/3 (1/3 → 2/3: alpha ×0.5, area ×0.93,
  ratio ≈ 0.46) fit this better than (tick−1)/3 (0 → 1/3: ≈ 0.62). The exact lerp weight in the effect draw is not
  traced.
  (`round23/p158_puddle_b0_bb_minus_p277_x8.png`: `plate_b0` and `plate_bb` minus the p277 `plate_b0`, ×8, floor
  strip x 400–1280.)
- The term is the same for the full, s8 and s456 .pats because pattern 158 draws set 302+. The solo .pats only move
  set-21 props. **plate_b0 is the correct backdrop**: it holds the puddle at the game shot's tick.
- Editor follow-up (not done): the editor previews only P1's pattern. Drawing EF type-1 spawns (pattern EFNO at
  EFPR x/y) would put the puddle in the editor frame.

### s456 and p238: the 8-bit model
`v2_quant.py` blends into the integer plate and quantizes after each layer, with round-to-nearest (`rn`) or truncation
(`tr`). Each layer is x_i + q_i·dst, where x_i is the editor's black shot and q_i = (grey − black) / 64. One layer is the
case shot itself. p238 can also use its s4 / s5 / s18 solo shots as separate layers, restricted to the pixels where they
make up the whole frame. The float row is the existing P + Q·B (it reproduces 0.598 / 0.232).

| case | float | **quant rn** | quant tr | quant rn histogram (|d| = 0 / 1 / ≥ 2) |
|---|---|---|---|---|
| mbtl_flipV_chr001_p157f2_solo_s456_z1 | 0.60 | **0.54** | 0.73 (signed +0.54) | 51.7% / 43.2% / 5.1% |
| mbtl_patctrl_chr001_p238f10_z1 | 0.23 | **0.15** | 0.36 (signed +0.35) | 85.3% / 14.5% / 0.3% |
| p238, layers s18 → s4 → s5 (24,170 px) | 0.32 | 0.21 | 0.51 | 79.4% / 20.3% / 0.3% |
| mbtl_flipV_chr001_p157f2_z1 | 0.18 | 0.17 | 0.22 | 84.9% / 13.8% / 1.3% |
| mbtl_flipV_chr001_p157f2_solo_s8_z1 | 0.01 | 0.01 | 0.01 | 99.1% / 1.0% / 0.0% |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | 0.00 | 0.00 | 100% |

- **The game rounds to nearest.** Truncation adds a +0.35 to +0.58 signed bias that the game shot does not have. The rn
  model has none (signed +0.04 s456, +0.01 p238).
- **Blend quantization is a small part of either floor.** It accounts for 0.06 of s456 (0.60 → 0.54) and 0.08 of p238
  (0.23 → 0.15). The rn prediction is an integer image, and what is left is whole-level differences in the layer's own
  value (the filtered texel × diffuse colour × alpha), not in the blend.
- **Round 22's √3 estimate is refuted.** The s456 arcs are not additive. Set 21 slots 4/5/6 have no PRAL (normal alpha
  blend), PRFL 1 (linear filter), A3 3D rotation and zoom 1.07–1.15. 12,341 of the 13,156 footprint px (94%) are on
  part edges (|∇P| > 4 levels/px), with rn residual 0.54 on the edges and 0.45 in the interior. So s456's floor is the
  ±1-level difference in the filtered, rotated thin lines' coverage: bilinear-weight precision and texture-coordinate
  interpolation between the game's D3D9 pipeline and the editor. It is not a stacking of blend roundings. p238's s18
  plane is mostly interior (rn 0.16 interior, 0.11 edge), so its floor is a single-plane ±1 on 14.5% of pixels.
- Draw order on p238: s18 → s4 → s5 composes back to the editor's case shot on 24,170 px with signed +0.02. s4 → s5
  → s18 gives +0.13 on its 15,256 px. The draw order is not traced and does not change the floor.

### Final table (zoom 1 `_sa` + plate_b0, `--subpix 0,0`; zoom 1.5 unchanged from round 22)
| case | float | 8-bit rn | measured floor and reason |
|---|---|---|---|
| mbtl_patctrl_chr001_p238f10_z1 | 0.23 | **0.15** | blend rounding 0.08 (measured). Rest: ±1 level on 14.5% of px of the s18 translucent plane's source value |
| mbtl_patctrl_chr001_p277f0_z1 | 0.00 | 0.00 | opaque point-sampled parts: pixel-exact |
| mbtl_flipV_chr001_p157f2_z1 | 0.18 | **0.17** | ±1 on 14% of px (s8 arc edges, set-21 props). The p158 puddle is in plate_b0 at the same tick |
| mbtl_flipV_chr001_p157f2_solo_s8_z1 | 0.01 | 0.01 | one opaque part: exact |
| mbtl_flipV_chr001_p157f2_solo_s456_z1 | 0.60 | **0.54** | blend rounding 0.06 (measured). Rest: ±1 on 43% / ±2 on 5% of px, on the linear-filtered, 3D-rotated thin arc edges (94% of the footprint) |
| mbtl_patctrl_chr001_p238f10 (zoom 1.5) | 0.24 | – | the zoom-1 floor (round 21) |
| mbtl_patctrl_chr001_p277f0 (zoom 1.5) | 0.00 | – | pixel-exact |
| mbtl_flipV_chr001_p157f2 (zoom 1.5) | 0.19 | – | the zoom-1 floor (round 21) |

**Status: the p157 floor term is identified (effect pattern 158, the puddle; in plate_b0 by construction). Every MBTL
row has a measured floor. What remains is a whole-level difference in the filtered source value, mostly on part edges.
Blend quantization is measured at 0.06–0.08, rounding to nearest.**
