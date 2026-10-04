// Queen of Heart 2001 ~Party's Breaker~ (pb2k1.exe, PE32, base 0x400000) character-file, satellite-file and runtime character types, IDA-parsable (idc.parse_decls).
// IDB: C:\dev\ida\server\pb2k1.exe.i64. Everything here is applied in the IDB. Doc: docs/formats/pb2k1.md. Verifier: tools/fb/pb2k1_verify.py (all 16 shipped character .DAT files).
// Evidence tags: T = traced in code (function names are the IDB names), D = verified on the shipped files (01.dat / 01p.dat / 02p.dat), U = unproven guess, X = proved never read by the game (data-bearing or constant).
// A field is named unused_<hexoff> only when it is constant in all shipped data AND no reader exists in any function (register-tracking census over all 932 functions); `unreferenced_<hexoff>` = no access seen in the traced code, not proven exhaustively.
// All offsets are STRUCT-RELATIVE. The runtime object (Pb2Object) of a fighter sits at slot+4 (Pb2FighterSlot), of a spawned object at slot+4 (Pb2ObjectSlot).

struct Pb2Object;
struct Pb2FighterSlot;
struct Pb2CharDatHead;
struct Pb2PatternHeader;
struct Pb2AnimFrame;
struct Pb2StateFrame;
struct Pb2FrameRecord;
struct Pb2Box;
struct Pb2IfRecord;
struct Pb2EfRecord;

// ============================================================================================
// 1. Enums
// ============================================================================================
enum Pb2ControlType : unsigned char { PB2CTRL_HUMAN=0, PB2CTRL_CPU=1 };
enum Pb2ObjKind : unsigned char { PB2OBJ_FIGHTER=0, PB2OBJ_GHOST_TRAIL=31, PB2OBJ_CHILD_8F=143, PB2OBJ_CHILD=255 };   // 0 fighter; 31 inert ghost/trail (PB_Objects_ScriptAndMotionStep counts lifeTimer down); >= 0xF0 non-fighter (PB_ObjScreenEdgeOverlap, PB_ObjOnActionChanged); bit 7 = spawned child
enum Pb2RunState : unsigned char { PB2RUN_IDLE=0, PB2RUN_PENDING=1, PB2RUN_FORCED=2 };
enum Pb2Stance : unsigned char { PB2STANCE_GROUND=0, PB2STANCE_AIR=1, PB2STANCE_CROUCH=2 };
enum Pb2AniFlag : unsigned char { PB2ANI_END_TO_PATTERN=0, PB2ANI_NEXT=1, PB2ANI_JUMP_TO_FRAME=2, PB2ANI_NEXT_LAND_TO_PATTERN=3, PB2ANI_JUMP_TO_FRAME_LAND_TO_PATTERN=4, PB2ANI_LOOP_COUNTED=5 };
enum Pb2DrawMode : unsigned char { PB2DRAW_NORMAL=0, PB2DRAW_MODE_1=1, PB2DRAW_ORIGIN_SHIFT_100=8 };   // index into dword_461108 (g_DrawModeTransformTable); 8 = origin shifted by 100 (PB_ObjBoxToWorld, PB_DrawFighterSprite); 0, 1, 8 in the data
enum Pb2BlendMode : unsigned char { PB2BLEND_OPAQUE=0, PB2BLEND_ALPHA=1, PB2BLEND_MODE_2=2 };   // non-zero = alpha-blended draw using alpha (PB_DrawFighterSprite); 0/1/2 in the data (meaning of 2 is U)
enum Pb2CancelPermission : unsigned char { PB2CANCEL_NONE=0, PB2CANCEL_ON_HIT_OR_GUARD=1, PB2CANCEL_ALWAYS=2 };
enum Pb2FrameFlags1 : unsigned int { PB2FF1_CARRY_VELOCITY_AS_INERTIA=1, PB2FF1_CLEAR_INERTIA=2, PB2FF1_MOTION_ONLY_ON_FIRST_ENTRY=2147483648 };   // T: PB_ObjApplyFrameMotionFlags / PB_ObjIntegrateMotion
enum Pb2FrameFlags2 : unsigned int { PB2FF2_REQUIRES_CONNECT_FOR_SUPER_CANCEL=1, PB2FF2_THROW_TECHABLE=2, PB2FF2_JUMP_CANCEL_ON_HIT=4, PB2FF2_BIT31=2147483648 };   // bit0 PB_SlotTryStartCommandMove (class 2 gate), bit1 PB_ObjTryThrowTech, bit2 PB_ObjDispatchInputByStance (jump cancel after a hit, U), bit31 PB_ObjDispatchInputByStance (blocks the dash-turn, U); the GOF1 invulnerability (b16..19) and counter (b20..23) bits are 0 in all PB data
enum Pb2AtFlagsA : unsigned char { PB2ATA_CHIP_DAMAGE=1, PB2ATA_NO_KO=2, PB2ATA_STANDING_REACTION=8, PB2ATA_KNOCKBACK_TOWARD_ATTACKER=16, PB2ATA_NOT_REFLECTABLE=32, PB2ATA_AIR_GUARD_RULE=64, PB2ATA_BASE_KNOCKBACK_ROW=128 };   // T: PB_ObjFighterStateAndHitReaction (1, 2, 8, 0x10), sub_43AB20 (0x40); 0x20 / 0x80 U (GOF1 shape)
enum Pb2AtFlagsB : unsigned char { PB2ATB_EXTRA_HIT_SPARK=1, PB2ATB_NO_HORIZONTAL_KNOCKBACK=2, PB2ATB_NO_COMBO_COUNT=4, PB2ATB_SCREEN_SHAKE=8, PB2ATB_AIR_COMBO_STARTER=16, PB2ATB_NO_THROW_TECH=32, PB2ATB_HITS_OWN_TEAM=64 };   // T: 1 (spark 257), 2 (PB_ObjStartKnockback a5), 4 (PB_ComboRecord_RegisterHit), 8 (screen shake), 0x10, 0x20 (PB_ObjFighterStateAndHitReaction); 0x40 U
enum Pb2AtHitClass : unsigned short { PB2ATCLASS_HIGH_STRIKE=0, PB2ATCLASS_LOW_STRIKE=1, PB2ATCLASS_HIGH_THROW=2, PB2ATCLASS_LOW_THROW=3, PB2ATCLASS_UNGUARDABLE=4 };
enum Pb2StatusEffect : unsigned char { PB2STATUS_NONE=0, PB2STATUS_RED_FLICKER=1, PB2STATUS_BLUE_FLICKER=2, PB2STATUS_YELLOW_BLACK_GREY_CYCLE=3 };   // AT.statusEffect -> obj.tintMode (PB_DrawFighterSprite colour modes); always 0 in the data
enum Pb2IfType : unsigned char { PB2IF_DIRECTION_ACTION=1, PB2IF_END_CONDITION=2, PB2IF_HIT_RESULT_JUMP=3, PB2IF_VELOCITY_SIGN_JUMP=4, PB2IF_INVINCIBLE_JUMP=5, PB2IF_INPUT_JUMP=6, PB2IF_INPUT_ACTION=7, PB2IF_RANDOM_JUMP=8, PB2IF_SET_LOOP_COUNTER=9, PB2IF_LOOP_ZERO_JUMP=10, PB2IF_COMMAND_MOVE_ACTION=11, PB2IF_NEAREST_DISTANCE_JUMP=12, PB2IF_SCREEN_EDGE_ACTION=13, PB2IF_BOX_HIT_JUMP=14, PB2IF_BOX_GRAB_JUMP=15, PB2IF_CAMERA_FOLLOW=16, PB2IF_HIT_COUNT_JUMP=17, PB2IF_PARENT_ACTION_JUMP=18, PB2IF_REFLECT_OBJECT=19, PB2IF_BOX_ENEMY_JUMP=20, PB2IF_FIGHTER_PRESENT_JUMP=21, PB2IF_STAGE_JUMP=22, PB2IF_GAME_MODE_JUMP=23, PB2IF_COUNTER_AT_LEAST_JUMP=24, PB2IF_VARIABLE_COMPARE_JUMP=25, PB2IF_ADJUST_VELOCITY_BY_INPUT=26, PB2IF_PARENT_GRABBED_JUMP=27, PB2IF_ROUND_END_JUMP=28, PB2IF_ALLY_BOX_JUMP=50 };   // T: PB_ObjRunFrameIfRecord 0x428160 (switch); byte_461048 (g_IfTypePhaseTable) puts 14, 15, 19, 20 into phase 1
enum Pb2EfType : unsigned char { PB2EF_SPAWN_CHILD_OBJECT=1, PB2EF_SPAWN_EFFECT=2, PB2EF_SPAWN_PARTICLES=3, PB2EF_CONTROL_GRABBED=4, PB2EF_THROW_DAMAGE=5, PB2EF_ACTOR_OP=6, PB2EF_SYSTEM_PARTICLE=7, PB2EF_SPAWN_EFFECT_DAT_OBJECT=8, PB2EF_PLAY_SOUND=9, PB2EF_SET_ACTOR_PARAMS=30, PB2EF_SPAWN_CHILD_FROM_PARENT=50 };   // T: PB_ObjRunFrameEntryEffects 0x413220 (switch)
enum Pb2EfActorOp : unsigned char { PB2OP_SET_TRAIL_EFFECT=0, PB2OP_SCREEN_SHAKE_KO_SLOWMO=1, PB2OP_SET_FREEZE_TIMERS=2, PB2OP_SET_AFTERIMAGE=3, PB2OP_ADD_LIFE_METER_RESERVE=4, PB2OP_FLIP_FACING=5, PB2OP_SET_VELOCITY_RANGE=6, PB2OP_SPAWN_FRAME_GHOST=7, PB2OP_AIM_AT_OPPONENT=8, PB2OP_SET_INPUT_LOCK=9, PB2OP_ADD_COUNTER=100, PB2OP_SUB_COUNTER=101, PB2OP_ADD_DASH_COUNT=102, PB2OP_SUB_DASH_COUNT=103, PB2OP_SET_OR_ADD_PARAM_VAR=105, PB2OP_SET_SCRIPT_FLAG=254, PB2OP_SET_INVULN_FLAG=255 };   // T: PB_EfType6_ActorOp 0x41CB20 (EF.subType)
enum Pb2ObjFlagsA : unsigned char { PB2OF_A_END_WITH_PARENT_STATE=1, PB2OF_A_FORCE_FACE_RIGHT=2, PB2OF_A_INHERIT_PARENT_MOTION=4, PB2OF_A_PART_OF_OWNER_ATTACK=8, PB2OF_A_SCREEN_RELATIVE_POSITION=16, PB2OF_A_END_ON_PARENT_ACTION_CHANGE=32, PB2OF_A_NO_GROUND_LANDING=64 };   // T: PB_InitChildObject copies EF.arg2 bits; readers PB_Objects_ScriptAndMotionStep (1, 4, 8, 0x20), PB_ObjIntegrateMotion (0x40), PB_InitChildObject (0x10 places the child relative to the camera)
enum Pb2ObjFlagsB : unsigned char { PB2OF_B_DRAW_SHADOW=1, PB2OF_B_STEP_FRAME_WITH_PARENT=2, PB2OF_B_END_WHEN_PARENT_AIR_OR_GRABBED=4, PB2OF_B_HITSTOP_TO_OWNER=8 };   // T: PB_InitChildObject copies EF.arg3 bits; PB_Objects_ScriptAndMotionStep tests 2 and 4
enum Pb2AttackResultFlags : unsigned char { PB2HITRES_HIT=1, PB2HITRES_GUARDED=2, PB2HITRES_TESTED_BIT4=4, PB2HITRES_EVADED_STRIKE=8, PB2HITRES_VICTIM_GROUND=16, PB2HITRES_VICTIM_AIR=32 };   // T: PB_ObjFighterStateAndHitReaction (1, 2, 0x10, 0x20 by the victim stance); 8 PB_ObjCheckAttackVsFighters
enum Pb2InputDirection : unsigned char { PB2DIR_NEUTRAL=0, PB2DIR_DOWN_LEFT=1, PB2DIR_DOWN=2, PB2DIR_DOWN_RIGHT=3, PB2DIR_LEFT=4, PB2DIR_RIGHT=6, PB2DIR_UP_LEFT=7, PB2DIR_UP=8, PB2DIR_UP_RIGHT=9 };   // numpad notation, facing-corrected (PB_ReadBattleInputForPlayer 0x424410)
enum Pb2InputButtonFlags : unsigned int { PB2BTN_A=1, PB2BTN_B=2, PB2BTN_C=4, PB2BTN_D=8, PB2BTN_E=16, PB2BTN_F=32, PB2BTN_A_PRESSED=4096, PB2BTN_B_PRESSED=8192, PB2BTN_C_PRESSED=16384, PB2BTN_D_PRESSED=32768, PB2BTN_E_PRESSED=65536, PB2BTN_F_PRESSED=131072 };   // T: PB_ReadBattleInputForPlayer (a3: held bits 0..5 | pressed bits 12..17, a4: released bits 0..5)
enum Pb2CommandMoveClass : unsigned char { PB2MOVE_NORMAL=0, PB2MOVE_SPECIAL=1, PB2MOVE_SUPER=2 };   // T: PB_SlotTryStartCommandMove 0x427C30 (0 uses state.normalCancel, 1 uses state.specialCancel, 2 uses specialCancel + flags2 bit 0 gate + super-cancel window)
enum Pb2CommandFlags : unsigned char { PB2CMD_STANCE_GROUND=1, PB2CMD_STANCE_AIR=2, PB2CMD_STANCE_CROUCH=4, PB2CMD_NO_CANCEL_ENTRY=8, PB2CMD_CLEAR_HISTORY_ON_ACTION=16, PB2CMD_SCRIPT_ONLY=32, PB2CMD_GUARD_CANCEL=64 };   // T: PB_SlotTryStartCommandMove / PB_SlotFindScriptedCommand; 0x40 is set in 166 shipped records (D)
enum Pb2CommandFlags2 : unsigned char { PB2CMD2_USE_RING_B=1, PB2CMD2_STRICT_SEQUENCE=2, PB2CMD2_EVADE_ONLY=64, PB2CMD2_EVADE_MOVE=128 };   // T: 0x80 (evade branch), 0x40 (refuses the normal path) in PB_SlotTryStartCommandMove; 1 / 2 in PB_SlotMatchCommandSequence
enum Pb2CtFlags : unsigned char { PB2CT_RESTRICT_REPEAT_MOVES=1, PB2CT_EVADE_ENABLED=2, PB2CT_JUST_GUARD_ENABLED=8, PB2CT_FLAG_0x10=16, PB2CT_JUMP_CANCEL_ENABLED=32 };   // T: bit 0 PB_ObjRunActionScript / PB_SlotTryStartCommandMove, bit 1 PB_ObjFighterStateAndHitReaction (evade), bit 3 / 4 (HitReaction), bit 5 PB_ObjDispatchInputByStance (jump cancel); 0x39 in every shipped CT file

