# Game vs new editor: S1 render comparison (2026-10-14)

## Method
- **Game frames:** tools/gamecap (outside git; frames in `tools/gamecap/captures/s1_compare/game`). UNI2 1.42 +
  UNI2-IM v0.7.7, training, stage pinned to 1, no HUD, GRD/Vorpal pinned at 0.
  - P1 is forced with the game's own `Object_SetPatternFrame` and stepped to the case's tick: the editor's
    `--tick T` is global from pattern start, so T+1 updates. The frame and tick actually drawn are in
    `game/<case>_game.json`.
  - A fighters-hidden **plate** of the next frame comes from the UNI2-IM harness.
- **Registration:** the game's own camera transform (sidecar). Scale is 1 game px per sprite px and 1/zoom vs the
  editor. No image matching, so blending differences cannot pull the alignment.
- **Blend model:**
  - New-editor shots on black and white backdrops (`editor/*_new_k/_w.png`, `--hide-boxes`) give
    E = P + Q·backdrop per pixel, which covers normal, additive, subtractive and multiply blends.
  - Predicted game pixel = P + Q·plate, compared on the editor's sprite footprint.
- **Lighting:** the game tints character sprites with the stage light; the editor shows raw palette colours.
  - Measured on two idle-sprite **controls** (chr025 p0, chr000 p0) on stage 1:
    k = (0.98, 0.86, 0.76), c = (17, 58, 99) per RGB.
  - "lit" columns apply it as k·P + c·(1−Q) + Q·plate.
- **Controls** (geometry identical; residual is edge antialiasing): raw 49.0 / 47.5 → **lit 5.6 / 5.2** mean |d|,
  18% of px > 24. Values above that floor are real differences.

## Per case (mean |d| over sprite px; % of px differing by > 24): `sheets/game_vs_new_lit_stage1.tsv`
| case | raw | lit | drawn frame/tick | finding | hypothesis |
|---|---|---|---|---|---|
| pral2_chr025_p133f1 | 13.8 / 42% | 20.1 / 68% | f1 t1 | Shape and position match. Lighting makes it worse. | Effect (PRAL 2 subtractive) parts are **not** stage-lit in game; the residual is the subtractive amount. Next: lit/unlit per layer, check the PRAL 2 blend constant. |
| flipH_chr000_p96f1 | 32.9 / 85% | 44.5 / 92% | f1 t1 | Main sweep matches. The thin lightning strand sits inside the sweep in game, outside on its right in the editor. | Game flip is UV-only (`PatPart_GetCutoutUVWithFlip` 1.42 0x413D10, same as old exe), and the editor does the same, so suspect the **position of the flipped part** (PPCC/pivot with negative UV span) or a strand that is a separate layer with a different offset. |
| flipV_chr000_p383f1 | 47.0 / 78% | 40.4 / 77% | f1 t1 | Game sweep is thicker and lower; the editor has an extra thin strand above. | Same family as flipH: V-flipped part placement. |
| flipHV_chr019_p153 | 43.3 / 91% | 44.9 / 89% | f0 t1 | Visually very close (same streak fan); the game's streaks are whiter. | Colour only: additive streaks with game-side brightening (lighting/additive scale), not geometry. That H+V is right while H and V alone are wrong points at flip-pivot handling. |
| afhk_pat_chr000_p72_t2 | 29.5 / 89% | 25.7 / 89% | f0 t3 | Small sprite (6.6k px); offsets of a few px. | AFHK PAT interpolation phase. Check the editor's lerp weight against game `+0x48` progress. |
| afhk_pat_chr019_p62_t7 | 23.5 / 66% | 23.4 / 65% | f1 t5 | Game burst concentrated near the ground with purple pillars; the editor's burst sits higher and wider. | Spawned effect objects and AFHK interpolation. The no-spawn editor variant is worse (31.7), so spawns are drawn in game; layout differs. |
| afhk_cg_chr020_p435_t27 | 35.7 / 81% | 21.3 / 72% | f0 t28 | Lighting helps; residual on rotation. | AFAZ rotation lerp at tick 27 of 55 (AFHK CG). |
| afhk_cg_chr020_p437_t2 | 25.4 / 78% | 25.3 / 78% | f0 t3 | Bunny shape matches; colour differs. | Colour/lighting of CG-layer sprites; AFZM scale lerp looks right. |
| rect_chr024_p76 | 17.5 / 74% | 13.5 / 61% | f0 t1 | Tiny (540 px) thin rect. | Rect shape fill colour/alpha. Too small to conclude. |
| pras_chr020_p5f2 | 31.7 / 79% | 32.9 / 79% | f2 t1 | Character body aligned; the hat/aura effect differs. | PRAS pivot: case note says UNI2 applies the pivot, check the effect part offsets. Lighting on chr020 may differ from the controls. |
| pups_chr027_p449 | n/a | n/a | f0 t1 | The editor shows a full-view black part (no footprint/axes), so it can't be compared this way. | Needs a cropped comparison. |
| mbtl_flipV_chr001_p157f2 | 66.5 / 97% | — | **f3 t3** (wanted f2) | Wrong frame. | MBTL's first step after the forced set runs ~5 ticks of catch-up (probe). Rerun with `step 3` before the call. |
| mbtl_pral2_chr004_p420f4 | n/a | — | **f5** (wanted f4) | Wrong frame, no footprint. | As above. |
| mbtl_pups_chr011_p309 | 56.8 / 64% | — | f0 t1 | Registration unverified for MBTL. | Needs an MBTL idle control (as for UNI2) before reading numbers. |

