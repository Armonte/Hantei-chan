# GOF2 (Glove on Fight 2) HAN2RBO container and 404-byte frame record

Source of truth for the types: `gof2_frame_types.h` and `gof2_container_types.h` (same folder, loaded into `GOF2.exe.i64` with `idc.parse_decls`;
sizes verified in the IDB: Gof2FrameRecord 0x194, Gof2PatternAreaHeader 0x60, Gof2PatternEntry 0xC, Gof2BoxRect 8, Gof2HanteiAnime 0x44,
Gof2PatternAreaView 0x28, Gof2Han2FileHeader 0x40, Gof2CharaDataBlock 0x3C). Offsets are checked by `tools/gof2/check_gof2_types.py`; the data invariants by
`tools/gof2/check_gof2_invariants.py` (all 68 GOF2 `.DT2`, 43 991 frames, everything below passes).

Evidence tags in the headers and tables: **T** reader traced in the exe, **I** reader traced but the meaning is inferred, **E** editor-only (nonzero in the data, no reader anywhere),
**U** unused (zero in every frame AND no reader), **V** validated by the loader.

## 1. How the engine loads a character (container)

Same three-stage flow as RBO, driven by `CharacterLoadStateMachine` 0x4477F0 (state 0, sub-states `State0_0..State0_4`):

1. `State0_0_LoadDT2File` 0x4478E0 -> `Gof2Han2_OpenPreferDt2ReadHeader32` 0x4469F0: renames the path to `.dt2`, opens it (falls back to the `.dat`), reads file bytes 0..0x1F and keeps `xorFlag` (+0x1C) in the loader object.
2. `State0_1_LoadPATandCHPFiles` 0x447990: seeks to 0x20 and reads the four `{offset,size}` area refs of the `.dt2` (a1[11..12] = pattern area) and of the `.dat` (a1[19..]); copies `xorFlag` to chara block +4.
3. `State0_2_BeginFileDataLoad` 0x447A30: `AllocateAndReadFileData(path, patternArea.offset, patternArea.size)` -> one GlobalAlloc block; `State0_3_ProcessLoadedData` 0x447AA0 takes the buffer (`Gof2Stream_TakeLoadedBuffer` 0x447240) into `chara block +8` (view.base).
4. `State0_4_NextFile` 0x447AE0: if `xorFlag` decrypts the 0x60-byte header (`Gof2_XorDecryptBuffer` 0x446400, key string aMe), then **validates** lead dwords: `[0]==0, [1]==8, [2]==0, [3]==5, [4..7]==0`, otherwise aborts (state 7). Then `Gof2PatternArea_ComputeSectionPointers` 0x446920 (`sectionSize` = header dwords 12..20, section n starts at `base+0x60+sum(previous sizes)`, a zero size gives a NULL pointer) and `Gof2PatternArea_DecryptSections0to5` 0x446860 (only sections 0..5 are decrypted when `xorFlag` is set; sections 6..8 never are. No shipped file sets the flag).

The chara record array: `Obj_BindCharaRecord` 0x4316A0 sets `Obj+640 = array + 32292*charaId + 4` (the chara data block) and `Obj+636 = that + 8` (the pattern view). `CHanteiAnime` (embedded in the 0x1264-byte `Obj` at +616, `Gof2HanteiAnime`) stores view at +0x14 and the current frame at +0x1C / +0x20.

Frame access (all 404-byte stride): `frame = view.frames + 404 * (view.patterns[pattern].firstFrameIndex + frameNo)` in `CHanteiAnime_CacheNextFrame` 0x448F70, `CHanteiAnime_CacheFrameAt` 0x449050 (the Actor_GetFrameRecord equivalent; also `CHanteiAnime_FindChainEndTarget` 0x4491B0, `Obj_SetActionAndRunScript` 0x431B90, `Obj_ScanAheadForAttackFrame` 0x4324F0, `Draw_ObjTreeLayerWithAfterimages` 0x49F9F0 = the 9 `imul 404` sites in the exe). `Obj+644/+648/+680` hold the frame pointers.

### File header (0x40) and pattern area header (0x60)

File header: identical to RBO except `subVersion = 2` (see `gof2_container_types.h`, `Gof2Han2FileHeader`). All 68 files: kind 3, version 2, sub 2, xorFlag 0.

Pattern area header (dwords): all 68 files have exactly