// 1b. Archive layer (PB/GOF1 container, handled by fbarctool): runtime handle and entry, PB_LoadPFile 0x423E20
struct Pb2ArchiveEntry {   // 64 bytes
 char name[56]; // +0x00 T D: CP932 upper-case, NUL padded; byte j of entry i is stored as name[j] ^ (3 * (j * i - 28)) (PB_LoadPFile decodes the whole table in place)
 unsigned int size; // +0x38 T D: payload size (stored in the file as size ^ 0xFA261EFB, decoded in place by PB_LoadPFile)
 unsigned int offset; // +0x3C T D: absolute file offset of the payload
};
struct Pb2ArchiveHandle {   // 28 bytes; g_DataArchiveSlots[10] 0x17E9FE0 (slot 7 = 02p.dat, 8 = 01p.dat, 9 = 01.dat; Load_File_From_PB_Archive searches slots 0..9 and the first archive that has the name wins, so 02p shadows 01p shadows 01), g_BgmArchiveSlots[5] 0x161E8F0 (slot 3 = 00p.dat, 4 = 00.dat)
 void * hFile; // +0x00 T: File_Open handle
 void * hMapping; // +0x04 T: CreateFileMappingA handle
 void * view; // +0x08 T: MapViewOfFile view
 unsigned int plainFlag; // +0x0C T D: first u32 of the file; non-zero = payload stored plain (PB_Archive_XOR_Decrypt_With_Filename returns early)
 int entryCount; // +0x10 T D: second u32 of the file ^ 0xFA261EFB
 unsigned int unreferenced_14; // +0x14 U: no access
 struct Pb2ArchiveEntry * entries; // +0x18 T: GlobalAlloc(64 * entryCount) decoded index
};

