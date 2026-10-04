# RBO script lists, sections 4/5, PAT part records, CG bank (rbo.exe)

Source: `C:\games\rbo\rbo.exe.i64`. Companion header with identical structs/enums and static_asserts: `rbo_scripts_pat.h`.
The same types exist in the IDB (`RboScriptListEntry`, `RboSection4Record`, `RboSection5Record`, `RboPatHeader`, `RboPatPart`
(flip = bitmask enum `RboPatPartFlip`), `RboPatPose`, `RboPatFileHead`, `RboPatImageHead`, `RboCgFileHead`, `RboCgEffectRecord`,
`RboCgBlock`, `RboPatRuntime`, `RboCgRuntime`, `RboHan2Dat`, `RboScriptBank`, enums `RboScriptKind`, `RboCgStorageType`,
`RboFrameEnterCategory`, `RboTransitionCategory`) and are applied to the loader/consumer prototypes.
Data statements ("all N files") were measured over the shipped archives with `han2lib.py` (346 HAN2RBO files, 208 `.DAT` with a PAT, 80 with a CG area).

## 0. Runtime container (needed to read the evidence)

`CharData` = `g_CharData + slot*0x734C`. `CharData+0x14` is a `RboHan2Dat` (size 0x6E80 = 28288; `CharData+0x6E94` = script bank id follows it directly: 0x14+28288 = 28308, confirming the size).

| Han2Dat off | Field | Proof |
|---|---|---|
| +0x00 | loaded flag | `Han2Dat_UpdateLoadedFlag` 0x4032E0: `base && (!pat.file_size || pat.pose_data)` |
| +0x04 | xor flag | `Han2Dat_LoadPatternSectionPreferDt2` stores header dword 0x1C here |
| +0x08 | pattern-area allocation = `actor+0x7F0` points here | `Actor_Init` 0x441A90 (`[ebx+7F0h] = char+0x1C`); `Han2Dat_ComputeSectionPointers` writes `a2[0]=base` |
| +0x0C..+0x28 | section 0..7 pointers | `Han2Dat_ComputeSectionPointers` 0x403550 (+4 .. +32 from actor+0x7F0) |
| +0x30 | `RboPatRuntime` (4220 B) | `Han2Dat_LoadPartsPatBlock(.., a2+12 ints)`, `Han2Dat_FreeSections` (`a1+48`) |
| +0x10AC | `RboCgRuntime` (24020 B) | `Han2Dat_LoadCgBank(.., a2+1067 ints)`, `Han2Dat_FreeSections` (`a1+4268`) |

Sections 0..5 are the only ones passed through `Han2Dat_DecryptBlockIfFlag` (`Han2Dat_DecryptPatternSections`); 6 and 7 never are (XOR flag is 0 in all files anyway).

## 1. Section 4 and 5

### 1.1 Readers: none in rbo.exe (proved by exhaustive search)

* All code that can reach the container does it through `actor+0x7F0` (11 functions contain that displacement: `Actor_RunFrameScriptList6/7`,
  `sub_41EE10` (now box test), `Actor_GetFrameRecord`, `Actor_CacheCurrentFrame`, `Actor_GetFrameSpriteId`, `Actor_CollectAttackBoxes2`,
  `Actor_CollectHurtBoxes3`, `Actor_CollectBoxes2_272`, `Actor_PickBoxRect_244`; `Actor_Init` only stores it) or through `actor+0x7EC` (`CharData`, 60+ users).
  Section 4 is `container+0x14` (= `CharData+0x30`), section 5 `container+0x18` (= `CharData+0x34`).
* IDAPython register-taint scans (source = `mov r,[x+7F0h]` resp. `mov r,[x+7ECh]` resp. return value of `CharData_GetSlot`/`sub_43D430`/`sub_43D580` resp. `offset g_CharData`) over every function:
  accesses at `+0x14/+0x18` (container) or `+0x30/+0x34` (CharData), and any non-constant index off such registers, find **no** reader. A positive control
  (`+0x1C/+0x20` = sections 6/7) finds exactly `Actor_RunFrameScriptList6/7`, so the scan works.
  `Han2Dat_FreeSections/UpdateLoadedFlag/ComputeSectionPointers/DecryptPatternSections` treat the block opaquely.