| Dword | Offset | Value in all 68 | Role |
|---|---|---|---|
| 0 | +0x00 | 0 | V must be 0 |
| 1 | +0x04 | 8 | V must be 8 (RBO: 3) |
| 2 | +0x08 | 0 | V must be 0 |
| 3 | +0x0C | 5 | V must be 5 (RBO: 1) |
| 4..7 | +0x10..0x1F | 0 | V must be 0 |
| 8 | +0x20 | 1 | E not validated, not read |
| 9..11 | +0x24..0x2F | 0 | E not read |
| 12..20 | +0x30..0x53 | section sizes 0..8 | T `Gof2PatternArea_ComputeSectionPointers` |
| 21..23 | +0x54..0x5F | 0 | E not read |

### Section table

| # | View ptr | Content | Record | Present in | Engine consumer |
|---|---|---|---|---|---|
| 0 | +4 | pattern table, 256 entries | 12 (`Gof2PatternEntry`: frameCount, flags, firstFrameIndex) | 68/68, always 3072 B | `CHanteiAnime_CacheNextFrame`. `flags` is 0 in all 17 408 entries and no code reads pattern+4 (E) |
| 1 | +8 | frames | 404 (`Gof2FrameRecord`) | 68/68 (max 851 228 B) | everything in section 3 |
| 2 | +12 | box rectangles `{x1,y1,x2,y2}` i16 | 8 | 29/68 (39 files have no boxes) | `CAppHantei*` box builders (`this+8*idx`) |
| 3 | +16 | attack records | **236** (RBO: 120) | same 29 files (size always 236 * n) | `CAppHanteiKougeki_BuildFromFrame` (`view+16 + 236*frame.attackRecordIdx`), `ObjScan_StoreAttackGuardInfo`, `HitJudge_*` (record bytes read: +0, +8, +32, +76, +80, +108, +110, +120, +124, +126, +128, +144, +196; not typed here) |
| 4 | +20 | unknown small table | 28-byte records inferred (sizes 28, 56, 112; 5 files) | 5/68 | **no reader of view+20 (E)** |
| 5 | +24 | unknown small table | 20-byte records inferred (sizes 20, 40; 4 files) | 4/68 | **no reader of view+24 (E)** |
| 6 | +28 | script id lists A | 20 = 5 dwords (script ids, 0 = empty), record 0 is an all-zero dummy | 68/68 | `CHanteiAnime_GetFrameScriptLists` 0x449A30 (frame +0xBC), run as ScriptVm kind 0 by `sub_43E1B0` |
| 7 | +32 | script id lists B | 20, record 0 dummy | 68/68 | same function (frame +0xC0), ScriptVm kind 1 by `sub_43E030` |
| 8 | +36 | effect-spawn records | 96 (record 0 dummy) | 68/68 | `Obj_SetActionAndRunScript` -> `Obj_SpawnFrameEffectRecord` 0x4327F0 (frame +0x190) |

Sections 2 and 3 are both absent in the same 39 files. Section 8 (96 bytes x 324 in the largest file) is new relative to RBO; RBO's 196/40-byte sections 4/5 became 28/20-byte unread tables.

## 2. Pattern table entry

`Gof2PatternEntry` (12 bytes): `frameCount` (0 = pattern absent; tested by `sub_4334C0`, `CHanteiAnime_CacheNextFrame`), `flags` (always 0, unread), `firstFrameIndex` (read by every frame accessor). `Obj_SetActionAndRunScript` also reads `patterns[10].frameCount` (state 10 last-frame test).

## 3. Frame record field map (404 bytes)

Byte count by class: **T 198, I 9, E 124, U 73** (sum 404). Per field below.