// 1c. On-disk archive forms and the boot integrity probes (docs/formats/pb2k1.md section 16). Documentation types: not applied to a global (the runtime forms above are).
struct Pb2ArchiveFileHeader {   // 8 bytes at archive offset 0; the 64-byte Pb2ArchiveDiskEntry records follow, then the payloads back to back (D: no gaps, no trailer in 02 / 03 / 04.dat)
 unsigned int plainFlag; // +0x00 T D: 0 = payload heads are enciphered with the entry name (game side), non-zero = plain; the shipped 02.dat / 04.dat carry 1, 03.dat carries 0, and their payloads are enciphered either way
 unsigned int entryCountXorKey; // +0x04 T D: count ^ 0xFA261EFB
};
struct Pb2ArchiveDiskEntry {   // 64 bytes
 unsigned char nameField[56]; // +0x00 T D: CP932 name, NUL terminated, upper case; byte j of entry i is stored as b ^ ((3 * (j * i - 28)) & 0xFF). The bytes after the NUL are stale editor memory (non-zero in 02 / 03 / 04.dat) and must be kept for a byte-exact rebuild
 unsigned int sizeXorKey; // +0x38 T D: payload size ^ 0xFA261EFB
 unsigned int offset; // +0x3C T D: absolute file offset of the payload
};
struct Pb2BootIntegrityProbe {   // 8 bytes; 7 probes, immediates in the code, see the table in section 16
 unsigned int fileOffset; // +0x00 T: absolute offset read with File_Seek / File_Read (4 bytes) in PB_MainInitAndFrameLoop
 unsigned int expectedDword; // +0x04 T D: the dword the file must hold there, else _exit(0); every probed file must also be >= 100000000 bytes (0x5F5E100)
};

// ============================================================================================
// ============================================================================================
// 2. Character .DAT: header, patterns, frames, tables (stage-2 cipher: docs/formats/pb2k1.md section 2)
// ============================================================================================
struct Pb2DatHeader {   // 0x41C bytes at file start
 char magic[8]; // +0x00 T X: 時は来た (CP932 8E 9E 82 CD 97 88 82 BD); written by the editor, never compared by Load_DAT_File_And_Runtime_Decrypt
 unsigned int editorFlags08; // +0x08 X D: 0x10000 in 8 files, other values 0x18000/0x10100/0x1010100/0x1E000/0x1FF00/0x668BF000/0x668AF000 (uninitialised editor memory); never read
 unsigned int editorValue0C; // +0x0C X D: 0 / 0x100 / 0x200 / 0x600100 / 0xD / 0x1000100 / garbage; never read
 unsigned int checkValue; // +0x10 T D: must be 15 (Load_DAT_File_And_Runtime_Decrypt returns 0 otherwise)
 unsigned int patternAreaEnd; // +0x14 T D: byte size of header + pattern area = file offset of the sprite bank; the loader reads this many bytes as g_CharPatternData[slot]
 unsigned int spriteBankSize; // +0x18 T D: size of the sprite bank (the loader reads this + 0x4000 bytes at file offset patternAreaEnd)
 unsigned int patternOffset[256]; // +0x1C T D: absolute file offset of pattern i, 0xFFFFFFFF = absent (PB_ObjResolveFramePointers 0x424780: *(u32*)(charData + 4*pattern + 28)); first present = 0x41C, strictly ascending
};
struct Pb2PatternHeader {   // 20 bytes; followed by frameCount * 96 frames, then the box, AT, IF, EF tables in that order (each present table starts where the previous ended)
 unsigned char frameCount; // +0x00 T D: 1..100; PB_ObjCheckAttackVsFighters / frame stepping index the 96-byte frames that follow
 unsigned char moveInfo; // +0x01 T D: bit 7 = double-resolution pattern (PB_ObjIsPatternDoubleRes halves the boxes in PB_ObjBoxToWorld; 17 patterns); bit 6 = linear texture filter (PB_ObjIsPatternLinearFiltered, never set in the data); low 7 bits + 1 = KO severity written to obj.koMoveType (PB_ObjFighterStateAndHitReaction reads pattern+1 & 0x7F); values 0, 2, 6, 0x80
 unsigned char moveLevel; // +0x02 T D: move priority class (PB_ObjGetPatternMoveLevel reads pattern+2; PB_ObjRequestAttackFromInput lets a request through only when the current level <= the requested one); 0, 2, 3, 4 in the data
 unsigned char editorTag; // +0x03 X D: 0xC7 in 1300 of 1461 patterns (0x87 in 80, 0 in 80, 0x6B in 1); never read
 int boxTableOffset; // +0x04 T D: relative to the pattern start, -1 = none; -> obj.boxTable (PB_ObjResolveFramePointers)
 int atTableOffset; // +0x08 T D: -1 = none; PB_ObjCheckAttackVsFighters reads pattern + this + 26 * frame.atIndex
 int ifTableOffset; // +0x0C T D: -1 = none; -> obj.ifTable
 int efTableOffset; // +0x10 T D: -1 = none; -> obj.efTable
};
struct Pb2AnimFrame {   // 24 bytes = frame[0..23] (obj.animFrame)
 unsigned short spriteId; // +0x00 T D: index into the bank groupOffset[1000] table, i.e. a sprite GROUP (PB_QueueFighterSprite: bank[spriteId + 5] = group offset, -1 = not drawn); every sprite id of the shipped data is a present group
 short offsetX; // +0x02 T D: draw offset X (PB_DrawFighterSprite v34; the hit shake subtracts from it)
 short offsetY; // +0x04 T D: draw offset Y
 unsigned char duration; // +0x06 T D: ticks before advancing (PB_ObjRunActionScript: ++obj.frameTicks >= anim[6])
 enum Pb2DrawMode drawMode; // +0x07 T D: index into dword_461108[2 * mode + facing]; 8 shifts the origin by 100 (PB_ObjBoxToWorld)
 enum Pb2BlendMode blendMode; // +0x08 T D: non-zero = blended draw using alpha
 unsigned char alpha; // +0x09 T D: 0..255, used with blendMode != 0 (PB_DrawFighterSprite v33)
 enum Pb2AniFlag aniFlag; // +0x0A T D: PB_ObjRunActionScript end-of-frame switch (0 end -> pattern jumpTarget, 1 next, 2 jump to frame, 3 / 4 same + land -> pattern, 5 counted loop)
 unsigned char jumpTarget; // +0x0B T D: pattern (aniFlag 0) or frame (2, 4, 5)
 unsigned char landJumpTarget; // +0x0C T D: frame entered when the actor lands (PB_ObjRunActionScript landing branch, obj.frame = anim[12] when aniFlag 3/4)
 unsigned char drawPriority; // +0x0D T D: 0 none, 1 front, 2 back; PB_ObjEnterCurrentAction -> PB_ObjApplyDrawPriority 0x426010 (obj.drawDepth)
 unsigned short zoom; // +0x0E T D: 256 = 1.0, 0 = none (PB_DrawFighterSprite v26); data 0..~500
 unsigned char loopCount; // +0x10 T D: loaded into obj.loopCounter when non-zero (PB_ObjEnterCurrentAction anim[16]); aniFlag 5 repeat count
 unsigned char loopEndFrame; // +0x11 T D: frame entered when the aniFlag 5 counter reaches 0 (PB_ObjRunActionScript case 5 reads anim[17])
 unsigned char unused_12[6]; // +0x12 D: zero in all 14694 frames of the 16 files; no access in the register-tracking census (obj.animFrame loads)
};
struct Pb2StateFrame {   // 36 bytes = frame[24..59] (obj.stateFrame)
 unsigned char clearVelX; // +0x00 T D: zero velX / accelX / maxVelX (PB_ObjApplyFrameMotionFlags)
 unsigned char clearVelY; // +0x01 T D: zero velY / accelY
 unsigned char addVelX; // +0x02 T D: add velX / accelX / maxVelX facing-aware
 unsigned char addVelY; // +0x03 T D: add velY / accelY
 short velX; // +0x04 T D: added to obj.velX when addVelX
 short velY; // +0x06 T D
 short accelX; // +0x08 T D
 short accelY; // +0x0A T D
 enum Pb2Stance stance; // +0x0C T D: -> obj.stanceCopy; read by ~14 functions (air physics, cancel rules)
 enum Pb2CancelPermission normalCancel; // +0x0D T D: class 0 command moves (PB_SlotTryStartCommandMove v4[13])
 enum Pb2CancelPermission specialCancel; // +0x0E T D: class 1 / 2 command moves (v4[14])
 unsigned char attackHitCount; // +0x0F T D: -> obj.attackHitsLeft on frame entry (PB_ObjEnterCurrentAction); attack boxes ignored when 0
 unsigned char canAct; // +0x10 T D: 1 = actor may act (PB_ObjRunActionScript, PB_ObjDispatchInputByStance: stance | canAct << 4)
 unsigned char unused_11[3]; // +0x11 D: zero in all frames; no reader in the register-tracking census
 enum Pb2FrameFlags1 flags1; // +0x14 T D: bit0 carry velocity as inertia, bit1 clear inertia, bit31 motion only on first entry (PB_ObjApplyFrameMotionFlags, PB_ObjIntegrateMotion)
 enum Pb2FrameFlags2 flags2; // +0x18 T D: see Pb2FrameFlags2; only bytes 0 and 3 are non-zero in the data
 short maxVelX; // +0x1C T D: added to obj.maxVelX with addVelX
 unsigned char unused_1E[6]; // +0x1E D: zero in all frames; no reader in the register-tracking census
};
struct Pb2FrameRecord {   // 96 bytes (PB_ObjResolveFramePointers: frame = pattern + 20 + 96 * obj.frame)
 struct Pb2AnimFrame anim; // +0x00 T D: obj.animFrame
 struct Pb2StateFrame state; // +0x18 T D: obj.stateFrame = frame + 24
 unsigned char atIndex; // +0x3C T D: index into the AT table, 0xFF = none (PB_ObjCheckAttackVsFighters reads the byte at frame+60); always < table size
 unsigned char atIndexHighByte; // +0x3D X D: 0 / 1 / 0xFF / editor garbage (119 distinct values), never read
 short ifIndex[3]; // +0x3E T D: IF table indices, -1 = empty (PB_ObjRunFrameEventsForPhase: i = 62; i < 68; i += 2)
 short efIndex[4]; // +0x44 T D: EF table indices, -1 = empty (PB_ObjRunFrameEntryEffects: i = 68; i < 76; i += 2)
 short boxIndex[10]; // +0x4C T D: obj.boxIndexBlock; indices into the box table, -1 = empty: [0] pushbox, [1..3] hurt boxes, [4] probe/grab box, [5] special, [6] reflect/clash box, [7] projectile hurt box, [8..9] attack boxes
};
struct Pb2Box {   // 4 bytes (GOF1/MBR use 8-byte 16-bit boxes)
 unsigned char x1; // +0x00 T D: frame space, origin (128, 224); PB_ObjBoxToWorld transforms it (byte reads at box + 4 * index + 0..3)
 unsigned char y1; // +0x01 T D
 unsigned char x2; // +0x02 T D
 unsigned char y2; // +0x03 T D
};
struct Pb2AtRecord {   // 26 bytes; pattern + atTableOffset + 26 * frame.atIndex
 unsigned char hitReaction; // +0x00 T D: row of the hit-reaction tables (byte_461E68 / byte_461EE0, PB_ObjFighterStateAndHitReaction)
 unsigned char hitstopLevel; // +0x01 T D: index into byte_161E8D8 (g_HitstopFramesByLevel, VECTOR.TXT section 3) -> hit-stop and screen shake
 unsigned char hitSparkClass; // +0x02 T D: < 14 selects the hit spark (word_461DB0 rows of 6 words) and the default hit sound
 unsigned char attackKind; // +0x03 T D: byte_461D5C / byte_461D3C indexed (PB_ObjCheckAttackVsFighters); switch in PB_ObjFighterStateAndHitReaction
 unsigned char hitSoundOverride; // +0x04 T D: PB_QueueSoundPlay when non-zero
 enum Pb2AtFlagsA flagsA; // +0x05 T D
 short damage; // +0x06 T D: PB_Damage_ScaleByComboAndLife input
 short meterGain; // +0x08 T D: PB_AddSuperMeter(attacker, +8), victim >> 2 (guarded >> 3)
 short guardBreakValue; // +0x0A T D: added to the victim guardGauge (HitReaction)
 unsigned char unused_0C[3]; // +0x0C D: zero in all 1380 records
 enum Pb2StatusEffect statusEffect; // +0x0F T D: -> obj.tintMode when non-zero
 enum Pb2AtFlagsB flagsB; // +0x10 T D
 unsigned char unused_11; // +0x11 D: zero in all records
 enum Pb2AtHitClass hitClass; // +0x12 T D: PB_ObjCheckAttackVsObjects reads word +0x12; guard-height check
 unsigned char unused_14[6]; // +0x14 D: zero in all records
};
struct Pb2IfRecord {   // 20 bytes; pattern + ifTableOffset + 20 * ifIndex (GOF1 has 28-byte records, same types 1..28 + 50)
 enum Pb2IfType type; // +0x00 T D: switch in PB_ObjRunFrameIfRecord 0x428160
 unsigned char unused_01[3]; // +0x01 D: zero in all 3072 records
 int param[4]; // +0x04 T D: type dependent (docs/formats/pb2k1.md section 6)
};
struct Pb2EfRecord {   // 12 bytes; pattern + efTableOffset + 12 * efIndex
 enum Pb2EfType type; // +0x00 T D: switch in PB_ObjRunFrameEntryEffects
 unsigned char subType; // +0x01 T D: child pattern / effect id / actor-op id
 short arg[5]; // +0x02 T D: five signed words, meaning per type (docs/formats/pb2k1.md section 7)
};