## Status
- **Verified:** for plain character sprites the editor renders like the game up to the stage lighting (controls).
- **Not yet proven:**
  - effect cases: flips, AFHK, PRAL;
  - the MBTL cases.
- **Next steps:**
  1. Per-case visual review with the lit tiles (`tools/gamecap/captures/s1_compare/lit/`).
  2. Flip-part placement.
  3. Whether effect layers are stage-lit.
  4. MBTL reruns with the catch-up fix plus an MBTL control.

## v2 (2026-10-14 late): full-frame, frame-verified comparison and the fix
Method change (user): no more pixel metrics on effect crops. Each case is captured as a whole game frame, with the
frame proven by the game's own state and by the Improvement Mod hitbox overlay, next to the editor at the same scale.
- **UNI2:** UNI2-IM Pause (F5) / Next frame (F6) / hitboxes (F2) through its harness pipe `keys` (in-process). P1 is
  put on the pattern with `Object_SetPatternFrame`, then stepped with F6. Shots are harness `grab`s (gamecap's own
  capture is black while UNI2-IM is paused). `--tick` cases start at frame 0 and run naturally to the tick.
- **MBTL:** MBTL-IM Pause/Next/hitboxes, pressed with gamecap virtual keys. The agent now patches only the MBTL-IM
  proxy's `GetAsyncKeyState`/`GetForegroundWindow` imports (MBTL only), so no focus change or global input is needed.
  One F6 = exactly one tick: the old catch-up drift (f3/f5 instead of f2/f4) is gone, every MBTL case asserted its
  frame. Shots are PrintWindow of our HWND (some come back black, the backbuffer copy is the fallback).
- **Editor:** `tools/uni/s1_compare/shoot_v2.sh` (zoom 1 = 1 game px per sprite px on both games' rest camera,
  boxes on). `v2_sheet.py` places it by the game's camera transform (P1 origin) and the editor axes.