| Offset | Type | Field | Tag | Evidence |
|---|---|---|---|---|
| 0x00 | __int16 | `spriteId` | T | CG image = id-10000 when >=10000, else PAT pose index (DrawHelper_Draw 0x49EF60, CHanteiAnime_CacheNextFrame 0x448F70, CHanteiAnime_ResolveCgImage 0x448E60) |
| 0x02 | __int16 | `offsetX` | T | draw offset X (DrawHelper_Draw, DrawHelper_EmitPolygons 0x440850) |
| 0x04 | __int16 | `offsetY` | T | draw offset Y (DrawHelper_Draw, DrawHelper_EmitPolygons) |
| 0x06 | unsigned __int16 | `duration` | T | ticks before advance (Obj_ScriptTick 0x434AD0 compares anime ticks to it; DrawHelper_Draw uses it as the tween denominator) |
| 0x08 | Gof2FlipMode | `flipMode` | T | DrawHelper_Draw indexes the flip table dword_5A18E0[2*flipMode] |
| 0x09 | Gof2BlendMode | `blendMode` | T | Obj_ApplyFrameBlendAlphaZoom 0x435620 copies non-zero to obj+1504 (DrawSort_Run blend table dword_5A1AA4) |
| 0x0A | unsigned char | `alpha` | T | alpha fade target when fxFlags.USE_ALPHA_FADE (Obj_ApplyFrameBlendAlphaZoom -> obj+1072/+1076) |
| 0x0B | Gof2AniFlag | `aniFlag` | T | CHanteiAnime_StepAniFlag 0x4490A0, CHanteiAnime_CacheNextFrame 0x448F70, Obj_ScanAheadForAttackFrame 0x4324F0 |
| 0x0C | unsigned char | `jumpTarget` | T | pattern (ANI_END_TO_PATTERN) or frame to jump to (CHanteiAnime_StepAniFlag) |
| 0x0D | unsigned char | `landJumpTarget` | T | Obj_ApplyLandingFrameJump 0x430A40 (frame for aniFlag 1/2, pattern for 3/4), Obj_ScanAheadForAttackFrame |
| 0x0E | unsigned char | `drawPriorityCode` | T | Obj_ApplyFrameDrawPriority 0x432E30 switch over codes 1..49 (1..8 add a depth step, 9..18/26..35/41..49 set depth + default layer, 19..25 and 36..40 set draw-order slots; table dword_5A2398) |
| 0x0F | unsigned char | `unused_0F` | U | zero in all 43991 frames, no reader of frame+15 |
| 0x10 | unsigned __int16 | `zoom` | T | 256 = 1.0; Obj_ApplyFrameBlendAlphaZoom copies to obj+1112 when fxFlags.USE_ZOOM |
| 0x12 | unsigned char | `loopCount` | T | loaded into the anime loop counter when non-zero (Obj_SetActionAndRunScript 0x431B90, Obj_ScanAheadForAttackFrame reads the low byte) |
| 0x13 | unsigned char | `loopEndFrame` | T | frame jumped to when the loop counter is exhausted (aniFlag 5 and 8; CHanteiAnime_StepAniFlag) |
| 0x14 | unsigned char | `unused_14[4]` | U | zero in all frames, no reader |
| 0x18 | unsigned char | `interpolationMode` | T | non-zero = tween toward the next frame (CHanteiAnime_CacheNextFrame gate; DrawHelper_Draw -> sub_49E9A0 blend factor ticks/duration) |
| 0x19 | Gof2FrameFxFlags | `fxFlags` | T | Obj_ApplyFrameBlendAlphaZoom |
| 0x1A | Gof2DrawStyle | `drawStyle` | I | non-zero copied to obj+1508, DrawSort_Run indexes dword_5A1AB4[style] (0/1/2 in data) |
| 0x1B | unsigned char | `unused_1B[13]` | U | zero in all frames, no reader |
| 0x28 | Gof2MoveFlags | `moveFlags` | T | Obj_ApplyFrameMoveFlags 0x434950, Obj_ScaleVelocityOnEnterFrame 0x42EC30 |
| 0x29 | unsigned char | `unused_29[3]` | U | zero in all frames (the flags byte is never read as a dword), no reader |
| 0x2C | __int16 | `speedX` | T | added to velocity X when MOVE_ADD_X (Obj_ApplyFrameMoveFlags, flip-aware via facing) |
| 0x2E | __int16 | `speedY` | T | added to velocity Y when MOVE_ADD_Y |
| 0x30 | __int16 | `accelX` | T | acceleration X when MOVE_ADD_X |
| 0x32 | __int16 | `accelY` | T | acceleration Y when MOVE_ADD_Y |
| 0x34 | Gof2StanceClass | `stanceClass` | T | CHanteiAnime__vf0 0x448EB0 copies the dword at +0x34 to anime+44; bit 4 = airborne (sub_42ED90, HitJudge); ScriptCond_MatchStanceAndCancel 0x42F6A0 masks it |
| 0x35 | Gof2CancelPermission | `normalCancel` | T | Obj_GetNormalCancelState 0x4302E0, ScriptCond_MatchStanceAndCancel |
| 0x36 | Gof2CancelPermission | `specialCancel` | T | Obj_IsSpecialCancelAllowed 0x430120, ScriptCond_MatchStanceAndCancel |
| 0x37 | unsigned char | `unused_37` | U | zero in all frames, no reader |
| 0x38 | Gof2AttackEventFlags | `attackEventFlags` | T | CAppHanteiKougeki_BuildFromFrame 0x449250 (1,2,0x10), CAppHanteiEtc__vf7 0x449750 (4,8), CAppHanteiSousai__vf6 0x449820 (0x20,0x40) |
| 0x39 | unsigned char | `unused_39[3]` | U | zero in all frames, no reader |
| 0x3C | unsigned int | `hurtMaskEnable` | T | 0 = anime vulnerability mask forced to 0 (CHanteiAnime__vf0 0x448EB0 reads *(frame+60)) |
| 0x40 | Gof2HurtMaskFlags | `hurtMaskFlags` | T | CHanteiAnime__vf0 builds anime+48 (class mask) and anime+52/+56 (flags A/B) from it |
| 0x44 | unsigned int | `editorOnly_44` | E | =1 in 4 of 43991 frames, no reader of frame+68 |
| 0x48 | unsigned int | `hitLimitFlags` | T | copied to the hit limiter on ATKEV_BEGIN (CAppHanteiKougeki_BuildFromFrame reads *(frame+72)); zero in all frames |
| 0x4C | unsigned int | `hitLimitCount` | T | copied to the hit limiter count (same function, *(frame+76)); zero in all frames |
| 0x50 | unsigned int | `editorOnly_50` | E | 0x01000000 in 10 frames and 1 in 2, no reader of frame+80 |
| 0x54 | unsigned int | `unused_54` | U | zero in all frames, no reader |
| 0x58 | unsigned int | `editorOnly_58` | E | 0x01000000 in 8 frames, no reader of frame+88 |
| 0x5C | Gof2MotionFlags | `motionFlags` | T | Obj_ApplyFrameMoveFlags (&2 -> Obj_ResetMoveTracks 0x430E90), Obj_ApplyFrameBrake 0x435280 (&4) |
| 0x5D | unsigned char | `unused_5D` | U | zero in all frames, no reader |
| 0x5E | __int16 | `brakePercent` | T | Obj_ApplyFrameBrake: velocity * brakePercent / -1000 (50 in 43831 frames) |
| 0x60 | unsigned int | `editorOnly_60` | E | 0x01000000 in 6 frames, no reader of frame+96 |
| 0x64 | Gof2FacingCommand | `facingCommand` | T | Obj_ApplyFrameFacingCommand 0x43CF70 switch (1..7), Obj_ApplyFacingKind 0x43CE50 |
| 0x68 | unsigned int | `editorOnly_68` | E | 0x01000000 in 10 frames, no reader of frame+104 |
| 0x6C | unsigned int | `unused_6C` | U | zero in all frames, no reader |
| 0x70 | unsigned int | `editorOnly_70` | E | 0x01000000 in 6 frames, no reader of frame+112 |
| 0x74 | unsigned int | `unused_74` | U | zero in all frames, no reader |
| 0x78 | unsigned int | `editorOnly_78` | E | 0x01000000 in 10 frames, no reader of frame+120 |
| 0x7C | unsigned int | `unused_7C` | U | zero in all frames, no reader |
| 0x80 | Gof2HitSuppressFlags | `hitSuppressFlags` | T | Obj_ResetHanteiBoxObjects 0x4356A0 tests 0x10000000 (24 frames), keeps the box objects alive |
| 0x84 | unsigned int | `unused_84` | U | zero in all frames, no reader |
| 0x88 | unsigned int | `editorOnly_88` | E | 0x01000000 in 6 frames, no reader of frame+136 |
| 0x8C | unsigned int | `unused_8C` | U | zero in all frames, no reader |
| 0x90 | unsigned int | `editorOnly_90` | E | 0x01000000 in 10 frames, no reader of frame+144 |
| 0x94 | unsigned int | `unused_94` | U | zero in all frames, no reader |
| 0x98 | unsigned int | `editorOnly_98` | E | 0x01000000 in 4 frames, no reader of frame+152 |
| 0x9C | unsigned int | `unused_9C` | U | zero in all frames, no reader |
| 0xA0 | unsigned int | `editorOnly_A0` | E | 0x01000000 in 8 frames, no reader of frame+160 |
| 0xA4 | unsigned int | `unused_A4` | U | zero in all frames, no reader |
| 0xA8 | unsigned int | `editorOnly_A8` | E | 0x01000000 in 10 frames, no reader of frame+168 |
| 0xAC | unsigned int | `unused_AC` | U | zero in all frames, no reader |
| 0xB0 | unsigned int | `editorOnly_B0` | E | 0x01000000 in 6 frames, no reader of frame+176 |
| 0xB4 | unsigned int | `unused_B4` | U | zero in all frames, no reader |
| 0xB8 | Gof2ScriptOrderFlags | `scriptOrderFlags` | T | CHanteiAnime_GetFrameScriptLists 0x449A30 returns *(frame+184)&1 |
| 0xB9 | unsigned char | `unused_B9[3]` | U | zero in all frames, no reader |
| 0xBC | unsigned int | `scriptListIndexA` | T | index into section 6 (20-byte script id lists, run as ScriptVm kind 0 by sub_43E1B0), 0 = none (CHanteiAnime_GetFrameScriptLists) |
| 0xC0 | unsigned int | `scriptListIndexB` | T | index into section 7 (run as ScriptVm kind 1 by sub_43E030), 0 = none |
| 0xC4 | unsigned int | `hasAttackRedundant` | E | 1 iff attackBoxCount>0 (1895 frames); no reader of frame+196, the engine tests attackBoxCount |
| 0xC8 | int | `attackRecordIdx` | T | index into section 3 (236-byte attack records), -1 none (CAppHanteiKougeki_BuildFromFrame, ObjScan_StoreAttackGuardInfo 0x432480) |
| 0xCC | int | `attackScratchFill[7]` | E | 0xCDCDCDCD (MSVC debug fill) in the 1895 attack frames, -1 in all others; no reader of frame+204..+231 |
| 0xE8 | unsigned int | `legacyBoxGroupACount` | E | 0/1 (1 in 10 frames); no reader of frame+232, its slots hold stale values outside the box table numbering |
| 0xEC | int | `legacyBoxGroupAIdx[3]` | E | -1 except 10 frames (0..3); no reader |
| 0xF8 | unsigned int | `legacyBoxGroupBCount` | E | 0/1 (1 in 6 frames); no reader of frame+248 |
| 0xFC | int | `legacyBoxGroupBIdx[4]` | E | -1 except +0x108 in 6 frames (0/1); no reader |
| 0x10C | unsigned int | `kasanariBoxEnable` | T | CAppHanteiKasanari__vf5 0x449620 gate (== number of non -1 slots) |
| 0x110 | int | `kasanariBoxIdx` | T | section 2 box index (single slot), -1 = empty |
| 0x114 | unsigned int | `hurtBoxCount` | T | CAppHanteiYarare__vf6 0x4494F0 gate |
| 0x118 | int | `hurtBoxIdx[6]` | T | section 2 box indices (6 slots read by CAppHanteiYarare__vf6), -1 = empty |
| 0x130 | unsigned int | `editorOnly_130` | E | 6 in 42369 frames, 0 in 1622; frame+304 is the Yarare parameter block (lea in CAppHanteiYarare__vf6) but nothing reads its first dword |
| 0x134 | Gof2GuardMode | `guardMode` | I | HitJudge_DefenderGuardsAttack 0x496930 switches on *(yarare+4) = frame+308 |
| 0x138 | Gof2ArmorMode | `armorMode` | I | HitJudge_DefenderArmorAbsorbs 0x497180 switches on *(yarare+4+4) = frame+312 |
| 0x13C | unsigned int | `etcBoxCount` | T | CAppHanteiEtc__vf7 0x449750 gate |
| 0x140 | int | `etcBoxIdx[3]` | T | 3 slots read; only the first is ever used in the data |
| 0x14C | unsigned int | `sousaiBoxCount` | T | CAppHanteiSousai__vf6 0x449820 gate |
| 0x150 | int | `sousaiBoxIdx[3]` | T | 3 slots read, only the first is ever used |
| 0x15C | unsigned int | `tobiBoxCount` | T | CAppHanteiTobi__vf6 0x449910 gate |
| 0x160 | int | `tobiBoxIdx[3]` | T | 3 slots read (RBO had 4) |
| 0x16C | unsigned int | `attackBoxCount` | T | CAppHanteiKougeki_BuildFromFrame gate |
| 0x170 | int | `attackBoxIdx[8]` | T | 8 slots read; slots 0..4 are used in the data |
| 0x190 | unsigned int | `effectRecordIdx` | T | 1-based index into section 8 (96-byte records), 0 = none (Obj_SetActionAndRunScript -> Obj_SpawnFrameEffectRecord 0x4327F0) |

