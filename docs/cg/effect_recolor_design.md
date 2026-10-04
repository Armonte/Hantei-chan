# Effect-sprite recolour: analysis and design (bake tool + PovertyCaster runtime)

Status: analysis + design, 2026-10-03. Nothing here is built yet. All numbers were measured by the scripts in `tools/fxrecolor/`
(output CSVs in `docs/cg/effect_recolor_data/`); the engine facts were read from `MBAA.exe` in IDA (functions renamed and the IDB saved) and
from the PovertyCaster tree (`pc-adapters/mbaacc`, `docs/games/mbaacc_shaders.md`). Credit: the manual pipeline reconstructed in section 3 is
DGV's "Better Akiha v2" project; this design automates that method.

## 0. Summary (read this first)

1. **"Effect sprite" is a property of the CG image type, and the rule is exact.** In `akiha.cg` the 749 images that DGV treated as Akiha's
   body are *all* type 0 (8-bit indices into the bank palette) and the 675 he treated as effects are *all* type != 0 (298 type 1 RGBA, 302 type 2
   indexed with their own palette, 75 type 4 own palette + alpha plane): 675/675 and 0 false positives. Across all 36 character banks the engine
   draws type 0 through the character's palette slot and everything else with colours baked into the image.
2. **Your hunch is half right.** In MB 2002 *every* character sprite (effects included) is 8-bit indexed with 8 palettes; the only true-colour bank was
   `EFFECT.DAT`. By ReAct/MBAC most effects were already "indexed with their own palette" (type 2) or RGB (type 1/3). MBAACC then converted a
   minority: ReAct type 2 -> MBAACC type 1 for 1015 images, 559 type 2 -> type 4 (alpha plane added); MB2002 8-bit -> MBAACC RGB/flat for
   ~1440 images, mostly in the effect-name range (sets 50+). So there is no big "indexed -> RGB" conversion; the real split is **follows the character
   palette (type 0 / future type 5)** versus **fixed colours (types 1-4)**, and the old games did not have the first kind for effects either.
3. **Why Aoko's orbs recolour and Arcueid's 22x does not:** Aoko's orb patterns (79-84, 204, 210-216, 488, ...: "stopped/moving orb") are type 0
   sprites on additive blend, drawing indices whose colours differ in every one of her 36 palette slots (mean RGB change ~55 per index): they follow the
   slot because they are *indexed into the character palette*. Arcueid's projectile/blade effects (pattern 203 "projectile part", 512, 400, 439, ... and the
   RGB blades 396/397/404/426) are type 4 / type 1: colours are inside the image, the slot is never consulted. A third case exists: Arc's "Hime" (116) and
   debris (237) are type 0 but use palette indices whose colour is *identical in every slot* (variance 0.0): they look fixed although they are indexed.
4. **Engine:** sprite pixels are converted to a 32-bit (or 16-bit) texture on the CPU **once, at CG load / palette change** (`CG_UploadAtlasPagesWithPalette`
   0x4020E0 -> `Texture_CopyConvert` 0x402EB0). There is no palette LUT in the shader. The draw is a fixed-function textured quad per layer (an optional
   per-quad D3DX effect exists only for 8-bpp character sprites). A draw-time recolour is feasible without touching sim state; the hook sits at
   `Obj_SubmitSpriteDraw` 0x41A390, where owner object, character, palette slot, pattern id and frame are all in hand.
5. **A cheap, important discovery for the bake tool:** the engine already implements **type 5 = 8-bit indices + alpha plane + the *global (slot)* palette**
   (decoded in `CG_ParseBank` 0x402970 and `CG_UploadAtlasPagesWithPalette`), but no shipped bank contains one. That is exactly "palette-following effect with soft
   alpha", the thing the bake tool needs for glows. Hantei-chan and cgm do not read/write type 5 yet; an in-game proof is a gate in the plan.
6. Recommendation: build the **runtime recolour first (2 weeks)** because it covers all 3000+ fixed effect sprites at once without editing banks and is the
   only way to give effect.cg (shared RGB effects) per-character colours, and build the **bake tool second (3-4 weeks)** for players on stock
   CCCaster/Hantei workflows. Both consume one recolour spec file (section 5.1). Decision questions are in section 8.

## 1. What the data says

### 1.1 Image types in a bank (`BMP Cutter3`, verified in IDA, see 2.1)

| type | payload | colours come from | follows palette slot? | texture page format |
|---|---|---|---|---|
| 0 | 8-bit indices | **global palette** (bank palette 0, overwritten by the `<char>.pal` slot at load) | yes | A1R5G5B5 (5-bit colour, **1-bit alpha**) unless the page also holds a non-type-0 image |
| 1 | BGRA | the pixels | no | A8R8G8B8 |
| 2 | 1024-byte own palette + indices | own palette (index 0 = transparent) | no | A8R8G8B8 |
| 3 | one colour dword + alpha plane | the colour (silhouette / tint / glow) | no | A8R8G8B8 |
| 4 | own palette + (index, alpha) planes | own palette, soft alpha | no | A8R8G8B8 |
| 5 | (index, alpha) planes, **global palette** | global palette, soft alpha | **yes** | A8R8G8B8 |

Type 5 appears in zero images of MB 2002, ReAct, MBAC and MBAACC (census below) although the engine decodes it.

### 1.2 Census, MBAACC (`/mnt/c/games/mbaacc/data`, 72 `.cg`, 36 playable banks + `effect.cg` + 35 `csel_*` which are all type 0)

Pattern classes (computed per effect pattern = a pattern spawned by an EF type 1/101/11/111/1000 of another pattern **or** using any type != 0 sprite;
columns: class decided from the types of the sprites the pattern draws, and for type-0 patterns by whether the palette indices it uses change between slots):
`follows` = type 0, colours vary by slot; `idx const` = type 0 but colour never changes in any of the 36 slots (looks fixed); `fixed idx` = types 2/4 only;
`RGB` = types 1/3 only; `mix f+x` = type 0 and fixed images in different frames; `mix x` = several fixed types. `free idx` = palette indices no type-0 sprite uses
(ramp space); `varying` = how many of those already differ between slots (the author already colours them per slot).

