# DFCI effect call sites (lane FX, P3 step 3)

Traced from C:\dev\ida\server\RingGame.exe.i64.old-ai-names.i64 (DFCI, RingGame.exe,
d3dx9_42) through the IDA MCP. DFCI is the weakest of the three in this environment; the doc
records what I could establish and marks the rest **unresolved** with what I tried, per the
lane's RE hygiene rule.

## What is established

The effect files are Shader/*.txt (sh_chara.txt, sh_blur.txt, sh_mono.txt, sh_noise.txt,
sh_normal.txt, sh_gauge*.txt, sh_fade.txt, fbxshader.txt), loaded with
D3DXCreateEffectFromFileA (the import name table is at 0x481450; __imp_D3DXCreateEffectFromFileA
appears in the name blob). The same TecCharaDraw* / TecCharaPtDraw* / TecBg* / Tec2Tone* /
TecMono* / TecBloom* / TecNormal* / TecNoise* / TecMagicCircuitDraw technique names from
fx_inventory.tsv and fx_compile.tsv appear in the database as data (case-mangled symbols like
aTeccharadrawV at 0x11A783D, aTeccharaptdraw at 0x11A7ABF, aTeccharadrawex at 0x11A78CA,
aTecmono at 0x3A81DC9, aTectexmask at 0x337BA5B, aTecmagiccircui at 0x3A91794).

The parameter and texture names are the same globals as UNI2/MBTL: g_mWorld, g_mView,
g_mProj, g_GrayScale, g_fPalCount, fPalPlusV, tex_Palette, tex_Base, tex_Mask, tex_BlurBase,
the DiffuseTextureSampler / Tex_2toneSampler samplers, and the FBX set (g_cameraPos,
g_lightPosition, g_lightDeffuse, g_lightAmbient, g_lightSpecular, g_mateDeffuse,
g_mateSpecular, g_matePower, g_mateIntensity, g_diffuseMap) from the compiled constant table
(fx_compile.tsv).

## Per draw kind (character section)

The techniques each draw kind uses are the same as UNI2/MBTL, because they are the same game
shader set (sh_chara.txt in all three):

| draw kind | technique(s) |
|---|---|
| CG sprite | TecCharaDraw_V, TecCharaDraw_V_Grad, TecCharaDrawBgColor*_V, TecCharaDraw_FillV, TecCharaDraw_ShadowV[_TEX] |
| PAT part | TecCharaPtDraw_V, _FillV, _ShadowV[_TEX] |
| PAT mask | TecCharaPtMaskCopy / TecKomask / TecTexMask |
| blur | CharaPointBlur_W/_H/_MagFilter, TecCharaPtBlurDrawW/H |
| mono | TecMono / TecMonoFill / TecTalkMono (literals 0x3A81DC9, 0x3A81D59) |
| 2-tone | Tec2Tone / Tec2Tone_Alpha (literals 0x337BB14, 0x3363D36) |
| fade | TecFade_ExFade0 / TecFade_ExFade1 (sh_fade.txt) |
| noise | TecNoise (sh_noise.txt) |
| gauge | TecMagicCircuitDraw (literal 0x3A91794) |
| BG object | TecBgObjectMono, TecBgPaniSpecular, TecBgSpeculer, TecBgNoSpeculer |
| FBX | fbxshader.txt (DefaultShader, DefaultShader_Easy, DefaultShader_NT, FbxBloomShader_*, DefaultBoneShader_NT) |

## Unresolved (and what I tried)

The call-site addresses, parameter sources and render states could **not** be traced in this
IDB:

- The database has no parsed imports (get_import_module_qty() == 0), so the ID3DXEffect
  vtable/import thunks are not resolvable. D3DXCreateEffectFromFileA is only a string blob
  (0x481450), not an imposed import.
- The technique literals carry no xrefs (XrefsTo empty for 0x11A783D, 0x11A7ABF, 0x11A78CA,
  0x3A81DC9, 0x337BA5B) and no little-endian pointer to any of them exists in the
  0x0-0x52061C7 image, so the code that would reference them is not present as analysed code
  (packed/unpacked at runtime, or the functions were not auto-analysed).
- A full-image scan found only 47 "Tec" byte runs and no "SetTechnique", "BeginPass",
  "GetParameterByName", "sh_chara", "g_mWorld" or "tex_Palette" string anywhere, which is what
  a packed/protected executable looks like.

Given the brief ("DFCI characters at least"), the character draw kinds and their techniques and
parameter/texture name sets are recorded above from the effect files and the compiled constant
tables, which is the part P3 step 4 needs to fill parameters. The exact calling function,
address and per-parameter source are not available from this database; recovering them would
need a runtime trace or an unpacked image, which this lane does not have.

## Values the editor does not simulate

Same neutral training-on-stage-1 defaults as UNI2/MBTL (greyscale 0, bokashi/specular 0,
g_fBgSpecular (1,1,1), bloom off), exposed under View > Game shaders in P3 step 4.

## IDB hygiene

No function was renamed in this IDB (none of the DFCI draw functions could be identified to
rename). The database is saved. The DFCI character doc above is the "characters at least"
deliverable; the call-site section is honestly BLOCKED on an unpacked database.