### Engine reads vs fill

* `+0xCC..+0xE7` (28 bytes, `attackScratchFill[7]`): `0xCDCDCDCD` (uninitialised MSVC debug heap fill) in exactly the 1895 frames that have an attack box, `-1` in the other 42 096. No instruction reads frame+204..+231 through any frame pointer. The editor writes this region uninitialised; a writer should reproduce: attack frame -> `0xCDCDCDCD`, other frames -> `-1`.
* `+0xE8..+0x10B` (two legacy box groups): never read; data: count 0/1 in 10 / 6 frames, slot values 0..3 that do **not** take part in the box-table numbering (including them leaves dangling indices in 3 files, see invariants). A writer must preserve them verbatim.
* `+0x50..+0xB7` stride-8 words `editorOnly_xx`: 0x01000000 (or 1) in 4..12 frames each, never read. `+0x80` is the one word in this block the engine reads (bit 0x10000000).

### How "no reader" was established

1. Every instruction in .text with a memory operand displacement 644, 648 or 680 (the three places the frame pointer lives in `Obj`: anime+0x1C/+0x20/+0x40) was found (50 functions, ~20 relevant) and a register/stack-slot forward taint run over each (`mov r,[x+644]`, `imul r,404`, `lea`, `mov`, `add`). Every displacement dereferenced through a tainted base is the read set.
2. The same for functions that receive the anime pointer (`[anime+0x1C]`, `[anime+0x20]`, `[anime+0x40]`): `CHanteiAnime__vf0`, `CAppHanteiKougeki_BuildFromFrame`, `CAppHanteiYarare__vf6`, `CAppHanteiKasanari__vf5`, `CAppHanteiEtc__vf7`, `CAppHanteiSousai__vf6`, `CAppHanteiTobi__vf6`, `CHanteiAnime_GetFrameScriptLists`, `CHanteiAnime_StepAniFlag` and friends, `DrawHelper_EmitPolygons`.
3. Hex-Rays text of every function in 0x400000..0x560000 searched for the frame-pointer idioms (`x[161]`, `x[162]`, `*(x+644|648|680)`, `*(a+28)`, `*(a+32)` and the variables assigned from them).
4. All nine `imul 404` sites and the `lea ...,[frame+N]` sub-block pointers (frame+40 and frame+304) were followed by hand (`Obj_ApplyFrameMoveFlags`, `CAppHanteiYarare__vf6` -> `HitJudge_DefenderGuardsAttack`/`HitJudge_DefenderArmorAbsorbs`).
5. Sections via view: `Obj+636` consumers and `[view+4..+36]` readers: view+20 and view+24 have none.

