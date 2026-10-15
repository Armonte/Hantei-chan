# 3D stage viewer for post-MBAACC French-Bread games: scope

Date: 2026-10-15. Branch `feat/stage3d` (worktree `wt/stage3d`, from `update/parity-1014`).

**Goal.** Add a game-accurate viewer for the 3D stages of the post-MBAACC French-Bread games: UNI2, UNI[st]/[cl-r] ("UNIB"), UNIEL, DFCI and MBTL. The same view is the alignment tool for porting those stages into MBAACC as 3D stages through PovertyCaster's `pc-stage3d`.

"Proper" means pixel-matching the game's own render. §3 defines that render from IDA. The first slice already matches the UNI2 camera and framing (§7).

## 1. Formats per game

### 1.1 Where the stages live

| Game | Stage model | Storage / encryption | Textures | BgList | Status here |
|---|---|---|---|---|---|
| UNI2 (1.42) | `bg\bgNNN\bg.fbx.bin` (fbxex) | inside the `d\` archive. The extracted copy is `gamedata/uni2/bg` (28 stages + bg090). | DDS: DXT1/DXT5, plus A8R8G8B8 on a few, no mips | `bg\BgList.txt` (Squirrel, cp932) + `BgList_str.ini` | **loads and renders** |
| UNI[st]/[cl-r] (UNIB arcade, `D:\Games\unib`) | fbxex | `d\` index + data archive (64-byte header, 128-byte folder records, 64/80-byte file entries; see Zanaylo `GameArchive.cpp`). Not encrypted. | DDS | same keys | needs an archive reader (P2) |
| UNIEL | fbxex | loose files, each **RC4**-encrypted with a fixed 16-byte key. Anime tracks are stored as *world* matrices and need world→local conversion. | DDS (RC4) | RC4 | P3 |
| DFCI (arcade, `C:\dev\transmission\dfci+transmission\bg`) | `bg.fbx`: **ASCII FBX 6.1.0** only, 14 stage folders | loose | DDS | `BgList.txt` + `BgList_str.txt` | needs an FBX-ASCII reader (P2) |
| MBTL (buildid 25646393) | fbxex | `data006.bin`, each file XOR-ciphered with a 1024-byte key table. The extracted copy is `gamedata/mbtl/bg` (40 stages, decrypted by `tools/gamedata/mbtl_extract.py`). | DDS | same keys, plus `bg_chara_height.csv` and `bg99Info.txt` | loads through the same path (point the viewer at `gamedata/mbtl/bg`); not compared to the game yet |

### 1.2 The fbxex container

The format is little-endian, verified against all 29 UNI2 files by `ctest stage3d`. The full layout is in `src/stage3d/s3d_model.h`. In summary:

- Magic `fbxex\0\0\0`, then two zero words, then four blocks. Each block is `{u32 size incl. 8-byte prefix, u32 count}`:
  1. Textures: 128-byte names.
  2. Materials: 204 bytes each. That is `name[128]`, `index`, `texIndex` and 17 floats of D3DMATERIAL9 data. The 4th float of each colour is a "present" flag.
  3. Nodes: `size`, `type` (0 = transform, 1 = mesh), `firstChild`, `nextSibling`. A mesh record holds:
     - `additive`, `flag2` (meaning unknown; set on 26 of 5077 UNI2 meshes), the world matrix;
     - vertices of 48 bytes = pos3, normal3, colour3, alpha, uv2;
     - submeshes of `{material, n, u32 idx[n]}`.
  4. Anime: per node, N local 4x4 matrices at 30 fps. Each track loops on its own length.
- Matrices are D3D row-vector; `world = local * parentWorld`.
- Lighting is **baked into the vertex colours**. There are no cameras or lights in the file.
- In-game vertex format: FVF 0x152 (36 bytes; the colour is packed to D3DCOLOR at load).
- Empty texture-table slots exist (UNI2 bg010, bg025).

### 1.3 DFCI ASCII FBX 6.1

The reader needs:
- `Objects/Model` with `Properties60` (Lcl T/R/S, Euler XYZ in degrees, pivots/offsets);
- the `adding` user property, which means additive;
- `Connections` (OO links Model/Material/Texture);
- the `LayerElement` Normal/UV/Color/Material/Texture data in all four mapping modes;
- `Takes`: cubic Hermite keys with time unit 46186158000/s, baked to 30 fps.

Two implementations already exist:
- `pc-stage3d/src/FbxAscii.cpp`. It is our own code (PovertyCaster), with no licence issue inside our projects.
- Zanaylo's `FbxAscii` + `FbxToFbxEx` (GPL-3.0, §2.3).

### 1.4 BgList parameters

Field table from IDA, UNI2 1.42 `BgList_LoadAll` 0x51C140, record 0x23C bytes at 0xA57CA0:

| Group | Keys (record offset) | Used by |
|---|---|---|
| Placement | `Scale[3]` 0x6C, `Position[3]` 0x78, `ViewRotationX/Y` 0x8C/0x90 (degrees) | stage world matrix |
| Camera | `FOV` 0x84 (degrees, see §3.2), `VanishingPoint` 0x88 (NDC lens shift), `StageW` 0x208 (px; camera clamp) | projection, view |
| Fog | `IsFog` 0x98, `FogStart/End` 0x9C/0xA0, `FogColor[4]` 0xA4 | Easy/Light pixel shader |
| Bloom | `IsBloom`, `BGBloomEnable/Blightness/Power/BiassR/G/B/BlurRadius/TextureSize/Alpha` 0xB4..0xD8 | `StageBloom_Apply` 0x517400 |
| FXAA | `BGTinyFXAAEnable/LerpT`. **The exe misspells the threshold key as "BGTinyFXAAT0reshold"**, so `BGTinyFXAAThreshold` is never read (always 0.2). | post |
| Target | `MSAA` 0xF0, `TargetW/H` 0xF4/0xF8 (1280x720) | stage render target |
| Shadows | `ShadowLightType` 0xFC, `ShadowLightStatus[≤8]` {Type, Position, PowerValue, Color[4]} 0x100, `ShadowReflexColor` 0x1E4, `ShadowScale/Alpha` 0x224/0x228 | character shadow pass |
| Light | `LightType` 0x1F4 (1 = "torch" technique), `LightColor` 0x1F8 | DefaultShader_Light |
| Character | `CharaColor` 0x214 (no reader found), `BlanchStage/Chara` (stage variants) | stage colour/specular/bokashi `.img` ramps light the *characters* |
| Misc | `ViewGrid`, `StageSelTex`, `DLCFlag`, `InterpolationType/Num`, `Select/Random/VsDisable` | UI, anime interpolation |

Notes on per-game differences:
- The parser in `src/stage3d/s3d_bglist.cpp` keeps every key, so DFCI/MBTL extras need no code change.
- DFCI and MBTL use the same key names. DFCI's `Shader/fbxshader.txt` has the fog lerp commented out.
- MBTL adds `bg_chara_height.csv` (per-character standard height for the character stage light).

## 2. What exists already

### 2.1 PovertyCaster `pc-stage3d` (read only; owned by the cg2/PovertyCaster side)

**Which tree.**
- `/home/teo/dev/pc-wt-melty3d/pc-stage3d` and `/home/teo/dev/pc-fm2k-native/pc-stage3d` are byte-identical. Every stage3d commit is on `origin/main`; the newest is `84741cf74` (2026-10-03, the world_scale slider).
- Use `pc-fm2k-native` or `origin/main` as the reference.

**Design doc:** `pc-fm2k-native/docs/recon/2026-10-03/melty_3d_stages.md`.

**What it does:**
- Reads `bg.fbx.bin` (fbxex), or DFCI's ASCII FBX 6.1, into one `Stage` structure.
- Renders with D3D9 inside MBAACC: a mid-hook on the world pass `DrawCommandList_ExecuteScene` call at 0x4331D4. It replaces bucket-10 stage nodes, into the 640x480 scene target, with a private D24S8 depth surface.
- Shader `psEasy`: tex × vcol, with an unclamped fog lerp and a lift/gain/contrast/gray grade. The torch technique is also implemented.
- Draws additive nodes with Z-write off and cull NONE.
- Per-stage `stage3d.ini` files, hot-reloaded. 4 UNI2 inis and 14 DFCI inis are generated by `tools/stage3d_stages/gen_uni2_inis.py`.
- An F3 tuning tab.

**What it does not do:**
- Bloom, FXAA, MSAA, the specular composite variant.
- Shadows in 3D (MBAACC's 2D shadows stay).
- UV scroll, UNIB/UNIEL/MBTL archives.

**Tests:**
- `tests/stage3d_fbxex`.
- `stage3d_fbxascii`, which compares the FBX reader with the `.bin` on UNI2 stages that ship both (within 0.006).
- `stage3d_camera`.
- A synctest/canary showing the stage cannot desync.

**Camera model** (`CameraMap.cpp`). It matches MBAACC's 2D similarity `screen = zoom*(world - cam) + (320, 432)` at the fighter plane z = 0:
- It treats BgList `FOV` as the **horizontal angle of a 16:9 frame**: `vfov = 2*atan(tan(f/2)*9/16)`.
- It puts the source camera **10 units** from the fighter plane: `F720 = 1547 px`, 154.7 px/unit.
- It dolly-zooms with `D = F/(zoom*s)`, and the eye follows `camX/s`.
- Fighter scale comes from `fighter_px 237 / uni_char_px 355`. Floor is 432, horizon 255.
- Ground is auto-detected from up-facing triangles near the origin. `pan_boost` adds extra depth parallax.

**The camera does not match the game (§3.2):**
- UNI2 feeds `FOV` as the vertical angle of an **aspect-1.0** projection and squeezes X by 9/16 in the stage world matrix.
- The eye sits `cot(FOV/2)` = 2.414 units from the plane, not 10. With a 10-unit camera, depth parallax and perspective convergence are about 4x too weak relative to the plane. That is likely why `pan_boost` was needed and why some stages "don't look right" (§5).

**Credits:** it credits Zanaylo's wiki for the format and takes no code from him.

### 2.2 Hantei-chan

- **MBAACC 2D stage viewer.** `src/background/*`; see `docs/HANTEI_STAGE_LINK.md` and `docs/bg_research/`. It is game-accurate, with a stage browser, bg_regress, and a `--ui-script` capture path. The 3D view reuses that tooling: `RenderTarget`, `--ui-script`, quill logging, and `tools/ui/ab_shot.sh`.
- **New in this slice:** `src/stage3d/` (§7).

### 2.3 Zanaylo's improvement mods

**Licences:**
- **UNI2-Improvement-Mod** (`/home/teo/dev/refs/UNI2-Improvement-Mod`) is **GPL-3.0**.
- **MBTL-Improvement-Mod** has **no LICENSE file**. Its stage files are copies of the GPL UNI2 files (FbxAscii, FbxExLocal, FbxToFbxEx, BgObjectFix, UnielCipher, MbtlCipher are identical apart from include paths), so treat them as GPL-3.0. The author's verbal permission to reuse covers MBTL-IM code.

**What it has that we lack:**
- `FbxToFbxEx` (ASCII FBX 6.1 → fbxex, 1328 lines) and `FbxExWriter`.
- `FbxExLocal`: UNIEL world→local anime.
- Archive readers: UNIB `d\` index, MBTL `data006` (+ hard-coded index), UNIEL RC4.
- `BgListText` (editor), `BgObjectFix` (object.txt 2D layer fixes).
- Runtime hooks: `CharacterLight`, `StageBloom`, `StageTint`.
- A Blender add-on `io_scene_fbxex.py` (byte-exact round trip on 28 stages).
- The wiki `Stage-models.md`: the format spec this slice was written from.

**What we take:** Hantei-chan's slice copies **no** code from either repo. The fbxex reader, DDS decoder and BgList parser were written from the format description and checked on the shipped files.

### 2.4 Licence question (user decision needed)

**Hantei-chan has no licence.** Options for the DFCI/UNIB/UNIEL/MBTL readers (P2/P3):
- **(a) Reimplement** from the format notes, as this slice did. For DFCI we can port PovertyCaster's own `FbxAscii.cpp` (our code) instead of Zanaylo's.
- **(b)** Put GPL-derived code (archive readers, FbxToFbxEx) in a separate, clearly marked module, e.g. `third_party/zanaylo_stages/` with its own `LICENSE` (GPL-3.0) and attribution. Note that linking GPL code into the editor makes the distributed binary GPL.
- **(c)** Get Zanaylo's written permission covering the UNI2-IM files too. His verbal permission covers MBTL-IM, but those files are themselves GPL copies.

Recommendation: **(a)**. Every format piece is small and documented: RC4 is about 20 lines, the MBTL XOR about 15, the archive index about 60, and the FBX reader can come from PovertyCaster's own code. Credit Zanaylo in the About box and in this doc either way.

## 3. The game render model (UNI2 1.42, from IDA)

IDB: `C:\dev\ida\server\uni2_142_4ebed985.exe.i64`. About 60 functions and globals were renamed (`Stage3D_*`, `BtlCamera_*`, `BgList_*`, `StageBloom_*`, `FbxMesh_*`, `CharaShadow_*`) and the IDB was saved. Image base 0x400000.

### 3.1 Frame pipeline (`Stage_DrawFrame` 0x51A850)

1. **Render the FBX** into `g_StageMsaaRT`: 1280x720 A8R8G8B8, BgList MSAA, D16 depth.
   - Clear to `0xFF006400` (dark green) with depth 1.0.
   - Nodes are drawn depth-first in hierarchy order (`FbxScene_DrawNodeRecursive` 0x5BC430), with no transparency sorting.
   - Z test LESSEQUAL with **Z write on for every node, blended or not**; alpha test GREATER 0.
   - Blend is SRCALPHA/INVSRCALPHA, or dest = ONE for the additive flag (node+0x58).
   - Cull comes from scene field +0x158: 1 = CW, 2 = CCW, otherwise NONE.
2. **2D bg objects** (`object.txt` + `.pat`, `StageObjects_Draw2D` 0x518D80), with the same camera.
3. **Resolve** with StretchRect.
4. **Bloom**, if `BGBloomEnable`:
   - `c = pow(c*Blightness, Power) * BiasRGB` into a TexSize² target.
   - `bloomN_N` blur with N = int(BlurRadius) ≤ 11.
   - Added back additively with alpha `BGBloomAlpha`.
5. **TinyFXAA**, if enabled.
6. **Composite** into the 2D draw list as a 1280x720 quad at **priority 250**:
   - `rgb = 0.10 + tex * (g_StageTintColor.rgb * 0.6708)`. The specular variant adds `spec*max(tex.rgb)`, then a grayscale lerp.
   - The tint is white unless the script calls `SetBgColor`.
   - Character shadows are composited at priority 300 (alpha `ShadowAlpha*255`). Sprites share no depth with the stage.

### 3.2 Camera (`BtlCamera_BuildMatrices` 0x534910; camera object `g_BtlCamera` 0xC5CCA0)

The fight camera is held in px×128 fixed point. Zoom is defined by the visible span: span = 163840/zoom, so zoom 1 = 1280 px.

```
Proj  = PerspectiveFovLH(FOV deg, aspect 1.0, zn 0.15, zf 1000) * Translation(0, -VanishingPoint, 0)
eye   = ( camX/640,  -(camY - 280*iz)/360 - iz*VanishingPoint,  -cot(FOV/2)*iz )     iz = 1/zoom
View  = LookAtLH(eye, eye + (0,0,1), up y)            -> pure translation, the camera never rotates
Stage = Scale(Sx*9/16, Sy, Sz) * T(Position) * RotX(ViewRotationX) * RotY(ViewRotationY)
Node  = NodeWorld (Z column negated) * Stage
```

Consequences:
- At z = 0 (the sprite plane), 1 stage unit = 360 game px on both axes.
- World y = 0 sits 280 px below the screen centre, at y 640 of 720.
- The 3D eye moves **1:1 with the fight camera** at the plane. Zoom is a dolly with FOV fixed.
- Deeper geometry parallaxes only through perspective. There is no per-layer factor.
- Screen shake (`BtlCamera_ApplyQuake` 0x5354E0) is applied before the matrices, so the stage shakes with parallax.
- The live capture agrees with this: view translation (0, -0.7778, 2.4142) at round start, and x = -1.09375 at camera x = 89600 (700 px).
- One open discrepancy: the captured projection implies zn = 0.2, while the decompile says 0.15. It has no visible effect.

### 3.3 Shading (`Shader/fbxshader.txt`, chosen per mesh)

- **DefaultShader_Easy** (textured): `tex * vertexColour`.
  - Fog: `f = FogEnd/(End-Start) - w/(End-Start)` on clip w, unclamped; `rgb = lerp(FogColor, rgb, f)`; alpha untouched.
  - Fog is active when `FogColor.a >= 0.001`. `IsFog = 0` zeroes FogColor.
  - Many UNI2 stages use **negative FogColor components** as a colour shift, e.g. bg001 `(0.3, -0.2, 0, 1)`.
- **DefaultShader_Light** (`LightType == 1`, the "torch"): `tex * LightColor * max(0.2, 1 - 0.3*1.125*|wpos*(1,1,0.05) - lightPos|)`, with the light at the camera xy, z = 0. Vertex colour is ignored.
- **DefaultShader_NT** (untextured): material diffuse plus Phong specular.
- **Samplers:** LINEAR/LINEAR, WRAP, no mips. Textures are loaded with Levels = 1.
- **Character light:** the `stage_color/specular/bokashi_alpha.img` ramps and `bg_chara_height.csv` light the *characters*, not the stage (`CgLayer_DrawWithCharaShader` 0x4BE140).

### 3.4 Still unknown

- Who writes `CharaColor` and the camera Y offset (+0x260).
- Exact sprite priorities relative to 250.
- The `.img` ramp format details.
- The `flag2` meaning.
- MBTL was not walked in IDA this pass. DFCI was (see §5): it has the same camera, no composite grade and no fog.

## 4. MBAACC alignment

### 4.1 MBAACC's 2D camera

From the MBAA IDB:
- `screen = zoom*(world - cam/128) + (320, 432)` in a 640x480 frame, so the floor is at y = 432.
- Camera clamp |x| ≤ 208 (sim); zoom comes from the fighter spread.
- Fighters are about 195-237 px tall.

### 4.2 Two exact mappings

A 3D stage maps to that camera exactly if the stage is drawn with the **source game's own camera**, re-expressed in MBAACC pixels:

```
k        = 480/720 = 2/3                       (UNI frame -> MBAACC frame, same 16:9 -> 4:3 crop question below)
unitPx   = 360 * k * s                          (s = per-stage "fighter scale" tune, 1.0 = UNI proportions)
eye      = ( camX_mb / (unitPx) ,  (floorOffset)/unitPx ,  -cot(FOV/2) / zoom_mb )   (same formula as §3.2)
floor    = world y 0 -> screen y 432 (MBAACC) vs 640/720 (UNI): set the projection's lens shift so y0 lands at 432
```

There are two ways to resolve the 16:9 vs 4:3 difference:
1. **Crop.** Keep the UNI vertical FOV and render 640x480 out of the centre of the 853x480 equivalent. The sides are lost and the proportions stay correct. Sidebars mode (16:9) can show the full width.
2. **Fit.** Scale X so the stage width fits MBAACC's wall-to-wall width. Proportions are lost; do not use this.

### 4.3 Wall distance and ground line

- In UNI2 the fight area is `StageW` px wide: 4096 on most UNI2 stages, clamped to ±(StageW/2 + 128) minus the half view. UNI2 bg001 has StageW 4096, i.e. ±2048 px = ±5.7 stage units at the plane.
- In MBAACC the camera clamps at |x| ≤ 208 sim px and the walls sit at about ±(208+320) = ±528 px from centre (`host_world_w 1056` in pc-stage3d). The discrepancy noted in pc-stage3d's doc needs checking in the MBAA IDB.
- The UNI stage is therefore about 3.9x wider than an MBAACC arena at UNI proportions. Two choices:
  - (i) Accept that MBAACC shows only the middle of the UNI stage.
  - (ii) Shrink the stage (`s < 1`) so the UNI wall props sit at MBAACC's walls. Fighters then look giant against it.

  (i) is the natural default. Tuning `centre_x` per stage picks the interesting part.
- The ground line needs no tuning beyond `Position.y`: world y = 0 is the floor in both games.

### 4.4 Parameters the user tunes per stage

Everything else comes from BgList.

| Param | Meaning | Default |
|---|---|---|
| `fighter_scale` (s) | MBAACC px per UNI px at the plane. 1.0 = UNI proportions (an UNI character of 355/720 px vs MBAACC ~210/480). | 0.9-1.0 |
| `centre_x` | Which slice of the wide UNI stage sits under MBAACC's arena (stage units). | 0 |
| `floor_px` | MBAACC floor row. | 432 |
| `y_bias` | Small floor fix when the mesh floor is not at y = 0. | 0 |
| `grade` | 0.10 + 0.6708·c (UNI2/MBTL) vs DFCI's own grade. Optionally match MBAACC's palette. | game |
| `fog`, `bloom` | As BgList. | game |
| `front_split` | Nodes drawn over the fighters (MBAACC band 600). | none |

The Hantei-chan view will show both pictures for the same camera, so these are tuned visually and written back to `stage3d.ini` (P3):
- **Game camera:** the source game at 1280x720.
- **MBAACC camera:** 640x480, with the 2D floor line, walls, a fighter-height ruler and an optional MBAACC character sprite.

## 5. DFCI issue list

The full list is in [DFCI_ISSUES.md](DFCI_ISSUES.md): per-stage tables, ranked issues, how to verify each one, and an ini-only workaround. It is based on IDA of DFCI's `RingGame.exe` (SDCS build; renames saved) and the 14 stage FBX files.

**DFCI's camera is the same as UNI2 1.42's.** It uses the same aspect-1 projection, the same H/W X squeeze, a VanishingPoint lens shift, a Z mirror and an eye at cot(FOV/2). The near plane is 0.2. There is no grade and no fog, and bloom is off on all 14 stages.

What goes wrong in MBAACC today, ranked:
1. pc-stage3d reads FOV as the horizontal angle of 16:9, so depth renders 1.78x too flat.
2. Its camera sits 10 units from the fighter plane instead of 2.1-3.2. Props on the plane draw at 0.38-0.56x size and far backdrops 1.78x too big.
3. VanishingPoint is ignored. The horizon is fixed at row 255 where DFCI has rows 317-389, so the camera ends up 4-6x too high.
4. Position.x is not divided by 0.5625. bg01 and bg15 are off by 1.17 units, bg02 by 1.32.
5. The automatic ground snap is applied on top of Position.y (bg05, bg14, bg18).
6. The `object.txt` effect sprites (snow, sakura, confetti) are missing on 9 stages.
7. `pan_boost` adds parallax DFCI does not have.

Issues 1-3 also apply to pc-stage3d's UNI2 stages. The fix belongs in pc-stage3d's `CameraMap`: port the game camera from §3.2. The PovertyCaster side owns that code. The Hantei-chan view (P3) will produce the numbers.

The data itself is clean: Y up, unit scale 1, one UV set, and all textures are present.

## 6. Plan

Sizes: S ≤ 1 day, M 2-3 days, L about a week.

| Phase | Content | Size |
|---|---|---|
| **P1 (this slice)** | fbxex + DDS + BgList readers, dockable "3D Stage" view, UNI2 camera/stage world exactly as IDA, Easy shader + fog, composite grade, script commands, ctest over all shipped UNI2 stages, side-by-side vs game. | done |
| P1.1 | Cull mode from the scene field (+0x158) and confirmation of the remaining look differences against the capture (§7); DefaultShader_Light (torch, bg090) and NT (untextured); `flag2`. | S |
| P1.2 | Bloom (`bg.txt` ColorCorrection + `bgN_N` blur at TexSize, additive with BloomAlpha) and TinyFXAA; MSAA. | M |
| P1.3 | Live camera: drag/scroll pan, zoom, StageW clamp, quake preview; anime clock; 2D `object.txt` layer (PAniData .pat already parsed in Hantei-chan). | M |
| P2 status (2026-10-15) | Done: ASCII FBX 6.1 (`s3d_fbxascii.*`, port of pc-stage3d's reader). Every DFCI stage (14) and every UNI[st]/[cl-r] arcade stage (11, `C:\games\unib\bg`, loose ASCII FBX) loads with its textures; the UNI2 dual-form stages agree with their .bin (stage3d_test). The view takes `bg.fbx.bin`, else `bg.fbx`. Not done: UNIEL (RC4 loose files) and the Steam UNI[cl-r] `d\` archive, because neither is installed here. Interactive camera (drag / wheel) done (P1.3 part). | - |
| P3 status (2026-10-15) | MBAACC alignment mode in the view (section 4): the centre 4:3 crop at the source FOV, driven by MBAACC cam x / zoom through fighter scale s and center_x; floor row via a lens shift; MBAACC walls (±528), the source StageW walls and a 195 px fighter ruler drawn on top; writes `stage3d.ini` (`fit_mode crop`, `center_x`, `fighter_px = 237 s`, `floor_px`, `stage_w`, `replaces`, `package`). Script: `stage3d align on <camX> <zoom> <s> <centerX>`, `stage3d alignini`. Shots: `evidence/ascii_fbx_2026-10-15/`. | - |
| P2 | Sources: DFCI ASCII FBX (port PovertyCaster's FbxAscii), MBTL extracted data (works already), UNIB `d\` archive, UNIEL RC4 + world→local, all reimplemented (§2.4). A stage browser across games with thumbnails. | M |
| P2.1 | IDA pass on DFCI and MBTL: camera/composite differences (DFCI grade, fog off), recorded in this doc. | S-M |
| P3 | MBAACC alignment mode: 640x480 MBAACC camera next to the game camera, floor/wall/ruler overlay, MBAACC fighter sprite at the plane, per-stage params (§4.4) written to pc-stage3d's `stage3d.ini` format, and a hand-off of the camera correction to pc-stage3d (owned by the PovertyCaster side). | L |
| P3.1 | Sweep: every UNI2/MBTL/DFCI stage, game capture vs Hantei render vs MBAACC render, with an issue per stage. | M |
| P4 | Optional editing: Blender round trip is already covered by Zanaylo's add-on; small in-tool edits (node hide, material/flag toggles, BgList text editor) and export of fbxex. | M-L |

## 7. First slice result

- **Code:** `src/stage3d/{s3d_model,s3d_dds,s3d_bglist,s3d_view}.{h,cpp}`, `tests/stage3d_test.cpp` (ctest `stage3d`), `tools/stage3d/` (shot scripts).
- **Opening the view:** Stage menu → "3D Stage view (UNI2 / UNI / MBTL)...", or the `--ui-script` `stage3d open <bg folder> <bgNNN>`.
- **Evidence:** `docs/stage3d/evidence/`: `uni2_bg001_sheet.png` (game | Hantei-chan, centre and side camera), the raw renders, and the game captures plus their notes (`uni2_bg001_game.txt`, gamecap case `tools/gamecap/cases/stage3d_uni2_bg001.gcs`).
- **Result (`evidence/uni2_bg001_sheet.png`):** UNI2 bg001 matches the game at both camera positions, with mean |difference| 3.4/255 (centre) and 3.2/255 (x = 700 px).
  - Geometry, camera, parallax, fog and the composite grade all line up pixel for pixel.
  - What is left: bloom and lens-flare glow (P1.2), the sky animation phase, and MSAA edges.
- **The comparison fixed two things:**
  - Texture V is stored bottom-up, so the view uses v' = 1 - v. pc-stage3d flips too.
  - The node Z mirror is required (without it the view is empty).
- **Cull:** with cull off the render matches. Culling the GL clockwise faces gives the same image; culling counter-clockwise removes most of the stage. Cull stays off (the game's scene field is probably 0) until the field is traced (P1.1).
- **Other games:** MBTL extracted stages (`gamedata/mbtl/bg`) parse with the same reader. Point the view's bg folder there; they have not been compared to the game yet.

