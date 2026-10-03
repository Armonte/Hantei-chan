// GOF1 (Glove on Fight 1, 2002, gof.exe) character .DAT types, IDA-parsable (idc.parse_decls). Single source of truth for the container, the 116-byte frame record and the per-pattern tables.
// Evidence for each field: docs/formats/gof1.md. Evidence prefix: T = reader traced in gof.exe.i64, I = reader traced but meaning inferred from the code shape/data, E = data-bearing (non-constant) but no reader found in gof.exe (editor-only), V = validated/checked by the loader.
// A field is named unused_<hexoffset> ONLY when it is constant zero in all 51 shipped .DAT files (8 characters + OBJECT, 3 archives) and no reader exists. Flag enums are made bitmask with the IDA enum bitfield flag. Names are prefixed Gof1/G1 so they never collide with the RBO/GOF2 enums.
enum Gof1AniFlag : unsigned char { G1ANI_END_TO_PATTERN=0, G1ANI_NEXT=1, G1ANI_JUMP_TO_FRAME=2, G1ANI_NEXT_LAND_TO_PATTERN=3, G1ANI_JUMP_TO_FRAME_LAND_TO_PATTERN=4, G1ANI_LOOP_COUNTED=5 };
enum Gof1DrawMode : unsigned char { G1DRAW_NORMAL=0, G1DRAW_FLIP_H=1, G1DRAW_FLIP_V=2, G1DRAW_ROT_90=3, G1DRAW_ROT_180=4, G1DRAW_ROT_270=5, G1DRAW_FLIP_H_ROT_90=6, G1DRAW_FLIP_H_ROT_270=7, G1DRAW_ROT_FREE=8, G1DRAW_ROT_FREE_FLIPPED=9 };
enum Gof1BlendMode : unsigned char { G1BLEND_OPAQUE=0, G1BLEND_ALPHA=1 };
enum Gof1Stance : unsigned char { G1STANCE_GROUND=0, G1STANCE_AIR=1, G1STANCE_CROUCH=2 };
enum Gof1CancelPermission : unsigned char { G1CANCEL_NONE=0, G1CANCEL_ON_HIT_OR_GUARD=1, G1CANCEL_ALWAYS=2 };
enum Gof1FrameFlags1 : unsigned int { G1FF1_CARRY_VELOCITY_AS_INERTIA=1, G1FF1_CLEAR_INERTIA=2, G1FF1_MOTION_ONLY_ON_FIRST_ENTRY=0x80000000 };
enum Gof1FrameFlags2 : unsigned int { G1FF2_THROW_TECHABLE=2, G1FF2_UNREAD_BIT2=4, G1FF2_INVULN_MASK=0xF0000, G1FF2_COUNTER_MASK=0xF00000 };
enum Gof1InvulnClass : unsigned char { G1INVULN_NONE=0, G1INVULN_EVADE_HIGH=1, G1INVULN_EVADE_LOW=2, G1INVULN_STRIKE=3, G1INVULN_THROW=4 };
enum Gof1CounterCommand : unsigned char { G1COUNTER_NONE=0, G1COUNTER_STATE_1=1, G1COUNTER_STATE_2=2, G1COUNTER_CLEAR=3 };
enum Gof1MoveInfo : unsigned char { G1MOVEINFO_TYPE_MASK=0x0F, G1MOVEINFO_LINEAR_FILTER=0x40, G1MOVEINFO_CG_DOUBLE_RES=0x80 };
enum Gof1AtFlagsA : unsigned char { G1ATA_CHIP_DAMAGE=1, G1ATA_NO_KO=2, G1ATA_STANDING_REACTION=8, G1ATA_KNOCKBACK_TOWARD_ATTACKER=0x10, G1ATA_NOT_REFLECTABLE=0x20, G1ATA_AIR_GUARD_RULE=0x40, G1ATA_BASE_KNOCKBACK_ROW=0x80 };
enum Gof1AtFlagsB : unsigned char { G1ATB_EXTRA_HIT_SPARK=1, G1ATB_NO_HORIZONTAL_KNOCKBACK=2, G1ATB_NO_COMBO_COUNT=4, G1ATB_SCREEN_SHAKE=8, G1ATB_AIR_COMBO_STARTER=0x10, G1ATB_NO_THROW_TECH=0x20, G1ATB_HITS_OWN_TEAM=0x40 };
enum Gof1AtHitClass : unsigned short { G1ATCLASS_HIGH_STRIKE=0, G1ATCLASS_LOW_STRIKE=1, G1ATCLASS_HIGH_THROW=2, G1ATCLASS_LOW_THROW=3, G1ATCLASS_UNGUARDABLE=4 };
enum Gof1StatusEffect : unsigned char { G1STATUS_NONE=0, G1STATUS_RED_FLICKER=1, G1STATUS_BLUE_FLICKER=2, G1STATUS_YELLOW_BLACK_GREY_CYCLE=3 };
enum Gof1IfType : unsigned char { G1IF_DIRECTION_JUMP=1, G1IF_END_CONDITION=2, G1IF_HIT_RESULT_JUMP=3, G1IF_VELOCITY_SIGN_JUMP=4, G1IF_INVINCIBLE_JUMP=5, G1IF_INPUT_JUMP=6, G1IF_INPUT_ACTION=7, G1IF_RANDOM_JUMP=8, G1IF_SET_LOOP_COUNTER=9, G1IF_LOOP_ZERO_JUMP=10, G1IF_COMMAND_MOVE_ACTION=11, G1IF_NEAREST_DISTANCE_JUMP=12, G1IF_SCREEN_EDGE_ACTION=13, G1IF_BOX_HIT_JUMP=14, G1IF_BOX_GRAB_JUMP=15, G1IF_CAMERA_FOLLOW=16, G1IF_HIT_COUNT_JUMP=17, G1IF_PARENT_ACTION_JUMP=18, G1IF_REFLECT_OBJECT=19, G1IF_BOX_ENEMY_JUMP=20, G1IF_FIGHTER_PRESENT_JUMP=21, G1IF_STAGE_JUMP=22, G1IF_GAME_MODE_JUMP=23, G1IF_COUNTER_AT_LEAST_JUMP=24, G1IF_VARIABLE_COMPARE_JUMP=25, G1IF_ADJUST_VELOCITY_BY_INPUT=26, G1IF_PARENT_GRABBED_JUMP=27, G1IF_ROUND_END_JUMP=28, G1IF_X_LIMIT_JUMP=29, G1IF_FACING_JUMP=30, G1IF_COMMAND_MOVE_JUMP=31, G1IF_CPU_PLAYER_JUMP=32, G1IF_ALLY_BOX_JUMP=50 };
enum Gof1EfType : unsigned char { G1EF_SPAWN_CHILD_OBJECT=1, G1EF_SPAWN_EFFECT=2, G1EF_SPAWN_PARTICLES=3, G1EF_CONTROL_GRABBED=4, G1EF_THROW_DAMAGE=5, G1EF_ACTOR_OP=6, G1EF_SYSTEM_PARTICLE=7, G1EF_SPAWN_COMMON_OBJECT=8, G1EF_PLAY_SOUND=9, G1EF_SET_ACTOR_PARAMS=30, G1EF_SPAWN_CHILD_FROM_PARENT=50 };
enum Gof1EfActorOp : unsigned char { G1OP_SET_TRAIL_EFFECT=0, G1OP_SCREEN_SHAKE_FREEZE=1, G1OP_SET_FREEZE_TIMERS=2, G1OP_SET_PERIODIC_PARAMS=3, G1OP_ADD_LIFE_METER_GAUGE=4, G1OP_FLIP_FACING=5, G1OP_SET_VELOCITY_RANGE=6, G1OP_SPAWN_FRAME_GHOST=7, G1OP_AIM_AT_OPPONENT=8, G1OP_SET_FLAG_215=9, G1OP_SET_WORD_VAR_236=10, G1OP_SET_HITSTOP=11, G1OP_MOVE_AND_FLASH=12, G1OP_FLASH=13, G1OP_ADD_COUNTER=100, G1OP_SUB_COUNTER=101, G1OP_ADD_BYTE_278=102, G1OP_SUB_BYTE_278=103, G1OP_SET_OR_ADD_WORD_VAR_280=105, G1OP_SET_BYTE_218=252, G1OP_FORCE_ROUND_END=253, G1OP_SET_FLAG_261=254, G1OP_SET_FLAG_260=255 };
enum Gof1PartFlipMode : unsigned char { G1PARTFLIP_NONE=0, G1PARTFLIP_H=1, G1PARTFLIP_V=2, G1PARTFLIP_HV=3 };
struct Gof1ArchiveFileHeader {         // 8 bytes at the start of gof_00..03.p (LoadArchiveAndDecryptIndex 0x423B50); the 64-byte index entries follow, then the file data
 unsigned int plainFlag;               // +0x00 T: 0 = file data is enciphered, non-zero = plain (Archive_XOR_Decrypt_With_Filename returns early when a1+12 != 0); 0 in gof_00/01/03, 1 in gof_02
 unsigned int entryCountXorKey;        // +0x04 T: entry count ^ 0xFA261EFB
};
struct Gof1ArchiveEntry {              // 64 bytes; entry i is enciphered in place by LoadArchiveAndDecryptIndex
 char name[56];                        // +0x00 T: CP932, NUL padded; byte j ^= (3*(j*i - 28)) & 0xFF
 unsigned int sizeXorKey;              // +0x38 T: size ^ 0xFA261EFB; Archive_Find_File_Get_Size reads +56
 unsigned int offset;                  // +0x3C T: absolute file offset of the data (data start = 8 + 64*count; entries are contiguous); Archive_XOR_Decrypt_With_Filename seeks +60
};
struct Gof1ArchiveHandle {             // 28-byte runtime handle: g_DataArchiveSlots[10] at 0x18001E8 (slot 9 = gof_03.p), g_ArchiveGof00/01/02
 void *hFile;                          // +0x00 T: File_Open handle
 void *hMapping;                       // +0x04 T: CreateFileMappingA
 void *view;                           // +0x08 T: MapViewOfFile
 unsigned int plainFlag;               // +0x0C T: first dword of the file (a2+12)
 int entryCount;                       // +0x10 T: decoded entry count (a2+16)
 unsigned int unused_14;               // +0x14 never written or read
 struct Gof1ArchiveEntry *entries;     // +0x18 T: GlobalAlloc(count << 6) holding the decoded index (a2+24)
};
struct Gof1FileHeader {                // 0x44 bytes at file start (SpriteDataSlot_LoadCharacterDat 0x42B330 decrypts the first 0x444 bytes, header + pattern table, with the "Memory..." key)
 char signature[8];                    // +0x00 E: CP932 "備前長船" (94 F5 91 4F 92 B7 91 44) in all 51 files; never read or compared by the game
 unsigned char unused_08[8];           // +0x08 zero in all files, not read
 unsigned int version;                 // +0x10 V: must be 18 (SpriteDataSlot_LoadCharacterDat: v10[4] != 18 -> return 0)
 unsigned int patternAreaEnd;          // +0x14 T: byte size of header + pattern area = start of the parts blob; the loader copies [0, patternAreaEnd) as the pattern block (GlobalAlloc(v5[5]))
 unsigned int partsSize;               // +0x18 T: size of the parts blob (old PAT v2); 0 = none; the loader allocates size+0x4000 and copies [patternAreaEnd, +partsSize)
 unsigned int cgOffset;                // +0x1C T: start of the CG blob = patternAreaEnd + partsSize (checked by gof1_dat.py; the loader reads file+cgOffset)
 unsigned int cgSize;                  // +0x20 T: size of the CG blob; 0 in all 51 shipped files (no CG bank is stored; every sprite is a parts pattern)
 unsigned char unused_24[0x20];        // +0x24 zero in all files, not read
};
struct Gof1CharDatHead {               // what g_CharPatternData[slot] points to: the first patternAreaEnd bytes of the file (Slot_BindCharacterData 0x42BC60 stores it at actor+604)
 struct Gof1FileHeader header;         // +0x00 T: see above
 int patternOffset[256];               // +0x44 T: absolute offset of pattern p inside this block, -1 = absent (Actor_ResolveFrameDataPointers 0x424590: dword at data + 4*pattern + 68)
};
struct Gof1PatternHeader {             // 0x14 bytes, followed by frameCount * 116-byte frame records, then the optional box/AT/IF/EF tables in that order
 unsigned char frameCount;             // +0x00 T: Actor_ResolveFrameDataPointers compares the frame index against this byte (*v3 <= frame -> error box); 1..100 in the data
 enum Gof1MoveInfo moveInfo;           // +0x01 T: low nibble = move type (Pattern_GetMoveType 0x425D60), bit 0x40 = linear filter and bit 0x80 = double-resolution CG (Pattern_IsCgDoubleRes 0x425D80, Pattern_GetLinearFilterBit 0x425DA0)
 unsigned char moveLevel;              // +0x02 T: move priority/level (Pattern_GetMoveLevel 0x425D40, compared < 100 by Actor_RequestLocomotionFromInput 0x425FC0); 0, 100 or 255 in the data
 unsigned char frameSizeTag;           // +0x03 E: 0 in the 9 retail files, 116 (the frame size) in the 17 LAST*.DAT backup files; no reader
 int boxTableOffset;                   // +0x04 T: offset (relative to the pattern start) of the box table (8-byte rects), -1 = none (Actor_ResolveFrameDataPointers -> actor+596)
 int atTableOffset;                    // +0x08 T: offset of the AT table (26-byte records), -1 = none (ObjCheckAttackVsFighters 0x43B070: pattern + [+8] + 26*frame.atIndex)
 int ifTableOffset;                    // +0x0C T: offset of the IF table (28-byte records), -1 = none (-> actor+620, ObjRunFrameEventsForPhase 0x427130)
 int efTableOffset;                    // +0x10 T: offset of the EF table (20-byte records), -1 = none (-> actor+624, Actor_RunFrameEFs 0x413750)
};
struct Gof1AnimFrame {                 // frame record +0x00..0x27 (the part of the frame the renderer and the animation stepper read; actor+612)
 __int16 spriteId;                     // +0x00 T: parts-pattern index when < 10000, CG image id-10000 otherwise (DrawFighterSprite 0x432150 -> sub_42FFE0 reads parts at +24+4*id; ObjBoxToWorld 0x42E4D0 tests < 10000); always < 512 in the data
 __int16 offsetX;                      // +0x02 T: draw offset X (DrawFighterSprite v3[1])
 __int16 offsetY;                      // +0x04 T: draw offset Y (v3[2])
 unsigned __int16 duration;            // +0x06 T: ticks before advancing (ObjRunActionScript 0x424A20 compares a1+14 against it; also the interpolation divisor in sub_42FFE0)
 enum Gof1DrawMode drawMode;           // +0x08 T: indexes g_DrawModeTransformTable 0x4640F8 (DrawFighterSprite), mode 8 shifts the origin by 100 (ObjBoxToWorld); 0 or 1 in the data
 enum Gof1BlendMode blendMode;         // +0x09 T: non-zero = alpha-blended draw using alpha (DrawFighterSprite v35); the blend code of sub_42FFE0 never reads a9
 unsigned char alpha;                  // +0x0A T: alpha 0..255, only used when blendMode != 0 (DrawFighterSprite v37)
 enum Gof1AniFlag aniFlag;             // +0x0B T: ObjRunActionScript / ObjEnterCurrentAction switch (0 end->pattern, 1 next, 2 jump, 3/4 same + land->pattern, 5 counted loop)
 unsigned char jumpTarget;             // +0x0C T: pattern (aniFlag 0) or frame (2, 4, 5) to go to
 unsigned char landJumpTarget;         // +0x0D T: frame (or pattern for aniFlag 3/4) entered when the actor lands (ObjRunActionScript landing branch)
 unsigned char drawPriority;           // +0x0E T: ObjEnterCurrentAction -> Actor_ApplyDrawPriority 0x425B80 (1 = front, 2 = back among the fighters)
 unsigned char unused_0F;              // +0x0F zero in all frames, not read
 unsigned __int16 zoom;                // +0x10 T: 256 = 1.0 (DrawFighterSprite v3[8]); 0 = no zoom; constant 0 in the data
 unsigned char loopCount;              // +0x12 T: loaded into actor+20 when non-zero (ObjEnterCurrentAction v1[18]); aniFlag 5 repeat count
 unsigned char loopEndFrame;           // +0x13 T: frame entered when the aniFlag 5 counter reaches 0 (ObjRunActionScript case 5 reads +19)
 unsigned char unused_14[4];           // +0x14 zero in all frames, not read
 unsigned char interpolateToNext;      // +0x18 T: non-zero = tween toward the next frame's parts pattern (sub_42FFE0: *(a22+24))
 unsigned char unused_19[15];          // +0x19 zero in all frames, not read
};
struct Gof1StateFrame {                // frame record +0x28..0x4F (movement, cancel, flags; actor+616)
 unsigned char clearVelX;              // +0x00 T: zero X velocity, X accel and X max (Actor_ApplyFrameMotionFlags 0x425310: *(_BYTE*)v1)
 unsigned char clearVelY;              // +0x01 T: zero Y velocity and accel
 unsigned char addVelX;                // +0x02 T: add velX/accelX/maxX facing-aware (sub_425310)
 unsigned char addVelY;                // +0x03 T: add velY/accelY
 unsigned char unused_04[4];           // +0x04 zero in all frames, not read
 __int16 velX;                         // +0x08 T: added to actor velX when addVelX (sub_425310 v1+8)
 __int16 velY;                         // +0x0A T: added to velY when addVelY (v1+10)
 __int16 accelX;                       // +0x0C T: added to accelX when addVelX (v1+12)
 __int16 accelY;                       // +0x0E T: added to accelY when addVelY (v1+14)
 enum Gof1Stance stance;               // +0x10 T: copied to actor+594 (ObjRunActionScript), air physics test in ObjIntegrateMotion 0x4257D0; Actor_DispatchInputByStance 0x4263E0 builds stance | canAct<<4
 enum Gof1CancelPermission normalCancel; // +0x11 T: Actor_RequestLocomotionFromInput 0x425FC0 (2 always, 1 when the actor has hit/been guarded: a1+205 & 0xF)
 enum Gof1CancelPermission specialCancel; // +0x12 T: Fighter_TryStartCommandMove 0x426BD0 (cancel table of class 1/2 command moves; 2 = always, 1 = after hit/guard; also gates the super-cancel window); 0/1/2 in the data (21 frames use 2)
 unsigned char attackHitCount;         // +0x13 T: loaded into actor+220 on frame entry (ObjEnterCurrentAction v3 = +19); attack boxes are ignored when it is 0 (ObjCheckAttackVsFighters 0x43B070)
 unsigned char canAct;                 // +0x14 T: 1 = actor may act (ObjRunActionScript v23+20 == 1, ObjFighterStateAndHitReaction 0x43BF40); combined with stance in Actor_DispatchInputByStance 0x4263E0
 unsigned char unused_15[3];           // +0x15 zero in all frames, not read
 enum Gof1FrameFlags1 flags1;          // +0x18 T: ObjIntegrateMotion / sub_425310 (bit0 carry velocity as inertia, bit1 clear inertia, bit31 motion only on first entry)
 enum Gof1FrameFlags2 flags2;          // +0x1C T: bit1 sub_425E10 throw tech; bits 16..19 invulnerability class (ObjCheckAttackVsFighters reads byte +0x1E & 0xF); bits 20..23 counter command (ObjEnterCurrentAction (dword>>20)&0xF)
 __int16 maxVelX;                      // +0x20 T: subtracted/added to actor maxVelX with addVelX (sub_425310 v1+32); 0 except 1 frame
 unsigned char unused_22[6];           // +0x22 zero in all frames, not read
};
struct Gof1FrameRecord {               // 116 bytes; Actor_ResolveFrameDataPointers sets actor+608/612 = frame, +616 = frame+40, +628 = frame+96
 struct Gof1AnimFrame anim;            // +0x00 T: see Gof1AnimFrame (actor+612)
 struct Gof1StateFrame state;          // +0x28 T: see Gof1StateFrame (actor+616)
 unsigned char atIndex;                // +0x50 T: signed char index into the AT table (26*idx), 0xFF = none (ObjCheckAttackVsFighters, ObjCheckAttackVsObjects, sub_43A740 read *(char*)(frame+80))
 unsigned char atIndexHighByte;        // +0x51 E: 0, 0xFF or stack garbage (45 distinct values); the engine reads atIndex as a signed char and never touches this byte
 __int16 ifIndex[3];                   // +0x52 T: IF table indices, -1 = empty (ObjRunFrameEventsForPhase loop i = 82; i < 88; i += 2)
 __int16 efIndex[4];                   // +0x58 T: EF table indices, -1 = empty (Actor_RunFrameEFs loop i = 88; i < 96; i += 2)
 __int16 boxIndex[10];                 // +0x60 T: box table indices, -1 = empty (actor+628): [0] pushbox (ObjResolvePushboxCollision), [1..3] hurt boxes (ObjCheckAttackVsFighters loop at +2), [4] grab/probe box (own kind 6 of Actor_FindOverlappingTarget, IF 14/15/19/20), [5] special2 (read by Actor_GetBoxSlotIndex only; 1 frame uses it), [6] reflect/clash box (ObjCheckReflectBoxVsAttacks +12), [7] projectile hurt box (target kind 6 of IF 19; never present in the data), [8..9] attack boxes (+16, +18); slot 9 is never present
};
struct Gof1Box {                       // 8 bytes, box table entry (frame space, origin (128,224) for CG, double-resolution parts-space for parts frames; ObjBoxToWorld 0x42E4D0 transforms it)
 __int16 x1;                           // +0x00 T: ObjResolvePushboxCollision reads *(__int16*)(box + 8*idx)
 __int16 y1;                           // +0x02 T: +2
 __int16 x2;                           // +0x04 T: +4
 __int16 y2;                           // +0x06 T: +6
};
struct Gof1RectI32 {                   // runtime rectangle ObjBoxToWorld fills from a Gof1Box (4 ints, passed to RectOverlap 0x43A540 / RectIntersection 0x43A590)
 int x1;                               // +0x00 T: ObjBoxToWorld a2[0]
 int y1;                               // +0x04 T: a2[1]
 int x2;                               // +0x08 T: a2[2]
 int y2;                               // +0x0C T: a2[3]
};
struct Gof1AtRecord {                  // 26 bytes; pattern + atTableOffset + 26 * frame.atIndex
 unsigned char hitReaction;            // +0x00 T: ObjFighterStateAndHitReaction 0x43BF40 v87 -> g_HitReactionClassTable / g_HitReactionActionByStance (what the victim plays); 0..5 in the data
 unsigned char hitstopLevel;           // +0x01 T: index into g_HitstopFramesByLevel (byte_1622408, loaded from vector.txt by sub_42C1D0) -> sub_43BEF0 hitstop and screen shake
 unsigned char hitSparkClass;          // +0x02 T: hit spark/sound class; < 14 selects the spark and word_4650D8[6*class] default sound
 unsigned char attackKind;             // +0x03 T: g_AtKindTargetsObjectsOnly[kind] / g_AtKindImmunityChannel[kind] (ObjCheckAttackVs*), and the switch in ObjFighterStateAndHitReaction; 0 or 7 in the data
 unsigned char hitSoundOverride;       // +0x04 T: Se_RequestPlay(this) when non-zero, else the class default sound
 enum Gof1AtFlagsA flagsA;             // +0x05 T: see Gof1AtFlagsA
 __int16 damage;                       // +0x06 T: sub_43FA70 / sub_43F9E0 (damage scaling), chip damage = scaled >> 3
 __int16 meterGain;                    // +0x08 T: AddSuperMeter(attacker, +8) and (victim, +8 >> 1 / >> 2)
 __int16 guardBreakValue;              // +0x0A T: added to the guarding victim's guard gauge (actor+108) (ObjFighterStateAndHitReaction v6+10); 0 in the data
 unsigned char unused_0C[3];           // +0x0C zero in all records, not read
 enum Gof1StatusEffect statusEffect;   // +0x0F T: written to actor+248 when non-zero (ObjFighterStateAndHitReaction v108); DrawFighterSprite tints the victim: 1 red pulse, 2 blue random shimmer, 3 yellow/black/grey cycle; 0 in the data
 enum Gof1AtFlagsB flagsB;             // +0x10 T: see Gof1AtFlagsB
 unsigned char unused_11;              // +0x11 zero in all records, not read
 enum Gof1AtHitClass hitClass;         // +0x12 T: row of g_AtClassVsInvulnHitTable (sub_43B050) and guard-height check (ObjFighterStateAndHitReaction v32); 0..4
 unsigned char unused_14[6];           // +0x14 zero in all records, not read
};
struct Gof1IfRecord {                  // 28 bytes; interpreted by Actor_RunFrameIfRecord 0x4271C0; parameters are per type (docs/formats/gof1.md section 3.6)
 enum Gof1IfType type;                 // +0x00 T: switch in Actor_RunFrameIfRecord; g_IfTypePhaseTable[type] picks the phase it runs in (14, 15, 19, 20 run in phase 1)
 unsigned char unused_01[3];           // +0x01 zero in all records, not read
 int param[4];                         // +0x04 T: *(a2+4), +8, +12, +16 (type dependent: target frame/action, mask, counter, ...); +20/+24 are never read
 unsigned char unused_14[8];           // +0x14 zero in all records, not read
};
struct Gof1EfRecord {                  // 20 bytes; dispatched by Actor_RunFrameEFs 0x413750
 enum Gof1EfType type;                 // +0x00 T: switch in Actor_RunFrameEFs
 unsigned char subType;                // +0x01 T: effect/pattern id, sound bank or Gof1EfActorOp (type 6) depending on the type
 __int16 arg[5];                       // +0x02 T: five signed words (x, y, speeds, flags...) read at +2, +4, +6, +8, +10 by the type handlers (InitChildObject 0x414710, sub_414DC0, sub_41DF00, ...)
 unsigned char unused_0C[8];           // +0x0C zero in all records, not read by any handler
};
struct Gof1PartsHeader {               // old PAT v2 parts blob header (file + patternAreaEnd), 0x8CBC bytes; SpriteDataSlot_UploadPartsAndCG 0x42B6D0
 unsigned int version;                 // +0x00 E: 2 (the loader never checks it)
 unsigned int magic;                   // +0x04 E: 0x01234567
 unsigned char unused_08[16];          // +0x08 zero in all files, not read
 unsigned int patternOffset[1000];     // +0x18 T: blob-relative offset of the parts pattern, 0 = absent (UploadPartsAndCG v8 + 6 dwords; patterns are 3680 bytes apart)
 char patternName[1000][32];           // +0xFB8 E: CP932 part-pattern names such as "000_00"; not read
 unsigned int textureRegionOffset;     // +0x8CB8 T: v8[9006] = blob offset of the texture region (right after the last parts pattern)
};
struct Gof1Part {                      // 92 bytes, one drawable piece; a parts pattern holds 40 of them (sub_42FFE0 loops 40, UploadPartsAndCG adds the texture slot base to +40)
 int x;                                // +0x00 T: position X (sub_42FFE0 *(v43+v24))
 int y;                                // +0x04 T: position Y
 int width;                            // +0x08 T: destination width before scaling
 int height;                           // +0x0C T: destination height before scaling
 enum Gof1PartFlipMode flipMode;       // +0x10 T: 1 = flip X, 2 = flip Y, 3 = both (sub_42FFE0 v76 switch on byte +16)
 unsigned char linearMagFilter;        // +0x11 T: non-zero = sub_4073E0(2) sets the magnification filter flag byte_4A8F80 to 2 (linear) for this part (sub_42FFE0); 3 parts of 96480
 unsigned char editorFlag_12;          // +0x12 E: 1 in 29% of the parts; no reader (the renderer reads bytes +16/+17 only)
 unsigned char unused_13;              // +0x13 zero in all parts, not read
 int scaleX;                           // +0x14 T: scale in 1/1000 (1000 = 1.0)
 int scaleY;                           // +0x18 T: scale in 1/1000
 int rotation;                         // +0x1C T: 1/10000 turn, x0.036 degrees (sub_42FEF0 / flt_15F0858)
 unsigned char colorA;                 // +0x20 T: modulation alpha (x HIBYTE(a10))
 unsigned char colorR;                 // +0x21 T: modulation red
 unsigned char colorG;                 // +0x22 T: modulation green
 unsigned char colorB;                 // +0x23 T: modulation blue
 unsigned char addR;                   // +0x24 T: additive red (+ dword_1864EDC)
 unsigned char addG;                   // +0x25 T: additive green
 unsigned char addB;                   // +0x26 T: additive blue
 unsigned char unused_27;              // +0x27 zero in all parts, not read
 int textureSlot;                      // +0x28 T: texture index inside the region; UploadPartsAndCG adds the character's slot base (50*char + 2)
 int srcX;                             // +0x2C T: cut-out x in the texture (*(v43+v24+44))
 int srcY;                             // +0x30 T: cut-out y
 int srcWidth;                         // +0x34 T: cut-out width (0 = part not drawn)
 int srcHeight;                        // +0x38 T: cut-out height
 unsigned char drawOrder;              // +0x3C T: draw pass number; the renderer collects parts per value 0..255 (sub_42FFE0 byte at v30 = part + 60)
 unsigned char unused_3D[3];           // +0x3D zero in all parts, not read
 int scaleOriginX;                     // +0x40 T: fixed point of the scale (sub_42FFE0 *(v43+v24+64))
 int scaleOriginY;                     // +0x44 T: fixed point of the scale (+68)
 unsigned char unused_48[20];          // +0x48 zero in all parts, not read
};
struct Gof1PartsPattern {              // 3680 bytes
 struct Gof1Part part[40];             // +0x00 T: UploadPartsAndCG / sub_42FFE0 loop 40 x 92 bytes
};
struct Gof1TextureRegion {             // 0x2E30 bytes of header before the first raster (the textures follow at the offsets in textureOffset, raw A8R8G8B8 squares)
 unsigned char unused_00[20];          // +0x00 zero in all files, not read
 unsigned int version;                 // +0x14 E: 1 in all files, not read
 unsigned int textureCount;            // +0x18 E: 3..7; the loader walks all 50 slots instead
 unsigned int textureOffset[50];       // +0x1C T: region-relative offset of each raster, 0 = unused slot (UploadPartsAndCG v10 = region + 28)
 char textureName[50][64];             // +0xE4 E: source bitmap names such as "ayu00.bmp"; not read
 unsigned int textureSize[50];         // +0xD64 T: edge length in pixels of the square raster (v10[850]); 512 in the data
 unsigned char unused_E2C[8196];       // +0xE2C zero in all files, not read (padding up to the first raster at +0x2E30)
};
struct Gof1PatternName {               // 64 bytes; 256 of them close the file (file end - 0x4000), enciphered with the blob key
 char name[64];                        // +0x00 E: CP932 pattern name, e.g. "立ち弱攻撃"; editor-only, the game never reads the tail
};
