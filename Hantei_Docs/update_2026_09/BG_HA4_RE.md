# French Bread BACKGROUND (bgmake stage) and Hantei4 HA4 formats: RE and implementation notes

Date: 2026-09-23. Sources: MBAA.exe (IDA session `212fe94c`), mbacPC.exe (`mbacpc`), hantei4.exe (`hantei4`), all 56 MBAACC stages (`/mnt/c/games/mbaacc/bg`) and all 68 MBAC stages (`/mnt/c/games/MB/AC/dump/05`), and all 50 MBAC character `.DAT` files.

Marks used in this document:
- **V-rt**: verified in runtime code (the function and address are given).
- **V-tool**: verified in editor/tool code.
- **V-data**: verified by a census over every shipped file.
- **INF**: inferred.

Function names are the names after this session's IDA renames. Old names are listed in section 5.

Part A covers the background format. Part B (HA4) is in `HA4_SECTION.md` next to this file, produced by the HA4 sub-investigation and summarised in section B below. Part C is the audit, part D the changes applied, and section 5 the IDA work.

Tools and data from this work:
- `tools/bgdump.py`: stage dumper, field census and byte-compare.
- `data/mbaacc_bg_census.txt` and `data/mbac_bg_census.txt`
- `data/mbaa_bg_decomp_*.c`, `data/mbaa_bg_disasm_render.asm`, `data/mbac_bg_decomp.c`: decompiler dumps of every bg function used below.
- HA4 tools: `tools/ha4lib.py`, `ha4_verify.py`, `ha4_vs_ha6.py`, `han4_parser_audit.py`; outputs in `data/ha4/`.

---

## A. bgmake stage format (MBAACC `bg\bgNN.dat`, MBAC `bg\bgNN.dat` / `bgNN_s.dat`)

### A.0 Key structural insight

A bgmake stage is a Hantei4 animation file cut down to stage objects:
- The 132-byte frame is the HA4 216-byte frame with the tail trimmed: AF(44) + AS(56) + 16 int16 indices (see `HA4_SECTION.md` §1.3).
- The two 8-entry index arrays point at 52-byte records, which are the HA4 IF/EF record layout. They serve as **position triggers** (IF-like) and **frame commands** (EF-like).

This explains every "padding" region the legacy docs listed.

MBAC and MBAACC use a **byte-identical container**. For example, MBAC `BG01.DAT` has the same MD5 as MBAACC `bg01.dat`, and the embedded CG is `BMP Cutter3` in both. The differences are in the renderers (A.6).

### A.1 File layout (V-rt `Background_LoadAndExpandData` 0x4b69b0, mbacPC `ParseBGMakeFile` 0x4037e0)

| Off | Type | Field | Notes |
|---|---|---|---|
| 0x00 | char[16] | `"bgmake"` + NULs | V-data |
| 0x10 | i32 | version = 1 | V-data (56/56 and 68/68 files); the runtime never reads it |
| 0x14 | i32 | pat_off | Old-format PAT block, V-rt |
| 0x18 | i32 | pat_len | 0 = no PAT |
| 0x1C | i32 | cg_off | BMP Cutter3 block |
| 0x20 | i32 | cg_len | |
| 0x24 | 48 B | zero | V-data |
| 0x54 | i32[256] | object offsets (absolute, -1 = empty slot) | V-rt |
| 0x454 | … | objects, each followed by its event records | |
| pat_off | … | old-format PAT (A.5) | |
| cg_off | … | BMP Cutter3 CG | |
| cg_off+cg_len | 16384 B | trailing block | V-data (always 16384); the runtime ignores it |

- **What the runtime keeps.** It copies `file[0 : cg_off]` (or `pat_off` when there is no CG) as the **object block**. The object block includes the header and the offset table. The PAT and CG are copied into separate buffers.
- **Object placement.** Objects are stored in ascending slot order.
- **Event records.** After each object's frames come its trigger and command record tables. The gap to the next block is always a multiple of 52 bytes. V-data: every one of the 124 files, with no other gap bytes.

### A.2 Object header (60 bytes)

Sources: V-rt `Background_SpawnInitialInstances` 0x4b6e00 and `BgInstance_Init` 0x4b6db0.

| Off | Type | Field | Status |
|---|---|---|---|
| +0 | i32 | frame count | V-rt |
| +4 | i32 (read as i16) | **parallax**, 256 = moves with the world | V-rt |
| +8 | i32 (read as i16) | **layer** (draw order within a band) | V-rt |
| +12 | i32 | **trigger-table offset**, relative to the object start, -1 = none | V-rt `Background_RunPositionTriggers` 0x4b8df0 |
| +16 | i32 | **command-table offset**, relative, -1 = none | V-rt `Background_RunFrameCommands` 0x4b8cd0 |
| +20 | u8 | **no auto-spawn**: object is created only by spawn commands | V-rt 0x4b6e4c. V-data: 3 objects (the bg16 snow flakes) |
| +21 | u8 | **foreground band** (see A.4) | V-rt. V-data: 37 MBAACC objects (bg18, bg46, bg47, bg55, bg59, …) |
| +22 | u8 | **linear texture filter** for the whole object (sampler mode 2) | V-rt. 0 in all shipped data |
| +23..59 | | zero | V-data |

The existing IDA comments at 0x4b6ea2/0x4b6ea6 had parallax and layer **swapped**; they are corrected. The runtime layout is parallax as i16 at instance+10 and layer as i16 at instance+12.

### A.3 Frame (132 bytes)

Sources: V-rt `Background_StepInstanceAnimations` 0x4b88b0, `BgInstance_EnterFrame` 0x4b6d10, `Background_IntegrateInstanceMotion` 0x4b8530 and `Background_DrawInstance` 0x4b7060.

