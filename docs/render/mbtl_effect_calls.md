# MBTL effect call sites (lane FX, P3 step 3)

Traced from `C:\dev\ida\server\MBTL_25646393.exe.i64` through the IDA MCP
(`MBTL.exe`, `d3dx9_42`). The functions I renamed are commented in the IDB, which is saved.
MBTL is a newer codebase than UNI2 and wraps `ID3DXEffect` in named helpers:

- `Effect_SelectTechniqueByName(name)` (0x490CA0) -> `ID3DXEffect::SetTechnique`;
- `Effect_SetMatrix(name, m, n)` (0x412090) -> `SetMatrix`;
- `Effect_SetFloatArray(param, src, count)` (0x412130) -> `SetFloat`/`SetVector`/`SetValue`;
- `Effect_SetTexture(name, tex)` (0x490390) -> `SetTexture`;
- `Shader_LoadEffectFromArchive` (0x48C490) and `Shader_LoadEffectsAndTechniques` (0x7F3F20)
  are the loaders; the same 33 technique names as UNI2 are registered.

Technique and parameter names are the shader globals, so the bindings line up with
`fx_compile.tsv`.

## CG sprite (character draw)

Caller `Han6Draw_SubmitCgSprite_ParentMtx` (0x4A0C40). It picks the technique from
`*((DWORD*)this + 7)` (the draw mode) and the bg-colour-filter struct at `*((float*)this + 19)`:

| technique | chosen by | parameters | textures |
|---|---|---|---|
| `TecCharaDraw_V` | mode 0, no filter, light off | `g_mWorld`, `g_mView`, `g_mProj`, `g_GrayScale` | palette |
| `TecCharaDrawBgColor_V` | mode 0, filter struct, `*(filter+4)==0` | `g_fCharaBgColorBase` (filter+8), `g_fCharaBgColorHeight` (filter+24), `g_fPalCount`, `g_fZeroPosV`, `g_fTexel` | palette |
| `TecCharaDrawBgColorFilter_V` | mode 0, filter struct, `*(filter+4)!=0` (**the zoom > 1 case**) | `g_fCharaBgColorBase`, `g_fCharaBgColorHeight`, `g_fCharaBgStandardPixelHeight` | palette |
| `TecCharaDrawBgColorSpecular_V` | mode 0, filter+specular, `*(filter+4)==0` | `g_fCharaBgSpecularBase` (filter+40), `g_fCharaBgSpecularHeight` (filter+56) | palette |
| `TecCharaDrawBgColorSpecularFilter_V` | mode 0, filter+specular | as above | palette |
| `TecCharaDrawBgColorBokashi_V` | mode 0, filter `==1` (bokashi) | `g_fCharaBgBokashiAlpha` (filter+72), `g_fStepTexture1DotSizeW/H` = 1/size | palette |
| `TecCharaDraw_V_Grad` | mode 1 | `g_mWorld`, transforms | palette |
| `TecCharaDraw_FillV` | mode 0xA | `g_mWorld` | - |
| `TecCharaDraw_ShadowV`/`_TEX` | mode 0x65 / default | `g_mWorld` | - |

`CharaBgColor_SetDrawLocalConsts` (0x4A14D0) fills `g_fCharaBgStandardPixelHeight` and the
`g_fStepTexture1DotSizeW/H` path. `Han6Draw_SubmitCgSprite` (0x4A1610) is the `TecCharaDrawEx`
variant (palette copy: `fPalPlusV`, `tex_Palette`; also `CharaObj_DrawEx` 0x6B2170).
Parameter sources are fields of the CG sprite object and the bg-colour-filter struct
(`filter+8/24/40/56/72`, `filter+4` = "use filter"), plus `this+56/60` (the view/proj
matrices). `Han6Draw_DrawLayer` (0x4A0240) is the caller that iterates layers;
`Han6Object_Draw` (0x597070) paints the object and its shadow.

## PAT part

`PatPartSet_Draw_CharaPtShader` (0x4A2240) picks the technique from `*(this+28)` and sets
`g_mView` (`this+56`), `g_mProj` (`this+60`), `g_GrayScale` (`this+84`):