// ============================================================================================
// 3. Sprite bank (second stage-2 section, file offset patternAreaEnd) - Parse_Character_DAT_Data 0x42C560
// ============================================================================================
struct Pb2SpriteRecord {   // 14 bytes; sprite table at bank + 12220 (the loader reads each record from +4: width, height, texU, texV, texturePage; the draw reads offsetX / offsetY at +0 / +2)
 short offsetX; // +0x00 T D: draw offset of this piece inside the group image (PB_QueueFighterSprite *v22 + frame offset); 0..848
 short offsetY; // +0x02 T D: 0..848
 short width; // +0x04 T D: Parse_Character_DAT_Data: the group pixel data holds width * height 8-bit palette indices (3 bytes per pixel in the 255 mode)
 short height; // +0x06 T D
 short texU; // +0x08 T D: placement (x) inside the 256 x 256 texture page
 short texV; // +0x0A T D: placement (y) inside the texture page
 short texturePage; // +0x0C T D: page 0..49 (the loader rewrites it to 50 * slot + 2 + page; > 50 pages = message box)
};
struct Pb2SpriteGroupHeader {   // 52 bytes at bank + groupOffset[i]; pixel data follows (sum of width * height of the group sprites, consecutively)
 char name[12]; // +0x00 T X D: e.g. ASA0000.TIM; not read by the loader
 unsigned char editorGarbage[24]; // +0x0C X D: stale editor memory after the name (S\TEMP\winbootdi ... in group 0)
 unsigned short rect[4]; // +0x24 X D: x1, y1, x2, y2 (D: 96..175 / 128..207 / 159.. / 223..); not read by the loader
 unsigned int firstSprite; // +0x2C T D: index of the first sprite of the group in the sprite table (PB_QueueFighterSprite draws spriteCount consecutive sprites from it)
 unsigned short spriteCount; // +0x30 T D: Parse_Character_DAT_Data reads 16 bits
 unsigned short unreferenced_32; // +0x32 X D: 0 except 36 of 5259 groups
};
struct Pb2SpriteBankHeader {   // sprite bank header (12220 bytes); the 14-byte Pb2SpriteRecord table follows (spriteCount records, ending exactly at the first group), then the groups
 unsigned char editorGarbage[16]; // +0x00 X D: key remnant (82 C9 82 E5 81 48 ...), never read
 unsigned int is24BitFlag; // +0x10 T D: == 255 selects the 24-bit BGR pixel path, else 8-bit palettised (always 0 in the shipped files)
 unsigned int groupOffset[1000]; // +0x14 T D: bank-relative offset of each sprite group, 0xFFFFFFFF = none
 unsigned int palette[2048]; // +0xFB4 T D: 8 palettes of 256 entries, 4 bytes {R, G, B, 1} (channel order from PB_BlitPalettedToSurface: byte 0 -> g_PixelTableChannel0 = red; the 4th byte is always 1 and never read); the loader copies palette [paletteIndex << 10] and forces entry 0 to 0 (transparent)
 unsigned int spriteCount; // +0x2FB4 D: total sprites (== sum of group counts)
 unsigned int groupRegionSize; // +0x2FB8 D: bytes from the first group to the end of the bank (spriteBankSize - firstGroupOffset)
};
struct Pb2CharDatHead {   // what g_CharPatternData[slot] points to: the first patternAreaEnd bytes of the file; the patterns follow the header (offsets are file offsets)
 struct Pb2DatHeader header; // +0x00 T D
};

