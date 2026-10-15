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