| technique | chosen by |
|---|---|
| `TecCharaPtDraw_V` | mode 0/1 (normal) |
| `TecCharaPtDraw_FillV` | mode 0xA |
| `TecCharaPtDraw_ShadowV_TEX` | mode 0x65 |
| `TecCharaPtDraw_ShadowV` | default |

The per-part world matrix is set in `PatPart_SetupDraw_ShaderWorldMtx` (0x411C10), which is
where the same technique string is resolved and `g_mWorld` is computed. Parts are submitted
through `PatPart_SubmitPrimByShapeType` (0x4A2710) and the primitive queue
(`PatPart_SetupDraw_PrimQueue` 0x411AB0, `PatPart_DrawPrimitive_DispatchByShapeType`
0x4A31B0); blend / filter follow with `g_PatPrimBlendMode` / `g_PatPrimTexFilter`.

## PAT mask and blur (character aura)

| technique | caller | parameters | textures |
|---|---|---|---|
| `TecCharaPtBlurDrawW` / `TecCharaPtBlurDrawH` | `CharaPtBlur_DrawHAndW` (0x7F3830) | `fBlurTexel` | `tex_BlurBase`, `tex_Mask` |
| `TecCharaPtMaskCopy` | `CharaPtMask_Copy` (0x7F3D80) | - | `tex_Base`, `tex_Mask` |

## Blur / mono / 2-tone / texmask

| draw kind | technique | caller |
|---|---|---|
| chara point blur | `CharaPointBlur_W` / `_H` / `_MagFilter` | registered at 0x7F3F20 (strings 0x9E44B0/0x9E452C/0x9E4510) |
| monochrome | `TecTalkMono` | `TalkMono_Draw` (0x83BEB0) |
| 2-tone | `Tec2Tone` / `Tec2Tone_Alpha` | registered at 0x7F3F20 |
| texture mask | `TecTexMask` | registered at 0x7F3F20 |

## BG object / stage / bloom

| technique | caller | parameters | textures |
|---|---|---|---|
| `TecBgObjectMono` | `BgObject_Draw` (0x4F0620) | - | `tex_Base` |
| `TecBgPaniSpecular` | `BgObject_Draw` | `g_fBgSpecular` | `tex_Base` |
| `TecBgSpeculer` / `TecBgNoSpeculer` | `Stage_DrawBattleStageFrame` (0x4F2080) | `g_GrayScale` | `tex_Base` |
| `TinyFXAA` | `Stage_DrawBattleStageFrame` | `g_Threshold`, `g_LerpT`, `g_OnePixelU` | ping-pong target |
| `bloom` | `Stage_ScreenBloomPass` (0x4EEDA0) | `g_Blightness`, `g_Power`, `g_BiassR/G/B`, `g_BlurRadius`, `g_OnePixelUV` | ping-pong target |

`Stage_DrawBattleBackground` (0x4F2C20) and `Stage_DrawBattleStageFrame` are the stage entry
points; the FBX model path is `FbxScene_Draw` (0x586A40) / `FbxScene_DrawNodeTree` (0x585DA0)
and uses `fbxshader.txt` (same constant set as UNI2's, from `fx_compile.tsv`).
**unresolved**: like UNI2 I did not trace the fade/gauge techniques through
`Effect_SelectTechniqueByName` in this build; the fade and gauge are drawn by screen-level
objects, not the character effect path, and no matching technique string is reached from the
draw kinds above.

## Values the editor does not simulate

Same as UNI2: stage light colour, the chara aura and the bloom parameters default to the
training-on-stage-1 neutral values (greyscale 0, bokashi alpha 0, specular 0, `g_fBgSpecular`
(1,1,1), bloom off) and are exposed under View > Game shaders in P3 step 4.

## Addresses renamed in the IDB

0x4F0620 `BgObject_Draw`, 0x83BEB0 `TalkMono_Draw`, 0x7F3830 `CharaPtBlur_DrawHAndW`,
0x7F3D80 `CharaPtMask_Copy`, 0x6B2170 `CharaObj_DrawEx`, 0x4EEDA0 `Stage_ScreenBloomPass`.
MBTL was already largely named; no `sub_` remains on anything I touched. IDB saved.