| Off | Type | Field | Status |
|---|---|---|---|
| +0 | i16 | sprite: ≥10000 is CG image `id-10000`; 0..9999 is a PAT **pattern** index; <0 draws nothing | V-rt |
| +2/+4 | i16 | x / y offset (px) | V-rt |
| +6 | **u16** | duration: the frame advances when `++timer >= duration`, so 0 and 1 both mean 1 tick | V-rt (`movzx word`) |
| +8 | u8 | unused | V-data: always 0 |
| +9 | u8 | CG blend: 0 = alpha forced to 255; 1 = alpha blend with +10; 2 = additive; **3 = MULTIPLY** (blend mode 4 = DESTCOLOR,ZERO) | V-rt, blend table 0x54CC50 |
| +10 | u8 | opacity (used when +9 ≠ 0) | V-rt |
| +11 | u8 | aniType (A.3.1) | V-rt |
| +12 | u8 | jump target | V-rt |
| +13..15 | | unused | V-data 0 |
| +16/+18 | i16 | **scale X/Y**, 256 = 1.0 and 0 = 1.0. **MBAC only** | V-rt mbacPC 0x4023c2. MBAACC ignores it; V-data 0 in all files |
| +20 | u8 | **interpolate to next frame**. CG: alpha lerp (MBAC also lerps scale). PAT: part scale lerp (MBAC also lerps rotation) | V-rt. V-data: 25 frames |
| +21 | u8 | loopEnd: target when a type-5 loop ends | V-rt |
| +22 | u8 | loopCount: reloads the loop counter on **every** frame entry when ≠0 | V-rt |
| +23..43 | | unused | V-data 0 |
| +44/+45 | u8 | clear X / clear Y (vel = acc = 0) | V-rt |
| +46/+47 | u8 | set X / set Y (vel = +52/+54, acc = +60/+62) | V-rt |
| +48..51 | | unused | |
| +52/+54 | i16 | velocity X/Y (1/128 px per tick) | V-rt |
| +56..59 | | unused | |
| +60/+62 | i16 | acceleration X/Y | V-rt |
| +64..99 | | unused | V-data 0 |
| +100 | i16[8] | **trigger refs** (-1 = none) | V-rt 0x4b8e73 (`+0xA0` from the object = frame+100) |
| +116 | i16[8] | **command refs** (-1 = none) | V-rt 0x4b8d73 (`+0xB0`). u4ick's writer fills +100..131 with 0xFF, which means "no events" |

#### A.3.1 Runtime stepping (verbatim)

Each tick, per live instance:
1. State 1 (new) becomes 2 (active), then `++timer`.
2. If `duration <= timer`, the frame expires and aniType decides what happens:
   - **0**: **despawn**; the instance goes back to state 0 and is not drawn again.
   - **1/3**: `cur+1`.
   - **2/4**: `cur = jump`, and the loop counter is decremented if it is >0.
   - **5**: decrement the loop counter if >0, then `cur = counter ? jump : loopEnd`.
3. After any change, `BgInstance_EnterFrame` runs: `timer = 0`, both latches are cleared, the loopCount reload happens, and the next frame is precomputed for interpolation.
4. If `cur >= frameCount`, the instance **despawns**.

Consequence: an object that "wraps" (a scrolling cloud) is a **new instance created by a spawner command**. The old copy despawns on its aniType-0 frame. It is not a reset of the same object. Several instances of one object coexist; bg01 keeps 14–15 live instances.

#### A.3.2 Motion (V-rt 0x4b8530)

- On the first tick after a frame entry, velocity and acceleration are loaded from the flags **without** moving the instance.
- On later ticks: `pos += vel; vel += acc`. Position is in 1/128 px.
- CG objects draw at `(pos >> 7) + offset`. PAT objects use `pos / 128.0` as a float.

#### A.3.3 Event records (52 bytes; the same layout as HA4 IF/EF records)

| Off | Field |
|---|---|
| +0 | i16 type |
| +2 | i16 w2 |
| +4.. | i32 d[1..12] |

**Trigger type 1** (position trigger, checked every tick, V-rt 0x4b8df0):
- `d[3]` is the axis: 0 = X, 1 = Y.
- `d[4]` is the comparison: 0 fires when `pos > d[2]`, 1 fires when `pos < d[2]`. `d[2]` is in 1/128 px.
- When it fires: `d[1] == -1` despawns the instance; otherwise `cur = d[1]` and EnterFrame runs.
- V-data: only bg16 uses triggers (snow flakes switch to their landing frames when `y > 0`).

**Command types** (run once per frame entry, guarded by the instance+5 latch, V-rt 0x4b8cd0):
- **1, spawn** (`BgCmd_SpawnObject` 0x4b8aa0): object slot `w2`, placed at `(d[1], d[2])` relative to the spawner (A.3.4).
- **2, random spawn** (0x4b8b20): slot `w2 + rand % max(1, i16@+20)`, at x uniform in `[d[1], d[3]]` and y uniform in `[d[2], d[4]]`.
- **100, random velocity** (0x4b8c10). Only when `w2 == 0`. The axis is `i32@+20` (0 = X, 1 = Y). Velocity is `rand in [d[1], d[2])` and acceleration is `rand in [d[3], d[4])`; when the two bounds are equal, the value is exact.

V-data: MBAACC uses types 1/2/100 in 45/3/7 records across 14 stages.

#### A.3.4 Spawn placement (V-rt `BgInstance_PlaceRelativeToParent` 0x4b89e0)

With pp and cp the parent and child parallax divided by 256:

```
child.posY = (y<<7) + pp*(parent.posY - camY) + cp*camY
child.posX = (x<<7) + pp*(parent.posX - camX + 0x6000) + cp*(camX - 0x6000)
```

The neutral camera is (0,0) (`Camera_ResetState` 0x44afd0).

- **Instance pool.** The pool has 2000 slots of 44 bytes at 0x750840.
- **Initial spawn.** File object i with `+20 == 0` goes into slot i.
- **Spawned instances.** Spawns take the first free slot.

Instance layout:

| Off | Field |
|---|---|
| +0 | state |
| +1 | objIdx |
| +2 | cur |
| +3 | next |
| +4 | loopCounter |
| +5 | cmdDone |
| +6 | motionLoaded |
| +7 | 1 |
| +8 | fg band |
| +9 | linear filter |
| +10 | i16 parallax |
| +12 | i16 layer |
| +16/+20 | pos |
| +24/+28 | vel |
| +32/+36 | acc |
| +40 | timer |

### A.4 Rendering

Sources: V-rt `Background_DrawAllInstances` 0x4b8f80, `Background_DrawInstance` 0x4b7060, `Camera_UpdateMatrices` 0x44bcb0 and `Sprite_EmitTransformedQuad`.

**Camera.** World to screen is `(world - cam/128) * zoom + (320, 432)`, the same matrix used for the fighters. Stage world (0,0) is the character origin: the floor at the centre of the stage.

**Parallax.** A translation of `(parallax/256 - 1) * camTranslation` is applied before the camera matrix. At zoom 1 the result is `screen = world - (parallax/256)*cam + (320, 432)`.