* The frame record fields that index them are never read either: frame `+0xD0` and `+0xE0` do not appear as displacements in any frame-tracking code (taint from `actor+0x564` and from `Actor_GetFrameRecord`); the only `+0D0h` hit in the exe is an unrelated table (`sub_4373D0`).
  Residual risk: a reader that rebuilds the frame pointer arithmetically; none was seen. The Ex-series exes (`rbo_ex3.exe` DB) have a different layout and were not checked.

### 1.2 What the data says (all 346 files; names are inferred)

* Frame record `+0xD0` (208) -> section 4, frame `+0xE0` (224) -> section 5. 1-based (`0` = none; per file min non-zero = 1, max = count in 13/162 resp. 12/144 files, never above the count; every other frame field exceeds the count somewhere).
* Section 4 record, 28 bytes (3115 records): `{type, a, b, c, d, 0, 0}`.
* Section 5 record, 20 bytes (2522 records): `{packed u8[4], a, b, 0, 0}`.
* Both start with an event type id from the same id space as the script event records (2,3,1,9,10,14,15,11 ... 249); type 249 is the "unlinked" type of `g_TransitionEventCategory`/`g_FrameEnterEventCategory`. They look like precompiled per-frame event slots for the editor (Hantei-chan frame +208/+224 fields). Parameter meanings are not provable from the exe.

| Rec | Off | Name | Evidence / confidence |
|---|---|---|---|
| S4 | 0x00 | event_type | data (values 1,2,3,9,10,11,14,15,249...) - inferred |
| S4 | 0x04 | param_a | data - inferred |
| S4 | 0x08 | param_b | data - inferred |
| S4 | 0x0C | param_c | data - inferred |
| S4 | 0x10 | param_d | data (0 in 2948/3115, else 3/10/1) - inferred |
| S4 | 0x14 | reserved_14 | unread by exe (search above), 0 in 3115/3115 |
| S4 | 0x18 | reserved_18 | same |
| S5 | 0x00 | packed_type_bytes | data: 4 bytes [type 1,3,4,6,9,249,..][0..28][0..21][0,1,2..] - inferred |
| S5 | 0x04 | param_a | data - inferred |
| S5 | 0x08 | param_b | data (0 or 655390 etc) - inferred |
| S5 | 0x0C | reserved_0C | unread, 0 in 2522/2522 |
| S5 | 0x10 | reserved_10 | unread, 0 in 2522/2522 |

## 2. Script lists (sections 6 and 7)

### 2.1 Record: `RboScriptListEntry` (20 bytes = 5 dwords)

| Off | Name | Proof |
|---|---|---|
| 0x00..0x10 | script_id[5] | `Actor_RunFrameScriptList6` 0x41DA50 / `Actor_RunFrameScriptList7` 0x41F900: `p = section + 20*idx + 16`, loop 5 times reading `*p` then `p -= 4` (slot 4 first, slot 0 last); id 0 = skip; else `Script_ResolveIndexedFunction(bank, kind, id)` |

* idx comes from frame record `+0xBC` (188, section 6, kind 0) / `+0xC0` (192, section 7, kind 1); record 0 is all zero (index 0 = none). Section 6 ids go 0..9999 (ACOLYTE_F: 118 records, max 9999) and index the FOB index table 0 (10000 ids); section 7 ids 0..2000 index table 1 (2001 ids). Verified with `ACOLYTE_F.FOB`.
* Neither section is ever decrypted (see 0).

### 2.2 `.FOB` script bank and index tables (`Script_LoadIntoBank` 0x4248B0)