| bank | images | t0 | t1 | t2 | t3 | t4 | effect pats | follows | idx const | fixed idx | RGB | mix f+x | mix x | free idx | free idx that vary by slot |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| akaakiha | 1493 | 853 | 118 | 53 | 388 | 79 | 129 | 26 | 0 | 12 | 59 | 9 | 23 | 221 | 11 |
| akiha | 1424 | 749 | 298 | 302 | 0 | 75 | 109 | 9 | 0 | 23 | 17 | 15 | 45 | 215 | 1 |
| aoko | 1317 | 778 | 279 | 257 | 0 | 0 | 129 | 48 | 0 | 26 | 7 | 12 | 36 | 106 | 106 |
| arc | 1527 | 988 | 382 | 0 | 0 | 156 | 94 | 19 | 7 | 6 | 18 | 2 | 42 | 185 | 3 |
| b_arc | 1678 | 1032 | 475 | 147 | 22 | 0 | 116 | 30 | 6 | 3 | 16 | 2 | 59 | 185 | 3 |
| b_kohaku_m | 1256 | 681 | 57 | 73 | 4 | 0 | 40 | 22 | 0 | 6 | 6 | 1 | 5 | 205 | 21 |
| b_m_hisui_p | 1533 | 265 | 24 | 53 | 0 | 0 | 14 | 7 | 0 | 3 | 1 | 1 | 2 | 223 | 47 |
| b_ryougi | 1437 | 1232 | 176 | 11 | 14 | 4 | 147 | 84 | 3 | 1 | 47 | 8 | 4 | 152 | 3 |
| ciel | 1489 | 1255 | 36 | 0 | 15 | 182 | 97 | 21 | 0 | 22 | 3 | 46 | 5 | 191 | 3 |
| effect | 653 | 0 | 646 | 0 | 0 | 0 | 134 | 0 | 0 | 0 | 127 | 0 | 7 | - | - |
| Hermes | 182 | 30 | 51 | 2 | 34 | 65 | 30 | 0 | 10 | 6 | 9 | 3 | 2 | 206 | 0 |
| hisui | 1238 | 971 | 146 | 0 | 0 | 120 | 141 | 73 | 27 | 9 | 3 | 6 | 23 | 166 | 1 |
| kishima | 1301 | 739 | 455 | 94 | 0 | 11 | 74 | 13 | 0 | 4 | 11 | 1 | 45 | 223 | 1 |
| kohaku | 1256 | 979 | 129 | 118 | 4 | 26 | 100 | 41 | 0 | 16 | 19 | 5 | 19 | 175 | 2 |
| kohaku_m | 1256 | 772 | 96 | 74 | 4 | 15 | 85 | 35 | 6 | 10 | 13 | 3 | 18 | 180 | 0 |
| len | 1364 | 888 | 231 | 245 | 0 | 0 | 83 | 22 | 3 | 11 | 6 | 3 | 38 | 130 | 3 |
| m_hisui | 1533 | 887 | 163 | 465 | 0 | 18 | 171 | 86 | 0 | 38 | 11 | 11 | 25 | 204 | 34 |
| m_hisui_m | 1533 | 634 | 110 | 0 | 0 | 144 | 75 | 38 | 0 | 10 | 7 | 4 | 16 | 217 | 41 |
| m_hisui_p | 1533 | 637 | 136 | 406 | 0 | 0 | 119 | 40 | 0 | 28 | 12 | 16 | 23 | 213 | 38 |
| miyako | 770 | 644 | 66 | 47 | 0 | 13 | 30 | 8 | 3 | 6 | 8 | 0 | 5 | 103 | 0 |
| nanaya | 1052 | 740 | 21 | 0 | 0 | 289 | 57 | 15 | 0 | 36 | 2 | 1 | 3 | 231 | 0 |
| nechaos | 1116 | 646 | 89 | 0 | 190 | 188 | 127 | 75 | 0 | 15 | 13 | 20 | 4 | 172 | 13 |
| neco | 849 | 521 | 142 | 125 | 57 | 3 | 85 | 48 | 3 | 5 | 6 | 17 | 6 | 171 | 23 |
| neco_p | 849 | 231 | 66 | 11 | 0 | 0 | 159 | 45 | 3 | 1 | 2 | 61 | 47 | 193 | 0 |
| nero | 1326 | 983 | 43 | 0 | 145 | 150 | 115 | 66 | 0 | 14 | 11 | 12 | 12 | 191 | 6 |
| p_arc | 1087 | 556 | 428 | 86 | 1 | 16 | 132 | 44 | 1 | 0 | 20 | 8 | 59 | 214 | 0 |
| p_ciel | 929 | 670 | 183 | 32 | 4 | 38 | 119 | 42 | 0 | 16 | 32 | 14 | 15 | 176 | 1 |
| ries | 1154 | 734 | 290 | 73 | 1 | 56 | 98 | 11 | 0 | 26 | 28 | 3 | 30 | 174 | 0 |
| roa | 935 | 571 | 290 | 32 | 14 | 8 | 144 | 26 | 0 | 4 | 14 | 56 | 44 | 205 | 0 |
| ryougi | 1437 | 1232 | 176 | 10 | 15 | 4 | 152 | 89 | 3 | 0 | 48 | 8 | 4 | 152 | 3 |
| s_akiha | 1326 | 849 | 262 | 116 | 0 | 98 | 141 | 46 | 0 | 7 | 15 | 6 | 67 | 200 | 2 |
| satsuki | 1115 | 735 | 177 | 189 | 0 | 13 | 94 | 10 | 27 | 30 | 8 | 2 | 17 | 159 | 1 |
| shiki | 1167 | 764 | 34 | 0 | 1 | 315 | 53 | 10 | 0 | 19 | 2 | 4 | 18 | 232 | 2 |
| sion | 1154 | 869 | 119 | 146 | 0 | 11 | 46 | 20 | 0 | 6 | 5 | 3 | 12 | 162 | 140 |
| v_sion | 1494 | 1005 | 183 | 280 | 0 | 23 | 81 | 16 | 6 | 24 | 3 | 12 | 20 | 106 | 3 |
| warakia | 991 | 602 | 59 | 1 | 260 | 66 | 79 | 10 | 0 | 15 | 29 | 20 | 5 | 210 | 2 |
| warc | 1409 | 807 | 247 | 42 | 196 | 116 | 93 | 12 | 0 | 9 | 26 | 15 | 31 | 216 | 197 |
| wlen | 1857 | 939 | 528 | 343 | 0 | 44 | 153 | 33 | 3 | 36 | 17 | 7 | 57 | 168 | 1 |
| **total** | 47020 | 28468 | 7411 | 3833 | 1369 | 2346 | 3845 | 1240 | 111 | 503 | 681 | 417 | 893 | | |