A displacement that appears in none of these is reported as unread. The scan cannot see a read that goes through a pointer the taint never reaches (an index computed from script data into the frame would show as an unusual displacement list, none was found). Fields whose every byte is also zero in all 43 991 frames are `unused_`; the others are named `editorOnly_` (nonzero but unread).

### Enum notes (values named by behaviour)

* `Gof2AniFlag` (`CHanteiAnime_StepAniFlag`, `ScanAhead`): 0 end of pattern -> switch to pattern `jumpTarget` (result 2); 1,3 next frame; 2,4 jump to frame `jumpTarget`; 5 counted loop (counter>0: decrement and jump to `jumpTarget`, else go to `loopEndFrame`); 6 stay on the frame and report end (result 3); 7 next while callback()==1 else frame `jumpTarget` (result 4); 8 next while counter!=0 else `loopEndFrame`; 9 callback variant of 7 (result 5). 3/4 differ from 1/2 only in `landJumpTarget` meaning (pattern instead of frame; `Obj_ApplyLandingFrameJump` 0x430A40 writes it to obj+700 / obj+704). Data: 0,1,2,5,6,7,8 occur (9 and 3,4 never).
* `loopCount` is loaded into the loop counter only when non-zero (low byte of the word at +0x12); aniFlag 5 frames in the data always have `loopCount == 0, loopEndFrame != 0`, i.e. the counter was set by an earlier frame.
* `Gof2HurtMaskFlags` -> `CHanteiAnime__vf0` 0x448EB0: `hurtMaskEnable == 0` forces the vulnerability mask to 0, otherwise mask = 127 with bit n cleared for each `G2HMF_NO_CLASS_n`; `0x200` sets bit 9, `0x100` stops the default bit 8; `0x1000000`/`0x2000000` become `anime.flagA/flagB` (consumers `sub_4301C0` family, `Obj_SetActionAndRunScript`). Data uses only classes 0..4 and 0x1000000.
* `Gof2AttackEventFlags` (`+0x38`): Kougeki build (bit 1 = begin, 2 = end, 0x10 = keep hit memory), Etc (4 begin / 8 end), Sousai (0x20 begin / 0x40 end). RBO's 4/8 pair was labelled kasanari; in GOF2 they belong to the Etc box class.
* `Gof2FacingCommand` (`+0x64`): `Obj_ApplyFrameFacingCommand` maps 1..7 to `Obj_ApplyFacingKind` kinds {3,4,2,0,1,5,6}: toward / away from the target object (`sub_4A2E10`), flip current facing, force direction 0 / 1, face toward / away from the held direction. Sets `obj+816` latch (a1[204]) and the pending facing (a1[177]). Data: command 1 in 197 frames, 3 in 54, 2 in 4, 4 in 2 (5..7 never).
* `Gof2GuardMode` (`+0x134`) and `Gof2ArmorMode` (`+0x138`): read as `*(yarare.params+4)` and `*(yarare.params+8)` by `HitJudge_DefenderGuardsAttack` / `HitJudge_DefenderArmorAbsorbs`, both compare bits of attack record byte +124 (and +126 bit 0). The names are by behaviour of the compare, the gameplay meaning (guard / armor) is inferred (tag I).