File: `u32 nFuncs; {char name[32]; u32 entryPc}[nFuncs]; u32 nIndexTypes; {u32 count; i32 entryPc[count]}[nIndexTypes]; u32 codeSize; u8 code[]`.
In ACOLYTE_F: 4 named functions (`call_set_emotion_data`, `CallItemCreate`, `set_command_check_data`, `CharaSeLoad`), index types of 10000 / 2001 / 19 / 2 ids (-1 = undefined).
Bank record `RboScriptBank` (288 B, `g_ScriptBanks[64]`), see header. `Script_ResolveIndexedFunction(bank, kind, id)` (was sub_425100) returns `types[kind].entryPc[id]`, or -1 after printing "Undef IndexType/Over IndexNo/Over IndexType" overlay errors (range checks `kind < nTypes`, `id < count`).

| Kind | Enum | Callers |
|---|---|---|
| 0 | FRAME_ENTER_ACTIONS | `Actor_RunFrameScriptList6` |
| 1 | TRANSITION_RULES | `Actor_RunFrameScriptList7` |
| 2 | VARIABLE | kind and id come from data: AT-record condition triples in `sub_41E810` (`Script cond list at attack+0x1C: {0,kind,id,argset}`), config `dword_AB16A4/AB16AC` in `sub_41B790`, `sub_419710`, `sub_4205C0`, `sub_420B30` |
| 3 | SYSTEM_HOOK | `sub_43D980` (id 0), `sub_43DA10` (id 1): fixed ids, called with `(value, actor)`, return value replaces `value` |

### 2.3 Script VM entry points (all renamed)

| Name | Addr | Behaviour |
|---|---|---|
| `Script_ResolveIndexedFunction` | 0x425100 | kind/id -> entry pc (above) |
| `Script_GetCurrentThread` | 0x424790 | returns current thread id (`dword_AE1D40`) |
| `Script_AllocThread` | 0x424610 | first free of 50 thread slots (550 dwords each at `dword_AC6D68`), marks active, clears wait fields; -1 if none |
| `Script_ActivateThread` | 0x424850 | if thread active (and not waiting on its frame counter) bind it via `Script_BindThreadContext`, return 1 |
| `Script_BindThreadContext` | 0x4247E0 | sets the current-thread globals (stack at `unk_AC6DAC+2200*t`, sp, pc, bank, ...) |
| `Script_SelectBank` | 0x424CB0 | selects `g_ScriptBanks[id]`, sets pc base |
| `Script_PushEntryCall` | 0x427C90 | pushes the frame (return pc, bank, marker 4), selects bank, sets pc = resolved entry (stack limit 512) |
| `Script_SetArg` / `Script_GetArg` | 0x4268C0/0x4268B0 | `g_ScriptArgs[i]` (0xAB6A64) in/out argument array |
| `Script_RunThreads` | 0x428EC0 | run one thread (or all 50 if -1) through `Script_RunInterpreter` until it yields/ends |
| `Script_LoadBankHandle` | 0x424E60 | `.fob` file -> bank handle (frees the old one) |
| `Script_LoadIntoBank`, `Script_FindFunction`, `Script_RunInterpreter` | already named | |

Frame-script protocol (both list runners): `thread = AllocThread; ActivateThread; PushEntryCall(bank, pc); SetArg(0, flag); SetArg(1, actor); RunThreads(thread)`.
Section-6 scripts get `arg0 = (actor+0x8A0 & 0x10) != 0` (`actor+2208`), section-7 scripts get the same pointer in arg1 and `arg0 = flag`. After the run
`arg[0]` = N and `arg[N]..arg[1]` are pointers to N event records inside the script data; each record's first dword is the **event type id**, the
rest are its parameters. A 12-byte (section 6, nodes at `actor+0x598 + 12*k`) resp. 8-byte (section 7, nodes at `actor+0x570 + 8*k`) node
`{next, ?, recordPtr}` is created and chained into a per-category list by the callback.

### 2.4 Callbacks

