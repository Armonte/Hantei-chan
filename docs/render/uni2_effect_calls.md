# UNI2 effect call sites (lane FX, P3 step 3)

Traced from `C:\dev\ida\server\uni2.exe.i64` (UNI2 1.42) through the IDA MCP. Every
function below is renamed to a behaviour name and commented in the IDB, which is saved.

UNI2 does **not** call `ID3DXEffect` methods directly per draw. It loads every `Shader/*.txt`
once and, per technique, builds a `CDxEffTag` command record. Per draw it:
1. resolves the technique by name: `DxEffTag_FindTechniqueByName(name)` (0x4B0230);
2. queues parameters into the record: `DxEffTag_QueueMatrixParam(name, ...)` (0x415AF0),
   `DxEffTag_QueueValueParam(name, ...)` (0x415B90), `DxEffTag_BindTextureByName(name, tex)` (0x4AF860);
3. opens the pass range with `DxEffTag_AllocBegin` (0x4B0420) / submits it with
   `DxEffTag_DrawRangePrims[Ex]` (0x4BC700 / 0x4BCCD0), and the frame boundary flushes it with
   `DxEffTag_FlushPass` (0x4BD2A0).

`DxEffectLib_LoadAndRegisterTechniques` (0x874BF0) is the loader: one
`DxEffTag_RegisterTechnique(name)` (0x4AFE20) call per technique in all the effect files
(the 34 names below). Parameter and texture names are the shader globals from the effect
files, so the register bindings here line up with `fx_compile.tsv`.

The visible draw kinds, with their techniques, parameters (name, source, address of the
resolving call) and stage textures. "State" is what the pass sets in the effect file (from
`fx_inventory.tsv`) plus any `SetRenderState`-level state the caller sets around the draw.

## CG sprite (character object layer)

| technique | chosen by | parameters | textures |
|---|---|---|---|
| `TecCharaDraw_V` | layer bg-colour-filter mode 0, no filter, light off | `g_mWorld`, `g_mView`, `g_mProj` (transform), `g_GrayScale` (1.0 when the layer requests it), `fPalPlusV`, `tex_Palette` | palette |
| `TecCharaDrawBgColor_V[_Grad]` | mode 0, `*(mode)!=0` | `g_fPalCount`, `g_fZeroPosV`, `g_fTexel`, `fPalPlusV` | palette |
| `TecCharaDrawBgColorFilter_V[_Grad]` | mode 0, filter set | `g_fCharaBgBokashiAlpha`, `g_fStepTexture1DotSizeW/H`, `g_fCharaBgStandardPixelHeight` | palette |
| `TecCharaDrawBgColorSpecular_V[_Grad]` | mode 0, specular | `g_fCharaBgSpecularBase`, `g_fCharaBgSpecularHeight` | palette |
| `TecCharaDrawBgColorSpecularFilter_V[_Grad]` | mode 0, specular + filter | as above | palette |
| `TecCharaDrawBgColorBokashi_V[_Grad]` | mode 0, bokashi | `g_fCharaBgBokashiAlpha`, `g_fStepTexture1DotSizeW/H` | palette |
| `TecCharaDraw_ShadowV[_TEX]` | shadow draw | `g_fPtLastColor` | palette |
| `TecCharaDraw_FillV` | fill colour | `g_fFillColor` | palette |
| `TecCharaDrawEx` | CG draw that needs the palette copy, `fPalPlusV` | `fPalPlusV`, `tex_Palette` | palette |