// ============================================================================================
// 4. Satellite files: <CHAR>_C.CT, <CHAR>.WMT, CHARSEL.CT, <CHAR>COM.TXT / *.CPF
// ============================================================================================
struct Pb2CommandMove {   // 42 bytes; record i of the CT file = Pb2FighterSlot.commands[i]
 unsigned char commandId; // +0x00 T D: own index, 0xFF = unused slot (PB_SlotTryStartCommandMove: *(u8*)(slot + 640 + 42 * i) == 0xFF)
 unsigned char unused_01; // +0x01 D: zero in all 1400 shipped records
 unsigned char sequence[32]; // +0x02 T D: input token string 0xFF terminated: 0..9 direction, 0x41..0x46 buttons A..F, 0x2B joiner, 0x54 hold; parsed backwards (PB_SlotMatchCommandSequence); bytes 16..31 (record +18..+33) are cleared by PB_InitFighterSlotForRound
 unsigned char targetPattern; // +0x22 T D: pattern requested when the command fires
 enum Pb2CommandMoveClass moveClass; // +0x23 T D
 unsigned char meterCostLevels; // +0x24 T D: super meter levels needed (obj.superMeter / 10000)
 unsigned char requestParam; // +0x25 T D: -> obj.requestParam; 0 in all data
 unsigned char counterRequirement; // +0x26 T D: tens digit = counter index (obj.tobiCount), units = amount
 unsigned char repeatLimit; // +0x27 T D: refused when it is <= obj.dashCount-like counter at obj+232 (PB_SlotTryStartCommandMove: *(slot+679) <= *(slot+236)); 0 / 1 / 2 in the data
 enum Pb2CommandFlags flags; // +0x28 T D
 enum Pb2CommandFlags2 flags2; // +0x29 T D
};
struct Pb2CtHeader {   // 28 bytes at CT file +4204 = Pb2FighterSlot.ct
 unsigned char maxAirActions; // +0x00 T D: air dashes / air jumps allowed (PB_ObjDispatchInputByStance compares obj.airActionState & 0x7F); 1, IKUMI 2
 unsigned char maxSuperLevels; // +0x01 T D: super meter cap = 10000 * this (PB_AddSuperMeter, EF type 6 op 4); 9 in all files
 unsigned char recoveryStyle; // +0x02 T D: PB_ObjRunActionScript switch on it (0 / 1 / 2); 1 in all files
 unsigned char unreferenced_03; // +0x03 D: zero in all 13 CT files, no reader
 unsigned char unreferenced_04; // +0x04 D: zero, no reader
 enum Pb2CtFlags flags; // +0x05 T D
 unsigned char unreferenced_06[2]; // +0x06 D: zero, no reader
 float knockbackScale; // +0x08 T D: PB_ObjStartKnockback multiplies the vertical knockback (0 = 1.0); 0.9 .. 1.3 per character
 float damageScaleByLifeTier[4]; // +0x0C T D: PB_Damage_ScaleByComboAndLife picks it by the victim life tier (2850 / 5700 / 8550); 0.9 in all files
};
struct Pb2CtFile {   // 4232 bytes: <CHAR>_C.CT (and an identical <CHAR>.CCT that the game never loads)
 unsigned int commandCount; // +0x00 T D: number of defined commands (PB_SlotLoadCommandTable stores it at slot+636)
 struct Pb2CommandMove commands[100]; // +0x04 T D: copied (0x1068 bytes) to slot+640
 struct Pb2CtHeader ct; // +0x106C T D: copied (0x1C bytes) to slot+608
};
struct Pb2WmtRecord {   // 154 bytes
 unsigned char opponentCharId; // +0x00 T D: chosen when the loser character id equals it; 0xFF = any (PB_PickWinQuote 0x40DF20)
 unsigned char unreferenced_01; // +0x01 D: zero
 unsigned short chanceDivisor; // +0x02 T D: 0 = always, n = accepted with probability 1/n (rand() % n == 0)
 char text[150]; // +0x04 T D: CP932 win quote, NUL padded (drawn by PB_SceneWinScreenTick with the record + 4 pointer)
};
struct Pb2CharTableEntry {   // 168 bytes; g_CharTable 0x177CDD0 (100 entries)
 char name[32]; // +0x00 T D: base name (sel_%s.bmp, win_%s.bmp, %s.wmt); also the CSS portrait key
 char datFile[32]; // +0x20 T D: <name>.dat, loaded with Load_DAT_File_And_Runtime_Decrypt (".\data\%s")
 char ctFile[32]; // +0x40 T D: <name>_c.txt, the extension is replaced by .ct (PB_SlotLoadCommandTable)
 char comFile[32]; // +0x60 T D: <name>com.txt, loaded as a 56048-byte CPU script (PB_SlotLoadCpuScript)
 unsigned int tableIndex; // +0x80 T: written by CSS_LoadCharselTxtGrid at load (the file value is 0 / garbage)
 unsigned int charFileId; // +0x84 T D: g_CharFileIdToTableIndex[charFileId] = table index; 34 and 20 are special-cased in PB_SetupFighterSlotFromCharTable
 unsigned int homeStage; // +0x88 T D: g_css_home_stage_of_char / default stage (sub_40F270)
 unsigned int selectVoiceId; // +0x8C T D: PB_QueueSoundPlay(selectVoiceId + 360) on selection
 unsigned int unlockMask; // +0x90 T D: hidden while (unlockMask & g_UnlockMask) == 0 and unlockMask != 0; 1 hides always (CSS_LoadCharselTxtGrid)
 unsigned int secondPage; // +0x94 T D: 1 = CSS grid page 2 (row + 10)
 unsigned int gridCell; // +0x98 T D: 10 * row + column on the CSS grid
 int portraitX; // +0x9C T D: CSS_DrawPanelPortraitAndInfo
 int portraitY; // +0xA0 T D
 unsigned int unreferenced_A4; // +0xA4 D: zero
};
struct Pb2CharSelFile {   // CHARSEL.CT: 4 + 168 * count
 unsigned int count; // +0x00 T D
 struct Pb2CharTableEntry entries[13]; // +0x04 T D: enciphered with aAiN (XOR_Decrypt_WithString from entries[0], p-relative); 13 entries in 01.dat, 14 in 01p/02p
};
struct Pb2CpfStep {   // 52 bytes
 unsigned char inputCode; // +0x00 T D: 0 neutral, 1..6 direction 6 4 2 8 9 7, 7..10 button 1 2 4 8 (dir 0), 11..14 down + button 1 2 4 8, 15 command move (PB_FighterCpuAiStep helper sub_404080)
 unsigned char unreferenced_01; // +0x01 D
 unsigned char commandMoveId; // +0x02 T D: command move tried when inputCode == 15
 unsigned char unreferenced_03; // +0x03 D
 short duration; // +0x04 T D: frames the step is held (-> obj.aiActionTimer-like counter slot+58)
 short durationRandom; // +0x06 T D: > 0: rand() % value is added
 unsigned char endOfScript; // +0x08 T D: == 1 ends the script (obj.aiScriptIndex = -1)
 unsigned char editorFlag09; // +0x09 X D: 0 / 1 (159 of 14000 steps); never read
 unsigned char unreferenced_0A[32]; // +0x0A D: zero in all 14000 steps
 unsigned char flags; // +0x2A T D: bit0 | bit1 advance early after a hit, bit2 sets obj.cpuCommandReady
 unsigned char commandDirection; // +0x2B T D: direction written to obj.inputDir when the command move starts (inputCode 15)
 unsigned char unreferenced_2C[8]; // +0x2C D: zero in all steps
};
struct Pb2CpfRow {   // 160 bytes; row = stance(3) * distance bucket(4) * opponent state: base + 208 + 160 * (opponentStance + 3 * (bucket + 4 * state))
 int scriptIndex[20]; // +0x00 T D: script chosen by the weighted pick (sub_404040: subtract weights from a 0..99 roll)
 int weight[20]; // +0x50 T D: +0x50, subtracted from the roll in order; the first entry that brings it to <= 0 wins, -1 when none
};
struct Pb2CpfScript {   // 1040 bytes
 struct Pb2CpfStep steps[20]; // +0x00 T D
};
struct Pb2CpfFile {   // 56048 bytes loaded to g_CpuScriptData + 56048 * slot (sub_403FE0 -> PB_Archive_XOR_Decrypt_With_Filename)
 unsigned char guardChancePercent; // +0x00 T D: rand() % 100 < this sets obj.cpuGuardRequest (PB_FighterCpuAiStep *v8)
 unsigned char reactChancePercent; // +0x01 T D: rand() % 100 >= this skips the reaction table (v8[1])
 unsigned char evadeChancePercentAndReversalA; // +0x02 T D: used as a percent (tech / evade, v8[2]) and as command move id #1 of the reversal list
 unsigned char reversalCommandB; // +0x03 T D: command move id #2 (v8[3])
 unsigned char reversalCommandC; // +0x04 T D: command move id #3 (v8[4])
 unsigned char unreferenced_05[3]; // +0x05 D
 unsigned char unreferencedValue08; // +0x08 X D: 0x5A in 15 of 18 files, never read
 unsigned char unreferenced_09[199]; // +0x09 D
 struct Pb2CpfRow rows[24]; // +0xD0 T D: +0xD0
 struct Pb2CpfScript scripts[50]; // +0xFD0 T D: +0xFD0 (PB_FighterCpuAiStep: base + 4048 + 1040 * script + 52 * step)
};