`Actor_LinkFrameEnterEvent` (was sub_41D9B0, section 6): `cat = g_FrameEnterEventCategory[*record]`; -1 -> reject (returns 0); 0 -> `actor+0x6B8` (`a1[430]`),
1 -> `actor+0x6C4` (`a1[433]`), 4 -> `actor+0x6D0` (`a1[436]`), others: only `node[2] = record`. Node = `{next, ?, record}`.
`Actor_LinkTransitionRuleEvent` (was sub_41F7F0, section 7): `cat = g_TransitionEventCategory[*record]`; -1 -> reject; 0 -> `+0x688`, 1 -> `+0x698`, 2 -> `+0x690`, 3 -> `+0x6A0`, 4 -> `+0x6A8`,
7 -> `+0x6B0`, 6 -> `actor+0x56C = record` (returns 2), others: node only. Node = `{next, record}`. Lists are reset each frame by `Actor_ClearFrameEnterLists`
(0x41D990) / `Actor_ClearTransitionRuleLists` (0x41F7C0).

Both category tables are 256 dwords (`g_FrameEnterEventCategory` 0x489ED0, `g_TransitionEventCategory` 0x489AD0) - only these types are non-(-1):

* Frame-enter: type 1 -> 3; 126..133, 233..241, 243..248, 250, 251 -> 0; 242 -> 4; 249 -> 2; 252, 253 -> 1.
* Transition: 11,13,21,127,128,197,201 -> 0; 15,17,123,125,200 -> 1; 14 -> 2; 20,50,52,196,198,199,202 -> 3; 16,53 -> 4; 249 -> 5; 122 -> 6; 51,124,126,203 -> 7.
  (In the table type 126 appears in both spaces; they are different tables.)

Consumers (confidence: medium, from decompiled dispatch, names are mine):
* Frame-enter category 0: `Actor_RunFrameEnterActions` (0x41D8E0) -> `Actor_ApplyFrameEnterAction` (0x41D640): switch on type 0x7E..0x85, 0xE9..0xFB (set speed, spawn child, play sound, set flags ...). Category 4: `Actor_RunChanceFrameEnterActions` (0x41D950): type 242 = `{range, threshold, nested record}` applied if `Rng_Range(range) < threshold` (`Actor_ApplyChanceAction` 0x41D910). Category 1 (types 252/253): read from the attacker's list in hit resolution (`sub_444530`).
* Transition rules (they all end by writing the next pose `actor+0x528` (u16 pattern) / `+0x52A` (u16 frame) / `+0x52C` (u8 facing) and usually calling `Actor_TickFrame`): category 0 `Actor_RunInputTransitionRules` (0x420F10: input-triggered jumps, types 11 `sub_41E020` move-command table, 13 `sub_41DE40` key-match, 21 `sub_41E400`, 127 `sub_4209B0`, 128 `sub_420900`, 197 `sub_41DD80`, 201 `sub_41E1A0` random weighted); category 1 `Actor_RunConditionalJumpList` (0x420460 via `Actor_EvalConditionalJumpRule`); category 7 `Actor_RunEndTransitionRules` (0x420DC0); category 4 `Actor_RunForcedJumpRules` (0x41F780) / `Actor_RunTimedJumpRules` (0x420520), `Actor_ApplyForcedJumpRule` (0x41F730: type 53 = `{pattern, frame, facing}`, -1 = keep).

## 3. PAT block (area 2 of HAN2RBO), `Han2Dat_LoadPartsPatBlock` 0x4037D0

Loaded range = `[area.off, area.off+area.size)` of the `.DAT` (never from the `.DT2`). Steps proven in the loader:
1. `pat.file_offset/size = area header +8/+12` (`a4[0..1]`).
2. read the first `0x8CBC = 36028` bytes into a stack buffer, decrypt if flag; **the 24-byte header and the name table are never inspected**: only the 1000 offsets (loop of 1000: `0 -> -1`, else `(off - 36028)/0x5C`) and the dword at 36024 (image offset) are used.
3. `pat.image_area_size = size - image_offset`; pose data = `[36028, image_offset)` loaded into a GlobalAlloc block `pat.pose_data` (`a4[1004]`).
4. `Han2Dat_LoadPatTextures` (0x403A70): loads `[image_offset, size)`, for each of 50 entries with non-zero offset (dword table at image-area +0x1C): `Texture_Create(res,res)` with `res = dword at +0xD64+4i`, then `Texture_UploadBgra` copies `res*res*4` bytes (B,G,R,A, 16-bit surfaces via per-channel LUTs) from `image + offset[i]` into texture `pat.texture[i]`.