Totals: 47 020 images (28 468 type 0 = 60.5 %, 18 552 fixed). `effect.cg`: 646 of 653 images are type 1 (RGB), no palette ever applies: all shared
hit/guard/clash/impact effects (`effect.HA6`, EF types 8/108 and presets) are fixed RGB for every character. 39.9 % of fixed effect sprites that are used at all
are drawn by more than one pattern (5604 of 14 039; Akiha 393/552, mean 3.0 patterns per sprite) - this is why per-pattern recolour needs sprite cloning in a bake.

Body sprites: in the checked bank (Akiha) 0 of 749 are non-type-0; the character bodies use only 24-153 of 256 palette indices, leaving 100-230 free
(Akiha 215, Arc 185, Hisui 166, Aoko 106, V.Sion 106, Miyako 103 - `docs/cg/effect_recolor_data/mbaacc/palfree.csv`). Free indices are `(0,255,0)` green in
many shipped palettes (Hisui 165 greens, Kohaku 174, P-Arc 214, Nero 185, Roa 203): **the "green background" convention is "unused palette entry = pure green"**;
it is also how DGV's `.act` files mark unused indices. Aoko's free area is not green: all 106 free indices already differ per slot, which is exactly the range her orbs/lasers use.

### 1.3 Per-pattern table

Every effect pattern of every character with its class, reason, slot variance, blend mode, sprite types and spawn parents:
`docs/cg/effect_recolor_data/mbaacc/effect_patterns.csv` (4086 rows; columns `bank,pid,name,spawnedBy,class,nsprites,types,slotVar,blend,hurt,atk,spriteIds`).
The class column answers "follows palette / fixed indexed / RGB" and `slotVar` gives the reason for `follows` vs `idx const`.

Worked examples (from that CSV):

| pattern | what | sprite types | slotVar | blend | verdict |
|---|---|---|---|---|---|
| aoko 79/80/81, 82-84 | stopped / moving orb (weak, strong, EX) | 0 | 59 / 51 / 55 | additive | follows palette (indexed, ramp changes per slot) |
| aoko 204, 210-216, 488 (mine), 565 | orb variants, appear, mine | 0 | ~55-60 | additive | follows palette |
| aoko 120, 125, 145 | laser, wave, EX laser | 2 | n/a | additive | fixed indexed (own palette) |
| aoko 110, 124, 127, 160, 162 | lasers, light orb, wire orb | 1+2 | n/a | additive | fixed (mixed RGB + own palette) |
| arc 203 "projectile part", 512 "slash eff", 400/401 blades, 439 EX claw | arc effects | 4 | n/a | additive/none | fixed indexed (own palette + alpha) |
| arc 396/397/404/426/437/438 blades | RGB blades | 1 | n/a | additive | fixed RGB |
| arc 116 "Hime", 237 debris | | 0 | 0.0 | normal | indexed but palette-constant |
| effect.HA6 (shared) | every spark/impact | 1 | n/a | mixed | fixed RGB, owner palette irrelevant |

(I could not confirm which pattern ids the community means by "Arcueid 22x"; every Arc pattern that is an energy/projectile effect is type 4 or 1, so the explanation
holds for whatever they are. Please give me the pattern id or a screenshot to pin it.)

### 1.4 Older games (names matched case-insensitively per character; `history_census.csv`, `history_name_diff.csv`, tool `history_census.py`)

| game | images | notes |
|---|---|---|
| MB 2002 (18 DATs) | 13 733 | **all 8-bit indexed** (8 palette slots, only 5 distinct) except `EFFECT.DAT`: 354 images 24-bit BGR |
| ReAct (23 DATs) | 21 026 | t0 14 193, t1 1385, t2 4508, t3 862, t4 68 |
| MBAC (02.p, 50 banks) | 26 064 | t0 17 706, t1 3060, t2 3503, t3 1557, t4 212 |
| MBAACC (compared) | 29 678 | t0 19 125, t1 4591, t2 2707, t3 1256, t4 1898 |

Transitions by sprite name, old -> MBAACC:

| from | to t0 | t1 | t2 | t3 | t4 | note |
|---|---|---|---|---|---|---|
| MB2002 idx8 (11 743 matches) | 8520 | 789 | 817 | 652 | 697 | sets 50+ (effect range): of 3431 matched, 1728 -> t0, 569 -> t2, 459 -> t4, 369 -> t1, 263 -> t3 |
| MB2002 24-bit EFFECT | | 209 | | | | whole overlap stays RGB |
| ReAct t0 (13 076 matches) | 12 535 | | | 213 | 287 | 541 left type 0 |
| ReAct t2 (3822 matches) | | 1015 | 2005 | 193 | 559 | **the biggest indexed->RGB move** |
| MBAC t0 (15 904 matches) | 15 147 | 223 | 259 | | 243 | 757 left type 0 |
| MBAC t2 (2943 matches) | | 226 | 2108 | | 602 | |