## 4. Box group layout (what the engine reads)

Six box classes, each built by `CAppHantei*` from `{count, slots}` in the frame; slot value = section-2 rectangle index, -1 = empty:

| Class | Frame count field | Slots read | Build function |
|---|---|---|---|
| CAppHanteiKasanari | +0x10C (flag) | +0x110 (1) | `CAppHanteiKasanari__vf5` 0x449620 |
| CAppHanteiYarare (hurt) | +0x114 | +0x118..+0x12C (6) | `CAppHanteiYarare__vf6` 0x4494F0; also keeps `&frame.+0x130` as its parameter block |
| CAppHanteiEtc | +0x13C | +0x140..+0x148 (3) | `CAppHanteiEtc__vf7` 0x449750 |
| CAppHanteiSousai | +0x14C | +0x150..+0x158 (3) | `CAppHanteiSousai__vf6` 0x449820 |
| CAppHanteiTobi | +0x15C | +0x160..+0x168 (3) | `CAppHanteiTobi__vf6` 0x449910 |
| CAppHanteiKougeki (attack) | +0x16C | +0x170..+0x18C (8) | `CAppHanteiKougeki_BuildFromFrame` 0x449250 |

### Complete ascending list of box index slots (24 slots)

`0x110 | 0x118 0x11C 0x120 0x124 0x128 0x12C | 0x140 0x144 0x148 | 0x150 0x154 0x158 | 0x160 0x164 0x168 | 0x170 0x174 0x178 0x17C 0x180 0x184 0x188 0x18C`