### 3.1 File layout (all offsets from the PAT area start)

| Off | Size | Name | Proof / status |
|---|---|---|---|
| 0x0000 | 24 | `RboPatHeader` | not read by the engine; constant in all 208 PATs: `{3, 0x01234567, 0,0,0,0}` (so `unused`/reserved, versioning only) |
| 0x0018 | 4000 | `pose_offset[1000]` | engine-proven (loop above). 0 = absent; value = 36028 + 92*firstPartIndex; poses packed contiguously in ascending order (all 208 files, offset % 92 == 0) |
| 0x0FB8 | 32000 | `pose_name[1000][32]` | NOT read by the engine (stack buffer discarded). Shift-JIS labels, non-constant -> editor/file only |
| 0x8CB8 | 4 | `image_area_offset` | engine-proven (`v12`, becomes `a4[2]`) |
| 0x8CBC | 3680*n | poses `RboPatPose[n]` | engine-proven: pose = 40 parts x 92 B (`Actor_DrawPartsPose` loops up to 40 via `Pose_CollectSortedParts`) |
| image_area_offset | 11824 | `RboPatImageHead` | below |
| +11824 | sum(res^2*4) | texture pixels | packed back to back in table order; no trailing bytes in any file |

Pose lookup: `firstPart = pat.pose_first_part[spriteId]` (`Actor_DrawPartsPose` `*(CharData+0x44+0x10+4*spriteId)`, `-1` = nothing drawn); part pointer = `pose_data + 92*firstPart`.
Sprite ids >= 10000 are CG sprites: `Actor_DrawCgSprite` uses `cg.effect[id-10000]`.

### 3.2 Image-area head (`RboPatImageHead`, 11824 B)

| Off | Size | Name | Status |
|---|---|---|---|
| 0x0000 | 20 | reserved_00[5] | unread by `Han2Dat_LoadPatTextures` (it only touches +0x1C.., +0xD64..); 0 in all 208 files |
| 0x0014 | 4 | one_05 | same; 1 in all 208 |
| 0x0018 | 4 | texture_count | same; = number of non-zero offsets (1..4) |
| 0x001C | 200 | texture_offset[50] | engine-proven: `(char*)image + offset[i]`, 0 = absent |
| 0x00E4 | 3200 | texture_name[50][64] | not read (names Shift-JIS); non-constant |
| 0x0D64 | 200 | texture_size_px[50] | engine-proven: `Texture_Create(w,h)` with both = this value (256/512/0); textures are square |
| 0x0E2C | 8196 | reserved_block | unread; all zero in 208/208 files |

Texture entry = `size^2 * 4` bytes, rows top-down, byte order B,G,R,A (verbatim copy into an A8R8G8B8 surface; the 16-bit LUT path indexes LUT0/1/2/3 with bytes b2/b1/b0/b3 = R/G/B/A).
Hantei-chan/PACNyx "RGBA" is therefore really BGRA in memory.

### 3.3 Part record `RboPatPart` (92 bytes) - all fields traced

Traced functions: `Actor_DrawPartsPose` 0x447720, `Pose_CollectSortedParts` 0x4461D0, `PosePart_GetFlippedSrcRect` 0x440A90, `PosePart_InterpolateTransform` 0x445ED0 (`PosePart_LerpOrigin` 0x445D40, `PosePart_LerpScale` 0x445DA0, `PosePart_LerpPosition` 0x445E10), `PosePart_LerpModulateColor` 0x446000, `PosePart_LerpAddColor` 0x4460E0, `Pose_SubmitPartQuad` 0x446740, `Quad_SetUvFrom256Rect` 0x449010, `Actor_GetFrameInterpolation` 0x445A50.
Interpolation: `t = elapsed/duration` (frame record +6 = u16 duration, `actor+0x68` float elapsed); if the frame has the interpolate flag (frame +0x18 != 0) and a next pose exists, each field is `lerp(cur, next, t)` using the same part slot of the next pose; if the next part's `src_w == 0` the current part is used as target; otherwise target == current pose.