Reading: (a) MB 2002 effects were indexed through the character palettes, so "old effects follow palettes" was true there; (b) ReAct/MBAC already moved most effect
art to own-palette (t2) or RGB; (c) MBAACC mostly *kept* those types (t2->t2 2005/2108, t1->t1 1101/2532) and added alpha planes (t2->t4); only ~1000 ReAct
sprites went t2 -> t1. Not a mass conversion, and it cannot be reversed from the shipped data: the palette-following information of the old effects is gone from MBAACC
(own palette = one fixed colour set). MBAC also has the `EFFECT` bank as t1 (225 of 308 matches) already.

## 2. The engine (MBAA.exe, IDA names applied, DB saved)

### 2.1 Loading and palettes

* `CG_ParseBank` 0x402970 (was sub_402970; `CG_ValidateAndCountBank` 0x402840 pre-counts): a CG object (0x330C bytes) holds image records (88 bytes: the 72-byte file header
  + palette pool offset +72, pixel pool offset +84), the cell/block table (12 bytes per block), a **pool of own palettes** (type 2 and 4: 256 dwords, type 3: one dword, other types: none)
  and the pixel pool. **Only bank palette 0 (1 KB at CG+0x10) is copied**; palettes 1-7 of the file header are ignored by MBAACC. Cell unit (16/32/8) comes from the header.
* `CG_UploadAtlasPagesWithPalette` 0x4020E0 (called with `eax` = the 1 KB palette it copies to CG+0x10; callers at 0x4180FB, 0x42FE76, 0x448C8F, 0x489A9E, 0x4B6F8F): for each
  256x256 page it creates a texture through `D3D_CreateTextureWithRetry` 0x4BF060 - pages **below** the first page that contains any non-type-0 image get format 25 (A1R5G5B5),
  later pages 21 (A8R8G8B8) (26 = A4R4G4B4 when the desktop is 16 bit). Then for each image/block it calls `CG_UploadCellPalettized` 0x403570 with a mode and a palette pointer:
  type 0 -> mode 0 + global palette; 2 -> mode 0 + own palette; 3 -> mode 1 + colour dword; 4 -> mode 2 + own palette; **5 -> mode 2 + global palette**; 1 -> BGRA copy.
  The conversion `Texture_CopyConvert` 0x402EB0 is a CPU loop: P8 source -> `A8R8G8B8` with alpha = (index != 0), or 40 (A8P8) with alpha from the palette entry, or the 16-bit
  packers (5-bit channels, a non-zero channel is forced to >= 1).
* So **palette slots are applied by re-uploading the atlas pages** (CCCaster/PovertyCaster palette hook: `LoadVectorFile` epilogue 0x41F873 patches the `.pal` buffer;
  `MbaaccSim_Palettes.cpp`). Palette effect on type 1-4 images: none.

### 2.2 Draw path

`Obj_SubmitSpriteDraw` 0x41A390 (cdecl, a1 -> {object, ...}) builds a draw record and calls `Sprite_BuildDrawRecordAndEmit` 0x407420 (edi = record) -> `Sprite_EmitTransformedQuad` 0x406AE0
(cdecl, record). Other callers of 0x41A390: 0x41B3C9/0x41B47C/0x41B58A (0x41AF10), 0x41B919 (0x41B7F0), 0x454639/0x4546E5/0x454807 (0x454130). `Sprite_EmitTransformedQuad` is also called from `sub_4B7060` (stage/background objects).

Facts at draw time (object = `*a1` of 0x41A390):

| datum | where |
|---|---|
| pattern id | `obj+0x0C` (index into the HA6 pattern vector `**(obj+0x32C)+4`) |
| frame index | `obj+0x10` |
| HA6 / CG owner | `obj+0x32C` -> {HA6 ptr, CG ptr, ...}; the record gets HA6 = a1[17], CG = a1[18/19] |
| character id | byte `obj+1` (indexes `g_charDescContainer`, whose +124 == 2 marks a special case) |
| team/side | byte `obj+4` |
| owner slot | byte `obj+0x2F0` (752) indexes the per-slot battle table (stride 0x2C from 0x74D838); the palette selected for that owner is at slot+0x04 (`BATTLE_SLOT_PALETTE`) - to be re-verified in the spike (the code reads slot+0x20 for a colour tint, not the palette) |
| sprite (image) id | record+56 (`v30`), image record = `CG[0xCBD] + 88*CG[id+260]` in `Sprite_EmitTransformedQuad`; image bpp at +44 |
| blend preset | record+48 / AFAL: 1 normal, 2 additive, 3 subtract, 4 multiply... (PovertyCaster `FrameLayers.hpp`) |

Pixel state: `Sprite_EmitTransformedQuad` emits one quad per layer. **Only images with bpp == 8 and no outline flags use the D3DX sh_chara techniques** (`TecCharaDraw`,
`TecCharaDrawEx` = "Ex" filter, `TecCharaDraw_Edge`), selected with `EffectTagRegistry_GetOrCreateByName` 0x4BEAA0 (eax = technique name) and parameters
`DxEffParam_Create` 0x405520 ("fTexelN", "fTexelX", "fEdgeColor"); the per-quad instance index is stored in the quad state and applied by `RenderBuffer_FlushAndDraw` 0x4BE290.
**32-bpp effect sprites are drawn fixed-function** (texture * vertex colour, blend preset). So there is nothing to "extend": a recolour has to *add* an effect path for the effect
quads, using the game's own tag mechanism (a second technique registered through `ShaderEffect_LoadTechniqueToSlot` 0x4BEB50, the seam PovertyCaster's
`MbaaccSim_ShaderTakeover.cpp` already owns). PovertyCaster already reproduces all 18 game techniques bit-exact (`PARITY.txt`).