- **Sheets:** `v2/<case>.png` (half size here; full size in the main checkout's docs/.../compare/v2/): game + IM
  hitboxes | game clean | ours | sosfiro (empty until work/sosfiro_capture writes v2/sosfiro/). Sidecars in
  `v2/game_meta/`, capture scripts in `v2/gamecap_scripts/`.

**Root cause of most mismatches (IDA, uni2 1.42):** every .pat part goes through
`PatPart_GetGlobalPerspective640x480` 0x4C1860 (Scale(1/640,1/480,1/1024) * LookAtLH * PerspectiveFovLH(1.3045) *
Scale(640,480,1)), called from `PatPart_BuildTransformMatrix` 0x4C10E0. The part space is **left-handed**: positive
part z is farther. The editor's frustum looked down -z, so every part with an X/Y rotation tilted the wrong way in
depth: the flipH/flipV sweeps came out far smaller and in the wrong place. Fixed in `partrender::PartPerspective`
(mirrors z; unit-checked against the game's matrix chain). The four primitive shapes (`CPAniPrimRender_Ring` 0x414120,
`_Donuts` 0x414590, `_Sphere` 0x414A60, `_Corn` 0x414DF0) build +z away from the eye; the editor had stored them
negated to make up for the old projection, so they are back to the game's sign. Flip (PRRV) itself was already right:
`PatPart_DrawPlaneQuad` 0x4BFCB0 never mirrors the quad, only the UV.

### Status per case (v2 sheets, after the fix)
| case | frame verified | old lit metric before -> after | status |
|---|---|---|---|
| flipH_chr000_p96f1 | p96 f1 t1 | 44.5/92% -> 32.9/90% | **Matches** (sweep size, shape, lightning inside the sweep). The crop metric stays high: misregistration of the old crop, not the render. |
| flipV_chr000_p383f1 | p383 f1 t1, IM box = editor box | 40.4/77% -> 15.1/45% | **Matches**, hitbox at the same place. |
| pral2_chr025_p133f1 | p133 f1 t1 | 20.1/68% -> 11.1/52% | **Matches** in shape/place; colour close. The blue arc beside it in game is not P1's pattern. |
| flipHV_chr019_p153 | p153 f0 t1 | 44.9/89% -> 16.4/71% | **Matches (closed, see "Chara aura" below).** Brown band = palette[214] tint, in game too; the first capture had a strong aura add. Aura held 0: 6.75/19.5% (floor). |
| afhk_pat_chr000_p72_t2 | p72 f0 t3 | 25.7/89% -> 19.9/80% | **Matches** (small swoosh, same place/shape). |
| afhk_pat_chr019_p62_t7 | p62 f1 t5 (natural run) | 23.4/65% -> 9.1/38% | **Matches** after the shape fix (cone spikes and pillars sit on the ring). Was the cone z sign. |
| afhk_cg_chr020_p435_t27 | p435 f0 t28 | 21.3/72% -> 21.3/72% | **Matches visually** (bunny place, size, tilt). Not affected by the fix (CG layer). Residual = colour/light. |
| afhk_cg_chr020_p437_t2 | p437 f0 t3 | 25.3/78% -> 25.3/78% | **Matches visually**; same as above. |
| rect_chr024_p76 | p76 f0 t1 | 13.5/61% -> 13.5/61% | **Matches** (thin line, same place). |
| pras_chr020_p5f2 | p5 f2 t1 | 32.9/79% -> 14.9/59% | **Close**: aura and hat placement match; the hat looks a bit larger/more visible in the editor. |
| pups_chr027_p449 | p449 f0 t1 | n/a | **Matches (closed).** (0,21,35) = the chara aura add; aura held 0 -> game (0,0,0) = editor, residual 0.00 over the whole part. |
| mbtl_ctrl_chr001_p0f0 | p0 f0 t1 | new control | Position and scale match (1 game px per sprite px). |
| mbtl_flipV_chr001_p157f2 | **p157 f2 t1** (was f3) | n/a | **Matches** (arc, sparkles, same place). |
| mbtl_pups_chr011_p309 | p309 f0 t1 | n/a | **Matches** (silhouette and hurtbox). |
| mbtl_pral2_chr004_p420f4 | **p420 f4 t1** (was f5) | n/a | **Matches (closed).** No cutscene camera at this frame (camera element 1 = pos 0, zoom 1); the sheet's editor-origin detection had taken the white fill's edge for an axis. Full frame 0.69/0.6%, PRAL2 cloud corr 0.93. |

The old metric is kept only as a trend; the v2 sheets are the evidence.

### Closing the last three (2026-10-14 night): chara aura + sheet registration
**Chara aura (uni2 1.42, IDB renamed and saved).**
- *Where the add comes from.* `CharaObj_DrawWithStageLight` 0x5CE3C0 builds, per character object, a layer mul colour and a layer **add** colour. They are stored at DrawObj +0x1C/+0x20 per layer (stride 12) by `CharaObj_DrawLayersWithLight` 0x5CD740.
- *How a .pat part uses them.* `PatPart_SetupWorldMatrixAndShader` 0x415230 does:
  - diffuse = lerp(PRCL) x mul (`Color_ModulateARGB` 0x4ACB70);
  - specular = lerp(PRSP) + add (`PatPart_ComputeSpecular_PRSPPlusObjAdd` 0x413BA0; skipped for PRAL 1 parts with no PRSP).
- *Shader.* `PS_CharaPtDrawV` (Shader/sh_chara.txt): `out = tex*diffuse; out.rgb *= g_fPtLastColor (palette[PPPA]); out += specular`.
- *What the add is.* The add is max(flash, `int(int(aura.rgb*255) * aura.strength)`) for the main object.
- *Where the aura lives.* `g_CharaAura` 0xC7A688 (4 x 0x70), slot `[obj+0x8C0]`. The character's squirrel script sets it (`SqBind_CharaAura_Set` 0x886810: type, period, power, colours).
- *Why it varies.* `CharaAura_UpdatePulse` 0x59E8A0 sets strength = |2*(t mod (period+1))/period - 1| every battle tick (`CharaObj_TickTimersAndAura` 0x541E30).
- *Values.* chr019 and chr027 have aura rgb (0, 0.102, 0.165), i.e. (0,26,42) x strength, period 60. Over time it runs from (0,0,0) up to (0,26,42).
- So the "stage light offset" seen on the .pat parts is this pulsing per-character aura. It is not stage data and not pattern data; it is runtime state the editor does not have.

**Live proof** (`tools/gamecap` `cases/v2_light_chr027.gcs`, `v2_light_chr019_027.gcs`, `v2_light_chr027b.gcs`, copies in `v2/gamecap_scripts/`):
- The aura is now recorded in the sidecar (`aura_rgb`, `aura_strength`, `aura_type`).
- A second capture has the aura RGB held at 0 with the agent's `hold`.

| capture | aura in sidecar | predicted add | game, black PUPS part |
|---|---|---|---|
| pups lit2 | strength 0.2333 | (0,6,9) | **(0,6,9)** |
| pups unlit | held 0 | (0,0,0) | **(0,0,0)** = editor |
| pups (first v2 capture) | not recorded | (0,21,35) = strength 25/30 | (0,21,35) |
| flipHV lit | strength 0.2000 | (0,5,8) | streak residual 6.99/19.8% |
| flipHV unlit | held 0 | (0,0,0) | 6.75/19.5% (control floor 5.2–6.9) |

**flipHV "brown band".** The band is part 4 of part set 45 (PRID 56, PPPA 0xD6 = 214). Palette 0 entry 214 is (101,85,58), brown; the other parts use entry 230, (222,219,177). The game multiplies by the same palette entry (`g_fPtLastColor` from the object palette at DrawObj+0x2C, `PatPart_SetupWorldMatrixAndShader`). The game band only looked light because the first capture caught the aura near full strength, which adds (0,21,35) to every streak. With the aura recorded or held at 0, the game shows the same gold streaks and brown band as the editor.

**Editor change: none needed.** PPPA, PUPS and PRAL handling were already right. An optional "game aura" preview tint would be the only addition; it was not done, because the aura is runtime character-script state.

**MBTL Last Arc.**
- *Registration.* The sheet's `editor_origin` failed on full-view shots: it took the white fill's edge for the axes, giving (0,143); PUPS gave (841,0). `sheet.KNOWN_ORIGINS` now fixes the origin by shot size, since the shoot scripts fix window, zoom and pan.
- *Camera.* The MBTL sidecar camera (element 1 via `GetCameraPosition` 0x7FB870) is pos (0,0), zoom 1 at this frame, and the IM origin cross sits at the computed (320,640). There is no separate cutscene camera when P1 itself plays p420.
- *Result.* Full-frame residual 0.69 mean, 0.6% > 24. The PRAL2 part (part 10, PPST 267 cloud texture, subtractive, alpha 0x28, covering the screen) gives the cloud structure with r = 0.93 against the game (std 1.75 vs 1.93); a flat white fill would be off by 4.75 mean.

Per-case numbers for all 15 cases: `v2/INDEX.md`.
