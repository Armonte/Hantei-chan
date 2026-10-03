// RBO pattern-area frame types, IDA-parsable (idc.parse_decls). Single source of truth: mirrored by src/han2/rbo_types.h.
// Evidence for each field: docs/formats/frenchbread_rbo_gof.md section 3. Flag enums are made bitmask with idc.set_enum_bf.
enum RboAniFlag : unsigned char { ANI_END_TO_PATTERN=0, ANI_NEXT=1, ANI_JUMP_TO_FRAME=2, ANI_NEXT_LAND_TO_PATTERN=3, ANI_JUMP_TO_FRAME_LAND_TO_PATTERN=4, ANI_LOOP_COUNTED=5, ANI_STEP_BACK=6, ANI_NEXT_WHILE_CHANNEL_ACTIVE=7, ANI_LOOP_COUNTED_TO_LOOP_FRAME=8, ANI_NEXT_UNTIL_CHANNEL_ENDS=9 };
enum RboFlipMode : unsigned char { FLIP_NONE=0, FLIP_H=1, FLIP_V=2, ROT_90=3, ROT_180=4, ROT_270=5, FLIP_H_ROT_90=6, FLIP_H_ROT_270=7, ROT_FREE=8, FLIP_H_ROT_FREE=9 };
enum RboBlendMode : unsigned char { BLEND_OPAQUE=0, BLEND_ALPHA=1, BLEND_ADDITIVE=2, BLEND_SUBTRACTIVE=3 };
enum RboFrameFxFlags : unsigned char { FXF_USE_ZOOM=1, FXF_USE_ALPHA_FADE=2 };
enum RboMoveFlags : unsigned char { MOVE_CLEAR_VEL_X=1, MOVE_CLEAR_VEL_Y=2, MOVE_ADD_X=4, MOVE_ADD_Y=8 };
enum RboStanceClass : unsigned char { STANCE_NONE=0, STANCE_GROUND=1, STANCE_CROUCH=2, STANCE_AIR=4 };
enum RboCancelPermission : unsigned char { CANCEL_NONE=0, CANCEL_ON_INPUT_MATCH=1, CANCEL_ALWAYS=2 };
enum RboAttackEventFlags : unsigned char { ATKEV_BEGIN=1, ATKEV_END=2, ATKEV_KASANARI_BEGIN=4, ATKEV_KASANARI_END=8, ATKEV_KEEP_HIT_MEMORY=0x10 };
enum RboHurtMaskFlags : unsigned int { HMF_NO_CLASS_8=0x100, HMF_NO_CLASS_9=0x200, HMF_NO_CLASS_0=0x10000, HMF_NO_CLASS_1=0x20000, HMF_NO_CLASS_2=0x40000, HMF_NO_CLASS_3=0x80000, HMF_NO_CLASS_4=0x100000, HMF_NO_CLASS_5=0x400000, HMF_NO_CLASS_6=0x800000 };
enum RboMotionFlags : unsigned char { MOTF_BRAKE_ON_ENTER=4 };
enum RboScriptOrderFlags : unsigned char { SCRIPTORD_LIST_A_FIRST=1 };
enum RboPatternFlags : unsigned int { PATF_NONE=0, PATF_FLAG_2=2, PATF_ALT_DRAW_MODE=0x40, PATF_FLAG_80=0x80 };
struct RboFrameRecord {
 __int16 spriteId;                     // +0x00 CG image = id-10000 when >=10000, else PAT pose index (Actor_GetFrameSpriteId 0x440B10, Actor_DrawTree 0x4483E0)
 __int16 offsetX;                      // +0x02 Actor_DrawTree: draw offset X
 __int16 offsetY;                      // +0x04 Actor_DrawTree: draw offset Y
 unsigned __int16 duration;            // +0x06 Actor_TickFrame 0x43F830: ticks before advance
 RboFlipMode flipMode;                 // +0x08 Actor_DrawTree indexes the draw-mode table dword_48AE2C
 RboBlendMode blendMode;               // +0x09 Actor_EnterFrame copies non-zero to actor+744
 unsigned char alpha;                  // +0x0A alpha fade target when fxFlags.USE_ALPHA_FADE (Actor_EnterFrame)
 RboAniFlag aniFlag;                   // +0x0B Actor_AdvanceByAniFlag 0x43F640
 unsigned char jumpTarget;             // +0x0C pattern (ANI_END_TO_PATTERN) or frame to jump to
 unsigned char landJumpTarget;         // +0x0D Actor_ApplyLandJump 0x440A50
 unsigned char drawPriorityCode;       // +0x0E sub_440DF0 (1..11 set layer, 12..19 add depth, 20..61 offset table dword_48A3B8)
 unsigned char unused_0F;              // +0x0F zero in all 57786 RBO frames, no reader found
 unsigned __int16 zoom;                // +0x10 256 = 1.0; Actor_EnterFrame -> sub_4496A0 when fxFlags.USE_ZOOM
 unsigned char loopCount;              // +0x12 ANI_LOOP_COUNTED repeat count (Actor_EnterFrame -> actor+108)
 unsigned char loopEndFrame;           // +0x13 frame jumped to when the loop is exhausted (Actor_ResolveNextFrameRecord 0x440970)
 unsigned char unused_14[4];           // +0x14 zero in all frames, no reader found
 unsigned char interpolationMode;      // +0x18 non-zero = tween toward next frame (Actor_GetFrameInterpolation 0x445A50)
 RboFrameFxFlags fxFlags;              // +0x19 Actor_EnterFrame
 unsigned char unused_1A[2];           // +0x1A zero in all frames, no reader found
 unsigned char unused_1C[12];          // +0x1C..0x27 zero in all frames, no reader found
 RboMoveFlags moveFlags;               // +0x28 Actor_FrameMoveClearAxes 0x44B390, Actor_FrameMoveAddSpeed 0x44B3D0
 unsigned char unused_29[3];           // +0x29 zero in all frames (the flags byte is never read as a dword)
 __int16 speedX;                       // +0x2C added to velocity X when MOVE_ADD_X (flip-aware)
 __int16 speedY;                       // +0x2E added to velocity Y when MOVE_ADD_Y
 __int16 accelX;                       // +0x30 added to acceleration X when MOVE_ADD_X
 __int16 accelY;                       // +0x32 added to acceleration Y when MOVE_ADD_Y
 RboStanceClass stanceClass;           // +0x34 becomes actor+2072, tested by script condition sub_4187C0
 RboCancelPermission normalCancel;     // +0x35 sub_4189E0, sub_43DBE0, sub_43DC70
 RboCancelPermission specialCancel;    // +0x36 sub_4189E0
 unsigned char unused_37;              // +0x37 zero in all frames, no reader found
 RboAttackEventFlags attackEventFlags; // +0x38 Actor_UpdateAttackState 0x441060, Actor_UpdateBoxes272State 0x4412A0
 unsigned char unused_39[3];           // +0x39 zero in all frames, no reader found
 unsigned int  hurtMaskEnable;         // +0x3C 0 = actor hurt mask forced to 0 (Actor_ApplyFrameStatusFlags 0x441330)
 RboHurtMaskFlags hurtMaskFlags;       // +0x40 Actor_ApplyFrameStatusFlags
 unsigned char unused_44[4];           // +0x44 non-zero in only 22 of 57786 frames; no reader found
 unsigned int  hitLimitFlags;          // +0x48 copied to the hit limiter flags on ATKEV_BEGIN (sub_441020 0x441020, sub_4429E0)
 unsigned int  hitLimitCount;          // +0x4C copied to the hit limiter hit count
 unsigned char unused_50[12];          // +0x50..0x5B zero in all frames, no reader found
 RboMotionFlags motionFlags;           // +0x5C sub_43F0E0 0x43F0E0
 unsigned char unused_5D[3];           // +0x5D zero in all frames, no reader found
 unsigned char unused_60[0x58];        // +0x60..0xB7 zero in all 57786 RBO frames, no reader found
 RboScriptOrderFlags scriptOrderFlags; // +0xB8 Actor_EnterFrame
 unsigned char unused_B9[3];           // +0xB9 zero in all frames
 unsigned int  scriptListIndexA;       // +0xBC index into pattern-area section 6 (Actor_RunFrameScriptList6 0x41DA50), 0 = none
 unsigned int  scriptListIndexB;       // +0xC0 index into section 7 (Actor_RunFrameScriptList7 0x41F900), 0 = none
 unsigned int  hasAttack;              // +0xC4 1 when attackRecordIdx is valid
 int           attackRecordIdx;        // +0xC8 index into section 3 (120-byte AT records, Actor_CollectAttackBoxes2 0x440F40), -1 none
 unsigned char unused_CC[4];           // +0xCC not read by the RBO engine; 0/-1/small values, 0xCDCD uninitialised fill in 1873 frames
 int           sousaiBoxCount;         // +0xD0 clash boxes (CAppHanteiSousai in GOF2); RBO engine never reads this group
 int           sousaiBoxIdx[3];        // +0xD4 section 2 box indices, -1 = empty
 int           tobiBoxCount;           // +0xE0 projectile-hit boxes (CAppHanteiTobi in GOF2); RBO engine never reads this group
 int           tobiBoxIdx[4];          // +0xE4
 int           effectBoxEnable;        // +0xF4 Actor_PickBoxRect_244 0x442190: >0 use effectBoxIdx as hit-spark rect, else random hurt box
 int           effectBoxIdx;           // +0xF8
 int           hurtBoxCount;           // +0xFC Actor_CollectHurtBoxes3 0x441120
 int           hurtBoxIdx[3];          // +0x100
 int           kasanariBoxEnable;      // +0x10C overlap/push box present (Actor_CollectBoxes2_272 0x4411E0)
 int           kasanariBoxIdx[4];      // +0x110 engine reads the first 2; GOF2 CAppHanteiKasanari reads 1
 int           attackBoxCount;         // +0x120 Actor_CollectAttackBoxes2
 int           attackBoxIdx[2];        // +0x124
};
struct RboPatternEntry {
 unsigned int  frameCount;             // +0x0 0 = pattern absent (Actor_GetFrameSpriteId)
 RboPatternFlags flags;                // +0x4 Actor_CacheCurrentFrame 0x440920; values seen 0,2,0x40,0x80
 unsigned int  firstFrameIndex;        // +0x8 index into section 1 (frames are packed in pattern order)
};
