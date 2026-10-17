// GOF2 (Glove on Fight 2) pattern-area frame types, IDA-parsable (idc.parse_decls). Single source of truth for the GOF2 .DT2 frame record (404 bytes).
// Evidence for each field: docs/formats/ida/gof2_frame.md. Evidence prefix: T = reader traced in GOF2.exe.i64, I = reader traced but meaning inferred from data/shape, E = editor-only (nonzero in data, no reader anywhere in the exe), U = unused (zero in every frame AND no reader found).
// Flag enums are made bitmask with the IDA enum bitfield flag (see gof2_frame.md). Enum member names are prefixed G2 so they never collide with the RBO enums (rbo_frame_types.h) when both files are loaded in one DB.
enum Gof2AniFlag : unsigned char { G2ANI_END_TO_PATTERN=0, G2ANI_NEXT=1, G2ANI_JUMP_TO_FRAME=2, G2ANI_NEXT_LAND_TO_PATTERN=3, G2ANI_JUMP_TO_FRAME_LAND_TO_PATTERN=4, G2ANI_LOOP_COUNTED=5, G2ANI_HOLD_FRAME_REPORT_END=6, G2ANI_NEXT_WHILE_CALLBACK=7, G2ANI_NEXT_WHILE_COUNTER=8, G2ANI_NEXT_WHILE_CALLBACK_B=9 };
enum Gof2FlipMode : unsigned char { G2FLIP_NONE=0, G2FLIP_H=1, G2FLIP_V=2, G2ROT_90=3, G2ROT_180=4, G2ROT_270=5, G2FLIP_H_ROT_90=6, G2FLIP_H_ROT_270=7 };
enum Gof2BlendMode : unsigned char { G2BLEND_KEEP_PREVIOUS=0, G2BLEND_ALPHA=1, G2BLEND_ADDITIVE=2, G2BLEND_SUBTRACTIVE=3 };
enum Gof2DrawStyle : unsigned char { G2DRAWSTYLE_DEFAULT=0, G2DRAWSTYLE_1=1, G2DRAWSTYLE_2=2 };
enum Gof2FrameFxFlags : unsigned char { G2FX_USE_ZOOM=1, G2FX_USE_ALPHA_FADE=2 };
enum Gof2MoveFlags : unsigned char { G2MOVE_CLEAR_VEL_X=1, G2MOVE_CLEAR_VEL_Y=2, G2MOVE_ADD_X=4, G2MOVE_ADD_Y=8 };
enum Gof2StanceClass : unsigned char { G2STANCE_NONE=0, G2STANCE_GROUND=1, G2STANCE_CROUCH=2, G2STANCE_AIR=4 };
enum Gof2CancelPermission : unsigned char { G2CANCEL_NONE=0, G2CANCEL_ON_INPUT_MATCH=1, G2CANCEL_ALWAYS=2 };
enum Gof2AttackEventFlags : unsigned char { G2ATKEV_BEGIN=1, G2ATKEV_END=2, G2ATKEV_ETC_BEGIN=4, G2ATKEV_ETC_END=8, G2ATKEV_KEEP_HIT_MEMORY=0x10, G2ATKEV_SOUSAI_BEGIN=0x20, G2ATKEV_SOUSAI_END=0x40 };
enum Gof2HurtMaskFlags : unsigned int { G2HMF_KEEP_NO_BIT8=0x100, G2HMF_SET_BIT9=0x200, G2HMF_NO_CLASS_0=0x10000, G2HMF_NO_CLASS_1=0x20000, G2HMF_NO_CLASS_2=0x40000, G2HMF_NO_CLASS_3=0x80000, G2HMF_NO_CLASS_4=0x100000, G2HMF_NO_CLASS_5=0x400000, G2HMF_NO_CLASS_6=0x800000, G2HMF_ANIME_FLAG_A=0x1000000, G2HMF_ANIME_FLAG_B=0x2000000 };
enum Gof2MotionFlags : unsigned char { G2MOTF_UNREAD_BIT0=1, G2MOTF_RESET_MOVE_TRACKS=2, G2MOTF_BRAKE_ON_ENTER=4 };
enum Gof2FacingCommand : unsigned int { G2FACE_NONE=0, G2FACE_TOWARD_TARGET=1, G2FACE_AWAY_FROM_TARGET=2, G2FACE_FLIP_CURRENT=3, G2FACE_SET_DIR0=4, G2FACE_SET_DIR1=5, G2FACE_TOWARD_INPUT_DIR=6, G2FACE_AWAY_FROM_INPUT_DIR=7 };
enum Gof2HitSuppressFlags : unsigned int { G2HITSUP_KEEP_BOX_OBJECTS=0x10000000 };
enum Gof2ScriptOrderFlags : unsigned char { G2SCRIPTORD_LIST_A_FIRST=1 };
enum Gof2GuardMode : unsigned int { G2GUARD_NONE=0, G2GUARD_ATTACK_TYPE_BIT0=1, G2GUARD_ATTACK_TYPE_BIT1=2, G2GUARD_ANY_BOX_ATTACK=3, G2GUARD_IF_ATTACK_QUERY=4 };
enum Gof2ArmorMode : unsigned int { G2ARMOR_NONE=0, G2ARMOR_ATTACK_TYPE_BIT0_OR_BIT2=1, G2ARMOR_ATTACK_TYPE_BIT1=2, G2ARMOR_ANY=3 };
struct Gof2FrameRecord {
 __int16 spriteId;                     // +0x00 T: CG image = id-10000 when >=10000, else PAT pose index (DrawHelper_Draw 0x49EF60, CHanteiAnime_CacheNextFrame 0x448F70, CHanteiAnime_ResolveCgImage 0x448E60)
 __int16 offsetX;                      // +0x02 T: draw offset X (DrawHelper_Draw, DrawHelper_EmitPolygons 0x440850)
 __int16 offsetY;                      // +0x04 T: draw offset Y (DrawHelper_Draw, DrawHelper_EmitPolygons)
 unsigned __int16 duration;            // +0x06 T: ticks before advance (Obj_ScriptTick 0x434AD0 compares anime ticks to it; DrawHelper_Draw uses it as the tween denominator)
 Gof2FlipMode flipMode;                // +0x08 T: DrawHelper_Draw indexes the flip table dword_5A18E0[2*flipMode]
 Gof2BlendMode blendMode;              // +0x09 T: Obj_ApplyFrameBlendAlphaZoom 0x435620 copies non-zero to obj+1504 (DrawSort_Run blend table dword_5A1AA4)
 unsigned char alpha;                  // +0x0A T: alpha fade target when fxFlags.USE_ALPHA_FADE (Obj_ApplyFrameBlendAlphaZoom -> obj+1072/+1076)
 Gof2AniFlag aniFlag;                  // +0x0B T: CHanteiAnime_StepAniFlag 0x4490A0, CHanteiAnime_CacheNextFrame 0x448F70, Obj_ScanAheadForAttackFrame 0x4324F0
 unsigned char jumpTarget;             // +0x0C T: pattern (ANI_END_TO_PATTERN) or frame to jump to (CHanteiAnime_StepAniFlag)
 unsigned char landJumpTarget;         // +0x0D T: Obj_ApplyLandingFrameJump 0x430A40 (frame for aniFlag 1/2, pattern for 3/4), Obj_ScanAheadForAttackFrame
 unsigned char drawPriorityCode;       // +0x0E T: Obj_ApplyFrameDrawPriority 0x432E30 switch over codes 1..49 (1..8 add a depth step, 9..18/26..35/41..49 set depth + default layer, 19..25 and 36..40 set draw-order slots; table dword_5A2398)
 unsigned char unused_0F;              // +0x0F U: zero in all 43991 frames, no reader of frame+15
 unsigned __int16 zoom;                // +0x10 T: 256 = 1.0; Obj_ApplyFrameBlendAlphaZoom copies to obj+1112 when fxFlags.USE_ZOOM
 unsigned char loopCount;              // +0x12 T: loaded into the anime loop counter when non-zero (Obj_SetActionAndRunScript 0x431B90, Obj_ScanAheadForAttackFrame reads the low byte)
 unsigned char loopEndFrame;           // +0x13 T: frame jumped to when the loop counter is exhausted (aniFlag 5 and 8; CHanteiAnime_StepAniFlag)
 unsigned char unused_14[4];           // +0x14 U: zero in all frames, no reader
 unsigned char interpolationMode;      // +0x18 T: non-zero = tween toward the next frame (CHanteiAnime_CacheNextFrame gate; DrawHelper_Draw -> sub_49E9A0 blend factor ticks/duration)
 Gof2FrameFxFlags fxFlags;             // +0x19 T: Obj_ApplyFrameBlendAlphaZoom
 Gof2DrawStyle drawStyle;              // +0x1A I: non-zero copied to obj+1508, DrawSort_Run indexes dword_5A1AB4[style] (0/1/2 in data)
 unsigned char unused_1B[13];          // +0x1B..0x27 U: zero in all frames, no reader
 Gof2MoveFlags moveFlags;              // +0x28 T: Obj_ApplyFrameMoveFlags 0x434950, Obj_ScaleVelocityOnEnterFrame 0x42EC30
 unsigned char unused_29[3];           // +0x29 U: zero in all frames (the flags byte is never read as a dword), no reader
 __int16 speedX;                       // +0x2C T: added to velocity X when MOVE_ADD_X (Obj_ApplyFrameMoveFlags, flip-aware via facing)
 __int16 speedY;                       // +0x2E T: added to velocity Y when MOVE_ADD_Y
 __int16 accelX;                       // +0x30 T: acceleration X when MOVE_ADD_X
 __int16 accelY;                       // +0x32 T: acceleration Y when MOVE_ADD_Y
 Gof2StanceClass stanceClass;          // +0x34 T: CHanteiAnime__vf0 0x448EB0 copies the dword at +0x34 to anime+44; bit 4 = airborne (sub_42ED90, HitJudge); ScriptCond_MatchStanceAndCancel 0x42F6A0 masks it
 Gof2CancelPermission normalCancel;    // +0x35 T: Obj_GetNormalCancelState 0x4302E0, ScriptCond_MatchStanceAndCancel
 Gof2CancelPermission specialCancel;   // +0x36 T: Obj_IsSpecialCancelAllowed 0x430120, ScriptCond_MatchStanceAndCancel
 unsigned char unused_37;              // +0x37 U: zero in all frames, no reader
 Gof2AttackEventFlags attackEventFlags;// +0x38 T: CAppHanteiKougeki_BuildFromFrame 0x449250 (1,2,0x10), CAppHanteiEtc__vf7 0x449750 (4,8), CAppHanteiSousai__vf6 0x449820 (0x20,0x40)
 unsigned char unused_39[3];           // +0x39 U: zero in all frames, no reader
 unsigned int  hurtMaskEnable;         // +0x3C T: 0 = anime vulnerability mask forced to 0 (CHanteiAnime__vf0 0x448EB0 reads *(frame+60))
 Gof2HurtMaskFlags hurtMaskFlags;      // +0x40 T: CHanteiAnime__vf0 builds anime+48 (class mask) and anime+52/+56 (flags A/B) from it
 unsigned int  editorOnly_44;          // +0x44 E: =1 in 4 of 43991 frames, no reader of frame+68
 unsigned int  hitLimitFlags;          // +0x48 T: copied to the hit limiter on ATKEV_BEGIN (CAppHanteiKougeki_BuildFromFrame reads *(frame+72)); zero in all frames
 unsigned int  hitLimitCount;          // +0x4C T: copied to the hit limiter count (same function, *(frame+76)); zero in all frames
 unsigned int  editorOnly_50;          // +0x50 E: 0x01000000 in 10 frames and 1 in 2, no reader of frame+80
 unsigned int  unused_54;              // +0x54 U: zero in all frames, no reader
 unsigned int  editorOnly_58;          // +0x58 E: 0x01000000 in 8 frames, no reader of frame+88
 Gof2MotionFlags motionFlags;          // +0x5C T: Obj_ApplyFrameMoveFlags (&2 -> Obj_ResetMoveTracks 0x430E90), Obj_ApplyFrameBrake 0x435280 (&4)
 unsigned char unused_5D;              // +0x5D U: zero in all frames, no reader
 __int16 brakePercent;                 // +0x5E T: Obj_ApplyFrameBrake: velocity * brakePercent / -1000 (50 in 43831 frames)
 unsigned int  editorOnly_60;          // +0x60 E: 0x01000000 in 6 frames, no reader of frame+96
 Gof2FacingCommand facingCommand;      // +0x64 T: Obj_ApplyFrameFacingCommand 0x43CF70 switch (1..7), Obj_ApplyFacingKind 0x43CE50
 unsigned int  editorOnly_68;          // +0x68 E: 0x01000000 in 10 frames, no reader of frame+104
 unsigned int  unused_6C;              // +0x6C U: zero in all frames, no reader
 unsigned int  editorOnly_70;          // +0x70 E: 0x01000000 in 6 frames, no reader of frame+112
 unsigned int  unused_74;              // +0x74 U: zero in all frames, no reader
 unsigned int  editorOnly_78;          // +0x78 E: 0x01000000 in 10 frames, no reader of frame+120
 unsigned int  unused_7C;              // +0x7C U: zero in all frames, no reader
 Gof2HitSuppressFlags hitSuppressFlags;// +0x80 T: Obj_ResetHanteiBoxObjects 0x4356A0 tests 0x10000000 (24 frames), keeps the box objects alive
 unsigned int  unused_84;              // +0x84 U: zero in all frames, no reader
 unsigned int  editorOnly_88;          // +0x88 E: 0x01000000 in 6 frames, no reader of frame+136
 unsigned int  unused_8C;              // +0x8C U: zero in all frames, no reader
 unsigned int  editorOnly_90;          // +0x90 E: 0x01000000 in 10 frames, no reader of frame+144
 unsigned int  unused_94;              // +0x94 U: zero in all frames, no reader
 unsigned int  editorOnly_98;          // +0x98 E: 0x01000000 in 4 frames, no reader of frame+152
 unsigned int  unused_9C;              // +0x9C U: zero in all frames, no reader
 unsigned int  editorOnly_A0;          // +0xA0 E: 0x01000000 in 8 frames, no reader of frame+160
 unsigned int  unused_A4;              // +0xA4 U: zero in all frames, no reader
 unsigned int  editorOnly_A8;          // +0xA8 E: 0x01000000 in 10 frames, no reader of frame+168
 unsigned int  unused_AC;              // +0xAC U: zero in all frames, no reader
 unsigned int  editorOnly_B0;          // +0xB0 E: 0x01000000 in 6 frames, no reader of frame+176
 unsigned int  unused_B4;              // +0xB4 U: zero in all frames, no reader
 Gof2ScriptOrderFlags scriptOrderFlags;// +0xB8 T: CHanteiAnime_GetFrameScriptLists 0x449A30 returns *(frame+184)&1
 unsigned char unused_B9[3];           // +0xB9 U: zero in all frames, no reader
 unsigned int  scriptListIndexA;       // +0xBC T: index into section 6 (20-byte script id lists, run as ScriptVm kind 0 by sub_43E1B0), 0 = none (CHanteiAnime_GetFrameScriptLists)
 unsigned int  scriptListIndexB;       // +0xC0 T: index into section 7 (run as ScriptVm kind 1 by sub_43E030), 0 = none
 unsigned int  hasAttackRedundant;     // +0xC4 E: 1 iff attackBoxCount>0 (1895 frames); no reader of frame+196, the engine tests attackBoxCount
 int           attackRecordIdx;        // +0xC8 T: index into section 3 (236-byte attack records), -1 none (CAppHanteiKougeki_BuildFromFrame, ObjScan_StoreAttackGuardInfo 0x432480)
 int           attackScratchFill[7];   // +0xCC E: 0xCDCDCDCD (MSVC debug fill) in the 1895 attack frames, -1 in all others; no reader of frame+204..+231
 unsigned int  legacyBoxGroupACount;   // +0xE8 E: 0/1 (1 in 10 frames); no reader of frame+232, its slots hold stale values outside the box table numbering
 int           legacyBoxGroupAIdx[3];  // +0xEC E: -1 except 10 frames (0..3); no reader
 unsigned int  legacyBoxGroupBCount;   // +0xF8 E: 0/1 (1 in 6 frames); no reader of frame+248
 int           legacyBoxGroupBIdx[4];  // +0xFC E: -1 except +0x108 in 6 frames (0/1); no reader
 unsigned int  kasanariBoxEnable;      // +0x10C T: CAppHanteiKasanari__vf5 0x449620 gate (== number of non -1 slots)
 int           kasanariBoxIdx;         // +0x110 T: section 2 box index (single slot), -1 = empty
 unsigned int  hurtBoxCount;           // +0x114 T: CAppHanteiYarare__vf6 0x4494F0 gate
 int           hurtBoxIdx[6];          // +0x118 T: section 2 box indices (6 slots read by CAppHanteiYarare__vf6), -1 = empty
 unsigned int  editorOnly_130;         // +0x130 E: 6 in 42369 frames, 0 in 1622; frame+304 is the Yarare parameter block (lea in CAppHanteiYarare__vf6) but nothing reads its first dword
 Gof2GuardMode guardMode;              // +0x134 I: HitJudge_DefenderGuardsAttack 0x496930 switches on *(yarare+4) = frame+308
 Gof2ArmorMode armorMode;              // +0x138 I: HitJudge_DefenderArmorAbsorbs 0x497180 switches on *(yarare+4+4) = frame+312
 unsigned int  etcBoxCount;            // +0x13C T: CAppHanteiEtc__vf7 0x449750 gate
 int           etcBoxIdx[3];           // +0x140 T: 3 slots read; only the first is ever used in the data
 unsigned int  sousaiBoxCount;         // +0x14C T: CAppHanteiSousai__vf6 0x449820 gate
 int           sousaiBoxIdx[3];        // +0x150 T: 3 slots read, only the first is ever used
 unsigned int  tobiBoxCount;           // +0x15C T: CAppHanteiTobi__vf6 0x449910 gate
 int           tobiBoxIdx[3];          // +0x160 T: 3 slots read (RBO had 4)
 unsigned int  attackBoxCount;         // +0x16C T: CAppHanteiKougeki_BuildFromFrame gate
 int           attackBoxIdx[8];        // +0x170 T: 8 slots read; slots 0..4 are used in the data
 unsigned int  effectRecordIdx;        // +0x190 T: 1-based index into section 8 (96-byte records), 0 = none (Obj_SetActionAndRunScript -> Obj_SpawnFrameEffectRecord 0x4327F0)
};
struct Gof2PatternEntry {
 unsigned int  frameCount;             // +0x0 T: 0 = pattern absent (CHanteiAnime_CacheNextFrame, sub_4334C0 test *(pattern+12*n))
 unsigned int  flags;                  // +0x4 E: zero in all 17408 entries of all 68 files; no reader of pattern+4
 unsigned int  firstFrameIndex;        // +0x8 T: index into section 1 (frames are packed in pattern order)
};
struct Gof2BoxRect {
 __int16 x1;                           // +0x0 T: CAppHanteiKasanari__vf6 0x449680 etc. read four shorts (left, top, right, bottom; mirrored when facing flips)
 __int16 y1;                           // +0x2 T: see x1
 __int16 x2;                           // +0x4 T: see x1
 __int16 y2;                           // +0x6 T: see x1
};
struct Gof2HanteiAnime {
 void *vtable;                         // +0x00 T: CHanteiAnime vftable, embedded in the 0x1264-byte Obj at +616 (Obj_ConstructPoolSlot 0x435850)
 int patternNo;                        // +0x04 T: current pattern (CHanteiAnime_CacheNextFrame)
 int frameNo;                          // +0x08 T: frame inside the pattern
 int frameTicks;                       // +0x0C T: ticks spent on the frame (Obj_ScriptTick increments, compared to frame.duration)
 int loopCounter;                      // +0x10 T: aniFlag 5/8 counter (CHanteiAnime_StepAniFlag)
 struct Gof2PatternAreaView *view;     // +0x14 T: pattern area view (obj+636 = chara record +12)
 void *charaRecord;                    // +0x18 T: chara data block (obj+640, holds the PAT parts bank at +52)
 struct Gof2FrameRecord *frame;        // +0x1C T: current frame read by the hit-box builders (obj+644)
 struct Gof2FrameRecord *frameTick;    // +0x20 T: same frame pointer read by the animation step and DrawHelper_Draw (obj+648)
 void *aniCallback;                   // +0x24 T: int (__cdecl *)(int) predicate for aniFlag 7/9 (CHanteiAnime_StepAniFlag)
 int aniCallbackArg;                   // +0x28 T
 unsigned int stanceDword;             // +0x2C T: copy of frame dword +0x34 (CHanteiAnime__vf0)
 unsigned int vulnerabilityMask;       // +0x30 T: class mask built from hurtMaskEnable/hurtMaskFlags (CHanteiAnime__vf0)
 int flagA;                            // +0x34 T: hurtMaskFlags & 0x1000000
 int flagB;                            // +0x38 T: hurtMaskFlags & 0x2000000
 int nextSpriteId;                     // +0x3C T: sprite id of the frame after this one (CHanteiAnime_CacheNextFrame), tween target
 struct Gof2FrameRecord *nextFrame;    // +0x40 T: frame after this one
};
