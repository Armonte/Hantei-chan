# CG image type 5: investigation and in-game proof (2026-10-03)

Context: effect recolour bake tool (DGV "Better Akiha" method automated, see `effect_recolor_design.md`). Question: is the engine's unused
type 5 usable for "palette-following effect with soft alpha", and is it the optimal bake target?

## 1. What type 5 is (MBAA.exe, IDA names applied + function comments, DB saved)

| piece | address | finding |
|---|---|---|
| `CG_ValidateAndCountBank` | 0x402840 | type 5 counted like type 4 for pixel bytes (2*w*h per owned block) but adds **no** palette dwords (types 2/4: +256, 3: +1) |
| `CG_ParseBank` | 0x402970 | type 5: image record +72 (palette pool index) = -1, i.e. **no own palette**; data per owned block = `w*h` index bytes **then** `w*h` alpha bytes; also lowers the "first non-type-0 page" boundary |
| `CG_UploadAtlasPagesWithPalette` | 0x4020E0 | `case 5`: mode 2 (index + alpha planes) with the **global palette `CG+0x10`** (the slot palette; same pointer type 0 uses). Re-run on every palette change, so type 5 follows the slot |
| `CG_UploadCellPalettized` | 0x403570 | mode 2: `RGBA = pal[index].rgb + alphaPlane[i]` (the palette's own alpha byte is ignored), then `Texture_CopyConvert` fmt 21 |

Bit layout, per owned block (the 24-byte align record's w x h, copy blocks own nothing): `[w*h x u8 palette index][w*h x u8 alpha]`. **Alpha is a full 8-bit plane**
(not 1 or 4 bit); index 0 has no special meaning (alpha plane decides). No header/palette prefix in the image blob (type 2/4 have 1024 bytes, type 3 has 4).
Texture format: **A8R8G8B8** (21; 26 = A4R4G4B4 on 16-bit desktops). Pages at or above the first page holding any non-type-0 image are 32-bit, pages below
are A1R5G5B5 (type 0 only). Type 5 therefore also upgrades the type-0 images that share its pages to 8-bit alpha / 8-bit colour.
Limits: none beyond the bank's (cell grid, 3000 images); the 256-entry global palette is shared with the body sprites.
Draw: header bpp is not checked by the loader; I used bpp = 32 (like type 4) so the quad goes through the fixed-function path (the sh_chara techniques are 8-bpp only).

## 2. Did anyone ship it?

No. Type-5 images: 0 in all 72 MBAACC banks (+ `mbaacc_dev`/`mbaacc_tag` copies), 0 in MB 2002, ReAct, MBAC (`effect_recolor_data/history_census.csv`, column t5).
Other FB titles (RBO, GOF2) ship packed and were not enumerated here; MBTL/UNI CG type table not checked. Not a buggy path: it decodes and draws correctly (below).
Inference (not provable from the binary): effects needing alpha were authored as type 4 (own palette) and the artists never needed global-palette effects; the BMP Cutter
tooling and cgtool (`types 2/3/4 are never produced`) never exposed it.

## 3. In-game proof (hard-linked proof dir `C:\games\mbaacc_fxproof`, windowed, `pc_shot.sh`, game killed by CIM PID filter)

Setup: `tools/fxrecolor/t5proof.py` rewrites `sion.cg` with **every type-0 image converted to type 5** (same indices, new alpha plane = vertical ramp 255 at the head -> 40 at the feet),
`cgpatch.py` rebuilds the offset table; `proof_run.sh` runs the AI battle and photographs. Screens in `docs/cg/type5_proof/`:

| shot | what | result |
|---|---|---|
| `base.png` | stock | reference (Sion pink hair) |
| `t5b_alpha90.png` | constant alpha 90 | Sion (left) ghost-translucent: **soft alpha honoured** |
| `t5c_ramp.png` | vertical alpha ramp | head opaque, legs fade into the stage: **8-bit gradient preserved** |
| `t5d_palswap.png` | `sion.pal` R/G/B-rotated | the type-5 Sion turns **green**: follows the slot palette |
| `t5f_additive_origpal.png`, `t5e_additive.png` | all Sion layers AFAL = additive (2,255) | translucent type 5 adds light correctly on dark stages (e: with the swapped palette) |

No crash, no corruption, no change to the second character (V.Sion keeps its own bank). The engine accepts type 5 with bpp 32.

## 4. Options

Numbers: Akiha effect set (675 images, the non-type-0 ones): 19.75 MB stored now (14.8 MB of it type-1 RGBA); type 5 would store 14.7 MB (2 B/px); type 0 7.3 MB.
36.4 % of the visible (alpha>0) effect pixels have partial alpha (1.05 M of 2.88 M).

| | (a) type 5 | (b) type 0 | (c) keep RGB, runtime shader only | (d) hybrid |
|---|---|---|---|---|
| alpha | 8-bit, exact | **1-bit**: 36 % of effect pixels (glows, smoke, soft edges) become hard-edged or vanish | exact | exact where type 5 / RGB |
| colour precision | 256 shared indices (index ramp, banding where a ramp has < ~64 steps; none of DGV's ramps had more than ~60-120 steps and the group palette fits) | 5-bit per channel (A1R5G5B5 on low pages) + same index limit | 8-bit | per image |
| palette-following | engine-native, per slot, **no runtime dependency**, works in stock MBAACC / CCCaster | native | needs PovertyCaster shader (not stock) | mix |
| file size | 2 B/px (-26 % vs now for Akiha effects) | 1 B/px | unchanged | between |
| load/perf | same CPU convert at load/palette change; textures are A8R8G8B8 either way | pages stay A1R5G5B5 (half the VRAM) only if the bank has no non-0 images | shader cost per effect quad, hook risk | |
| compatibility | stock MBAA.exe/CCCaster fine (engine-native); other editors (cgtool, BMP Cutter) do not know type 5; Hantei-chan needs the new type | any tool | no file change, PovertyCaster only | |
| palette budget | each ramp consumes global indices (>= 103 free in every bank, median ~190) | same | none | |
| per-pattern colours | clone sprite + re-point layers | same | per-draw keying | |

## 5. Recommendation

**Type 5 is the right bake target (option a), with a hybrid fallback (d):** bake an effect image to type 5 when its colours fit the ramp budget (monochrome/duotone groups, most of the 675),
keep type 0 only for hard-alpha sprites that are already type 0, and leave RGB images that cannot be expressed as ramps (photographic / many-hue sprites, effect.cg shared bank) to the
PovertyCaster runtime shader (c). Type 5 beats type 0 on exactly what DGV lost (soft alpha, 36 % of pixels) and beats RGB-only on being a plain data mod that works on stock MBAACC and
shrinks the bank. Cost: a global-palette budget (ramps compete with body indices) and no support in cgtool/BMP Cutter (Hantei-chan gets type 5 read/write).

## Status

Parked: user wants no data changes. The bake-to-type-5 path is not being built; this report and the proof stay as research. Direction is runtime-only recolour (PovertyCaster shader), with Hantei-chan as the authoring/preview tool.