**CG sprite pivot.** A CG image quad spans `bounds - (128, 224)`. In other words, the canvas point (128, 224) sits at the object origin. That point is the u4ick "floor at +224, centre at +128" (not 127.5). **PAT patterns get no pivot.**

**Draw order.**
- Active instances are bucketed into 2 bands × 300 layers.
- Band 1 (objhdr+21 set) is drawn at render priority **600** (200 while a super-flash overlay is up). Band 0 is drawn at priority **10**.
- The draw list starts with the lowest priority, so band 0 comes first and band 1 is in front of everything else, including the fighters (INF: the fighters use a priority between the two).
- Within a band, layers are drawn in ascending order (layer 255 in band 0 is moved to 279), and then by slot in ascending order.
  - V-rt `Background_PushInstanceToLayerBucket` / `Background_NextInstanceFromLayerBuckets` and `DrawCommandList_InsertCommand` (which prepends).
  - The resulting order (layers ascending within a band, and band 1 always after band 0) is the rule the editor now uses.

**CG path.**
- Blend is taken from +9 (table above).
- Vertex RGB is **0xEA (234/255)** of the pass colour (V-rt 0x4b791e), so stage CG draws at about 92% brightness. The device runs COLOROP/ALPHAOP = MODULATE (`D3D_InitializeDeviceRenderStates` 0x4bf2a0), so the tint takes effect.
- With frame +20 set, alpha is lerped from the current frame to the next over the frame's duration.
- Band-1 instances also get the global bg alpha and the fade state (EF6 No.152).

**PAT path.**
- The frame's blend and opacity are **ignored**.
- For each part in `BgPat_BuildPartDrawOrder` order (priority +71 descending, then part index descending):
  - Diffuse ARGB comes from +64..67 and the specular (add) RGB from +68..70 (SPECULARENABLE = 1).
  - +49 sets additive blend.
  - +50 or the object's +22 sets **linear filtering**. It is not a blend flag.
  - The flip value at +48 mirrors the UVs.
  - Scale is `int/1000` at +52/+56.
  - Position is **int32** at +36/+40, loaded with `fild` at 0x4b75c1/0x4b75d0.
- The quad spans `(-origin .. quad-origin)` of the cutout. It is scaled, translated by the part position, then by the frame offset plus `pos/128`.
- With frame +20 set, the part scales are lerped toward the **next frame's pattern**. If the next frame has no pattern, the object is not drawn at all.

### A.5 Old-format PAT block (magic `2, 0x01234567`)

Sources: V-rt `Pat_ParseOldFormatToBank` 0x404cc0, `BgPat_UploadTexturesAndScaleCutouts` 0x418420 and the PAT branch of 0x4b7060.

Top-level tables:

| Off | Content |
|---|---|
| +40 | 1000 pattern offsets (0 = absent) |
| +4040 | 1000 cutout offsets |
| +8040 | 1000 × 32-byte pattern names |
| +40040 | texture region offset |

**Texture region.** +28 holds 50 texture offsets and +3428 holds 50 square sizes. The textures are raw A8R8G8B8.

**Pattern.** A pattern is 40 parts of 104 bytes; a cutout reference of `-1` at +32 marks an empty part. See A.4 for the part fields.

**Cutout** (84 bytes):

| Off | Field |
|---|---|
| +0 | texture |
| +4 | 1 or 2 (unused, INF) |
| +8..+20 | src x, y, w, h in **256-unit space** |
| +24/+28 | quad w/h (may be negative, which mirrors the quad) |
| +32/+36 | origin |
| +52 | name |

The loader multiplies the src rect by `texSize/256` whenever the texture is larger than 256. V-rt; the same code exists in mbacPC `BgPat_UploadTexturesAndScaleCutouts` 0x43e160. V-data: bg07's 512-pixel texture has cutout `(0, 0, 256, 64)` with quad 512×128.

The same PAT format with the same magic is embedded in HA4 character files (`HA4_SECTION.md` §1.8).

### A.6 MBAC vs MBAACC

**Same in both:**
- The container, object, frame and record layout.
- The 44-byte instance.
- The stepping, motion, commands, triggers and spawn placement (every function compared).

**MBAC only:**
- Frame +16/+18 scale (CG).
- Part +60 rotation (int, 1/10000 of a turn, ×0.036°), with scale and rotation lerp.
- A 3D projection with focal length 500. Screen x is `(pos>>7) - (par*(cam-0x6000)/256 + 0x6000 >> 7) + 384` for CG and `+512` for PAT.
- `bgNN_s.dat` variants.
- `bgNNlight.txt`: a count followed by `pos,power` lines (`LoadLightingData` 0x401680).
- The animation tick is skipped while a screen fill hides the background.

**MBAACC only:**
- The 0xEA tint.
- The multiply blend for +9 == 3. MBAC maps it to its own blend mode 4.
- The band-1 render priority system (priorities 600/200/10).
- `BgList.ini`: `[Bg_%03d]` sections with `DataFile`, `InfoFile`, `IsSelectAble`, `IsGiantStage` and `StageColorVal` (`StageSelect_LoadStageData` 0x4b4a80).
- `bgNNInfo.txt` `[Data]`: `LightNum`, `Light%02dPos`/`Power`, `DropObj`, `DropObj_Type` (0 = bmp sprite particles with `DropObjFile`/`PatNum`/`FrameNum`/`Wait`/`W`/`H`; 1 = procedural with `Max`/`Alpha`/`H`/`Wait`) (`DropObject_LoadConfigFromFile` 0x4b6380).
- MBAACC does not read `bgNNlight.txt`; the file is a leftover.

**Data:** MBAC shipped files never use +16/+18 or +60 either (census), so the two formats render the same in practice.

---

## B. HA4 (Hantei4 `.DAT`) — summary

The full byte layout, the HA4→HA6 field rules and the audit are in **`HA4_SECTION.md`**. Key points:

**File structure.**
- HA4 is a **fixed-record binary**, not a tag format.
- 0x44-byte header, then a pattern offset table of 256 entries at 0x44 (absolute, so the first pattern is at 0x444).
- Patterns are packed, followed by a parts blob (the old PAT from A.5), a CG blob, and 256 × 64-byte names at EOF−0x4000.
- A frame is 216 bytes: AF 44 + AS 56 + i16 idx[58]. The index slots are:
  - 0: the attack record
  - 8..15: IF
  - 16..23: EF
  - 24: the collision box
  - 25..48: boxes 1..24
  - 49..56: attack boxes