Slots that are ever not -1 in the data: 0x110, 0x118..0x12C, 0x140, 0x150, 0x160, 0x164, 0x170..0x180 (0x144, 0x148, 0x154, 0x158, 0x168, 0x184..0x18C are -1 in all 43 991 frames but the engine reads them).

## 5. Invariants relied on by a writer (all 68 files, 43 991 frames)

| Invariant | Result |
|---|---|
| Lead dwords exactly (0,8,0,5,0,0,0,0,1,0,0,0); tail dwords (0,0,0); kind 3, sub 2 | 68/68 |
| Section 1 size is a multiple of 404; section 2 multiple of 8; sections 2 and 3 both empty or both present | 68/68 |
| Frames packed in pattern order: entries with frameCount>0 in index order have `firstFrameIndex` = running sum, sum of counts = frame count | 68/68 |
| Pattern `flags` all 0 | 17 408/17 408 entries |
| Box table numbered in first-use order walking frames in order and the 24 slots above in ascending frame offset; box count in the table = number of distinct indices used (no tail, no dangling indices) | 68/68 |
| Each box group count equals the number of non -1 slots (Kasanari flag, hurt, Etc, Sousai, Tobi, attack) | 43 991/43 991 per group |
| `hasAttackRedundant` (+0xC4) == (`attackBoxCount` > 0) == (`attackRecordIdx` >= 0) | 68/68 |
| `attackRecordIdx` numbering: 0,1,2,... in frame order, no sharing, section 3 size == 236 * count | 68/68 |
| Script list A (+0xBC) and B (+0xC0): 1-based first-use numbering, 0 = none, record 0 is an all-zero dummy, section size == 20 * (max+1) | 68/68 |
| Effect record index (+0x190): 1-based first-use numbering, record 0 dummy, section 8 size == 96 * (max+1) | 68/68 |

Adding the stale legacy slots (+0xEC / +0x108) to the slot set leaves 3 files with indices that point past the (empty) box table, and adding the `0xCDCDCDCD` region as well breaks the numbering in 29 files; that is how they were excluded from the box slots.

## 6. Differences from the RBO 300-byte frame