// ============================================================================================
// 5. Runtime: input rings, character slot, object slot
// ============================================================================================
struct Pb2InputRing {   // 512 bytes
 unsigned int state[64]; // +0x00 T: (direction << 24) | buttons for each distinct input state, newest first (PB_UpdateFighterInputHistoryRings)
 unsigned int age[64]; // +0x100 T: frames spent in state[i], capped at 1000000; PB_SlotInvalidateInputHistory 0x427730 sets 1000000
};
struct Pb2IntRect { int x1; int y1; int x2; int y2; };   // 16 bytes (PB_RectOverlap / PB_RectIntersection)
struct Pb2Object {   // 0x25C = 604 bytes; the object of a fighter sits at Pb2FighterSlot+4, of a spawned object at Pb2ObjectSlot+4 (PB_InitChildObject memsets 0x25C)
 unsigned char slotIndex; // +0x00 T: fighter slot / owner player index 0..3; EF type 9 sound bank arg0 + 50 * (slotIndex + 8); children copy it (PB_InitChildObject)
 unsigned char characterId; // +0x01 T: charFileId (PB_SetupFighterSlotFromCharTable); PB_ObjRunActionScript compares it with 34
 enum Pb2ControlType controlType; // +0x02 T: 0 human, 1 CPU (PB_SetupFighterSlotFromCharTable a5 == 1)
 unsigned char unreferenced_03; // +0x03 U: no access
 unsigned char paletteIndex; // +0x04 T: palette chosen at character select (PB_SetupFighterSlotFromCharTable a4)
 enum Pb2ObjKind objKind; // +0x05 T: see Pb2ObjKind
 unsigned char pattern; // +0x06 T: current pattern (action) id; PB_ObjResolveFramePointers
 unsigned char frame; // +0x07 T: current frame inside the pattern
 unsigned short frameTicks; // +0x08 T: ticks on the current frame, compared with anim.duration (PB_ObjRunActionScript)
 enum Pb2RunState ifRunState; // +0x0A T: IF-table run gate (PB_ObjEnterCurrentAction sets 1, PB_ObjOnActionChanged 2)
 enum Pb2RunState efRunState; // +0x0B T: EF-table run gate; PB_ObjRunFrameEntryEffects clears it after running
 unsigned char airReactionActive; // +0x0C T: 1 while a launch/air hit stage (hitStage >= 240) runs (PB_ObjApplyFrameMotionFlags, PB_ObjRunActionScript)
 unsigned char animationEnded; // +0x0D T: set when the animation reached its last frame (aniFlag 0 branch); IF 2 reads it
 unsigned char unreferenced_0E; // +0x0E U: no access
 unsigned char loopCounter; // +0x0F T: anim.loopCount is loaded here; IF 9 / 10 set / test it; aniFlag 2 / 5 consume it
 enum Pb2ObjFlagsA spawnFlagsA; // +0x10 T: child flags from EF arg2 (PB_InitChildObject)
 unsigned char unreferenced_11; // +0x11 U: no access
 enum Pb2ObjFlagsB spawnFlagsB; // +0x12 T: child flags from EF arg3
 unsigned char unreferenced_13; // +0x13 U: no access
 unsigned char homingMode; // +0x14 T: EF arg4 low byte; PB_ObjUpdateHomingObject (1 / 2)
 unsigned char homingBaseAction; // +0x15 T: EF subType = first of the homing action family (action base + 1..3)
 unsigned char unreferenced_16[2]; // +0x16 U: written 0 by PB_InitChildObject only
 unsigned short homingTick; // +0x18 T: counts up while homingTimeout != 0 (PB_ObjUpdateHomingObject)
 unsigned short homingTimeout; // +0x1A T: EF type 30 sub 0 arg2
 short homingOffsetX; // +0x1C T: target offset X, 128 = centred (EF arg0 / EF type 30 arg0)
 short homingOffsetY; // +0x1E T: target offset Y, 224 = baseline
 unsigned short homingCooldown; // +0x20 T: set to 40 after a hit (PB_ObjUpdateHomingObject)
 short homingParam[10]; // +0x22 T: EF type 30 sub 1: word[arg0] = arg1; [0] = speed
 unsigned short aiActionTimer; // +0x36 T: CPU AI frames left in the running step (sub_404080 sets it from step.duration)
 unsigned short aiReactCooldown; // +0x38 T: CPU AI frames before the next decision (PB_FighterCpuAiStep)
 short aiScriptIndex; // +0x3A T: running CPU script, -1 = none
 short aiScriptStep; // +0x3C T: step in the script
 unsigned char cpuCommandReady; // +0x3E T: set by CPU script step flag 4, consumed by IF 11 (PB_ObjRunFrameIfRecord)
 unsigned char unreferenced_3F; // +0x3F U: no access
 int life; // +0x40 T: hit points, 11400 at round start (PB_InitFighterSlotForRound); <= 0 is KO
 int lifeDisplay; // +0x44 T: HUD life bar follower, 11400 at round start (PB_DrawLifeMeterGuardHud)
 int superMeter; // +0x48 T: super meter, 10000 per level, capped at 10000 * ct.maxSuperLevels (PB_AddSuperMeter); preserved across rounds
 unsigned char superLevelCrossMark; // +0x4C T: PB_AddSuperMeter sets 10 when a 10000 boundary is crossed (read by the HUD)
 unsigned char unreferenced_4D[3]; // +0x4D U: no access
 int reserveGauge; // +0x50 T: third gauge 0..10000 (EF type 6 op 4 adds arg2, PB_ObjAddReserveFromHit); preserved across rounds
 int unreferenced_54; // +0x54 U: no access
 int reservePending; // +0x58 T: reserve gain waiting to be banked (sub_4254F0 adds it to reserveGauge, PB_ObjAddReserveFromHit)
 unsigned short reserveHoldTimer; // +0x5C T: 600-frame countdown while the reserve gauge is full (PB_ObjTickReserveGauge)
 unsigned char superMoveState; // +0x5E T: reserve gauge mode: 0 charging, 1 full (hold 600 frames), 2 draining after a super (PB_ObjTickReserveGauge); HitReaction halves guard damage while the owner is in mode 2 (PB_ObjOnActionChanged sets 2 when superMoveFlag)
 unsigned char superMoveFlag; // +0x5F T: armed by PB_SlotTryStartCommandMove when a command move is accepted, consumed by PB_ObjOnActionChanged
 unsigned char clearHistoryOnAction; // +0x60 T: command flag 0x10 (PB_SlotTryStartCommandMove)
 unsigned char guardRegenDelay; // +0x61 T: frames before the guard gauge decays (45 / 60 / 90 set by HitReaction, 120 by guard cancel)
 unsigned char unreferenced_62[2]; // +0x62 U: no access
 int guardGauge; // +0x64 T: guard gauge, guard break above 10000 (PB_ObjFighterStateAndHitReaction adds AT.guardBreakValue); DrawFighterSprite flashes above 8000
 int unreferenced_68; // +0x68 U: no access
 int posX; // +0x6C T: world X, 1/256 pixel units (PB_InitChildObject adds (offset << 8))
 int posY; // +0x70 T: world Y, 0 = ground, negative = up (PB_ObjIntegrateMotion lands at > 0)
 int unreferenced_74; // +0x74 U: no access
 int posXAfterIntegrate; // +0x78 T: posX written when PB_ObjIntegrateMotion returns; never read
 int posXAtTickStart; // +0x7C T: posX before the tick (PB_ObjIntegrateMotion)
 int posYAtTickStart; // +0x80 T: posY before the tick
 int velX; // +0x84 T: velocity X (PB_ObjApplyFrameMotionFlags adds state.velX)
 int velY; // +0x88 T
 short accelX; // +0x8C T
 short accelY; // +0x8E T: gravity
 short maxVelX; // +0x90 T: X speed clamp (PB_ObjIntegrateMotion)
 unsigned char unreferenced_92[2]; // +0x92 U: no access
 int knockVelX; // +0x94 T: knockback velocity X (PB_ObjStartKnockback)
 int knockVelY; // +0x98 T
 short knockAccelX; // +0x9C T
 short knockAccelY; // +0x9E T
 int inertiaX; // +0xA0 T: carried inertia added to posX each tick, decays by inertiaDecay
 int carriedVelX; // +0xA4 T: velX saved when state.flags1 bit 0 is set
 short inertiaDecay; // +0xA8 T: carriedVelX / 16
 unsigned short carryPattern; // +0xAA T: pattern in which carriedVelX was saved
 short grabOffsetX; // +0xAC T: position of a grabbed actor relative to the grabber, added by PB_ObjBoxToWorld when grabState != 0 (EF type 4 writes it)
 short grabOffsetY; // +0xAE T
 unsigned char clearInertiaRequest; // +0xB0 T: set by PB_ObjStartKnockback
 enum Pb2AttackResultFlags attackResult; // +0xB1 T: result of this actor attack on its victim (reset by PB_ObjOnActionChanged); tested by IF 3 / 6, PB_ObjRequestAttackFromInput
 enum Pb2AttackResultFlags ownerAttackResult; // +0xB2 T: copy of a child attack result written into its owner
 unsigned char drawDepth; // +0xB3 T: draw order key 0x7D..0x80 (PB_ObjApplyDrawPriority); 0x80 at round start
 unsigned char drawModeVariant; // +0xB4 T: EF type 4 sets it (frame / 1000); PB_DrawFighterSprite
 unsigned char activeMoveMeterCost; // +0xB5 T: meter levels of the running command move (slot + 185 in PB_SlotTryStartCommandMove)
 unsigned char hitstopFrames; // +0xB6 T: remaining hit-stop (PB_ObjRunActionScript / PB_ObjIntegrateMotion skip while non-zero)
 unsigned char hitstopTrailing; // +0xB7 T: set to 1 by HitReaction; counted down with hitstopFrames
 unsigned char contactMark; // +0xB8 T: set to 1 by PB_ObjCheckAttackVsFighters when an attack box touched something
 unsigned char grabState; // +0xB9 T: 0 free, 1 held by a fighter, 2 held by a child object (PB_ObjTryGrabFighter)
 unsigned char inputLock; // +0xBA T: EF type 6 op 9; blocks input dispatch (sub_437420)
 unsigned char freezeTimerA; // +0xBB T: EF type 6 op 2 arg0; counted down by PB_ObjRunActionScript
 unsigned char freezeTimerB; // +0xBC T: EF type 6 op 2 arg1
 unsigned char koFlag; // +0xBD T: set at KO / knock-out stage (PB_ObjRunActionScript, PB_ObjResetFighterSlotKeepingDataPointers)
 unsigned char cameraExcludeFlag; // +0xBE U: PB_ObjRunActionScript writes it, no reader
 unsigned char attackHitsLeft; // +0xBF T: hits the current attack may still land (state.attackHitCount); PB_ObjCheckAttackVs* decrement
 unsigned char guardState; // +0xC0 T: non-zero while guarding (HitReaction sets 1; > 4 gates guard-cancel moves)
 unsigned char cpuGuardRequest; // +0xC1 T: written by PB_FighterCpuAiStep (guard chance)
 unsigned char turnaroundWindow; // +0xC2 T: 8 after a facing flip (PB_ObjDispatchInputByStance), counted down
 unsigned char recoverMode; // +0xC3 T: wake-up / recovery selector set by HitReaction (0, 1, 25)
 unsigned char techLatch; // +0xC4 U: PB_ObjTryEvasiveRoll latch (set when PB_ObjIsSlammingDownward fires)
 unsigned char bounceCount; // +0xC5 T: wall / ground bounces taken (PB_ObjRunActionScript wraps > 2 to -16)
 unsigned char recoveryTimer; // +0xC6 T: frames since the last hit (PB_ObjRunActionScript increments, HitReaction resets)
 unsigned char throwTechSeen; // +0xC7 T: set on the grabber when a throw tech succeeds (PB_ObjTryThrowTech)
 unsigned char stageEdgeSide; // +0xC8 T: 1 / 2 = pushed past the right / left stage edge (PB_ObjStageEdgeClamp)
 unsigned char airComboStarterLatch; // +0xC9 T: set by HitReaction when an AT flagsB 0x10 hit lands
 unsigned char throwTechAllowed; // +0xCA T: HitReaction sets !(AT.flagsB & 0x20); PB_ObjTryThrowTech requires it
 unsigned char inputBlockFlag; // +0xCB T: PB_ObjRunActionScript clears it, PB_ObjTryEvasiveRoll sets it
 unsigned char hitQueueClear; // +0xCC T: PB_ObjCheckAttackVs* set 1 to discard the last tick hit queue
 unsigned char hitQueueCount; // +0xCD T: valid hitAt / hitRect / hitAttacker entries, max 8
 enum Pb2StatusEffect tintMode; // +0xCE T: AT.statusEffect (PB_DrawFighterSprite tint)
 unsigned char shakeTimer; // +0xCF T: draw shake after a hit (PB_DrawFighterSprite, counted down by PB_ObjRunActionScript)
 unsigned char hitStage; // +0xD0 T: 0 not in hit-stun; 1..239 remaining stun frames; >= 240 launch / air stages (PB_ObjApplyFrameMotionFlags)
 unsigned char juggleState; // +0xD1 T: non-zero while the victim is in an air juggle (PB_ObjStartKnockback, HitReaction)
 unsigned char hitReactionId; // +0xD2 T: reaction index of the last hit (PB_ObjStartKnockback a2); PB_ObjCanWallBounce compares 9 / 13 / 27
 unsigned char hitRecoverFrames; // +0xD3 T: recovery frames computed from hitStage and hitsTakenInCombo by HitReaction
 unsigned char unreferenced_D4; // +0xD4 U: no access
 unsigned char invulnFlags; // +0xD5 T: non-zero = takes no damage (HitReaction); EF type 6 op 255 sets 0x10
 unsigned char scriptFlag; // +0xD6 T: EF type 6 op 254 sets 1; no reader
 unsigned char koMoveType; // +0xD7 T: (attacker pattern moveInfo & 0x7F) + 1 when this actor is KO by it (HitReaction)
 unsigned char airActionState; // +0xD8 T: bit 7 = neutral seen, bits 0..6 = air actions used (PB_ObjDispatchInputByStance vs ct.maxAirActions)
 unsigned char wallBounceLatch; // +0xD9 T: set by PB_ObjResolvePushboxCollision when the actor was thrown into the screen edge
 unsigned char guardRecoveryFlag; // +0xDA T: 1 after a guard-cancel / early guard (PB_ObjStartKnockback, HitReaction)
 unsigned char unused_DB; // +0xDB U proven: no instruction in the binary has displacement 219 or 223 (slot-relative)
 unsigned short mashCounter; // +0xDC T: button presses while in hit-stun (PB_ObjMashRecoverOrGuardPush); PB_Damage_ScaleByComboAndLife reduces damage by 0.3% each
 signed char tobiCount[10]; // +0xDE T: ten projectile counters; IF 2 / 3 / 17 / 24 / EF 6 op 100 / 101 index them by tens digit
 unsigned char dashCount; // +0xE8 T: EF 6 op 102 / 103 add / subtract; PB_ObjStartEvade sets 100
 unsigned char unused_E9; // +0xE9 U proven: no instruction in the binary has displacement 233 or 237
 short paramVar[10]; // +0xEA T: ten word variables (IF 25, EF 6 op 105)
 unsigned char trailMode; // +0xFE T: ghost trail colour mode + 1 (EF 6 op 0; PB_DrawFighterSprite spawns a ghost every 7th tick)
 unsigned char unreferenced_FF; // +0xFF U: no access
 unsigned short decorLifeTotal; // +0x100 U: ghost object total life
 unsigned short decorFlags; // +0x102 U
 short decorZoomGrowX; // +0x104 U
 short decorZoomGrowY; // +0x106 U
 unsigned char afterimageMode; // +0x108 T: afterimage config id + 1 (EF 6 op 3)
 unsigned char afterimageSteps; // +0x109 T: <= 119 (EF 6 op 3 arg1)
 unsigned char afterimageSpacing; // +0x10A T: EF 6 op 3 arg2
 unsigned char afterimageRestart; // +0x10B T: set 1 when the afterimage mode changes; never read
 unsigned char justGuardActive; // +0x10C T: set when a just-guard absorbs a hit (HitReaction); PB_ObjStartKnockback pushes the attacker back
 unsigned char unused_10D; // +0x10D U proven: no instruction in the binary has displacement 269 or 273
 unsigned short locomotionUsedMask; // +0x10E T: bit n = pattern n (1..9) used since the last landing (PB_ObjRunActionScript); PB_ObjRequestAttackFromInput tests it
 unsigned char commandUsedMask[14]; // +0x110 T: bit i = command move i used in this chain (PB_ObjRunActionScript sets it, PB_SlotTryStartCommandMove refuses repeats)
 unsigned char pendingCommandId; // +0x11E T: command move accepted this tick, 0xFF = none
 unsigned char startedByCommandId; // +0x11F T: command id that started the current action (PB_ObjRunActionScript); never read
 short hitsTakenInCombo; // +0x120 T: consecutive hits taken (HitReaction increments)
 unsigned short attackHitsConnected; // +0x122 T: hits landed by the current action (PB_ObjCheckAttackVs* increment; IF 17 compares)
 unsigned char evadeDir; // +0x124 T: 1 forward / 2 back double-tap evade (PB_SlotPollEvadeInput)
 unsigned char evadeTimer; // +0x125 U: evade window counter
 unsigned short knockoutStage; // +0x126 T: 0 normal; counts to 60 then launches; 4095 = flying (PB_ObjRunActionScript)
 unsigned char flashMode; // +0x128 T: colour flash mode (PB_DrawFighterSprite switch 0..4)
 unsigned char flashTimer; // +0x129 T: remaining flash frames (counted down by PB_ObjRunActionScript)
 unsigned char requestParam; // +0x12A T: argument a5 of PB_ObjRequestAction
 unsigned char counterWindowState; // +0x12B T: counter-hit window (PB_ObjRunActionScript counts it down; HitReaction reads it)
 short launchVelX; // +0x12C T: stage-2 launch velocity X (PB_ObjStartKnockback fills it from VECTOR.TXT, PB_ObjApplyFrameMotionFlags consumes it)
 short launchVelY; // +0x12E T
 short launchAccelX; // +0x130 T
 short launchAccelY; // +0x132 T
 struct Pb2AtRecord * hitAt[8]; // +0x134 T: AT record of each hit received this tick (PB_ObjCheckAttackVsFighters)
 struct Pb2IntRect hitRect[8]; // +0x154 T: world rectangle of each overlap (hit spark position)
 struct Pb2Object * hitAttacker[8]; // +0x1D4 T: attacking object of each hit
 struct Pb2Object * lastAttacker; // +0x1F4 T: attacker of the last accepted hit (HitReaction); PB_ObjResolvePushboxCollision
 struct Pb2Object * grabbedBy; // +0x1F8 T: grabber while grabState != 0 (PB_ObjTryGrabFighter: victim + 504 = grabber)
 struct Pb2Object * ownerFighter; // +0x1FC T: root fighter of a child object (PB_ObjGetOwnerFighter); 0 for fighters
 struct Pb2Object * linkedActor; // +0x200 T: parent object of a child / grab victim of a fighter
 enum Pb2InputDirection inputDir; // +0x204 T: numpad direction, facing-corrected (PB_ReadBattleInputForPlayer; recorded by PB_ReplayRecordOrPlaybackFighterInput)
 unsigned char unreferenced_205[3]; // +0x205 U: no access
 enum Pb2InputButtonFlags inputButtons; // +0x208 T: buttons held (bits 0..5) and pressed (bits 12..17)
 enum Pb2InputButtonFlags inputButtonsReleased; // +0x20C T: buttons released this tick (bits 0..5)
 unsigned char teamIndex; // +0x210 T: side 0 / 1 (PB_InitFighterSlotForRound a2); same-team test in the hit passes
 unsigned char endConditionMet; // +0x211 T: set by IF 2 END_CONDITION; the object is removed next tick
 unsigned char facingLeft; // +0x212 T: 0 faces right, 1 faces left
 unsigned char nextFacingLeft; // +0x213 T: facing to turn to when the action changes
 unsigned char unreferenced_214; // +0x214 U: written 0xFF only (PB_InitFighterSlotForRound, PB_ObjResetFighterSlotKeepingDataPointers)
 unsigned char mirrorHistoryPending; // +0x215 T: PB_ObjFaceNearestOpponent sets 1 on a facing change; PB_UpdateFighterInputHistoryRings mirrors the rings and clears it
 unsigned short pendingPattern; // +0x216 T: pattern requested by PB_ObjRequestAction, 0xFFFF = none
 short pendingPatternPriority; // +0x218 T: -1 = none; a request wins when its priority is higher
 unsigned short pendingFrame; // +0x21A T: frame requested by IF jumps, 0xFFFF = none
 short pendingFramePriority; // +0x21C T
 unsigned char inputActionLatch; // +0x21E T: IF 6 / 7 with flag 4 set 1; PB_ObjRunActionScript steps 1 -> 2 -> 0
 unsigned char actionChangedFlag; // +0x21F T: PB_ObjOnActionChanged sets 1; children with spawnFlagsA bit 0x20 die
 unsigned char landed; // +0x220 T: 1 when posY reached the ground (PB_ObjIntegrateMotion)
 unsigned char actionEnded; // +0x221 T: PB_ObjRunActionScript sets it when a landing / animation end finished the action
 enum Pb2Stance stanceCopy; // +0x222 T: state.stance of the current frame (PB_ObjRunActionScript)
 unsigned char unreferenced_223; // +0x223 U: no access
 struct Pb2Box * boxTable; // +0x224 T: pattern + boxTableOffset or 0 (PB_ObjResolveFramePointers)
 struct Pb2CharDatHead * charData; // +0x228 T: g_CharPatternData[slot] (PB_BindCharacterData 0x42CA10)
 struct Pb2PatternHeader * patternHeader; // +0x22C T: current pattern (PB_ObjResolveFramePointers)
 struct Pb2FrameRecord * frameRecord; // +0x230 T: current 96-byte frame
 struct Pb2AnimFrame * animFrame; // +0x234 T: same address as frameRecord
 struct Pb2StateFrame * stateFrame; // +0x238 T: frameRecord + 24
 struct Pb2IfRecord * ifTable; // +0x23C T: pattern + ifTableOffset or 0
 struct Pb2EfRecord * efTable; // +0x240 T: pattern + efTableOffset or 0
 short * boxIndexBlock; // +0x244 T: frameRecord + 76, ten box indices
 void * spriteBank; // +0x248 T: g_CharSpriteBank[slot] (PB_BindCharacterData)
 struct Pb2FighterSlot * slotPtr; // +0x24C T: the owning slot (PB_InitFighterSlotForRound: slot + 592 = slot); PB_AddSuperMeter / HitReaction read the CT header through it
 struct Pb2Object * selfPtr; // +0x250 U: written by PB_InitFighterSlotForRound with the slot; read by PB_ObjRunActionScript (v15 = *(a1+592))
 unsigned int actionAge; // +0x254 T: ticks since the last action change (++ in PB_ObjRunActionScript, 0 in PB_ObjOnActionChanged); drives flicker (% 3, % 7)
 unsigned char ranThisTick; // +0x258 T: PB_ObjRunActionScript clears it at entry and sets 1 after the script ran (PB_DrawFighterSprite spawns trails only when 1)
 unsigned char unused_259[3]; // +0x259 U proven: no instruction in the binary has displacement 601..603 or 605..607 (tail padding)
};