**Attack record.** The legacy offsets were wrong: the flags are at **+0x40** and the hit effect at **+0x28**. The IDA type is corrected.

**Conversion to HA6.**
- The AS movement bytes are clearX, clearY, addX, addY. Clear+add becomes HA6 "Set" (0x10/0x01) and add alone becomes "Add" (0x20/0x02).
- Tables are appended in frame order, not deduplicated.
- CG ids differ between MBAC and MBAACC, so an import must use the CG embedded in the `.DAT`.

**Verification.** `ha4_verify.py` passes all 50 MBAC files (4446 patterns, 67721 frames). `mbacdat.py` agrees with it. han4_parser decodes 0 of 4446 patterns correctly.

**Hantei-chan master has no HA4 loader.** The only import code is on the unmerged `han4` branch, and every one of its field-offset bugs is listed with file:line in `HA4_SECTION.md` §3.1. Nothing in the user's working tree loads HA4, so there was no safe HA4 fix to make in the editor.

---

## C. Audit of existing code and docs (background)

### C.1 Hantei-chan background code (state before this session's changes; line numbers are from the pre-change working tree)

| # | Where | Bug | Status |
|---|---|---|---|
| 1 | `bg_file.cpp` Save (header write `w32(-1) w32(-1) wpad(40)`) | Object +12/+16 (event-table offsets) and +20/+21/+22 flags were overwritten with -1/0 | **Fixed** |
| 2 | `bg_file.cpp` Save (frame `wpad`… `ffBuf`) | Frame +16..20 (scale, interpolate) and +100..131 (trigger/command refs) were overwritten. Every stage with events lost its spawners: bg01 changed in 40 bytes, and stages 01, 08, 09, 13, 16, 29, 31, 34, 43, 44, 45, 49, 57, 58 were broken on save | **Fixed** |
| 3 | `bg_file.cpp` Save (gap handling "52-byte gaps we can't predict") | The gaps are the event record tables; they were zero-filled | **Fixed** (kept verbatim, offsets shifted when the frame count changes) |
| 4 | `bg_file.cpp` `Object::Update` | aniType 0 was treated as "respawn same object"; in the game it despawns. The code had no spawners, triggers, random spawns or random velocity, no `noAutoSpawn`, and only one instance per object (scrolling layers wrap because spawners create new instances). A jump to the same frame did not reload loopCount. | **Fixed**: new instance-pool runtime (D) |
| 5 | `bg_types.h` Frame comment | "0 = stop (halts)", which contradicts both `bg_file.cpp` and the game (0 = despawn) | **Fixed** |
| 6 | `bg_pat.cpp` Parse | Part position was read as **float**; it is int32. Of 341 parts, 196 store 320, which read as 4.5e-43 ≈ 0, so these parts were misplaced by 320 px | **Fixed** |
| 7 | `bg_pat.cpp` Parse | +50 was treated as additive; it is the linear-filter flag. The +68..70 specular colour and +60 rotation were not parsed | **Fixed** (parsed) |
| 8 | `bg_renderer.cpp` BuildPatParts | UV = texel·256/texSize, but cutout rects are already in 256-unit space. Every 512-px PAT texture (bg07, 18, 19, 20, 30, 43, 47, 55, 56, 57) sampled only a quarter | **Fixed** |
| 9 | `bg_renderer.cpp` PAT draw | The CG pivot (−127.5, −224) was also applied to PAT objects, drawing every PAT object 128 px left and 224 px up | **Fixed** |
| 10 | `bg_renderer.cpp` PAT draw | Frame opacity/blend was applied to PAT parts (the game ignores them) and the part ARGB was ignored (172 parts have alpha 120) | **Fixed** |
| 11 | `bg_renderer.cpp` sort | Sorting by layer only. The foreground band (objhdr+21) must be drawn after all band-0 objects (bg18, 46, 47, 55, 59) | **Fixed** |
| 12 | `bg_renderer.cpp` | u4ick's "skip dur==0 && ani==1 frame" is not in the game | **Fixed** (removed) |
| 13 | `bg_renderer.cpp` DrawSprite | Blend 3 was treated as normal; it is multiply. The 0xEA CG tint was missing | **Fixed** |
| 14 | `bg_renderer.cpp` | Frame +20 alpha interpolation missing | **Fixed** (CG alpha; PAT scale lerp in Wave 2) |
| 15 | `bg_types.h` | `STAGE_CENTER_X = 127.5`; the game pivot is 128 | **Fixed** |
| 16 | renderer / host | Band-1 (foreground) objects should draw **in front of the character**; the editor draws the whole stage behind it | **Fixed** in Wave 2 (Back/Front passes) |
| 17 | renderer | Linear-filter flags (+50, objhdr+22) and specular colour are not applied (0 in all data) | **Fixed** in Wave 2 |
| 18 | editor | `BgList.ini` / `bgNNInfo.txt` (DropObj weather particles, lights, `StageColorVal`) are not loaded | **Fixed** in Wave 2 (loaded, shown; weather rendered, lights as markers) |
| 19 | editor | MBAC-only scale (+16/+18) and PAT rotation (+60) are not rendered (0 in all shipped files) | **Fixed** in Wave 2 (MBAC mode) |
| 20 | `bg_test.cpp` | Its "0 field-level diffs" check was blind to #1–3 | Superseded: use `bgdump.py --roundtrip` (D.2) |

### C.2 Legacy docs: wrong claims (`docs/legacy/BGMAKE_*.md`)