* Offsets +0x00..+0x5F: same fields at the same offsets (sprite, offsets, duration, flip, blend, alpha, ani flag, jump targets, priority, zoom, loop, interpolation, fx flags, move flags, speeds, accel, stance, cancels, attack events, hurt mask, hit limiter, motion flags). Enum members are the same except attackEvent bits 4/8 (Etc in GOF2) and the new 0x20/0x40 (Sousai).
* New or re-purposed: `+0x1A drawStyle` (RBO `unused_1A`), `+0x5E brakePercent` (RBO `unused_5D`), `+0x64 facingCommand`, `+0x80 hitSuppressFlags`, the stride-8 editor words `+0x50..+0xB0`, `editorOnly_44`.
* +0xB8 scriptOrderFlags, +0xBC/+0xC0 script list indices, +0xC4 hasAttack, +0xC8 attackRecordIdx: unchanged. In GOF2 `hasAttack` is redundant (not read).
* Box block moved and grew. RBO: sousai +0xD0 (3 slots), tobi +0xE0 (4), effect box +0xF4/+0xF8, hurt +0xFC (3 slots), kasanari enable +0x10C with 4 slots, attack +0x120 (2 slots). GOF2: legacy groups +0xE8/+0xF8 (unread), kasanari flag +0x10C with **1** slot, hurt +0x114 with **6** slots, hurt parameter block +0x130..0x138 (new), Etc +0x13C (new class, 3), sousai +0x14C (3), tobi +0x15C (**3**, RBO 4), attack +0x16C (**8**, RBO 2), effect record index +0x190 (new). The RBO "effect box" (hit-spark rectangle) group does not exist.
* RBO attack records 120 bytes -> GOF2 236 bytes; RBO sections 4/5 196/40 bytes -> GOF2 28/20 byte tables; GOF2 adds section 8 (96-byte effect-spawn records); header 0x58 -> 0x60 (one extra lead dword +0x20 and one extra section size).
* In RBO the frame region +0x60..+0xB7 was all zero and unread; in GOF2 it carries the editor words and two engine-read dwords (+0x64, +0x80).

## 7. Functions renamed in GOF2.exe.i64

`Gof2_XorDecryptBuffer` 0x446400, `Gof2PatternArea_DecryptSections0to5` 0x446860, `Gof2PatternArea_ComputeSectionPointers` 0x446920, `Gof2Han2_ReadAreaRefs32` 0x4469B0, `Gof2Han2_OpenPreferDt2ReadHeader32` 0x4469F0, `Gof2Stream_TakeLoadedBuffer` 0x447240, `CharacterLoad_BeginPartsPatBlock` 0x447B70,
`CHanteiAnime_CacheNextFrame` 0x448F70, `CHanteiAnime_CacheFrameAt` 0x449050, `CHanteiAnime_StepAniFlag` 0x4490A0, `CHanteiAnime_FindChainEndTarget` 0x4491B0, `CHanteiAnime_ResolveCgImage` 0x448E60, `CHanteiAnime_GetFrameScriptLists` 0x449A30, `CAppHanteiKougeki_BuildFromFrame` 0x449250,
`ObjScan_IsBlockableAttackFrame` 0x432420, `ObjScan_StoreAttackGuardInfo` 0x432480, `Obj_ScanAheadForAttackFrame` 0x4324F0, `Obj_ApplyFrameDrawPriority` 0x432E30, `Obj_ApplyFrameBlendAlphaZoom` 0x435620, `Obj_ApplyFrameBrake` 0x435280, `Obj_ApplyFrameMoveFlags` 0x434950, `Obj_ScaleVelocityOnEnterFrame` 0x42EC30, `Obj_ApplyFrameFacingCommand` 0x43CF70, `Obj_ApplyFacingKind` 0x43CE50, `Obj_ResetHanteiBoxObjects` 0x4356A0, `Obj_ResetMoveTracks` 0x430E90, `Obj_SpawnFrameEffectRecord` 0x4327F0, `Obj_ApplyLandingFrameJump` 0x430A40, `Obj_GetNormalCancelState` 0x4302E0, `Obj_IsSpecialCancelAllowed` 0x430120, `Obj_GetNormalCancelStateScriptGated` 0x4301C0, `ScriptCond_MatchStanceAndCancel` 0x42F6A0, `HitJudge_DefenderGuardsAttack` 0x496930, `HitJudge_DefenderArmorAbsorbs` 0x497180.
Prototypes with the new types are applied to the loader pair (`Gof2PatternArea_ComputeSectionPointers`, `Gof2PatternArea_DecryptSections0to5`), `CHanteiAnime__vf0`, the five `CAppHantei*` box builders, and the `CHanteiAnime_*` frame accessors, so the decompile reads `anime->frame->attackBoxIdx[...]`, `anime->view->boxes[...]` etc.