### 2.3 Netplay / sim safety

The whole chain above reads game state and writes only draw-command memory, texture pages and the D3DX effect instance list (freed every frame by
`EffectTagInstances_ClearFrame` 0x4BE8B0 at 0x40E500). It never touches the player/object structs or RNG. The only caveat is PovertyCaster's interpolation canary
(`MbaaccSim_InterpCanary`, replays the frame's draw list after the present: new effect + old instance handles): the recolour hook must be a pure function of (object, record) with no
state kept between calls, which it is by design. Peers may therefore see different colours; the hash covers sim state only (`SimAffecting::No` patch tables).
A load-time recolour (alternative B below) changes textures only, equally safe.

## 3. DGV's method, reconstructed from his data

Project: `Better Akiha v2` (10 596 files). Stages (folder -> content), all counts measured:

| folder | files | content |
|---|---|---|
| `01-1 ACTUAL SPRITES` | 1424 | every `akiha.cg` image as exported by cgtool (`ID_n_name.bmp.png`): 1126 mode P (749 body + 377 own-palette effects), 298 RGBA (type 1) |
| `01-1 RELAVENT ACTUAL SPRITES` | 1425 | the same split by hand into `AKIHA ONLY` 749 / `EFFECTS ONLY` 675 / `the singular bg` 1. **The effect list is exactly the non-type-0 images.** |
| `01-2 FRAME DISPLAY SPRITES` | 1424 | Hantei-chan "display" renders by sprite name: effects converted to RGBA (675), body stays P |
| `01-3 Partitioned Display Sprites` | 1383 | **the grouping step**: effect sprites hand-sorted into folders = recolour groups: `00` 134, `01` 51, `02` 32 (grayscale groups, saved as LA = luminance+alpha), `10` 72, `20` 131, `21` 18, `30` 170, `80` 66, `90` 1 |
| `02-1 modded display sprites` | 1433 | the same sprites **indexed** (mode P), per group, with that group's `.act` as palette |
| `03 Renamed modded sprites` / `05 FINAL PIECE` | 675 / 1424 | renamed back to `ID_n_...` for cgtool make; final = 749 body (bank palette) + 675 effects (8 distinct group palettes) |
| `*.act` (11) | | Photoshop palettes: `00 Red`, `01 Inverted Red`, `02 Grayscale Red`, `11 Red t Pink no deep`, `22 Red t Pink`, `30 crimson`, `90 last arc`, tests |

The `.act` files are 256 colours + 4 trailer bytes (count 0x0100, transparent index 0); **every unused index is `(0,255,0)` green**, index 0 is transparent, and the ramps live in 48..255:
`00 Red` = one white -> red -> black ramp on indices 48-108; `22 Red t Pink` = that ramp (48-124) plus a pink ramp on 179-208; `02 Grayscale Red` = a 28-step gray ramp
on alternating indices 49-107; `30 crimson` = 183-255; `90 last arc` = the full 48-255 union (it is the master palette: group palette 8 of the final set equals it exactly).

**Indexing rule (verified by re-computation on 25 files per group, ~0.1-0.8 M pixels each):** for the RGBA groups (00, 10, 20, 21, 30, 80, 90) the stored index is
**exactly the nearest palette colour in squared RGB distance among the non-green, non-zero entries, no dithering**: 100.0 % exact for groups 00/10/20/30/21, 99.4 % for 80,
i.e. Photoshop "Indexed Color > Custom palette, no dither". Because the artist first built each group's ramp from the sprite's own gradient (pick the colour in Photoshop, make the gradient in PalMod),
this is equivalent to **"V (max channel) -> ramp position"** for monochrome groups (97-99 % match when the ramp position is chosen by the max channel). For the grayscale groups
(01, 02: LA images) the rule is "gray level -> nearest gray step" (89-91 % within 3 indices, 53-55 % exact: Photoshop's gray conversion differs from a pure Euclid nearest by a gamma/rounding detail, not worth copying).
Multi-colour groups (20, 80) are *not* a function of luminance (index std 33-55 within a luminance bin): the ramps are chosen by hue. This is the "rainbowify -> rainbow palette ->
swap the rainbow palette for the wanted ramps" trick: map hue to disjoint index ranges first, then recolour the ranges in the palette.

Group palettes overlap (indices 48-108 are in five different palettes; 67 indices in the final set carry different colours in different groups), and `cgtool make` writes type 0 only with *one*
`-pal` palette, so the final bank could only carry one resolved union palette; the per-slot `.pal` files are not in the archive. We therefore do not know how he coloured the 36 slots; the design below makes that explicit.
He also gave up ("stuff that didnt make the cut": 675 P sprites) on some alternatives. Limits of the manual pipeline: per-sprite grouping by eye, 8-bit alpha lost (type 0 pages are A1R5G5B5 with a 1-bit alpha, and
cgtool has no type 2/4/5 writer), no re-pointing of frame layers, and every change means a full Photoshop pass.

## 4. Bake tool design (Hantei-chan CG manager, `tools > Effect recolour`, plus CLI `cgmtool fxbake`)

### 4.1 Inputs / outputs

Input: a bank (`.cg`) + the character's HA6 files + `<char>.pal` + a **recolour spec** (5.1). Output: a modded `.cg` (indexed sprites, optional cloned images), edited HA6
(frame layers re-pointed, only when clones were needed) and an edited `<char>.pal` (ramp colours written into all slots). Always previewable in the Hantei renderer; one undo step per bake.

### 4.2 Auto-classification (step 1)

1. Effect image := `type != 0` (exact rule), plus type-0 images used only by effect patterns (spawn targets or non-body ids), flagged `already follows palette`.
2. Per effect *pattern*: class from the census (follows / idx const / fixed idx / RGB / mixed) with the reason (types, slot variance), spawn parent, blend mode, sprite list. This is the table in section 1.3 computed live.
3. Group candidates: cluster the opaque pixels of each image in OKLab -> "colour signature" (up to 4 dominant hue/luma clusters); images with similar signature (same hue family, same luma range) are
   suggested as one group (DGV did this by eye: 9 groups for 675 sprites). A group's signature feeds the default ramp.

### 4.3 The review grid (step 2)

Thumbnail grid of all effect images sorted by group, group colour bar on the left (editable ramp preview), per tile: id, name, type, patterns using it (click to jump), `follows/fixed`, a
"shared by N patterns" badge. Actions: drag tiles between groups, split group, per-group mapping (below), per-group ramp (auto-extracted or picked from the character's palette), per-region mask (paint with
the existing hitbox-like brush, region 1..4), live preview with a slot selector (shows the result under each of the 36 palettes), "apply to patterns..." scope picker (4.6), Accept -> bake.

### 4.4 Mappings

* **Luminance -> ramp** (DGV's grayscale trick). Value v = max(R,G,B) (default; Y = Rec.601 selectable) of the pixel, position t = v normalised to the group's [vmin,vmax] (robust percentiles),
  index = ramp[round(t * (n-1))]. Ramp = N colours (default auto: median-cut of the group's own pixels sorted by luma, N <= 64, so an unmodified bake reproduces the sprite within quantisation;
  or a user gradient from 2-5 stops, or a range copied from a body ramp). Equivalent to Photoshop's nearest-colour when the source lies on the ramp (verified at 100 %).
* **Rainbow remap**. Convert pixel to (hue class h in K bins, value v in M steps). K x M cells become one *2-D index block*: index = base + h*M + t(v). The ramp for hue class h is chosen
  afterwards, per slot, so a "red + pink" or "fire + smoke" sprite keeps two separate gradients (index std within a luminance bin was 33-55 in groups 20/80, exactly this). K default 6, M default 16 (= 96 indices);
  region masks can override the hue binning. Achromatic pixels get their own class. Two-pixel-wide anti-aliased borders are assigned to the dominant neighbour class to avoid colour fringes.
* **Hue-preserving "keep"**: groups marked keep stay as-is (no bake) - the runtime shader handles them.
* No dithering anywhere (DGV's results are undithered; dithering destroys flat effect art).

### 4.5 Palette allocation and per-slot colours

1. Ranges: candidate free indices per bank from `palfree.csv` logic (indices no type-0 sprite uses, excluding index 0). Allocation puts each *distinct ramp* in a contiguous run, ordered by group; identical ramps are
   shared; runs avoid the indices already used by body sprites. Capacity is not an issue (>= 103 free indices in all 36 banks, median ~190; the biggest need measured: Akiha 9 groups x ~60 = within 215).
   If a request does not fit: fall back to fewer ramp steps (halve N) and report.
2. Per-slot colours: for slot s (0-35) a ramp's entries are produced by a **slot transform** on the group's base ramp: `hueShift`, `satScale`, `valueScale`, `gradient map (2-5 stops)` or
   `copy range from the body palette of slot s` (so "the effect has the same accent as that slot's clothes"). Presets: *follow body accent* (auto: the slot's dominant body ramp, found by the
   census-style histogram of body indices), *fixed* (same in all slots), *authored* (table of stops per slot). Output is written into `<char>.pal` slots 0-35 (+ bank palette 0).
   Slot count (36 colours + 28 unused of 64) is read from the `.pal`, not assumed.
3. Soft alpha. Type 0 pages are A1R5G5B5 (1-bit alpha, 5-bit colour): glows/smoke with soft alpha cannot be type 0. Bake target per image: *hard alpha OK* -> type 0;
   *soft alpha* -> **type 5** (indices + alpha plane + global palette, engine-supported) after the in-game proof; *keep soft RGB* -> leave as type 1/4 and let the runtime shader handle it.
   The tool must add type 5 read/write to `CG`/cgm (Hantei renderer: P8 + alpha + `palette`, same as the type 4 path with the bank palette).
4. Hard alpha is thresholded per DGV's rule (index 0 is transparent; alpha > 50 % -> opaque) with a warning count of dropped half-transparent pixels.

### 4.6 Per-pattern recolour in the bake output

Scope of a recolour rule = a set of (pattern id, frame range) pairs, or "all patterns using these sprites". Algorithm:
1. Build the usage index (cgm `cgm_usage`): sprite -> {(pattern, frame, layer)} (this tool's `usage.py` is the prototype: it reads AFGP/AFGX layers of main + moon-variant HA6 files).
2. For each sprite S to recolour with different results for different patterns P1..Pk (e.g. Akiha 22A vs 22B sharing sprites): create clone images S_2..S_k appended to the bank (name `S~recolorN`,
   own cells; pixel-dedupe copy blocks that pointed *into* S are dependants and must be cloned or unshared first, see cgm design section 1: "pixels of image A can be drawn by image B"), indexed with
   the rule of that pattern group. S itself keeps the recolour of the first group.
3. Re-point the AFGP/AFGX layer of every frame of P2..Pk from S to S_n (one undo step, same machinery as cgm M5 renumber/fix-up). `usePat` layers are skipped (the usage index excludes them).
4. Dedupe: two rules that yield the identical index image share one clone. Report: images added, KB added, patterns touched (a typical Akiha bake: 393 shared sprites, but only those whose patterns land in different groups are cloned).
5. Spawn inheritance: children spawned by a pattern are **not** implicitly covered; the scope picker can add "and patterns it spawns" using the same spawn graph (`usage.spawn_targets`).
6. Effects drawn from `effect.HA6` use `effect.cg`: baking those means touching a bank shared by all characters, so rules for effect.cg are limited to "all characters" (recommend runtime instead, section 6).

### 4.7 Quality gates

`export unchanged -> byte-exact`, bake with the identity ramp -> pixel error <= quantisation bound, bake of Akiha group 00 with the `00 Red.act` reproduces DGV's `02-1` indices (100 % on RGBA groups - the regression
test comes straight from the zip), clone + re-point round trip, slot preview equals game render (`pc_shot.sh`), cgm suite sections.

## 5. Shared recolour spec

### 5.1 Format (JSON, versioned, one file per character, same file read by the bake tool and the runtime)

```
{ "version":1, "char":"aoko", "bank":"aoko.cg",
  "ramps":{ "orbA":{ "stops":[[0.0,"#ffffff"],[0.5,"#4fa3ff"],[1.0,"#000"]], "n":48 }, ... },
  "rules":[
    { "id":"arc-projectile", "match":{ "patterns":[203,512], "spriteTypes":[4], "palettes":"*", "blend":"any" },
      "map":{ "kind":"lumRamp", "by":"max", "ramp":"orbA", "slot":{ "kind":"followBodyAccent" } } },
    { "id":"fire-rainbow", "match":{ "sprites":[804,805], "palettes":[0,1,2] },
      "map":{ "kind":"rainbow", "hueBins":6, "steps":16, "ramps":["r0","r1","r2","r3","r4","r5"] } } ],
  "bake":{ "target":"type0|type5|keepRgb", "cloneForPatterns":true } }
```
`match` fields are ANDed; absent = any. The runtime reads `match` + `map` directly (shader); the bake tool consumes `map` + `bake`.

## 6. Runtime design (PovertyCaster, shader at effect draw time)

### 6.1 Config

`<game>/recolor/<charname>.fxr.json` (+ `recolor/shared_effect.fxr.json` for effect.HA6 effects keyed by *owner* character), loaded at battle start and hot-reloaded (`PC_UI_LIVE` style).
Keyed lookup: (character id, owner palette slot, HA6 source: main / moon variant / effect.ha6, pattern id, sprite id, image type, blend preset) -> rule index; precomputed into a small hash on load so
the per-quad cost is one lookup. Presets: per character `default` rule, per palette override list, per pattern override list.

### 6.2 Shader

New technique `TecRecolor` (ps_3_0, in the PovertyCaster takeover effect file next to `sh_chara`): inputs the quad's colour/specular like `TecCharaDraw` (`In.color`, `In.spec`), plus params:
`fMode` (0 off, 1 lumRamp, 2 rainbow, 3 hsvShift), `fRampRow` + `RampTex` (a 256 x R texture: R ramps, row = rule's ramp, built on the CPU from the rule + palette slot, sampler s1 linear clamp),
`fBy` (max/luma), `fVrange` (vmin,vmax), `fHueBins`, `fHsv` (hue shift, sat, val). lumRamp: `t = (by(src.rgb)-vmin)/(vmax-vmin)`, `rgb = tex2D(RampTex,(t,row))`; rainbow: `h = hue(src.rgb)`, bin -> row, same lookup;
hsvShift: HSV round trip. Alpha and the game's blend preset are untouched (colour only), so additive/subtract effects stay correct. The ramp texture is regenerated when the slot changes (palette select).
ps_3_0 is already in use for `TecCharaDrawEx`; no vertex shader needed.

### 6.3 Draw-hook keying (spike A: 2-3 days)

1. Detour `Obj_SubmitSpriteDraw` 0x41A390 (cdecl, `a1`): read `obj = *a1`; compute the key from `obj+1` (char), `obj+0x2F0` (owner slot -> `BATTLE_SLOTS_BASE + 0x2C*slot + 0x04` palette),
   `obj+0x0C`/`+0x10` (pattern/frame) and the HA6 pointer from the animation state `*(obj+0x32C)`; store it in a TLS "current draw context", call the original, clear it. All reads are bounds-checked with `VaReadable`-style guards (no SEH on MinGW).
2. In `Sprite_EmitTransformedQuad` 0x406AE0, where the game decides the technique (the `bpp == 8` branch, the call at 0x406DEE/0x406E7D), add a MidHook/branch: if the context has a rule and
   the image is 32 bpp (or any type != 0): `inst = EffectTagRegistry_GetOrCreateByName("TecRecolor")`, `DxEffParam_Create(...)` for the params above, and store the instance index in the quad exactly like the game does (`sub_4BE950`/`sub_4BEA20` pair at 0x406F01-0x406F07). This is the
   only new RE; the shape is proven by the existing chara path.
3. Register the new slot: call `ShaderEffect_LoadTechniqueToSlot` 0x4BEB50 for the technique name inside the existing takeover seam (it already substitutes our effect for the `.cfx` bytes), so `FindByName` resolves it.
4. Hygiene: batch cost. Each recoloured quad owns an instance and breaks the batch; effect-heavy frames have 100-300 effect quads. Mitigation: reuse the last instance when consecutive quads have identical params (the state byte is the instance index, so consecutive quads batch).
   Measure on a worst-case super (Akiha/Arc) before shipping; fall back to rule coalescing per pattern.
5. Also cover type-0 effects for *non-slot* recolours (e.g. a global "monochrome VFX" option): they are drawn by the same path with bpp == 8, i.e. only when the sh_chara branch is not taken; the context lets the hook skip them unless a rule matches.

### 6.4 Alternative B (no shader, 3 days, per-image only)

Hook `Texture_CopyConvert` 0x402EB0 / `CG_UploadCellPalettized` 0x403570 and apply the rule's gradient map to the *converted BGRA* of type 1-4 images during the atlas upload, keyed by (CG ptr -> character,
image id, palette slot). Zero per-draw cost and uses the exact bake mapping code, **but**: (1) per-image only - copy blocks make images share cells, so recolouring one image recolours its dependants (section 4.6),
(2) it needs a full atlas re-upload on every palette change (the game already does that), (3) no per-pattern granularity. Recommended only as a quick path or for `effect.cg` global tints.

### 6.5 Netplay

Local presentation. The hook reads the owner/palette of *whichever peer's* object it is drawing; two peers with different config files render different colours for the same battle; replays and spectators use the viewer's own config.
No config value ever enters a checksum, input, or sim write. Add a `fxr` section to the `SimAffecting::No` patch table registration so the harness proves it (digest identical with recolour on/off).

## 7. Effort and risks

| item | estimate | risk |
|---|---|---|
| spec + loader + keying lib (char, slot, pattern, sprite, blend) | 2 d | low |
| runtime hooks: spike (context + force-effect path + slot registration) | 2-3 d | **medium**: the 0x406AE0 branch (replacing the `v13` effect decision for 32 bpp) is new RE; owner-slot/palette field must be verified |
| shader + ramp textures + parity-style test for off-mode (must be bit-identical when no rule matches) | 2-3 d | low |
| UI: F-menu panel (pick rule preset per character/palette, reload) | 3 d | low |
| in-game verification (pc_shot, 5 characters, 36 slots, netplay loopback + hash) | 2 d | low |
| **runtime total** | **~2 weeks** | |
| bake lib: classify, group suggestion, mappings (lumRamp, rainbow), allocation, slot transforms | 5 d | low (math proven on DGV data) |
| type 5 reader/writer in cg/cgm + engine proof (a baked type-5 sprite on screen with a slot change) | 2 d | **medium**: untested engine path (A8P8 vs P8 alpha handling) |
| review grid UI (ImGui) + live slot preview | 5 d | medium |
| per-pattern clone + AFGP re-point (depends on cgm M5 fix-up) | 3-4 d | medium (copy-block dependants) |
| `.pal` writer, CLI, suite (identity round trip, DGV regression) | 3 d | low |
| **bake total** | **~3.5-4 weeks** | |
| fallback B (load-time per-image recolour) | 3 d | low |

## 8. Decision questions

1. **Order of work**: runtime first (2 wk) then bake (4 wk)? *Recommend yes*: the runtime fixes all 18 552 fixed effect images and effect.cg without bank edits, needs no cloning, and its config doubles as the bake spec.
2. **Which rule defines "effect"?** `type != 0` plus type-0 images used only by effect patterns. *Recommend yes* (exact on DGV's Akiha: 675/675, 0 false positives).
3. **Soft alpha in baked sprites**: use type 5 (engine decodes it, none shipped, Hantei lacks it), type 4 (own palette, no slot following), or dither/threshold to type 0 (A1R5G5B5, 1-bit alpha)?
   *Recommend:* run the type-5 proof first; if it works, type 5 for soft glows and type 0 for hard-edged; never silently threshold (warn with the dropped-pixel count).
4. **Per-slot colours for baked ramps**: derive automatically ("follow the slot's body accent") or only author them? *Recommend:* both - auto-suggest with a per-slot override table, because the shipped palettes have no consistent accent ramp (Aoko authored hers).
5. **Palette ownership**: the bake tool may write into the shipped `<char>.pal` slots 0-35 (free indices only, never body indices). OK? *Recommend yes*, with a diff report and a refusal if a requested index is used by any body sprite.
6. **Runtime key**: pattern id only, or also "inherit from the spawning pattern" (children of Akiha 22A recolour with 22A)? *Recommend:* pattern id + sprite id now, add inheritance after the spawn-graph shows how many children are not spawned by EF (hard-coded object spawns exist, so inheritance cannot be complete).
7. **effect.cg (shared RGB sparks)**: runtime only, keyed by owner character + palette. *Recommend yes*; baking would require per-character copies of a bank used by all.
8. **Distribution**: bake output is a 24 MB `.cg` per character; runtime config is KBs. Ship runtime configs as the default and bake as an "export for stock CCCaster" button? *Recommend yes.*
9. **Test set**: Akiha (DGV's data = regression oracle), Aoko (already recolours: proves "follows" path), Arcueid (fixed type 4 / type 1: proves the shader). OK? *Recommend yes.* Also: which pattern ids are "Arc 22x" / "Akiha 22A/22B" exactly?
10. **Credit and contact**: credit DGV's Better Akiha in the docs/UI; contact him for the `.pal` slots he shipped? *Recommend:* credit yes; ask only if you want his per-slot palette data.
11. **Quality knob**: type-0 pages are A1R5G5B5 (5-bit colour). Baked effects will have 5-bit colour banding that the original type 1 sprites do not. Accept for hard-edged art, keep gradients/glows as RGB+runtime? *Recommend yes*, tool warns per image.

## Appendix A. Files

* Tools: `tools/fxrecolor/{cgparse.py, ha6walk.py, usage.py, survey.py, palrange.py, palfree.py, history_census.py}`
  (`survey.py <data> <out>` regenerates the census and per-pattern CSV; `palfree.py <data>` the free-index table; `palrange.py <data> [char...]` slot variance and effect-only index ranges).
* Data: `docs/cg/effect_recolor_data/mbaacc/{census.csv, effect_patterns.csv, palfree.csv}`, `docs/cg/effect_recolor_data/{history_census.csv, history_name_diff.csv, history_skipped.txt}`.
* DGV project extracted at `/home/teo/dev/refs/dgv_better_akiha/extracted/Better Akiha v2/` (not committed).
* IDA (`C:\games\mbaacc\MBAA.exe.i64`, saved): `Obj_SubmitSpriteDraw` 0x41A390, `Sprite_BuildDrawRecordAndEmit` 0x407420, `Sprite_EmitTransformedQuad` 0x406AE0, `CG_ParseBank` 0x402970,
  `CG_ValidateAndCountBank` 0x402840, `CG_FileHasBmpCutterMagic` 0x401FA0, `CG_UploadAtlasPagesWithPalette` 0x4020E0, `CG_UploadCellPalettized` 0x403570, `Texture_CopyConvert` 0x402EB0,
  `EffectTagRegistry_GetOrCreateByName` 0x4BEAA0, `DxEffParam_Create` 0x405520, `Obj_AnimStepFrameFlow` 0x419C00.
