# v2 game-vs-editor index (2026-10-14, fix/render-match)

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

## Cases
| case | game frame | status | residual (no light → aura model) | aura |
|---|---|---|---|---|
| flipHV_chr019_p153 | p153 f0 t1 | **Fixed (root cause found)**. The "brown band" is palette[214] = (101,85,58) via PPPA, in the game too. The earlier capture had a strong blue aura add (0,21–23,35–37) that lifted it. | old frame: 19.00 → 7.51 / 20.8%. New lit frame: 8.98 → **6.99 / 19.8%**. Aura held 0: **6.75 / 19.5%** | sidecar (0,5,8); held 0 |
| pups_chr027_p449 | p449 f0 t1 | **Fixed (root cause found)**. The black part is (0,0,0) in the editor and in the game with the aura held at 0. The game lifts it by exactly the aura add. | old frame 18.67 → **0.00** with c=(0,21,35). lit2 frame (0,6,9): **0.00**. Aura held 0: **0.00** (whole black area, 0% > 24) | sidecar (0,6,9); held 0 |
| mbtl_pral2_chr004_p420f4 | p420 f4 t1 | **Matches**. There is no cutscene camera at this frame: the sidecar shows camera element 1 at pos (0,0), zoom 1 (MBTL `GetCameraPosition` 0x7FB870). The sheet only looked wrong because the editor origin detection read the white fill's edge as an axis (fixed). | full frame **0.69 / 0.6%**. Brush window 1.88 / 3.4%. PRAL2 cloud texture (part 10, subtractive, α 0x28) correlates with the game at r = 0.93; a flat white fill would give 4.75 | n/a |
| pral2_chr025_p133f1 | p133 f1 t1 | Matches | 6.85 → **4.68 / 13.8%** | fit (0,17,28) |
| flipH_chr000_p96f1 | p96 f1 t1 | Matches | 14.52 → **0.76 / 0.5%** | fit (0,20,33) |
| flipV_chr000_p383f1 | p383 f1 t1 | Matches | 4.45 → **3.43 / 10.2%** | fit (2,4,6) |
| afhk_pat_chr000_p72_t2 | p72 f0 t3 | Matches | 11.68 → **3.83 / 14.4%** | fit (0,21,35) |
| afhk_pat_chr019_p62_t7 | p62 f1 t5 | Matches | 11.48 → **3.04 / 6.0%** | fit (66,103,116)? Large: probably a flash or spawn; geometry is fine |
| afhk_cg_chr020_p435_t27 | p435 f0 t28 | Matches visually. CG layer: the residual is the CG BgColor stage shading, which the aura model does not cover | 23.25 → 14.53 / 13.2% | fit (10,10,16) |
| afhk_cg_chr020_p437_t2 | p437 f0 t3 | Matches visually. CG layer, as above | 19.28 → 9.60 / 8.9% | fit (12,11,15) |
| rect_chr024_p76 | p76 f0 t1 | Matches visually. A 540 px line; the refine hit its bound (−10), so the number is not meaningful | 17.54 → 11.67 / 60.9% | fit |
| pras_chr020_p5f2 | p5 f2 t1 | Matches | 31.43 → **5.45 / 13.5%** | fit (14,63,93) incl. CG |
| mbtl_ctrl_chr001_p0f0 | p0 f0 t1 | Control: position and scale match | visual only | n/a |
| mbtl_flipV_chr001_p157f2 | p157 f2 t1 | Matches visually (arc, sparkles, place) | No plate metric: the MBTL plates predate the v2 frames (refine hits ±10) | n/a |
| mbtl_pups_chr011_p309 | p309 f0 t1 | Matches visually (silhouette, hurtbox) | No plate metric, as above | n/a |

## Still to do
- MBTL plate-based numbers: capture an MBTL-IM fighters-hidden plate in the v2 session.
- Record the aura in every UNI2 sidecar: `games/uni2_meta_aura.gcs` / `uni2_v2_case_aura.gcs`.

## Reproduce
```
CASES=<re> OUTDIR=docs/.../v2/editor_bw tools/uni/s1_compare/shoot_bw2.sh
python3 tools/uni/s1_compare/v2_model.py <case> --plate <plate> [--game-name <capture>] [--aura-fit]
python3 tools/uni/s1_compare/v2_fullframe.py <case> --rect x0,y0,x1,y1 [--game-name N] [--aura]
python3 tools/uni/s1_compare/v2_sheet.py <case> [--game-name N --out NAME]
```
Game captures: `tools/gamecap/run.sh cases/v2_light_*.gcs`. Copies are in `gamecap_scripts/`, sidecars in `game_meta/`.