- **"100% complete / all fields mapped"** (`BGMAKE_FORMAT_COMPLETE_100_PERCENT.md`): wrong. Missing are the event tables, object flags +20/+21/+22, frame +16..22 and +100..131, and the whole runtime event system.
- **Object +12/+16 "reserved, always -1"** (`BGMAKE_COMPLETE_FIELD_MAP.md:101-102`, `…100_PERCENT.md:57-58`): wrong. They are the event-table offsets; 38 objects have commands and 3 have triggers.
- **Object +20..59 "40 bytes padding"**: wrong at +20, +21 and +22 (flags).
- **Frame "+13 32 bytes padding"**: wrong. It contains scale +16/+18, interpolate +20, loopEnd +21 and loopCount +22.
- **Frame "+100 32 bytes unused end buffer"**: wrong. These are the trigger and command refs.
- **"+45/+46 enable x/y vec, +51/+53 xvec/yvec"** (100_PERCENT:90-97, u4ick's reading): wrong. The flags are at +44..+47 and velocity at +52/+54; acceleration at +60/+62 is missing.
- **"Sprite id stored as id+10000, subtract 10000"** (`FIELD_MAP.md:141,184`): only CG sprites. Values <10000 are PAT pattern indices.
- **"draw_type 2 = subtractive"** (`FIELD_MAP.md:216`): wrong. 2 is additive, 3 is **multiply**, 1 is alpha and 0 is opaque (alpha 255).
- **Anim-type table** (100_PERCENT:110-118, "0 Normal advance, 1 Loop to frame 0, …"): wrong. See A.3.1: 0 despawns, 1/3 advance, and running off the end despawns.
- **"v8 = … Reading parallax at +8"** (`FIELD_MAP.md:120`): +8 is the layer. The same swap was in the IDA comments.
- **"layer: higher = foreground"**: only within a band. Foreground (in front of the characters) is objhdr+21.
- **"parallax 384 = foreground"**: no shipped object uses >276. Foreground-ness is +21, not parallax.
- **The "+8 unknown byte"** claim: unused (always 0).
- **`INTEGRATION_GUIDE.md` / `bg_types.h`** claim that the u4ick floor is at +224: correct, but for the reason above (the CG pivot), and it applies to CG only.

---

## D. Changes applied

All edits are additive or local, written into the user's uncommitted working tree. Nothing was committed, stashed or reverted.

### D.1 Code

**`src/background/bg_types.h`** (user-modified file)
- Frame comment corrected (aniType 0 = despawn).
- `STAGE_CENTER_X` 127.5 → 128.
- New `CG_PIVOT_X/Y = 128/224`.
- New Frame fields: `scaleX`, `scaleY`, `interpolate`, `triggerRef[8]`, `commandRef[8]`, `raw[132]`, `hasRaw`.
- New `EventRecord`.
- New Object fields: `triggerTableOff`, `commandTableOff`, `noAutoSpawn`, `foreground`, `linearFilter`, `rawHeader[60]`, `hasRawHeader`, `recordBytes`, `triggers`, `commands`.
- New `Instance` struct (the 44-byte game slot).

**`src/background/bg_file.h`** (user-modified): adds `GetInstances()`, `ResetRuntime()`, `TickRuntime()` and private pool helpers.

**`src/background/bg_file.cpp`** (user-modified)
- `LoadObjects` parses the header flags, the new frame fields, the raw bytes, and the event tables (the bytes between the end of the frames and the next block).
- `Save` writes each header and frame from the loaded raw bytes and overlays the modelled fields. It writes the event tables after the frames and shifts their offsets by 132 × (frame-count delta). The dead legacy frame writer and `ffBuf` were removed.
- `Load` calls `ResetRuntime()` and `Free` clears the pool.
- `UpdateAnimations()` now runs `TickRuntime()` and mirrors the first live instance of each object into `Object::currentFrame/posX/posY`, so the stage panel keeps working.
- `StepObjectForward/Backward` also move that object's first live instance.
- New runtime at the end of the file: a 1:1 port of the MBAA functions in A.3. `Object::Update/Reset` (the user's port) are left in place but are no longer called.
- The only intentional deviation is the RNG: xorshift instead of the game's lagged-Fibonacci stream. (Superseded in Wave 2: the game RNGs are ported.)

**`src/background/bg_pat.h/.cpp`** (untracked user files)
- Part position is now read as int32.
- +49 is additive; +50 is `linearFilter`.
- +60 `rotation` and +68..70 add colour are parsed.
- The comments are corrected.

**`src/background/bg_renderer.cpp`** (user-modified)
- The shader gains a `uTint` vec4 (diffuse modulate).
- `DrawSprite`: blend 3 is multiply (`GL_DST_COLOR, GL_ZERO`), and the 234/255 CG tint is applied.
- `BuildPatParts`: UV uses the 256-unit cutout rect directly.
- `DrawPatPatternFlat`: uses the part ARGB and ignores the frame blend and opacity.
- `Render`: iterates live **instances**, sorted by (foreground band, layer with 255→279, slot).
  - Removes the u4ick dur-0 skip.
  - CG uses `pos>>7`; alpha is interpolated for frame +20.
  - PAT gets the `+CG_PIVOT` correction and fractional `pos/128`.
- The user's debug dump code is untouched.

**`src/bg_test.cpp`** (tracked, unmodified before): new `--sim N` mode that ticks the runtime and prints the live-instance count.

**`docs/tag_research/tools/mbacdat.py:63`**: X MAX is read as int16; it was a byte. Found by the HA4 sub-investigation. All 50 DATs still parse.

### D.2 Verification

**Build.** `flock /tmp/claude-1000/hantei_build.lock ./build.sh` (MinGW) succeeded: gonptechan.exe, bg_test and all other targets.

**Round trip.** Every one of the 124 stage files (56 MBAACC and 68 MBAC) went through `bg_test.exe in out` and was byte-compared with `cmp` / `bgdump.py --roundtrip`. **124/124 are byte-identical.** Before the change, bg01 lost 40 bytes: its spawner commands.

**Runtime simulation** (`bg_test.exe bgNN.dat --sim 3600`):

| Stage | Live instances over 60 s |
|---|---|
| bg01 | stable at 14–15 (clouds respawned by spawners) |
| bg09 | oscillates 11–19 |
| bg16 | about 500 (random-spawned snow despawning via Y triggers) |
| bg34 | 44–45 |
| bg43 | 16 |
| bg57 | 22–24 |

None of these grow without bound and none empty out.

**Not verified:** the rendered output was not visually compared against the game in this session. The renderer's `C:/dev/bg_dump.png` self-capture can be used for that.

---

## Wave 2 changes (branch `feat/bgfix`, merged with `update/ex-mbac` db83c66)

Date: 2026-09-23. New RE in this wave is marked as before; decompiler dumps of every function used are in `data/mbaa_bg_decomp_wave2.c`.

### W.1 New RE

**Stage RNG (V-rt).** The stage code draws from **stream 0** of the MBAA RNG bank `g_RngStreamBank` (0x563780; 11 streams × 57 dwords `{index, seed[56]}`).
- `RngState_Initialize` 0x421af0 (eax = seed, ecx = stream) is Knuth's subtractive generator seeding, identical to .NET `System.Random(seed)`: MBIG 0x7FFFFFFF, MSEED 161803398, lags 55/21.
- `Rng_UpdateState` 0x421c10 returns the next int; `Rng_GetFloat01` 0x421c50 multiplies it by 4.656612875028957e-10; `Rng_Stream0_NextInt` 0x4b4f70 (was `Rng_LaggedFibonacci`) is an inlined copy for stream 0.
- Verified against the port: seed 0 gives 1559595546, 1755192844, 1649316166 (the .NET values).
- Stage consumers, in call order: `BgCmd_SpawnRandomObject` (slot, then x, then y), `BgCmd_RandomizeVelocity`, `DropObject_InitializeParticles`, `DropObject_UpdateParticles`. Some effect presets also draw stream 0, so an in-match sequence matches a stage-only replay only while none of those run.
- The game reseeds every stream from its master seed at round start (`Replay_BeginRoundRngSeeds`, `ResetBattleMode`).
- MBAC: `Rng_NextForStream(0)` 0x45ef40 is an LCG, `s = s*0x41C64E6D + 0x3039`, result `(s>>16) & 0x7FFF`.

**BgList.ini (V-rt `StageSelect_LoadStageData` 0x4b4a80).**
- Sections `Bg_001`..`Bg_099` fill `g_StageListEntries` (0x74FC08). Each entry is 48 bytes: `+0 DataFile[32]`, `+32 InfoFile`, `+36 IsSelectAble`, `+40 IsGiantStage`, `+44 float StageColorVal`.
- A section is skipped when `<DataFile>.dat` is missing. Absent keys read as 0.
- `InfoFile` is parsed but never read. `Background_LoadInfoFile` 0x4b6ab0 opens `<DataFile>Info.txt` for **every** stage, which is why bg28/37/38/52/56 have lights without `InfoFile = 1`.
- `StageColorVal` is the `fColorHosei` parameter of the `BgPointBlur` post effect (`Background_CalculateTransitionAlpha` → `PostFx_ApplyBgPointBlur`). It is not a stage tint.

**bgNNInfo.txt (V-rt `DropObject_LoadConfigFromFile` 0x4b6380).** Keys are read from the whole file (no section select).
- **Lights:** `LightNum` (≤ 10) sets `dword_74FEA8[0]`, `Light%02dPos` fills `g_BgLightPos` and `Light%02dPower` fills `g_BgLightPower`.
- **DropObj:** the count is 100. `DropObj` → `dword_76E64C[86]`; `DropObj_Type` (absent → −1) → `[87]`.
  - Type 1 (rain): `_Max` (default 100) is the count, `_Alpha` 100, `_H` 150 (streak length), `_Wait` 50 (fall speed).
  - Type 0 (bitmap): `DropObjFile` (`.\Bg\<name>.bmp`), `_PatNum` → `[88]` (texture rows), `_FrameNum` → `[89]` (columns), `_Wait` (ticks per frame), `_W`/`_H` (cell size).
  - Type −1 (bg99): `HudSpriteQueue_RenderGridWith3DTransform`.
- The particle array at 0x766008 holds exactly 100 × 44 bytes. A larger `_Max` would overflow it, so the port clamps.
- INF: when a key appears twice (bg56 has `Light01Pos` twice and no `Light00*`), the first occurrence wins.

**Lights (V-rt `Character_Render` 0x41b411).** Lights never touch the stage draw. For each light:
- `lightX = Pos − 512` in world px, and `w = 1 − |charX − lightX| / Power`.
- When `w > 0`, the fighter is drawn again as a black shadow (sprite mode 110) projected from `(lightX, −200)`, with alpha `min(255, w·alpha/2)`.
- With no lights, the ordinary flat shadow is drawn.
- In bg28 the one light (Pos 300) lands on the left street lamp.

In MBAC, `bgNNlight.txt` (count, then `pos,power` lines) feeds the same shadow pass in `Character_Draw` with `w = 1 − |(x>>7) − pos + 256| / power`. The editor shows it at `pos − 256` (INF: this assumes the MBAC fighter origin matches MBAACC's). MBAACC ships `bgNNlight.txt` copies but never reads them.

**Weather particles (V-rt 0x4b4fc0 / 0x4b5210 / 0x4b5cc0).**
- **Particle layout (44 bytes):** `+0/+4` world x/y (floor y = 0), `+8` row, `+12` frame (float), `+16` wait counter (float), `+20` alpha, `+24` `rand·256` (never read), `+28/+32` velocity, `+36/+40` acceleration (always 0).
- **Init, type 0:** `x = F·1024 − 512`, `y = −F·1024`, `pat = Next % PatNum`, `vy = F·2 + 1`, `vx = F − 0.5`.
- **Init, type 1:** `x = F·1124 − 512`, `y = −F·1024`, angle `(F·0.005 + 0.5)·2π`, speed `Wait + F·Wait/5`, `vx = sin·speed`, `vy = −cos·speed`.
- **Tick:** runs after the triggers. Each particle does `pos += vel; vel += acc`.
  - Rain wraps at `y > 64` to `y = −1024` with a new x.
  - Petals lose 16 alpha per tick below the floor and respawn at alpha < 0.
  - The animation frame advances every `Wait` ticks.
- **Draw:** particles are drawn in world space with the fighters' camera matrix, at render priority **522**, between band 0 (10) and band 1 (600).
  - Rain is a line from the particle to `RotZ(Atan2Normalized(v)·2π)·(0, H)` (that is, H px behind it), coloured `(Alpha<<24)|EAEAEA` fading to `00FFFFFF`.
  - Petals are a W×H quad centred on the particle, with uv `(W·frame, H·pat, W, H)` and colour `alpha<<24|EAEAEA`, plus the `TecSakuraBloom` post effect (not reproduced).

**PAT frame +20 lerp detail (V-rt 0x4b7473).**
- `t = timer / (float)(u16)duration`, with no zero guard.
- Slot *i* scale = `((next[i] − cur[i])·t + cur[i]) · 0.001`, taken from the **next pattern's same slot index**, even when that slot is empty. Part position is not lerped.
- The next frame's pattern is looked up even when +20 is off; if it is missing, the instance is not drawn.
- Sampler state: byte `0x56447F` = 1 (point) by default; object +22 or part +50 sets 2 (linear). The CG path honours object +22 as well.

**MBAC renderer (V-rt mbacPC `Background_DrawInstance` 0x401ea0).**
- The PAT branch **never reads the part position +36/+40**; the disassembly only touches cutout fields there.
- Part +60 rotation (degrees = `rot·0.036`, lerped by `Math_LerpAngle10000ToDeg`) is applied about the cutout origin after the part scale.
- CG +16/+18 scale (0 → 256) is lerped like alpha and applied about the object origin (the canvas point 128, 224). INF: this reading of the matrix order.
- CG vertex colour is 0xFFFFFF (no 0xEA tint).
- Net offsets: CG draws at `(384, 192) + pos` and PAT at `(512, 416) + pos`, the same (128, 224) CG pivot difference as MBAACC.
- **`bgNN_s.dat`:** `LoadStageBackground` 0x4038a0 loads it when the flag copied from the match config (`unk_790CE8`, `Battle_ApplyMatchSettings`) is set. It uses the same format with a smaller CG.

### W.2 Code (Hantei-chan `feat/bgfix`)

| Item | Where | What |
|---|---|---|
| Foreground objects in front of characters | `bg_renderer` `Pass::{All,Back,Front}`; `main_frame.cpp` DrawBack | Back = band 0 before the character layers. Front = weather + band 1 + light markers, run by a scope guard after the character draw on every exit path. Stage-only views get back → grid → front |
| PAT scale fade | `DrawPass` / `DrawPatPatternFlat` | Frame +20 lerp as in W.1; missing next pattern skips the draw. `PatPattern` keeps raw per-slot scale/rotation for all 40 slots |
| Smooth filter, additive colour | `EmitQuad`, fragment shader | Per-draw point/linear sampling (object +22, part +50). Part +68..70 specular is added after the texture modulate (`uAdd`). PAT textures are no longer always linear |
| BgList.ini / Info.txt / light.txt | `bg_info.{h,cpp}`, `File::ReloadSideFiles` | Parsed from the stage's folder (case-insensitive) and shown in the inspector ("Stage files") |
| Weather | `DropSystem` (bg_info.cpp), `Renderer::DrawWeather` | Port of Init/Update/Render: rain streaks and bitmap petals, loaded from `<DropObjFile>.bmp`. Type −1 and the bloom are not reproduced |
| Lights | `File::ActiveLights`, `Renderer::DrawLights` | Markers: a cross at `(lightX, −200)` and a floor bar fading over ±Power |
| MBAC | `File::GetGame/SetGame` (MBAACC/MBAC) | Guessed at load (BgList.ini → MBAACC; `_s` sibling or upper-case dump name → MBAC) and switchable in the inspector. MBAC mode: CG scale, PAT rotation without part position, no tint, integer PAT position, LCG RNG, no weather. "Open _s variant / full variant" button |
| Game RNG | `bg_rng.h`, `File::SetSeed` | Replaces the xorshift. `ResetRuntime` reseeds, spawns, then initialises weather, matching the game's order. Seed, tick and draw count are shown in the inspector |
| Editing | `bg_inspector.cpp`, `File::InsertFrame/DeleteFrame/AddRecord/DeleteLastRecord`, `EventRecord::SyncRaw` | Object header (parallax, layer, +20/+21/+22), every modelled frame field and the 16 event refs, and trigger/command records with decoded summaries. Save/Save As |
| Stage undo/redo | `File::CommitEdit/Undo/Redo`; stage-view handler in `MainFrame` ctor (`shortcuts.setContextHandler`) | Own history of object snapshots (≤ 100 steps, visibility kept), one step per inspector gesture. While the stage context has focus (stage tab or the focused Background Inspector), Ctrl+Z/Ctrl+Y undo/redo stage edits and Ctrl+S saves the stage. It never touches the character undo stack |
| Save invariant | `File::Save` | Unedited objects write their record bytes verbatim, so shipped files stay byte-identical. Edited records are patched in place. Adding or removing a record rebuilds that object's block as `[triggers][commands]` with fresh offsets. Only records reachable through frame refs survive a rebuild |
| Debug capture | `Renderer::RequestDebugDump`, `HANTEI_STAGE_PREVIEW` env hook | The self-screenshot used to re-arm every 1.5 s and wrote `C:/dev/bggeom_log.txt`. It now runs only on request |

`main_ui_impl.h` replaces the old object list and frame editor with a call to `bg::DrawInspector`. The pan/diagnostic block above it is unchanged.

### W.3 Verification

`tools/bg_regress.sh [ticks]` (in the Hantei-chan repo) runs over all 56 MBAACC (`/mnt/c/games/mbaacc/bg`) and 68 MBAC (`/mnt/c/games/MB/AC/dump/05`) stage files:
- **Round trip:** `bg_test in out` now byte-compares the output with the input. **124/124 are identical.**
- **Bounded sim:** `--sim 3600 --seed 1234` on each file. The run fails if the live instance count reaches 1900 (pool 2000). All 124 pass; the maximum is 525 (MBAC BG16 snow).
- **Determinism:** bg16, bg41 and bg55 give the same state hash for the same seed and a different hash for a different seed.
- **Edit test:** on bg01 and bg16, change a command record, add a trigger record plus a frame, change frame and flag fields, save, reload and verify, then run 600 ticks. Then undo the edit (the saved file must be byte-identical to the input) and redo it (it must match the edited file).
- **RNG:** checks the .NET vectors for seed 0.

The last full run took 2 m 38 s and reported 0 failures.

**Visual check** with `HANTEI_STAGE_PREVIEW`:
- bg41: petals fall and animate.
- bg55: rain streaks fall; the foreground bamboo (band 1) draws over the grid.
- bg28: the light marker sits at the street lamp.
- bg18: the PAT foreground draws in the front pass.
- MBAC BG07 draws in MBAC mode.

**Not verified:** a side-by-side against the running game with the same seed. The game's stream 0 is reseeded per round and shared with some effects.

### W.4 IDA (saved)

**MBAA (`212fe94c`)**
- Renames: `Rng_LaggedFibonacci` → `Rng_Stream0_NextInt`, and the globals `g_RngStreamBank` 0x563780, `g_DropObjCount` 0x54CE98, `g_DropObjRainAlpha` 0x54CE9C, `g_DropObjH` 0x74FEC8, `g_DropObjW` 0x74FD9C, `g_DropObjWait` 0x76713C, `g_BgLightPos` 0x7671C4, `g_BgLightPower` 0x76719C, `g_StageListEntries` 0x74FC08.
- Function comments: `Rng_Stream0_NextInt`, `RngState_Initialize`, `Rng_GetFloat01`, `DropObject_InitializeParticles/UpdateParticles/RenderWithBloom/LoadConfigFromFile`, `StageSelect_LoadStageData`.
- Instruction comments at 0x41b4a5 (light shadow), 0x4b7473 (PAT lerp) and 0x4b9516 (`StageColorVal`).

**mbacPC (`mbacpc`)**
- Function comments: `LoadLightingData`, `LoadStageBackground` (`_s` flag), `Rng_NextForStream`.
- Instruction comment at 0x40215f (part position not read; rotation path).

### W.5 Still open

- The `TecSakuraBloom` post effect and DropObj type −1 (bg99 grid) are not reproduced.
- The fighters' light shadows need a character in the stage view. Character views do not show a stage yet; the Front pass is ready for it.
- The MBAC light x origin and the matrix order for MBAC CG scale (INF).
- MBAC blend mode 4 for frame +9 == 3 is still drawn as multiply.
- Band-1 global bg alpha / fade state (EF6 No.152) and the super-flash priority 200 are not modelled.

---

## 5. IDA work

### MBAA.exe (`212fe94c`, bg/stage code only; saved)

**23 functions renamed.** Several old names were wrong; the 404xxx functions are character/effect PAT loaders, not stage code.

| Address | Old name | New name |
|---|---|---|
| 0x404a40 | LoadStageBackground | Pat_LoadFileAnyFormat |
| 0x404940 | PANI_LoadStageForeground | Pat_FileIsPAniDataFile |
| 0x405360 | StageBackground_LoadDataFile | Pat_LoadOldFormatFile |
| 0x404cc0 | StageData_LoadFromFile | Pat_ParseOldFormatToBank |
| 0x404740 | LoadStageData | Pat_LoadPaniBmpTextures |
| 0x418420 | CharPortrait_LoadSurfaces | BgPat_UploadTexturesAndScaleCutouts |
| 0x4b4d00 | DynamicArray_PushToSlot | Background_PushInstanceToLayerBucket |
| 0x4b4dc0 | DynamicArray_GetNextElement | Background_NextInstanceFromLayerBuckets |
| 0x4b4d70 | DynamicArray_CleanupAllSlots | Background_ClearLayerBuckets |
| 0x4b6d10 | BackgroundLayer_UpdateLayerState | BgInstance_EnterFrame |
| 0x4b6db0 | BackgroundLayer_Initialize | BgInstance_Init |
| 0x4b6e00 | Background_InitializeLayers | Background_SpawnInitialInstances |
| 0x4b89e0 | Background_InterpolateLayerPosition | BgInstance_PlaceRelativeToParent |
| 0x4b8aa0 | Background_InitializeLayer | BgCmd_SpawnObject |
| 0x4b8b20 | Background_InitializeLayerRandom | BgCmd_SpawnRandomObject |
| 0x4b8c10 | Background_SetLayerRandomRanges | BgCmd_RandomizeVelocity |
| 0x4b8cd0 | Background_ProcessLayerCommands | Background_RunFrameCommands |
| 0x4b8df0 | Background_UpdateLayerTransitions | Background_RunPositionTriggers |
| 0x4b8f80 | Background_RenderLayers | Background_DrawAllInstances |
| 0x4b7060 | Background_RenderLayerWithPalette | Background_DrawInstance |
| 0x4b7000 | Background_BuildLayerIndexMap | BgPat_BuildPartDrawOrder |
| 0x4b88b0 | Background_UpdateLayerAnimations | Background_StepInstanceAnimations |
| 0x4b8530 | Background_UpdateLayerPositions | Background_IntegrateInstanceMotion |

**Other changes.**
- 1 global renamed: `dword_74FED0` → `g_BgLayerBuckets`. The remaining bg globals (0x750830 object block, 0x750840 pool, 0x767138 PAT, 0x766000 bg alpha, …) sit outside defined segments and cannot be named; they are documented here instead.
- **27 function comments**, each with `CONF: high` and evidence: 23 new, plus 4 appended to LoadAndExpandData, LoadInfoFile, CG_Background_LoadSpriteData and UpdateAndRender.
- **6 instruction comments**, including the corrected parallax/layer swap and the int32 part positions.

**Outside the bg/stage scope.** Not renamed, but worth fixing later: `RenderState_SetTextureStageState` 0x4c0690 actually applies **blend render states** (BLENDOP/SRC/DEST).

### mbacPC.exe (`mbacpc`; saved)

**15 functions renamed:**

| Address | Old name | New name |
|---|---|---|
| 0x43e160 | ProcessPATData | BgPat_UploadTexturesAndScaleCutouts |
| 0x401730 | Background_ResetObjectAnimFrame | BgInstance_EnterFrame |
| 0x4017d0 | Background_InitRuntimeObject | BgInstance_Init |
| 0x401830 | bg_parse_object_headers | Background_SpawnInitialInstances |
| 0x403160 | Background_ComputeObjectTransform | BgInstance_PlaceRelativeToParent |
| 0x403230 | Background_SpawnObject | BgCmd_SpawnObject |
| 0x4032c0 | Background_SpawnObjectRandom | BgCmd_SpawnRandomObject |
| 0x4033c0 | Background_ApplyRandomVelocityEvent | BgCmd_RandomizeVelocity |
| 0x4034b0 | Background_ProcessObjectSpawnEvents | Background_RunFrameCommands |
| 0x4035b0 | Background_UpdateTriggers | Background_RunPositionTriggers |
| 0x401ea0 | Background_RenderLayer | Background_DrawInstance |
| 0x401e40 | Background_SortPartsByPriority | BgPat_BuildPartDrawOrder |
| 0x403020 | Background_TickAnimations | Background_StepInstanceAnimations |
| 0x402c90 | Background_UpdateLayerPositions | Background_IntegrateInstanceMotion |
| 0x4019a0 | Background_ProcessTextures | Background_UploadCgAndPatTextures |

**18 function comments** with CONF and evidence, including the MBAC-only scale and rotation, the screen formula, and the light file.

**HA4 sub-investigation, also in this database:** 5 globals and 12 comments (`HA4_SECTION.md` §5).

### hantei4.exe (`hantei4`)

- 24 local-variable renames.
- The attack-record type is corrected.
- 5 function comments.

All come from the HA4 sub-investigation; see `HA4_SECTION.md` §5.

### Tools looked for

- **No French Bread BGMAKE.exe** was found under `/mnt/c/games` or `/mnt/c/dev`.
- u4ick's C# bgmaketool (`src/dpu4/stagesrc/bgmaketool`) was used only as a secondary reference.
- `amapani.exe.i64` was opened. It is a PAniDataFile/3D (Lua, X-mesh) tool with no bgmake or old-PAT code, so it is not relevant here. It was closed without changes.
