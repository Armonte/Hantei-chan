# Render match 3 handoff (2026-10-15, fix/render-match3 in wt/render-match3, from update/parity-1014 14ebf48)

The final table is in `docs/audit_2026_10_14/evidence/compare/v2/INDEX.md`, section "Round 4 / render-match3". The
raw log is `round4.log` and the per-run JSON is in `r4_json/`. Reproduce with:

```
tools/uni/s1_compare/shoot_batch.sh      # all 72 editor views in one hidden --headless --batch process
tools/uni/s1_compare/v2_round4.sh        # every case and control
```

## Done
1. **pras_chr020_p5f2: 7.15 / 24.6% -> 4.29 / 8.5%, band 1.63 / 1.4%** (floor 1.29 / 0.6%).
   - The cause was the measurement, not the gradient. The .pat layer is additive (PRAL 1, slot 237). On the white
     backdrop shot its add clipped at 255, so Q came out < 1 and the model under-predicted every additive pixel.
   - `--grey`: the second backdrop is now 64 grey. The shoot script makes `_new_g`, and `v2_model` extrapolates W
     from it. All cases now use it.
   - The per-layer model (`--layers`, `--layer` passes `pras_chr020_p5f2_cg` / `_pat`) is implemented. The draw order
     came out as CG over .pat. It gives 4.33 / 8.7%, so restricting the gradient to the CG layer changes nothing
     measurable here. It is kept in the tool.
2. **MBTL PUPS halo: 27.78 / 61.7% -> 0.91 / 1.9%.** This was an editor bug, now fixed.
   - The PPPA tint reads the base `.pal` of the selected colour, never the PUPS bank.
   - IDA MBTL 25646393 (saved): `PatPart_SetupDraw_ShaderWorldMtx` 0x411C10, `Han6Object_Draw` 0x597070 (+0x7C =
     `CCharaDataSet+0x118`), `CharaDataSet_ApplyColorBuildPalTexAndTintCopy` 0x593030 (slot 0 copy).
   - Code: `src/parts/parts.cpp`, `partrender::PartTintPalette` and `PaletteEntryRgb` in `src/parts/part_render_math.h`,
     `CG::paletteNumber()`. Unit check `TestPppaTintPalette`.
3. **MBTL light k > 1: the real formula** replaces the fitted k. It is the BgColor three-pass CG shading: colour ×
   gradient, a specular add, and a bokashi add of 0.40 × mean9x9(pow(tex, 4)) on bg007. The pow(tex, 4) add is the
   brightening.
   - `v2_model.py --mbtl-light bg007,330`. The MBTL control goes from 7.30 / 31.0% to **0.27 / 0.0%**.
   - IDA: `Han6Draw_DrawFrameLayers_2to0` 0x4A0750, `Han6Draw_SubmitCgSprite_ParentMtx` 0x4A0C40,
     `Stage_SampleCharaBgStrip` 0x4EFD50, `Stage_ScreenBloomPass` 0x4EEDA0 (MBTL-IM BloomStrength only).
   - MBTL `Shader/sh_chara.txt` was extracted to `gamedata/mbtl/Shader/`.
   - The editor does not render this stage light (it is stage state). Adding it would mean a 3-pass CG shader with a
     9x9 pow(tex, 4) blur.
4. Every case was re-measured, including the six first-pass ones (S1 plates under `captures/s1_compare/game`).
   - The MBTL flipV and PUPS frames are .pat-only (AFGX type 1), so they now use the raw PatPart model with no CG light.
   - ctest 41/41.
   - uni_regress after the C++ change: `v2/uni_regress_r4.log`, started at the end of the time box. Check its last
     line (rc).

## Open
- **rect_chr024_p76 top row.** The game lights row -1 at full coverage on the side columns: (32, 81) on row -1 vs
  (32, 83) on row 0. Only the centre column R differs (66 vs 100), which looks like a bilinear edge texel at v ≈ 0.
  - The geometry is ruled out. uni2 1.42 `CPAniPrimRender_Rect_Setup` 0x413F40 and `_GetVtxTL/TR/BL/BR`
    0x413E80..0x413F00 (renamed and saved) build plain corners (x, y)..(x+w, y+h), the same as the editor.
  - `PatPart_GetGlobalPerspective640x480` adds no y offset.
  - So the game's part top edge sits about 1 px above the sprite origin (gorg y 639.99999). This is not the top-left
    rule: a 1e-5 edge offset cannot cover pixel 639's centre.
  - Not fixed and no unit check: the reason is not proven. Next step: read the world matrix of the part from memory
    (`PatPart_SetupWorldMatrixAndShader` 0x415230) for this frame and project y = 0.
- **flipHV_chr019_p153**: 9.23 / 27.4%, band 3.85 / 10.3%, against a control of 3.39 / 8.1%. This is the first-pass
  "brown band" and was not investigated.
- **mbtl_flipV_chr001_p157f2**: 4.03 / 13.5%, band 2.20 / 4.7%. This is AA on a large translucent .pat arc. There is
  no .pat-only MBTL control to give it a floor of its own.
- The afhk_cg cases are about 0.8 mean above the chr020 floor (a sub-pixel edge on rotated/scaled point-sampled CG).
- The **Vorpal glow** model ring is unchanged.
- The **stage** is not pinned in the MBTL capture scripts. bg007 was identified from the sky texture.

## Commits
- WIP b9bb380: tool and editor changes.
- Final commit: INDEX round 4, handoff, editor_bw shots, sheets, logs.

No push or merge.