| Off | Name | Units / semantics | Proof | Conf |
|---|---|---|---|---|
| 0x00 | x | pixels, +right, relative to actor anchor; lerped | `PosePart_LerpPosition`: `x = cur + (nxt-cur)*t` | proven |
| 0x04 | y | pixels, +down (data is negative above the anchor); lerped | same | proven |
| 0x08 | dest_width | quad width in pixels before scaling; **cur pose only** (not lerped) `a1[2]*scaleX` | `PosePart_LerpPosition` | proven |
| 0x0C | dest_height | same, Y | same | proven |
| 0x10 | flip_flags | bit0: mirror X, bit1: mirror Y: `src_x += src_w-1; src_w = -src_w` resp. Y; only the low byte is read | `PosePart_GetFlippedSrcRect` | proven (bits 8,16: data only) |
| 0x14 | scale_x_permille | 1000 = 1.0, `(cur+(nxt-cur)t)*0.001`, multiplied by actor scale/256 | `PosePart_LerpScale` | proven |
| 0x18 | scale_y_permille | same | same | proven |
| 0x1C | rotation_10000 | 10000 = full turn, converted `*0.036` to degrees, **shortest-arc** lerp (wraps 5000 apart) about the origin pivot | `Math_LerpAngle10000ToDeg` via `PosePart_InterpolateTransform` -> `out[40]` -> draw desc `[17]` | proven (rotation sign: engine matrix convention, not decoded) |
| 0x20 | modulate_argb_bytes | memory bytes A,R,G,B (0xFFFFFFFF in 947714 parts = no change). Result vertex diffuse = `lerp(byte) * tint(float)`, packed A<<24,R<<16,G<<8,B; tint = actor/draw-item colour (`Pose_SplitTintColor`) | `PosePart_LerpModulateColor` | proven |
| 0x24 | add_color_rgb | bytes R(+0x24) G(+0x25) B(+0x26) added to the per-channel tint-add term, clamped 255, submitted as vertex specular (additive); byte +0x27 never read (0 in data) | `PosePart_LerpAddColor` | proven |
| 0x28 | texture_index | 0..49 -> `pat.texture[i]`; `0xFFFF` marks a clip-rectangle part (the engine's clip support is a stub: `Actor_DrawPartsPose` returns without drawing) | `Pose_CollectSortedParts`, `Actor_DrawPartsPose` `4*idx+0xFF8` | proven |
| 0x2C | src_x | s16, source rect X, 1/256 of texture width (UV = v/256, textures 256 or 512 px) | `PosePart_GetFlippedSrcRect` reads words; `Quad_SetUvFrom256Rect` `*0.00390625` | proven |
| 0x2E | src_x_hi | high word, unread, 0 in data (values 0..256) | search | unread (constant) |
| 0x30 | src_y | s16, 1/256 of height | same | proven |
| 0x32 | src_y_hi | unread, 0 | | unread (constant) |
| 0x34 | src_w | s16 width 1/256; `src_w == 0` (checked as full dword) = slot unused/skipped | `Pose_CollectSortedParts` tests `+52` and `+56` dwords | proven |
| 0x36 | src_w_hi | unread, 0 | | unread (constant) |
| 0x38 | src_h | s16 height | same | proven |
| 0x3A | src_h_hi | unread, 0 | | unread (constant) |
| 0x3C | layer | u8; draw order = ascending `(layer<<8) + partIndex` (bubble sort of (index, ptr, key) triples) | `Pose_CollectSortedParts` | proven |
| 0x3D | layer_hi[3] | unread; 0 in data (max 255) | | unread (constant) |
| 0x40 | origin_x | pivot X offset from the part top-left in pixels (scale and rotation about `(x+origin_x, y+origin_y)`); lerped | `PosePart_LerpOrigin`, `PosePart_LerpPosition`: `topLeft = x + ox - ox*scale` | proven |
| 0x44 | origin_y | same | same | proven |
| 0x48..0x58 | reserved_48,4C,50,54,58 | no function that receives a part pointer reads them (all part readers listed above address offsets <= 0x44) and every reader of `pat.pose_data` is `Actor_DrawPartsPose`/`Actor_GetFrameInterpolation` + callees (searched the `+0FB0h` displacement over the whole exe: the only pose-data readers are those two, the rest are loader/free code or an unrelated struct); 0 in 998720/998720 parts of 24968 poses | unread (constant) |

Final geometry (`PoseQuad_FillDrawDesc` 0x4462F0 + `Pose_SubmitPartQuad`): quad corners `(x', y')`, `(x'+w*sx, y'+h*sy)` with
`w = dest_width * scaleX/1000 * actorScale/256`, rotated about `(x+origin)*scale`, offset by the actor anchor, then `+320, +448` (640x480 virtual screen, anchor = bottom centre) and `invW`/`z` fixed; culled if fully off screen.
Blend mode per actor draw-mode byte (`byte_48AEFC`).

### 3.4 Texture entry / pose records: field count summary

Pose offset entry (u32) 1, pose name (32 B) 1, `image_area_offset` 1, part record 23 dwords (31 named fields incl. hi halves), image-area head 7 dword slots + 3 tables + 8196-byte block, texture entry (pixels).

## 4. CG bank (area 3), `Han2Dat_LoadCgBank` 0x4038F0 + `Han2Dat_BuildCgTextures` 0x403DA0

Header read = first `0x4F30 = 20272` bytes (`RboCgFileHead`): signature (20) at 0, 8 palettes x 1024 at +0x14 (`Han2Dat_Load`'s `a3`/`paletteSet` picks `1024*a3+20`; entry 0 forced to 0 = transparent), `block_count` at +0x201C, `effect_offset[3000]` at +0x2044, `blocks_offset` at +0x4F24.
* `cg.blocks` = `[CG.off + blocks_offset, +24*block_count)` (`a4[6002]`, `RboCgBlock[]`).
* `cg.effect[i] = {dword at record+0x40 (first block), u16 at record+0x44 (count)}` for each non-(-1) `effect_offset[i]`, else `{0,0}`; the whole CG area is also read once more to resolve them.
* `Han2Dat_BuildCgTextures`: walks all effects/blocks (`Han2Dat_ForEachCgBlock`), `CgBlock_TrackMaxTextureSlot` -> `cg.texture_count = max(slot)+1`, `CgBlock_CreateTexture` creates 256x256 textures per used slot, `CgBlock_BlitToTexture` decodes the effect image into `block.src_x/src_y` of `textures[block.texture_slot]`, `block.width x block.height` texels, advancing the pixel pointer by `w*h*bytesPerPixel`.
* Draw: `Actor_DrawCgSprite` 0x4470E0: one quad per block: dest rect `(dest_x, dest_y, width, height)`, UV from `(src_x/256, src_y/256, width/256, height/256)` (`CgBlock_SetUv` 0x4457A0, with half-texel nudges in some filter modes).

`RboCgEffectRecord` (72 B + pixels): engine reads +0x20 (storage type), +0x40, +0x44; the rest is file-only (name, source size, bpp 32 in 5513/5513, 4-int bounding box, +0x46 = 0 in all).
`RboCgStorageType` (`CgEffect_InitPixelDecoder` 0x403B80, `CgPixel_Decode` 0x412270): 0 shared palette 8-bit index (index 0 transparent); 1 BGRA32 at +0x48; 2 own 1024-byte palette at +0x48, indices at +0x448; 3 solid colour (3 bytes at +0x48) + 8-bit alpha at +0x4C; 4 own palette, index plane at +0x448 then alpha plane; 5 shared palette + alpha plane at +0x148 (never in shipped data); 0xFFFFFFFF = 18 placeholder records. Counts in data: type1 3703, 2 1135, 4 604, 3 53.

## 5. Renamed (IDA) / typed

Script VM: `Script_ResolveIndexedFunction` (sub_425100), `Script_GetCurrentThread` (424790), `Script_AllocThread` (424610), `Script_ActivateThread` (424850), `Script_BindThreadContext` (4247E0), `Script_PushEntryCall` (427C90), `Script_SetArg` (4268C0), `Script_GetArg` (4268B0), `Script_RunThreads` (428EC0), `Script_SelectBank` (424CB0), `Script_LoadBankHandle` (424E60); globals `g_ScriptBanks` (0xAB2250, `RboScriptBank[64]`), `g_ScriptArgs` (0xAB6A64), `g_FrameEnterEventCategory` (0x489ED0), `g_TransitionEventCategory` (0x489AD0).
Actors: `Actor_LinkFrameEnterEvent` (41D9B0), `Actor_LinkTransitionRuleEvent` (41F7F0), `Actor_ClearFrameEnterLists` (41D990), `Actor_ClearTransitionRuleLists` (41F7C0), `Actor_ApplyFrameEnterAction` (41D640), `Actor_RunFrameEnterActions` (41D8E0), `Actor_RunChanceFrameEnterActions` (41D950), `Actor_ApplyChanceAction` (41D910), `Actor_RunInputTransitionRules` (420F10), `Actor_RunEndTransitionRules` (420DC0), `Actor_RunIdleTransitionRules` (420570), `Actor_RunConditionalJumpList` (420460), `Actor_EvalConditionalJumpRule` (420300), `Actor_RunForcedJumpRules` (41F780), `Actor_ApplyForcedJumpRule` (41F730), `Actor_RunTimedJumpRules` (420520).
Pose/quad: `Pose_CollectSortedParts` (4461D0), `PosePart_GetFlippedSrcRect` (440A90), `PosePart_InterpolateTransform` (445ED0), `PosePart_LerpOrigin` (445D40), `PosePart_LerpScale` (445DA0), `PosePart_LerpPosition` (445E10), `PosePart_LerpModulateColor` (446000), `PosePart_LerpAddColor` (4460E0), `Pose_SplitTintColor` (445F50), `PoseQuad_FillDrawDesc` (4462F0), `Pose_SubmitPartQuad` (446740), `Quad_SetUvFrom256Rect` (449010), `Quad_ComputeRhw` (449080), `CgBlock_SetUv` (4457A0), `Quad_FixWinding` (4466A0).
Loaders: `Han2Dat_LoadPatTextures` (403A70), `Han2Dat_ReleasePatTextures` (403240), `Han2Dat_FreePatBlock` (4033A0), `Han2Dat_FreeCgBank` (4033E0), `Han2Dat_ReleaseCgTextures` (403270), `Han2Dat_BuildCgTextures` (403DA0), `Han2Dat_ForEachCgBlock` (403FF0), `CgBlock_TrackMaxTextureSlot` (403EF0), `CgBlock_CreateTexture` (403F20), `CgBlock_BlitToTexture` (403C50), `CgEffect_InitPixelDecoder` (403B80), `Texture_BlitDecodedBlock` (4123C0), `CgPixel_Decode` (412270), `Texture_UploadBgra` (412970), `Han2Dat_LoadTextures` (403D50), `Han2Dat_DecryptIfFlag` (403520), `Texture_Create` (40E290).
Prototypes with the new struct types applied to: `Han2Dat_Load`, `Han2Dat_LoadPartsPatBlock`, `Han2Dat_LoadCgBank`, `Han2Dat_LoadPatTextures`, `Han2Dat_BuildCgTextures`, `Han2Dat_LoadTextures`, `Han2Dat_ComputeSectionPointers`, `Pose_CollectSortedParts`, `PosePart_GetFlippedSrcRect`, `Actor_DrawPartsPose`, `Actor_DrawCgSprite`, `Script_*`, the two Link callbacks.

## 6. Open items

* Sections 4/5: unread by rbo.exe; parameter semantics unknown; the Ex-series exes were not searched.
* Rotation direction sign and exact anchor (320/448) rely on the matrix helpers `sub_43AF10/42EFF0/42F040`, not decoded.
* Event record parameter layouts of the ~50 frame-enter/transition types (only the dispatch and the category structure were traced).