struct Pb2FighterSlot {   // 5864 bytes; g_fighter_slots 0x161E980 (4 slots, ends exactly at g_demo_attract_mode 0x1624520)
 unsigned char active; // +0x00 T: 1 = slot in use (PB_InitFighterSlotForRound, PB_BindCharacterData set 1; PB_ObjResetFighterSlotKeepingDataPointers 0)
 unsigned char unreferenced_01[3]; // +0x01 U: no access
 struct Pb2Object object; // +0x04 T: slot + 4, memset 0x25C at round start
 struct Pb2CtHeader ct; // +0x260 T D: copied from the CT file by PB_SlotLoadCommandTable
 int commandCount; // +0x27C T D: CT file +0
 struct Pb2CommandMove commands[100]; // +0x280 T D: copied from the CT file (0x1068 bytes at slot + 640); PB_InitFighterSlotForRound clears record bytes 18..33
 struct Pb2InputRing ringA; // +0x12E8 T: buttons only (PB_UpdateFighterInputHistoryRings at slot + 4840)
 struct Pb2InputRing ringB; // +0x14E8 T: buttons | released (slot + 5352)
};
struct Pb2ObjectSlot {   // 608 bytes; g_object_slots 0x1624528 (1000 slots)
 unsigned char active; // +0x00 T: 0 = free (PB_SpawnChildObjectFromFrameEffect scans for it)
 unsigned char unreferenced_01[3]; // +0x01 U: no access
 struct Pb2Object object; // +0x04 T: slot + 4
};
