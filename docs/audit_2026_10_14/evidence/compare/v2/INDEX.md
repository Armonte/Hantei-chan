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