Callers: `CharaObj_DrawLayer` (0x4BF100) picks the technique from the layer's bg-colour-filter
mode and sets `g_fCharaBgBokashiAlpha` / `g_fStepTexture1DotSizeW,H` / `g_GrayScale`;
`CharaObj_DrawWithStageLight` (0x5CD810, the brief's 0x5CE3C0) does the same for the
stage-light path; `CharaObj_DrawEx` (0x4BFCE0) sets `fPalPlusV` and `tex_Palette`.
Parameter source: fields of the character object (`this+72` = the render object, `+76` = the
bg-colour-filter struct), the compiled matrix at `this+56/60`, and the palette texture at
`tex_Palette` (0x942714). The layer object is `this+28` (mode) and `this+32/44` (gradient
factors). Pass states: none set in the effect file; `byte_9F1AFD` / `byte_9F1AFC` drive blend
at the draw-range level. **Stage light colour and aura are not simulated by the editor**;
they use the training-on-stage-1 neutral values (see the P3 step 4 note below).

## PAT part

| technique | chosen by | parameters | textures |
|---|---|---|---|
| `TecCharaPtDraw_V` | part-set mode 0/1 (normal) | `g_mWorld` (composed world), `g_mView`, `g_mProj`, `g_fPtLastColor` | palette |
| `TecCharaPtDraw_FillV` | part-set mode 0xA (fill) | `g_mWorld`, `g_fPtLastColor` | - |
| `TecCharaPtDraw_ShadowV` | default (shadow) | `g_mWorld` | - |
| `TecCharaPtDraw_ShadowV_TEX` | part-set mode 0x65 (shadow + texture) | `g_mWorld` | - |

Callers: `CharaPt_DrawLayerTechniques` (0x4C0930) sets `g_mView`/`g_mProj` once per layer,
then `CharaPt_DrawPartSet` (0x415630, the brief's `PatPart_ComputeSpecular_PRSPPlusObjAdd`
0x413BA0 is reached from here) sets `g_mWorld` (the composed matrix at `Src`: base *
`this+48` * part world `a2[13]`) and draws each part with `DxEffTag_DrawRangePrimsEx`.
The technique string is `sub_415630`'s `a2[7]` (the part-set mode); the technique table entry
`TecCharaPtDraw_*` is at 0x9425FC..0x942660. Pass states: none in the effect file (blend is
the CDxEffTag level).

## PAT mask and blur (character aura)

| technique | caller | parameters | textures |
|---|---|---|---|
| `TecCharaPtMaskCopy` | `CharaPtMask_Copy` (0x874A80) | - | `tex_Base`, `tex_Mask` |
| `TecCharaPtBlurDrawW` | `CharaPtBlur_DrawHAndW` (0x874550) | `fBlurTexel` = (1/w, 1/h)*blur | `tex_BlurBase`, `tex_Mask` |
| `TecCharaPtBlurDrawH` | same (after the W pass) | `fBlurTexel` | `tex_BlurBase`, `tex_Mask` |
| `TecCharaPtBlurMakeMask_V` | `CharaPtBlur_MakeMaskPass` (0x4C0EB0) | - | `tex_Mask` |

States: the blur techniques set `AlphaBlendEnable=True, AlphaTestEnable=False, ZEnable=False,
ZWriteEnable=False` (effect file). `CharaPtBlur_DrawHAndW` reads the blur radius at
`dword_9986A0` and the base texture at `xmmword_3F3CDD0`, then draws 1280x720 regions.

## Blur / mono / fade / noise / gauge (full-screen effect passes)

| draw kind | technique | caller | notes |
|---|---|---|---|
| chara point blur (aura vertical/horizontal) | `CharaPointBlur_W` / `CharaPointBlur_H` / `_MagFilter` | registered at 0x874BF0 (string 0x955BC4/0x955BF0/0x955B88) | the "blur" draw kind; loaded from `sh_blur.txt`, states `AlphaBlendEnable=True, ZEnable=False, ZWriteEnable=False` |
| monochrome (talk / win-quote) | `TecTalkMono` | `TalkMono_Draw` (0x8BEFE0) | technique string 0x955B58 |
| 2-tone overlay | `Tec2Tone` / `Tec2Tone_Alpha` | 0x874BF0 registered (0x955BA4/0x955B78) | |
| texture mask | `TecTexMask` | 0x874BF0 registered (0x955B4C) | |
| fade | not applied through `DxEffTag` in this build | `FadeProc_Set` (0x956ED4) | the fade is drawn by the screen-fade object (`CDrawScreenFade`), not the effect library; **unresolved**: I could not find an `ID3DXEffect` technique call for it in UNI2 1.42 |
| noise | not applied through `DxEffTag` in this build | **unresolved** | no `TecNoise` string in this build (the file exists in `Shader/` but no name constant matches) |
| gauge (magic circuit) | not applied through `DxEffTag` in this build | **unresolved** | no `TecMagicCircuitDraw` string in this build |

## BG object / BG specular / BG bloom

| technique | caller | parameters | textures |
|---|---|---|---|
| `TecBgObjectMono` | `BgObject_Draw` (0x519DE0), when `byte_A46451` | - | `tex_Base` |
| `TecBgPaniSpecular` | `BgObject_Draw`, when `dword_A46498` | `g_fBgSpecular` = (B,G,R)/255 from `dword_A46498` | `tex_Base` |
| `TecBgSpeculer` / `TecBgNoSpeculer` | `BgStage_Draw` (0x51B8C0) | `g_GrayScale` | `tex_Base` |
| `bloom` / `TinyFXAA` | `Bg_ApplyBloomPost` (0x518460) and `BgStage_Draw` | `g_Blightness`, `g_Power`, `g_BiassR/G/B`, `g_BlurRadius`, `g_OnePixelUV` (bloom); `g_Threshold`, `g_LerpT`, `g_OnePixelU` (FXAA) | ping-pong targets |

`BgStage_Draw` / `BgObject_Draw` are the stage call sites; `Bg_ApplyBloomPost` is the stage
bloom. States: `BgStage_Draw` sets the cull mode (`Src` = 0) around the draw.

## FBX (3D model in the stage view)

The FBX path uses `fbxshader.txt` (`Diffuse_VS` / `Diffuse_PS` / `..._Easy` / `..._NT` /
`DiffuseBone_VS`: `vs_3_0`/`ps_3_0`), with `g_mWorld`, `g_mView`, `g_mProj`,
`g_cameraPos`, `g_lightPosition`, `g_lightDeffuse`, `g_lightAmbient`, `g_lightSpecular`,
`g_mateDeffuse`, `g_mateSpecular`, `g_matePower`, `g_mateIntensity`, `diffuseSampler`, and for
bones `g_mWorldArray[80]` (from the `fx_compile.tsv` constants). `unresolved`: I did not trace
the FBX draw function itself in this session (it is a stage-view path, P4's `fbxshader` topic);
the parameter set above is read from the compiled constant table.

## Values the editor does not simulate

Stage light colour, the chara aura, and the BG bloom parameters are gameplay state the editor
does not reproduce. Per the plan, they default to the neutral values the game uses in training
on stage 1, recorded here:
- `g_GrayScale` = 0, `g_fCharaBgBokashiAlpha` = 0, `g_fCharaBgSpecularBase/Height` = 0;
- `g_fBgSpecular` = (1,1,1);
- bloom off (`BGBloomEnable` false).

These are exposed under View > Game shaders in P3 step 4; changing them is out of scope for
this lane.

## Addresses renamed in the IDB

The renamed functions and their addresses are listed at the top of this file's "Per draw kind"
tables. The IDB (`uni2.exe.i64`) is saved with these names and comments; no `sub_` remains on
anything touched.
